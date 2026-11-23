from __future__ import annotations

from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.database import get_db
from app.core.security import require_roles
from app.models.auth import User
from app.models.project import OptimizationConfigModel, Project, default_slot_templates
from app.schemas.project import OptimizationConfigSchema, SlotTemplatesUpdate

router = APIRouter(prefix="/projects/{project_id}", tags=["config"])


@router.get("/config", response_model=OptimizationConfigSchema)
async def get_config(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    cfg = await db.get(OptimizationConfigModel, project.id)
    if cfg is None:
        cfg = OptimizationConfigModel(project_id=project.id)
        db.add(cfg)
        await db.commit()
        await db.refresh(cfg)
    return OptimizationConfigSchema.model_validate(cfg, from_attributes=True)


@router.put("/config", response_model=OptimizationConfigSchema)
async def update_config(
    req: OptimizationConfigSchema,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    cfg = await db.get(OptimizationConfigModel, project.id)
    if cfg is None:
        cfg = OptimizationConfigModel(project_id=project.id)
        db.add(cfg)
    for k, v in req.model_dump().items():
        setattr(cfg, k, v)
    await db.commit()
    await db.refresh(cfg)
    return OptimizationConfigSchema.model_validate(cfg, from_attributes=True)


@router.get("/slot-templates")
async def get_slot_templates(project: Project = Depends(get_project_for_user), db: AsyncSession = Depends(get_db)):
    cfg = await db.get(OptimizationConfigModel, project.id)
    return {"slot_templates": cfg.slot_templates if cfg else default_slot_templates()}


@router.put("/slot-templates")
async def update_slot_templates(
    req: SlotTemplatesUpdate,
    project: Project = Depends(get_project_for_user),
    _: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    cfg = await db.get(OptimizationConfigModel, project.id)
    if cfg is None:
        cfg = OptimizationConfigModel(project_id=project.id)
        db.add(cfg)
    cfg.slot_templates = {k: [s.model_dump() for s in v] for k, v in req.slot_templates.items()}
    await db.commit()
    return {"slot_templates": cfg.slot_templates}
