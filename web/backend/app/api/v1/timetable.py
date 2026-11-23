from __future__ import annotations

import uuid

from fastapi import APIRouter, Depends, HTTPException, Query
from fastapi.responses import HTMLResponse, Response
from sqlalchemy import delete, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.database import get_db
from app.core.security import require_roles
from app.models.academic import DisciplinaModel
from app.models.auth import User
from app.models.project import Project
from app.models.timetable import AllocatedLessonModel, TimetableVersion
from app.schemas.timetable import (
    AllocateManualRequest,
    AllocatedLessonSchema,
    ChangeRoomRequest,
    EvaluateMoveRequest,
    EvaluateSwapRequest,
    MoveLessonRequest,
    PinAllRequest,
    PinLessonRequest,
    SlotConflictInfoSchema,
    SwapLessonRequest,
    TimetableResponse,
    TimetableVersionSummary,
)
from app.services.conflict_engine import (
    evaluate_lesson_at_slot,
    evaluate_lesson_swap,
    verify_timetable_conflicts,
)
from app.services.project_io import build_project_snapshot
from app.services.report_exporter import export_csv_report, export_html_report
from app.services.time_utils import minutes_to_time_str, normalize_day, time_str_to_minutes

router = APIRouter(prefix="/projects/{project_id}", tags=["timetable"])


async def _ensure_active_version(db: AsyncSession, project: Project, user_id: uuid.UUID | None = None) -> TimetableVersion:
    if project.active_timetable_version_id:
        ver = await db.get(TimetableVersion, project.active_timetable_version_id)
        if ver:
            return ver
    existing = (
        await db.execute(
            select(TimetableVersion).where(TimetableVersion.project_id == project.id).order_by(TimetableVersion.version_number.desc())
        )
    ).scalars().first()
    if existing:
        project.active_timetable_version_id = existing.id
        await db.commit()
        return existing
    ver = TimetableVersion(
        project_id=project.id,
        version_number=1,
        label="Edição Manual v1",
        salas_teoricas_designadas={},
        estatisticas={"status": "MANUAL", "total_aulas_alocadas": 0, "total_aulas_esperadas": 0},
        mensagens_diagnostico=[],
        created_by=user_id,
    )
    db.add(ver)
    await db.flush()
    project.active_timetable_version_id = ver.id
    await db.commit()
    await db.refresh(ver)
    return ver


def _to_aloc_schema(a: dict) -> AllocatedLessonSchema:
    return AllocatedLessonSchema(
        aula_id=a["aula_id"],
        disciplina_id=a["disciplina_id"],
        disciplina_nome=a["disciplina_nome"],
        tipo=a["tipo"],
        dia=a["dia"],
        inicio=minutes_to_time_str(a["start_min"]),
        fim=minutes_to_time_str(a["end_min"]),
        start_min=a["start_min"],
        end_min=a["end_min"],
        professores=a["professores"],
        turmas=a["turmas"],
        salas=a["salas"],
        is_pinned=a["is_pinned"],
        regime=a["regime"],
        semestre=a["semestre"],
        semestre_alocado=a["semestre_alocado"],
    )


