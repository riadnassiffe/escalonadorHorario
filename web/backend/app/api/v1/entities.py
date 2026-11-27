from __future__ import annotations

import copy
import uuid

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.exc import IntegrityError
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.database import get_db
from app.core.security import require_roles
from app.models.academic import DisciplinaModel, ProfessorModel, RoomModel, TurmaModel
from app.models.auth import User
from app.models.project import Project
from app.models.timetable import AllocatedLessonModel, TimetableVersion
from app.schemas.academic import (
    BatchProvisionRequest,
    BatchProvisionResponse,
    DisciplinaCreate,
    DisciplinaResponse,
    DisciplinaUpdate,
    ProfessorCreate,
    ProfessorCreateResponse,
    ProfessorCredentialResponse,
    ProfessorResponse,
    ProfessorUpdate,
    ProvisionAccessRequest,
    RoomCreate,
    RoomResponse,
    RoomUpdate,
    TurmaCopyRequest,
    TurmaCopyResponse,
    TurmaCreate,
    TurmaResponse,
    TurmaUpdate,
)
from app.services.professor_accounts import provision_professor_account
from app.services.time_utils import next_sequential_code

router = APIRouter(prefix="/projects/{project_id}", tags=["entities"])


def _prof_resp(p: ProfessorModel, user_map: dict[uuid.UUID, User] | None = None) -> ProfessorResponse:
    linked_user = user_map.get(p.user_id) if (user_map and p.user_id) else None
    return ProfessorResponse(
        id=p.code,
        code=p.code,
        nome=p.nome,
        email=p.email or (linked_user.email if linked_user else None),
        has_user_account=bool(p.user_id and linked_user is not None),
        must_change_password=bool(linked_user.must_change_password) if linked_user else False,
        preferencia_dias=p.preferencia_dias,
        disponibilidade=p.disponibilidade,
        turnos_pretendidos=p.turnos_pretendidos,
        carga_maxima_diaria_minutos=p.carga_maxima_diaria_minutos,
        max_aulas_por_dia=p.max_aulas_por_dia,
        peso_janela=p.peso_janela,
    )


def _room_resp(r: RoomModel) -> RoomResponse:
    return RoomResponse(
        id=r.code,
        code=r.code,
        nome=r.nome,
        tipo=r.tipo,
        capacidade=r.capacidade,
        recursos=r.recursos,
    )


def _turma_resp(t: TurmaModel) -> TurmaResponse:
    return TurmaResponse(
        id=t.code,
        code=t.code,
        nome=t.nome,
        turno=t.turno,
        dias_letivos=t.dias_letivos,
        jornada_padrao=t.jornada_padrao,
        excecoes_por_dia=t.excecoes_por_dia,
        sala_teorica_id=t.sala_teorica_id,
        salas_teoricas_permitidas=t.salas_teoricas_permitidas,
    )


def _disc_resp(d: DisciplinaModel) -> DisciplinaResponse:
    return DisciplinaResponse(
        id=d.code,
        code=d.code,
        nome=d.nome,
        tipo=d.tipo,
        duracao_minutos=d.duracao_minutos,
        aulas_semanais=d.aulas_semanais,
        regime=d.regime,
        semestre=d.semestre,
        professores=d.professores,
        turmas=d.turmas,
        salas_alternativas=d.salas_alternativas,
        exigir_aulas_consecutivas=d.exigir_aulas_consecutivas,
        max_aulas_por_dia_turma=d.max_aulas_por_dia_turma,
    )


async def _load_user_map(db: AsyncSession, rows: list[ProfessorModel]) -> dict[uuid.UUID, User]:
    user_ids = [r.user_id for r in rows if r.user_id is not None]
    if not user_ids:
        return {}
    users = (await db.execute(select(User).where(User.id.in_(user_ids)))).scalars().all()
    return {u.id: u for u in users}


