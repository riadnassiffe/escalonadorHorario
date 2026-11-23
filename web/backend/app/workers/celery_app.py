from __future__ import annotations

from celery import Celery

from app.core.config import settings

celery_app = Celery(
    "escalonador",
    broker=settings.REDIS_URL,
    backend=settings.REDIS_URL,
    include=["app.workers.tasks"],
)

celery_app.conf.update(
    task_serializer="json",
    accept_content=["json"],
    result_serializer="json",
    timezone="UTC",
    enable_utc=True,
    worker_prefetch_multiplier=1,
    task_acks_late=True,
    task_track_started=True,
    task_default_queue="optimization",
    task_routes={
        "app.workers.tasks.run_optimization_job": {"queue": "optimization"},
    },
)
