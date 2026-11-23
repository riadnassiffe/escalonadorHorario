from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field


class OptimizeRequest(BaseModel):
    algoritmo: str | None = None
    escopo_semestre: str | None = None
    sincronizar_anuais_entre_semestres: bool | None = None
    tempo_limite_segundos: float | None = Field(default=None, gt=0)
    sem_limite_tempo: bool | None = None
    max_memoria_mb: int | None = Field(default=None, ge=0)
    sem_limite_memoria: bool | None = None
    num_threads: int | None = Field(default=None, ge=1)
    usar_distribuicao_como_entrada: bool | None = None
    label: str | None = None


class OptimizationJobResponse(BaseModel):
    id: uuid.UUID
    project_id: uuid.UUID
    celery_task_id: str | None
    status: str
    algorithm: str
    config_snapshot: dict[str, Any]
    progress_ratio: float
    progress_message: str
    logs: list[str]
    result_version_id: uuid.UUID | None
    error_detail: str | None
    created_at: datetime
    started_at: datetime | None
    finished_at: datetime | None