async def _apply_professor_email_change(
    db: AsyncSession,
    project: Project,
    row: ProfessorModel,
    new_email: str | None,
) -> None:
    if new_email is None:
        return
    normalized = new_email.strip().lower()
    if not normalized:
        return

    other_user = (await db.execute(select(User).where(User.email == normalized))).scalar_one_or_none()
    if other_user is not None and other_user.institution_id != project.institution_id:
        raise HTTPException(status_code=400, detail="Este e-mail já está vinculado a outra instituição.")

    if row.user_id is not None:
        linked_user = await db.get(User, row.user_id)
        if linked_user is not None:
            if normalized != linked_user.email:
                if other_user is not None:
                    row.user_id = other_user.id
                    row.email = other_user.email
                else:
                    linked_user.email = normalized
                    row.email = normalized
            else:
                row.email = normalized
            return

    if other_user is not None:
        row.user_id = other_user.id
        row.email = other_user.email
    else:
        row.email = normalized


def _validate_professor_code(raw_code: str) -> str:
    code = raw_code.strip()
    if not code:
        raise HTTPException(status_code=400, detail="ID do professor é obrigatório.")
    if len(code) > 64:
        raise HTTPException(status_code=400, detail="ID do professor deve ter no máximo 64 caracteres.")
    if "/" in code or "\\" in code:
        raise HTTPException(status_code=400, detail="ID do professor não pode conter '/' ou '\\'.")
    return code


def _replace_code_in_list(items: list[str] | None, old_code: str, new_code: str) -> tuple[list[str], bool]:
    if not items or old_code not in items:
        return list(items or []), False
    updated: list[str] = []
    for item in items:
        mapped = new_code if item == old_code else item
        if mapped not in updated:
            updated.append(mapped)
    return updated, True


async def _propagate_professor_code_rename(
    db: AsyncSession,
    project_id: uuid.UUID,
    old_code: str,
    new_code: str,
) -> None:
    disciplinas = (
        await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project_id))
    ).scalars().all()
    for disc in disciplinas:
        updated, changed = _replace_code_in_list(disc.professores, old_code, new_code)
        if changed:
            disc.professores = updated

    version_ids = list(
        (
            await db.execute(select(TimetableVersion.id).where(TimetableVersion.project_id == project_id))
        ).scalars().all()
    )
    if version_ids:
        alocacoes = (
            await db.execute(
                select(AllocatedLessonModel).where(AllocatedLessonModel.version_id.in_(version_ids))
            )
        ).scalars().all()
        for aloc in alocacoes:
            updated, changed = _replace_code_in_list(aloc.professores, old_code, new_code)
            if changed:
                aloc.professores = updated


# --- Professores ---
@router.get("/professores", response_model=list[ProfessorResponse])
async def list_professores(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id).order_by(ProfessorModel.code))
    ).scalars().all()
    user_map = await _load_user_map(db, list(rows))
    return [_prof_resp(r, user_map) for r in rows]


@router.get("/professores/next-id")
async def next_professor_id(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    codes = (await db.execute(select(ProfessorModel.code).where(ProfessorModel.project_id == project.id))).scalars().all()
    return {"next_id": next_sequential_code(list(codes), "P")}


@router.post("/professores", response_model=ProfessorCreateResponse, status_code=status.HTTP_201_CREATED)
async def create_professor(
    req: ProfessorCreate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    codes = list((await db.execute(select(ProfessorModel.code).where(ProfessorModel.project_id == project.id))).scalars().all())
    code = (
        _validate_professor_code(req.code)
        if (req.code is not None and req.code.strip())
        else next_sequential_code(codes, "P")
    )
    if code in codes:
        raise HTTPException(status_code=400, detail=f"Professor '{code}' já existe.")
    model = ProfessorModel(
        project_id=project.id,
        code=code,
        nome=req.nome,
        email=req.email,
        preferencia_dias=dict(req.preferencia_dias),
        disponibilidade={k: [s.model_dump() for s in v] for k, v in req.disponibilidade.items()},
        turnos_pretendidos={k: list(v) for k, v in req.turnos_pretendidos.items()},
        carga_maxima_diaria_minutos=req.carga_maxima_diaria_minutos,
        max_aulas_por_dia=req.max_aulas_por_dia,
        peso_janela=req.peso_janela,
    )
    db.add(model)
    await db.flush()
    try:
        prov = await provision_professor_account(db, project, model, reset_if_exists=False)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    await db.commit()
    await db.refresh(model)
    user_map = await _load_user_map(db, [model])
    base = _prof_resp(model, user_map)
    return ProfessorCreateResponse(
        **base.model_dump(),
        initial_password=prov.initial_password,
        account_created=prov.account_created,
        email_sent=prov.email_sent,
        delivery_note=prov.delivery_note,
    )


@router.post("/professores/provision-access-batch", response_model=BatchProvisionResponse)
async def provision_professores_access_batch(
    req: BatchProvisionRequest,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    rows = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id).order_by(ProfessorModel.code))
    ).scalars().all()
    credentials: list[ProfessorCredentialResponse] = []
    created_count = 0
    reset_count = 0
    skipped_count = 0

    for row in rows:
        if req.only_missing and row.user_id is not None:
            skipped_count += 1
            continue
        try:
            prov = await provision_professor_account(db, project, row, reset_if_exists=not req.only_missing)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=f"{row.code}: {exc}") from exc

        if prov.account_created:
            created_count += 1
        elif prov.password_reset:
            reset_count += 1
        else:
            skipped_count += 1

        if prov.account_created or prov.password_reset:
            credentials.append(
                ProfessorCredentialResponse(
                    code=prov.code,
                    nome=prov.nome,
                    email=prov.email,
                    initial_password=prov.initial_password,
                    account_created=prov.account_created,
                    password_reset=prov.password_reset,
                    email_sent=prov.email_sent,
                    delivery_note=prov.delivery_note,
                )
            )

    await db.commit()
    return BatchProvisionResponse(
        credentials=credentials,
        total_processed=len(rows),
        created_count=created_count,
        reset_count=reset_count,
        skipped_count=skipped_count,
    )


