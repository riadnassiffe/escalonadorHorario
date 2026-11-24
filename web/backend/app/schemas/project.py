from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field

from app.schemas.academic import TimeSlotSchema


class OptimizationConfigSchema(BaseModel):
    algoritmo: str = "CP_SAT"
    escopo_semestre: str = "TODAS_SEM_FILTRO"
    sincronizar_anuais_entre_semestres: bool = True
    lambda_janelas: int = Field(default=8, ge=0)
    lambda_dias: int = Field(default=5, ge=0)
    lambda_preferencias: int = Field(default=3, ge=0)
    lambda_preferencia_dias: int = Field(default=7, ge=0)
    lambda_preferencia_turnos: int = Field(default=8, ge=0)
    lambda_lacunas_turma: int = Field(default=2, ge=0)
    modelo_preferencia_dias: str = "POR_DIA_TRABALHADO"
    preenchimento_integral_jornada: bool = False
    distribuir_disciplinas_na_semana: bool = True
    max_aulas_disciplina_dia_turma: int = Field(default=2, ge=0)
    tempo_limite_segundos: float = Field(default=30.0, gt=0)
    sem_limite_tempo: bool = False
    max_memoria_mb: int = Field(default=2048, ge=0)
    sem_limite_memoria: bool = False
    num_threads: int = Field(default=4, ge=1)
    log_search_progress: bool = False
    usar_distribuicao_como_entrada: bool = True


class ProjectCreate(BaseModel):
    name: str = Field(min_length=1, max_length=200)
    description: str = ""
    academic_year: str = "2026"
    load_default_example: bool = False


class ProjectUpdate(BaseModel):
    name: str | None = Field(default=None, min_length=1, max_length=200)
    description: str | None = None
    academic_year: str | None = None


class ProjectResponse(BaseModel):
    id: uuid.UUID
    institution_id: uuid.UUID
    name: str
    description: str
    academic_year: str
    active_timetable_version_id: uuid.UUID | None
    allow_professor_preferences_edit: bool = True
    created_at: datetime
    updated_at: datetime
    counts: dict[str, int] = Field(default_factory=dict)


class ProfessorPreferencesAccessRequest(BaseModel):
    allow_edit: bool


class SlotTemplatesUpdate(BaseModel):
    slot_templates: dict[str, list[TimeSlotSchema]]


class ImportJsonRequest(BaseModel):
    data: dict[str, Any]
