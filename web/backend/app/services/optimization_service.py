from __future__ import annotations

import uuid
from datetime import datetime, timezone
from typing import Any

import redis.asyncio as aioredis
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import settings
from app.models.project import OptimizationConfigModel
from app.models.timetable import OptimizationJob
from app.schemas.optimization import OptimizeRequest
from app.workers.celery_app import celery_app
from app.workers.tasks import run_optimization_job


async def enqueue_optimization_job(
    db: AsyncSession,
    project_id: uuid.UUID,
    user_id: uuid.UUID,
    req: OptimizeRequest,
    dispatch_celery: bool = True,
) -> OptimizationJob:
    cfg = await db.get(OptimizationConfigModel, project_id)
    snapshot = req.model_dump(exclude_none=True)
    algorithm = req.algoritmo or (cfg.algoritmo if cfg else "CP_SAT")

    job = OptimizationJob(
        project_id=project_id,
        created_by=user_id,
        status="PENDING",
        algorithm=algorithm,
        config_snapshot=snapshot,
        progress_ratio=0.0,
        progress_message="Job enfileirado para execução assíncrona...",
        logs=[],
    )
    db.add(job)
    await db.commit()
    await db.refresh(job)

    if dispatch_celery:
        try:
            async_res = run_optimization_job.delay(str(job.id))
            job.celery_task_id = async_res.id
            await db.commit()
            await db.refresh(job)
        except Exception as exc:
            # Keep job record with informative message if broker is unreachable
            job.progress_message = f"Aguardando broker Redis ({exc})"
            await db.commit()

    return job


async def cancel_optimization_job(db: AsyncSession, job: OptimizationJob) -> OptimizationJob:
    if job.status in ("COMPLETED", "FAILED", "INFEASIBLE", "CANCELLED"):
        return job

    if job.celery_task_id:
        try:
            celery_app.control.revoke(job.celery_task_id, terminate=True, signal="SIGTERM")
        except Exception:
            pass

    job.status = "CANCELLED"
    job.progress_message = "Cancelamento solicitado pelo usuário."
    job.finished_at = datetime.now(timezone.utc)
    await db.commit()
    await db.refresh(job)

    try:
        r = aioredis.from_url(settings.REDIS_URL, decode_responses=True)
        await r.publish(
            f"job:{job.id}:events",
            '{"type":"done","status":"CANCELLED","message":"Cancelado pelo usuário."}',
        )
        await r.aclose()
    except Exception:
        pass

    return job