@router.post("/professores/{code}/provision-access", response_model=ProfessorCredentialResponse)
async def provision_single_professor_access(
    code: str,
    req: ProvisionAccessRequest,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id, ProfessorModel.code == code))
    ).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Professor não encontrado.")

    if req.email is not None and req.email != row.email:
        await _apply_professor_email_change(db, project, row, req.email)

    try:
        prov = await provision_professor_account(db, project, row, reset_if_exists=req.reset_password)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc

    await db.commit()
    return ProfessorCredentialResponse(
        code=prov.code,
        nome=prov.nome,
        email=prov.email,
        initial_password=prov.initial_password,
        account_created=prov.account_created,
        password_reset=prov.password_reset,
        email_sent=prov.email_sent,
        delivery_note=prov.delivery_note,
    )


@router.put("/professores/{code}", response_model=ProfessorResponse)
async def update_professor(
    code: str,
    req: ProfessorUpdate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id, ProfessorModel.code == code))
    ).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Professor não encontrado.")

    old_code = row.code
    if req.code is not None:
        new_code = _validate_professor_code(req.code)
        if new_code != old_code:
            existing_id = (
                await db.execute(
                    select(ProfessorModel.id).where(
                        ProfessorModel.project_id == project.id,
                        ProfessorModel.code == new_code,
                        ProfessorModel.id != row.id,
                    )
                )
            ).scalar_one_or_none()
            if existing_id is not None:
                raise HTTPException(status_code=400, detail=f"Professor '{new_code}' já existe.")
            row.code = new_code
            await _propagate_professor_code_rename(db, project.id, old_code, new_code)

    if req.email is not None:
        await _apply_professor_email_change(db, project, row, req.email)
    elif row.user_id is None:
        row.email = None

    row.nome = req.nome
    if row.user_id is not None:
        linked_user = await db.get(User, row.user_id)
        if linked_user is not None and linked_user.role == "PROFESSOR":
            linked_user.full_name = req.nome

    row.preferencia_dias = dict(req.preferencia_dias)
    row.disponibilidade = {k: [s.model_dump() for s in v] for k, v in req.disponibilidade.items()}
    row.turnos_pretendidos = {k: list(v) for k, v in req.turnos_pretendidos.items()}
    row.carga_maxima_diaria_minutos = req.carga_maxima_diaria_minutos
    row.max_aulas_por_dia = req.max_aulas_por_dia
    row.peso_janela = req.peso_janela
    try:
        await db.commit()
    except IntegrityError as exc:
        await db.rollback()
        raise HTTPException(status_code=400, detail=f"Professor '{row.code}' já existe.") from exc
    await db.refresh(row)
    user_map = await _load_user_map(db, [row])
    return _prof_resp(row, user_map)


@router.delete("/professores/{code}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_professor(
    code: str,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id, ProfessorModel.code == code))
    ).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Professor não encontrado.")
    await db.delete(row)
    await db.commit()


