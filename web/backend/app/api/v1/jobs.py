from __future__ import annotations

import asyncio
import json
import uuid

import redis.asyncio as aioredis
from fastapi import APIRouter, Depends, HTTPException, Query, WebSocket, WebSocketDisconnect, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.api.v1.deps import get_project_for_user
from app.core.config import settings
from app.core.database import AsyncSessionLocal, get_db
from app.core.security import decode_token, get_current_user, require_roles
from app.models.auth import User
from app.models.project import Project
from app.models.timetable import OptimizationJob
from app.schemas.optimization import OptimizationJobResponse, OptimizeRequest
from app.services.optimization_service import cancel_optimization_job, enqueue_optimization_job

router = APIRouter(tags=["jobs"])


def _to_job_resp(j: OptimizationJob) -> OptimizationJobResponse:
    return OptimizationJobResponse(
        id=j.id,
        project_id=j.project_id,
        celery_task_id=j.celery_task_id,
        status=j.status,
        algorithm=j.algorithm,
        config_snapshot=j.config_snapshot,
        progress_ratio=j.progress_ratio,
        progress_message=j.progress_message,
        logs=j.logs,
        result_version_id=j.result_version_id,
        error_detail=j.error_detail,
        created_at=j.created_at,
        started_at=j.started_at,
        finished_at=j.finished_at,
    )


@router.post(
    "/projects/{project_id}/optimize",
    response_model=OptimizationJobResponse,
    status_code=status.HTTP_202_ACCEPTED,
)
async def start_optimization(
    req: OptimizeRequest,
    project: Project = Depends(get_project_for_user),
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    job = await enqueue_optimization_job(db, project.id, current_user.id, req)
    return _to_job_resp(job)


@router.get("/projects/{project_id}/jobs", response_model=list[OptimizationJobResponse])
async def list_project_jobs(
    project: Project = Depends(get_project_for_user),
    db: AsyncSession = Depends(get_db),
):
    rows = (
        await db.execute(
            select(OptimizationJob)
            .where(OptimizationJob.project_id == project.id)
            .order_by(OptimizationJob.created_at.desc())
            .limit(30)
        )
    ).scalars().all()
    return [_to_job_resp(j) for j in rows]


@router.get("/jobs/{job_id}", response_model=OptimizationJobResponse)
async def get_job(
    job_id: uuid.UUID,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    job = await db.get(OptimizationJob, job_id)
    if job is None:
        raise HTTPException(status_code=404, detail="Job não encontrado.")
    project = await db.get(Project, job.project_id)
    if project is None or project.institution_id != current_user.institution_id:
        raise HTTPException(status_code=404, detail="Job não encontrado.")
    return _to_job_resp(job)


@router.post("/jobs/{job_id}/cancel", response_model=OptimizationJobResponse)
async def cancel_job(
    job_id: uuid.UUID,
    current_user: User = Depends(require_roles("ADMIN", "COORDENADOR")),
    db: AsyncSession = Depends(get_db),
):
    job = await db.get(OptimizationJob, job_id)
    if job is None:
        raise HTTPException(status_code=404, detail="Job não encontrado.")
    project = await db.get(Project, job.project_id)
    if project is None or project.institution_id != current_user.institution_id:
        raise HTTPException(status_code=404, detail="Job não encontrado.")
    job = await cancel_optimization_job(db, job)
    return _to_job_resp(job)


@router.websocket("/jobs/{job_id}/stream")
async def stream_job_progress(websocket: WebSocket, job_id: uuid.UUID, token: str = Query(...)):
    try:
        payload = decode_token(token)
        inst_id = uuid.UUID(str(payload["inst"]))
    except Exception:
        await websocket.close(code=4401)
        return

    async with AsyncSessionLocal() as db:
        job = await db.get(OptimizationJob, job_id)
        if job is None:
            await websocket.close(code=4404)
            return
        project = await db.get(Project, job.project_id)
        if project is None or project.institution_id != inst_id:
            await websocket.close(code=4403)
            return

        await websocket.accept()
        await websocket.send_json({
            "type": "done" if job.status in ("COMPLETED", "FAILED", "INFEASIBLE", "CANCELLED") else "progress",
            "status": job.status,
            "ratio": job.progress_ratio,
            "message": job.progress_message,
            "result_version_id": str(job.result_version_id) if job.result_version_id else None,
        })
        if job.status in ("COMPLETED", "FAILED", "INFEASIBLE", "CANCELLED"):
            await websocket.close()
            return

    redis_conn = None
    pubsub = None
    try:
        redis_conn = aioredis.from_url(settings.REDIS_URL, decode_responses=True)
        pubsub = redis_conn.pubsub()
        await pubsub.subscribe(f"job:{job_id}:events")
        while True:
            msg = await asyncio.wait_for(pubsub.get_message(ignore_subscribe_messages=True, timeout=1.0), timeout=2.0)
            if msg and msg.get("data"):
                data = json.loads(msg["data"])
                await websocket.send_json(data)
                if data.get("type") == "done":
                    break
    except (WebSocketDisconnect, asyncio.CancelledError):
        pass
    except Exception:
        pass
    finally:
        if pubsub is not None:
            try:
                await pubsub.aclose()
            except Exception:
                pass
        if redis_conn is not None:
            try:
                await redis_conn.aclose()
            except Exception:
                pass
        try:
            await websocket.close()
        except Exception:
            pass
