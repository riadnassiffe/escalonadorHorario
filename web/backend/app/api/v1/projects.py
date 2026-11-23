from __future__ import annotations

import uuid

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import func, select
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.database import get_db
from app.core.security import get_current_user, require_roles
from app.models.academic import DisciplinaModel, ProfessorModel, RoomModel, TurmaModel
from app.models.auth import User
from app.models.project import OptimizationConfigModel, Project
from app.schemas.project import ImportJsonRequest, ProjectCreate, ProjectResponse, ProjectUpdate
from app.services.project_io import build_default_example_json, export_project_json, import_project_json

router = APIRouter(prefix="/projects", tags=["projects"])


async def _project_response(db: AsyncSession, project: Project) -> ProjectResponse:
    p_cnt = (await db.execute(select(func.count()).where(ProfessorModel.project_id == project.id))).scalar_one()
    r_cnt = (await db.execute(select(func.count()).where(RoomModel.project_id == project.id))).scalar_one()
    t_cnt = (await db.execute(select(func.count()).where(TurmaModel.project_id == project.id))).scalar_one()
    d_cnt = (await db.execute(select(func.count()).where(DisciplinaModel.project_id == project.id))).scalar_one()
    return ProjectResponse(
        id=project.id,
        institution_id=project.institution_id,
        name=project.name,
        description=project.description,
        academic_year=project.academic_year,
        active_timetable_version_id=project.active_timetable_version_id,
        created_at=project.created_at,
        updated_at=project.updated_at,
        counts={
            "professores": p_cnt,
            "salas": r_cnt,
            "turmas": t_cnt,
            "disciplinas": d_cnt,
        },
    )


@router.get("", response_model=list[ProjectResponse])
async def list_projects(
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    rows = (
        await db.execute(
            select(Project)
            .where(Project.institution_id == current_user.institution_id)
            .order_by(Project.updated_at.desc())
        )
    ).scalars().all()
    return [await _project_response(db, p) for p in rows]


@router.post("", response_model=ProjectResponse, status_code=status.HTTP_201_CREATED)
async def create_project(
    req: ProjectCreate,
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    project = Project(
        institution_id=current_user.institution_id,
        created_by=current_user.id,
        name=req.name,
        description=req.description,
        academic_year=req.academic_year,
    )
    db.add(project)
    await db.flush()
    db.add(OptimizationConfigModel(project_id=project.id))
    await db.commit()
    await db.refresh(project)

    if req.load_default_example:
        await import_project_json(db, project.id, build_default_example_json(), current_user.id)
        await db.refresh(project)

    return await _project_response(db, project)


@router.get("/{project_id}", response_model=ProjectResponse)
async def get_project(
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    return await _project_response(db, project)


@router.patch("/{project_id}", response_model=ProjectResponse)
async def update_project(
    req: ProjectUpdate,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    if req.name is not None:
        project.name = req.name
    if req.description is not None:
        project.description = req.description
    if req.academic_year is not None:
        project.academic_year = req.academic_year
    await db.commit()
    await db.refresh(project)
    return await _project_response(db, project)


@router.delete("/{project_id}", status_code=status.HTTP_204_NO_CONTENT)
async def delete_project(
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    await db.delete(project)
    await db.commit()


@router.post("/{project_id}/import-json")
async def import_json(
    req: ImportJsonRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    try:
        version = await import_project_json(db, project.id, req.data, current_user.id)
    except ValueError as exc:
        raise HTTPException(status_code=400, detail=str(exc)) from exc
    return {
        "ok": True,
        "version_id": str(version.id) if version else None,
    }


@router.get("/{project_id}/export-json")
async def export_json(
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    return await export_project_json(db, project.id)


@router.post("/{project_id}/load-example")
async def load_example(
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    await import_project_json(db, project.id, build_default_example_json(), current_user.id)
    return {"ok": True}