# --- Salas ---
@router.get("/salas", response_model=list[RoomResponse])
async def list_salas(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (await db.execute(select(RoomModel).where(RoomModel.project_id == project.id).order_by(RoomModel.code))).scalars().all()
    return [_room_resp(r) for r in rows]


@router.get("/salas/next-id")
async def next_sala_id(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    codes = list((await db.execute(select(RoomModel.code).where(RoomModel.project_id == project.id))).scalars().all())
    return {"next_id": next_sequential_code(codes, "S")}


@router.post("/salas", response_model=RoomResponse, status_code=status.HTTP_201_CREATED)
async def create_sala(
    req: RoomCreate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    codes = list((await db.execute(select(RoomModel.code).where(RoomModel.project_id == project.id))).scalars().all())
    code = req.code.strip() if req.code else next_sequential_code(codes, "S")
    if code in codes:
        raise HTTPException(status_code=400, detail=f"Sala '{code}' já existe.")
    row = RoomModel(project_id=project.id, code=code, nome=req.nome, tipo=req.tipo, capacidade=req.capacidade, recursos=req.recursos)
    db.add(row)
    await db.commit()
    await db.refresh(row)
    return _room_resp(row)


@router.put("/salas/{code}", response_model=RoomResponse)
async def update_sala(
    code: str,
    req: RoomUpdate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (await db.execute(select(RoomModel).where(RoomModel.project_id == project.id, RoomModel.code == code))).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Sala não encontrada.")
    row.nome = req.nome
    row.tipo = req.tipo
    row.capacidade = req.capacidade
    row.recursos = req.recursos
    await db.commit()
    await db.refresh(row)
    return _room_resp(row)


@router.delete("/salas/{code}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_sala(
    code: str,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (await db.execute(select(RoomModel).where(RoomModel.project_id == project.id, RoomModel.code == code))).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Sala não encontrada.")
    await db.delete(row)
    await db.commit()


# --- Turmas ---
@router.get("/turmas", response_model=list[TurmaResponse])
async def list_turmas(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project.id).order_by(TurmaModel.code))).scalars().all()
    return [_turma_resp(r) for r in rows]


@router.get("/turmas/next-id")
async def next_turma_id(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    codes = list((await db.execute(select(TurmaModel.code).where(TurmaModel.project_id == project.id))).scalars().all())
    return {"next_id": next_sequential_code(codes, "T")}


@router.post("/turmas", response_model=TurmaResponse, status_code=status.HTTP_201_CREATED)
async def create_turma(
    req: TurmaCreate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    codes = list((await db.execute(select(TurmaModel.code).where(TurmaModel.project_id == project.id))).scalars().all())
    code = req.code.strip() if req.code else next_sequential_code(codes, "T")
    if code in codes:
        raise HTTPException(status_code=400, detail=f"Turma '{code}' já existe.")
    row = TurmaModel(
        project_id=project.id,
        code=code,
        nome=req.nome,
        turno=req.turno,
        dias_letivos=req.dias_letivos,
        jornada_padrao=req.jornada_padrao.model_dump(),
        excecoes_por_dia={k: v.model_dump() for k, v in req.excecoes_por_dia.items()},
        sala_teorica_id=req.sala_teorica_id or None,
        salas_teoricas_permitidas=req.salas_teoricas_permitidas,
    )
    db.add(row)
    await db.commit()
    await db.refresh(row)
    return _turma_resp(row)


@router.put("/turmas/{code}", response_model=TurmaResponse)
async def update_turma(
    code: str,
    req: TurmaUpdate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project.id, TurmaModel.code == code))).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Turma não encontrada.")
    row.nome = req.nome
    row.turno = req.turno
    row.dias_letivos = req.dias_letivos
    row.jornada_padrao = req.jornada_padrao.model_dump()
    row.excecoes_por_dia = {k: v.model_dump() for k, v in req.excecoes_por_dia.items()}
    row.sala_teorica_id = req.sala_teorica_id or None
    row.salas_teoricas_permitidas = req.salas_teoricas_permitidas
    await db.commit()
    await db.refresh(row)
    return _turma_resp(row)


@router.delete("/turmas/{code}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_turma(
    code: str,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project.id, TurmaModel.code == code))).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Turma não encontrada.")
    await db.delete(row)
    await db.commit()


@router.post("/turmas/{code}/copy", response_model=TurmaCopyResponse, status_code=status.HTTP_201_CREATED)
async def copy_turma(
    code: str,
    req: TurmaCopyRequest,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    source = (
        await db.execute(select(TurmaModel).where(TurmaModel.project_id == project.id, TurmaModel.code == code))
    ).scalar_one_or_none()
    if source is None:
        raise HTTPException(status_code=404, detail="Turma não encontrada.")

    nome = req.nome.strip()
    if not nome:
        raise HTTPException(status_code=400, detail="Nome da turma é obrigatório.")

    existing_codes = list(
        (await db.execute(select(TurmaModel.code).where(TurmaModel.project_id == project.id))).scalars().all()
    )
    new_code = req.code.strip() if req.code and req.code.strip() else next_sequential_code(existing_codes, "T")
    if new_code in existing_codes:
        raise HTTPException(status_code=400, detail=f"Turma '{new_code}' já existe.")

    sala_teorica_id = (req.sala_teorica_id or "").strip() or None
    if sala_teorica_id is not None:
        salas_permitidas = (
            list(req.salas_teoricas_permitidas)
            if req.salas_teoricas_permitidas is not None
            else [sala_teorica_id]
        )
    else:
        if req.salas_teoricas_permitidas is not None:
            salas_permitidas = list(req.salas_teoricas_permitidas)
        elif source.sala_teorica_id and (source.salas_teoricas_permitidas or []) == [source.sala_teorica_id]:
            rooms = (
                await db.execute(select(RoomModel).where(RoomModel.project_id == project.id).order_by(RoomModel.code))
            ).scalars().all()
            salas_permitidas = [r.code for r in rooms if (r.tipo or "").upper() == "TEORICA"]
        else:
            salas_permitidas = list(source.salas_teoricas_permitidas or [])

    all_discs: list[DisciplinaModel] = []
    discs_to_copy: list[DisciplinaModel] = []
    if req.copy_disciplinas:
        all_discs = list(
            (
                await db.execute(
                    select(DisciplinaModel)
                    .where(DisciplinaModel.project_id == project.id)
                    .order_by(DisciplinaModel.code)
                )
            ).scalars().all()
        )
        linked_discs = [d for d in all_discs if source.code in (d.turmas or [])]
        if req.disciplina_codes is not None:
            requested_codes: list[str] = []
            for raw_code in req.disciplina_codes:
                cleaned = raw_code.strip()
                if cleaned and cleaned not in requested_codes:
                    requested_codes.append(cleaned)
            linked_code_set = {d.code for d in linked_discs}
            invalid_codes = [c for c in requested_codes if c not in linked_code_set]
            if invalid_codes:
                raise HTTPException(
                    status_code=400,
                    detail=f"Disciplina(s) inválida(s) ou não vinculada(s) à turma '{source.code}': {', '.join(invalid_codes)}",
                )
            requested_set = set(requested_codes)
            discs_to_copy = [d for d in linked_discs if d.code in requested_set]
        else:
            discs_to_copy = linked_discs

    new_turma = TurmaModel(
        project_id=project.id,
        code=new_code,
        nome=nome,
        turno=source.turno,
        dias_letivos=copy.deepcopy(source.dias_letivos or []),
        jornada_padrao=copy.deepcopy(source.jornada_padrao or {"slots": []}),
        excecoes_por_dia=copy.deepcopy(source.excecoes_por_dia or {}),
        sala_teorica_id=sala_teorica_id,
        salas_teoricas_permitidas=salas_permitidas,
    )
    db.add(new_turma)

    copied_models: list[DisciplinaModel] = []
    if req.copy_disciplinas and discs_to_copy:
        used_disc_codes = [d.code for d in all_discs]
        used_disc_set = set(used_disc_codes)

        source_prefix = f"{source.code}_"
        for d in discs_to_copy:
            if d.code.startswith(source_prefix):
                candidate = f"{new_code}_{d.code[len(source_prefix):]}"
                if len(candidate) <= 64 and candidate not in used_disc_set:
                    new_disc_code = candidate
                else:
                    new_disc_code = next_sequential_code(used_disc_codes, "D")
            else:
                new_disc_code = next_sequential_code(used_disc_codes, "D")

            used_disc_codes.append(new_disc_code)
            used_disc_set.add(new_disc_code)

            new_turmas: list[str] = []
            for t_code in d.turmas or []:
                mapped = new_code if t_code == source.code else t_code
                if mapped not in new_turmas:
                    new_turmas.append(mapped)

            new_disc = DisciplinaModel(
                project_id=project.id,
                code=new_disc_code,
                nome=d.nome,
                tipo=d.tipo,
                duracao_minutos=d.duracao_minutos,
                aulas_semanais=d.aulas_semanais,
                regime=d.regime,
                semestre=d.semestre,
                professores=copy.deepcopy(d.professores or []),
                turmas=new_turmas,
                salas_alternativas=copy.deepcopy(d.salas_alternativas or []),
                exigir_aulas_consecutivas=d.exigir_aulas_consecutivas,
                max_aulas_por_dia_turma=d.max_aulas_por_dia_turma,
            )
            db.add(new_disc)
            copied_models.append(new_disc)

    await db.commit()
    await db.refresh(new_turma)
    for m in copied_models:
        await db.refresh(m)

    return TurmaCopyResponse(
        turma=_turma_resp(new_turma),
        copied_disciplinas_count=len(copied_models),
        copied_disciplinas=[_disc_resp(m) for m in copied_models],
    )


# --- Disciplinas ---
@router.get("/disciplinas", response_model=list[DisciplinaResponse])
async def list_disciplinas(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (
        await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project.id).order_by(DisciplinaModel.code))
    ).scalars().all()
    return [_disc_resp(r) for r in rows]


@router.get("/disciplinas/next-id")
async def next_disciplina_id(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    codes = list((await db.execute(select(DisciplinaModel.code).where(DisciplinaModel.project_id == project.id))).scalars().all())
    return {"next_id": next_sequential_code(codes, "D")}


@router.post("/disciplinas", response_model=DisciplinaResponse, status_code=status.HTTP_201_CREATED)
async def create_disciplina(
    req: DisciplinaCreate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    codes = list((await db.execute(select(DisciplinaModel.code).where(DisciplinaModel.project_id == project.id))).scalars().all())
    code = req.code.strip() if req.code else next_sequential_code(codes, "D")
    if code in codes:
        raise HTTPException(status_code=400, detail=f"Disciplina '{code}' já existe.")
    row = DisciplinaModel(
        project_id=project.id,
        code=code,
        nome=req.nome,
        tipo=req.tipo,
        duracao_minutos=req.duracao_minutos,
        aulas_semanais=req.aulas_semanais,
        regime=req.regime,
        semestre=req.semestre,
        professores=req.professores,
        turmas=req.turmas,
        salas_alternativas=req.salas_alternativas,
        exigir_aulas_consecutivas=req.exigir_aulas_consecutivas,
        max_aulas_por_dia_turma=req.max_aulas_por_dia_turma,
    )
    db.add(row)
    await db.commit()
    await db.refresh(row)
    return _disc_resp(row)


@router.put("/disciplinas/{code}", response_model=DisciplinaResponse)
async def update_disciplina(
    code: str,
    req: DisciplinaUpdate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (
        await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project.id, DisciplinaModel.code == code))
    ).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Disciplina não encontrada.")
    row.nome = req.nome
    row.tipo = req.tipo
    row.duracao_minutos = req.duracao_minutos
    row.aulas_semanais = req.aulas_semanais
    row.regime = req.regime
    row.semestre = req.semestre
    row.professores = req.professores
    row.turmas = req.turmas
    row.salas_alternativas = req.salas_alternativas
    row.exigir_aulas_consecutivas = req.exigir_aulas_consecutivas
    row.max_aulas_por_dia_turma = req.max_aulas_por_dia_turma
    await db.commit()
    await db.refresh(row)
    return _disc_resp(row)


@router.delete("/disciplinas/{code}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_disciplina(
    code: str,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    row = (
        await db.execute(select(DisciplinaModel).where(DisciplinaModel.project_id == project.id, DisciplinaModel.code == code))
    ).scalar_one_or_none()
    if row is None:
        raise HTTPException(status_code=404, detail="Disciplina não encontrada.")
    await db.delete(row)
    await db.commit()
