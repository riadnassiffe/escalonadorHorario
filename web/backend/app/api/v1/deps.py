from __future__ import annotations

import uuid

from fastapi import Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.database import get_db
from app.core.security import get_current_user
from app.models.academic import ProfessorModel
from app.models.auth import User
from app.models.project import Project


async def get_project_for_user(
    project_id: uuid.UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
) -> Project:
    result = await db.execute(
        select(Project).where(
            Project.id == project_id,
            Project.institution_id == current_user.institution_id,
        )
    )
    project = result.scalar_one_or_none()
    if project is None:
        raise HTTPException(
            status_code=status.HTTP_404_NOT_FOUND,
            detail="Projeto não encontrado.",
        )
    if current_user.role == "PROFESSOR":
        linked = (
            await db.execute(
                select(ProfessorModel.id).where(
                    ProfessorModel.project_id == project.id,
                    ProfessorModel.user_id == current_user.id,
                )
            )
        ).scalar_one_or_none()
        if linked is None:
            raise HTTPException(
                status_code=status.HTTP_404_NOT_FOUND,
                detail="Projeto não encontrado.",
            )
    return project