@router.get("/timetable", response_model=TimetableResponse)
async def get_timetable(
    version_id: uuid.UUID | None = Query(default=None),
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    snap, ver = await build_project_snapshot(db, project.id, version_id=version_id)
    conflitos_raw = verify_timetable_conflicts(snap)
    conflitos = {
        aid: [SlotConflictInfoSchema(**c.to_dict()) for c in items]
        for aid, items in conflitos_raw.items()
    }
    return TimetableResponse(
        version_id=ver.id if ver else None,
        version_number=ver.version_number if ver else None,
        label=ver.label if ver else None,
        alocacoes=[_to_aloc_schema(a) for a in snap.alocacoes],
        salas_teoricas_designadas=ver.salas_teoricas_designadas if ver else {},
        estatisticas=ver.estatisticas if ver else {},
        mensagens_diagnostico=ver.mensagens_diagnostico if ver else [],
        conflitos=conflitos,
    )


@router.get("/timetable/versions", response_model=list[TimetableVersionSummary])
async def list_versions(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    rows = (
        await db.execute(
            select(TimetableVersion)
            .where(TimetableVersion.project_id == project.id)
            .order_by(TimetableVersion.version_number.desc())
        )
    ).scalars().all()
    return [
        TimetableVersionSummary(
            id=v.id,
            version_number=v.version_number,
            label=v.label,
            estatisticas=v.estatisticas,
            created_at=v.created_at,
            is_active=(v.id == project.active_timetable_version_id),
        )
        for v in rows
    ]


@router.post("/timetable/versions/{version_id}/activate")
async def activate_version(
    version_id: uuid.UUID,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await db.get(TimetableVersion, version_id)
    if ver is None or ver.project_id != project.id:
        raise HTTPException(status_code=404, detail="Versão não encontrada.")
    project.active_timetable_version_id = ver.id
    await db.commit()
    return {"ok": True, "active_timetable_version_id": str(ver.id)}


@router.get("/timetable/conflicts")
async def get_conflicts(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    snap, _ = await build_project_snapshot(db, project.id)
    conflitos = verify_timetable_conflicts(snap)
    return {aid: [c.to_dict() for c in items] for aid, items in conflitos.items()}


@router.get("/timetable/unallocated")
async def get_unallocated(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    snap, _ = await build_project_snapshot(db, project.id)
    allocated_ids = {a["aula_id"] for a in snap.alocacoes}
    unallocated = []
    for d_code, disc in snap.disciplinas.items():
        for i in range(1, int(disc["aulas_semanais"]) + 1):
            aula_id = f"{d_code}_{i}"
            if aula_id not in allocated_ids:
                unallocated.append({
                    "aula_id": aula_id,
                    "disciplina_id": d_code,
                    "disciplina_nome": disc["nome"],
                    "tipo": disc["tipo"],
                    "duracao_minutos": disc["duracao_minutos"],
                    "regime": disc["regime"],
                    "semestre": disc["semestre"],
                    "professores": disc["professores"],
                    "turmas": disc["turmas"],
                })
    return {"unallocated": unallocated}


@router.post("/timetable/evaluate-move")
async def evaluate_move(
    req: EvaluateMoveRequest,
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    snap, _ = await build_project_snapshot(db, project.id)
    items = evaluate_lesson_at_slot(
        snap,
        req.aula_id,
        normalize_day(req.dia),
        time_str_to_minutes(req.inicio),
        time_str_to_minutes(req.fim),
        ignore_aula_ids=req.ignore_aula_ids,
        salas_override=req.salas_override,
    )
    return {"conflicts": [c.to_dict() for c in items]}


@router.post("/timetable/move")
async def move_lesson(
    req: MoveLessonRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    lesson = (
        await db.execute(
            select(AllocatedLessonModel).where(
                AllocatedLessonModel.version_id == ver.id,
                AllocatedLessonModel.aula_id == req.aula_id,
            )
        )
    ).scalar_one_or_none()
    if lesson is None:
        raise HTTPException(status_code=404, detail="Aula não encontrada na grade ativa.")
    lesson.dia = normalize_day(req.dia)
    lesson.start_min = time_str_to_minutes(req.inicio)
    lesson.end_min = time_str_to_minutes(req.fim)
    if req.salas is not None:
        lesson.salas = req.salas
    if req.pin_after_move:
        lesson.is_pinned = True
    await db.commit()
    return {"ok": True}


@router.post("/timetable/evaluate-swap")
async def evaluate_swap(
    req: EvaluateSwapRequest,
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    snap, _ = await build_project_snapshot(db, project.id)
    items = evaluate_lesson_swap(snap, req.aula_a_id, req.aula_b_id)
    return {"conflicts": [c.to_dict() for c in items]}


@router.post("/timetable/swap")
async def swap_lessons(
    req: SwapLessonRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    a = (
        await db.execute(
            select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == req.aula_a_id)
        )
    ).scalar_one_or_none()
    b = (
        await db.execute(
            select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == req.aula_b_id)
        )
    ).scalar_one_or_none()
    if a is None or b is None:
        raise HTTPException(status_code=404, detail="Uma ou ambas as aulas não foram encontradas.")

    dur_a = a.end_min - a.start_min
    dur_b = b.end_min - b.start_min
    dia_a, start_a = a.dia, a.start_min
    dia_b, start_b = b.dia, b.start_min

    a.dia, a.start_min, a.end_min = dia_b, start_b, start_b + dur_a
    b.dia, b.start_min, b.end_min = dia_a, start_a, start_a + dur_b
    if req.pin_both:
        a.is_pinned = True
        b.is_pinned = True
    await db.commit()
    return {"ok": True}


@router.post("/timetable/change-room")
async def change_room(
    req: ChangeRoomRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    lesson = (
        await db.execute(
            select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == req.aula_id)
        )
    ).scalar_one_or_none()
    if lesson is None:
        raise HTTPException(status_code=404, detail="Aula não encontrada.")
    old_salas = list(lesson.salas)
    lesson.salas = req.salas
    if req.pin_after:
        lesson.is_pinned = True
    if req.swap_with_aula_id:
        other = (
            await db.execute(
                select(AllocatedLessonModel).where(
                    AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == req.swap_with_aula_id
                )
            )
        ).scalar_one_or_none()
        if other:
            other.salas = old_salas
            if req.pin_after:
                other.is_pinned = True
    await db.commit()
    return {"ok": True}


@router.post("/timetable/allocate-manual")
async def allocate_manual(
    req: AllocateManualRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    disc = (
        await db.execute(
            select(DisciplinaModel).where(DisciplinaModel.project_id == project.id, DisciplinaModel.code == req.disciplina_id)
        )
    ).scalar_one_or_none()
    if disc is None:
        raise HTTPException(status_code=404, detail="Disciplina não encontrada.")

    existing = (
        await db.execute(
            select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == req.aula_id)
        )
    ).scalar_one_or_none()
    if existing:
        existing.dia = normalize_day(req.dia)
        existing.start_min = time_str_to_minutes(req.inicio)
        existing.end_min = time_str_to_minutes(req.fim)
        existing.salas = req.salas
        existing.semestre_alocado = req.semestre_alocado
        existing.is_pinned = req.is_pinned
    else:
        db.add(
            AllocatedLessonModel(
                version_id=ver.id,
                aula_id=req.aula_id,
                disciplina_id=disc.code,
                disciplina_nome=disc.nome,
                tipo=disc.tipo,
                dia=normalize_day(req.dia),
                start_min=time_str_to_minutes(req.inicio),
                end_min=time_str_to_minutes(req.fim),
                professores=list(disc.professores),
                turmas=list(disc.turmas),
                salas=req.salas,
                is_pinned=req.is_pinned,
                regime=disc.regime,
                semestre=disc.semestre,
                semestre_alocado=req.semestre_alocado,
            )
        )
    await db.commit()
    return {"ok": True}


@router.patch("/timetable/lessons/{aula_id}/pin")
async def pin_lesson(
    aula_id: str,
    req: PinLessonRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    lesson = (
        await db.execute(
            select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == aula_id)
        )
    ).scalar_one_or_none()
    if lesson is None:
        raise HTTPException(status_code=404, detail="Aula não encontrada.")
    lesson.is_pinned = req.pinned
    await db.commit()
    return {"ok": True, "is_pinned": lesson.is_pinned}


@router.post("/timetable/pin-all")
async def pin_all_lessons(
    req: PinAllRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    rows = (await db.execute(select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id))).scalars().all()
    for r in rows:
        r.is_pinned = req.pinned
    await db.commit()
    return {"ok": True, "count": len(rows)}


@router.delete("/timetable/lessons/{aula_id}")
async def remove_lesson_from_timetable(
    aula_id: str,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    ver = await _ensure_active_version(db, project, current_user.id)
    await db.execute(
        delete(AllocatedLessonModel).where(AllocatedLessonModel.version_id == ver.id, AllocatedLessonModel.aula_id == aula_id)
    )
    await db.commit()
    return {"ok": True}


@router.delete("/timetable")
async def clear_timetable(
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    if project.active_timetable_version_id:
        await db.execute(
            delete(AllocatedLessonModel).where(AllocatedLessonModel.version_id == project.active_timetable_version_id)
        )
        await db.commit()
    return {"ok": True}


@router.get("/export/csv")
async def export_csv(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    snap, _ = await build_project_snapshot(db, project.id)
    csv_text = export_csv_report(snap)
    return Response(
        content=csv_text.encode("utf-8"),
        media_type="text/csv; charset=utf-8",
        headers={"Content-Disposition": f'attachment; filename="{project.name}_grade.csv"'},
    )


@router.get("/export/html")
async def export_html(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    snap, ver = await build_project_snapshot(db, project.id)
    html_text = export_html_report(
        project.name,
        snap,
        ver.estatisticas if ver else {},
        ver.salas_teoricas_designadas if ver else {},
    )
    return HTMLResponse(
        content=html_text,
        headers={"Content-Disposition": f'attachment; filename="{project.name}_grade.html"'},
    )
