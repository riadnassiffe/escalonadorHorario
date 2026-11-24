from __future__ import annotations

import uuid

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.database import get_db
from app.core.security import get_current_user
from app.models.academic import ProfessorModel, RoomModel, TurmaModel
from app.models.auth import User
from app.models.project import Project
from app.models.timetable import AllocatedLessonModel, TimetableVersion
from app.schemas.professor_portal import (
    ProfessorPreferencesUpdate,
    ProfessorProfileSummary,
    ProfessorTimetableResponse,
)
from app.schemas.timetable import AllocatedLessonSchema
from app.services.time_utils import ALL_DAYS, minutes_to_time_str

router = APIRouter(prefix="/professor", tags=["professor-portal"])

_DAY_ORDER = {day: idx for idx, day in enumerate(ALL_DAYS)}


def _to_profile_summary(prof: ProfessorModel, project: Project) -> ProfessorProfileSummary:
    return ProfessorProfileSummary(
        professor_id=prof.id,
        project_id=project.id,
        project_name=project.name,
        academic_year=project.academic_year,
        allow_preferences_edit=project.allow_professor_preferences_edit,
        code=prof.code,
        nome=prof.nome,
        email=prof.email,
        preferencia_dias=prof.preferencia_dias,
        turnos_pretendidos=prof.turnos_pretendidos,
        disponibilidade=prof.disponibilidade,
        peso_janela=prof.peso_janela,
        carga_maxima_diaria_minutos=prof.carga_maxima_diaria_minutos,
        max_aulas_por_dia=prof.max_aulas_por_dia,
    )


async def _get_owned_professor(
    db: AsyncSession,
    professor_id: uuid.UUID,
    current_user: User,
) -> tuple[ProfessorModel, Project]:
    row = (
        await db.execute(
            select(ProfessorModel, Project)
            .join(Project, Project.id == ProfessorModel.project_id)
            .where(
                ProfessorModel.id == professor_id,
                ProfessorModel.user_id == current_user.id,
                Project.institution_id == current_user.institution_id,
            )
        )
    ).first()
    if row is None:
        raise HTTPException(status_code=status.HTTP_404_NOT_FOUND, detail="Perfil de professor não encontrado.")
    return row[0], row[1]


@router.get("/me/profiles", response_model=list[ProfessorProfileSummary])
async def list_my_professor_profiles(
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    rows = (
        await db.execute(
            select(ProfessorModel, Project)
            .join(Project, Project.id == ProfessorModel.project_id)
            .where(
                ProfessorModel.user_id == current_user.id,
                Project.institution_id == current_user.institution_id,
            )
            .order_by(Project.updated_at.desc(), ProfessorModel.code)
        )
    ).all()
    return [_to_profile_summary(prof, proj) for prof, proj in rows]


@router.patch("/me/profiles/{professor_id}/preferences", response_model=ProfessorProfileSummary)
async def update_my_professor_preferences(
    professor_id: uuid.UUID,
    req: ProfessorPreferencesUpdate,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    prof, project = await _get_owned_professor(db, professor_id, current_user)
    if not project.allow_professor_preferences_edit:
        raise HTTPException(
            status_code=status.HTTP_403_FORBIDDEN,
            detail="A edição de preferências está bloqueada pelo administrador da instituição.",
        )

    prof.preferencia_dias = {
        d: int(req.preferencia_dias.get(d, prof.preferencia_dias.get(d, 5)))
        for d in ALL_DAYS
    }
    prof.turnos_pretendidos = {
        d: list(req.turnos_pretendidos.get(d, prof.turnos_pretendidos.get(d, [])))
        for d in ALL_DAYS
    }
    if req.disponibilidade is not None:
        prof.disponibilidade = {
            d: [s.model_dump() for s in req.disponibilidade.get(d, [])]
            for d in ALL_DAYS
        }
    if req.peso_janela is not None:
        prof.peso_janela = req.peso_janela

    await db.commit()
    await db.refresh(prof)
    return _to_profile_summary(prof, project)


@router.get("/me/profiles/{professor_id}/timetable", response_model=ProfessorTimetableResponse)
async def get_my_professor_timetable(
    professor_id: uuid.UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    prof, project = await _get_owned_professor(db, professor_id, current_user)

    turmas = (await db.execute(select(TurmaModel).where(TurmaModel.project_id == project.id))).scalars().all()
    salas = (await db.execute(select(RoomModel).where(RoomModel.project_id == project.id))).scalars().all()
    turmas_map = {t.code: t.nome for t in turmas}
    salas_map = {r.code: r.nome for r in salas}

    version: TimetableVersion | None = None
    alocs_out: list[AllocatedLessonSchema] = []

    if project.active_timetable_version_id:
        version = await db.get(TimetableVersion, project.active_timetable_version_id)
        if version is not None and version.project_id == project.id:
            rows = (
                await db.execute(
                    select(AllocatedLessonModel).where(AllocatedLessonModel.version_id == version.id)
                )
            ).scalars().all()
            filtered = [r for r in rows if prof.code in (r.professores or [])]
            filtered.sort(key=lambda a: (_DAY_ORDER.get(a.dia, 99), a.start_min, a.disciplina_nome))
            alocs_out = [
                AllocatedLessonSchema(
                    aula_id=a.aula_id,
                    disciplina_id=a.disciplina_id,
                    disciplina_nome=a.disciplina_nome,
                    tipo=a.tipo,
                    dia=a.dia,
                    inicio=minutes_to_time_str(a.start_min),
                    fim=minutes_to_time_str(a.end_min),
                    start_min=a.start_min,
                    end_min=a.end_min,
                    professores=list(a.professores or []),
                    turmas=list(a.turmas or []),
                    salas=list(a.salas or []),
                    is_pinned=a.is_pinned,
                    regime=a.regime,
                    semestre=a.semestre,
                    semestre_alocado=a.semestre_alocado,
                )
                for a in filtered
            ]

    return ProfessorTimetableResponse(
        project_id=project.id,
        project_name=project.name,
        professor_code=prof.code,
        professor_nome=prof.nome,
        version_number=version.version_number if version else None,
        version_label=version.label if version else None,
        alocacoes=alocs_out,
        turmas_map=turmas_map,
        salas_map=salas_map,
    )
