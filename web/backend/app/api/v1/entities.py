from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.database import get_db
from app.core.security import require_roles
from app.models.academic import DisciplinaModel, ProfessorModel, RoomModel, TurmaModel
from app.models.auth import User
from app.models.project import Project
from app.schemas.academic import (
    DisciplinaCreate,
    DisciplinaResponse,
    DisciplinaUpdate,
    ProfessorCreate,
    ProfessorResponse,
    ProfessorUpdate,
    RoomCreate,
    RoomResponse,
    RoomUpdate,
    TurmaCreate,
    TurmaResponse,
    TurmaUpdate,
)
from app.services.time_utils import next_sequential_code

router = APIRouter(prefix="/projects/{project_id}", tags=["entities"])


def _prof_resp(p: ProfessorModel) -> ProfessorResponse:
    return ProfessorResponse(
        id=p.code,
        code=p.code,
        nome=p.nome,
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
        turnos_permitidos=d.turnos_permitidos,
        salas_alternativas=d.salas_alternativas,
        exigir_aulas_consecutivas=d.exigir_aulas_consecutivas,
        max_aulas_por_dia_turma=d.max_aulas_por_dia_turma,
    )


# --- Professores ---
@router.get("/professores", response_model=list[ProfessorResponse])
async def list_professores(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (
        await db.execute(select(ProfessorModel).where(ProfessorModel.project_id == project.id).order_by(ProfessorModel.code))
    ).scalars().all()
    return [_prof_resp(r) for r in rows]


@router.get("/professores/next-id")
async def next_professor_id(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    codes = (await db.execute(select(ProfessorModel.code).where(ProfessorModel.project_id == project.id))).scalars().all()
    return {"next_id": next_sequential_code(list(codes), "P")}


@router.post("/professores", response_model=ProfessorResponse, status_code=status.HTTP_201_CREATED)
async def create_professor(
    req: ProfessorCreate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    codes = list((await db.execute(select(ProfessorModel.code).where(ProfessorModel.project_id == project.id))).scalars().all())
    code = req.code.strip() if req.code else next_sequential_code(codes, "P")
    if code in codes:
        raise HTTPException(status_code=400, detail=f"Professor '{code}' já existe.")
    model = ProfessorModel(
        project_id=project.id,
        code=code,
        nome=req.nome,
        preferencia_dias=req.preferencia_dias,
        disponibilidade={k: [s.model_dump() for s in v] for k, v in req.disponibilidade.items()},
        turnos_pretendidos=req.turnos_pretendidos,
        carga_maxima_diaria_minutos=req.carga_maxima_diaria_minutos,
        max_aulas_por_dia=req.max_aulas_por_dia,
        peso_janela=req.peso_janela,
    )
    db.add(model)
    await db.commit()
    await db.refresh(model)
    return _prof_resp(model)


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
    row.nome = req.nome
    row.preferencia_dias = req.preferencia_dias
    row.disponibilidade = {k: [s.model_dump() for s in v] for k, v in req.disponibilidade.items()}
    row.turnos_pretendidos = req.turnos_pretendidos
    row.carga_maxima_diaria_minutos = req.carga_maxima_diaria_minutos
    row.max_aulas_por_dia = req.max_aulas_por_dia
    row.peso_janela = req.peso_janela
    await db.commit()
    await db.refresh(row)
    return _prof_resp(row)


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
        turnos_permitidos=req.turnos_permitidos,
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
    row.turnos_permitidos = req.turnos_permitidos
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
