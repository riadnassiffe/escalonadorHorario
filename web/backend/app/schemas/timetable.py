from __future__ import annotations

import uuid
from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field


class SlotConflictInfoSchema(BaseModel):
    type: str
    short_label: str
    detail: str
    related_aula_id: str = ""


class AllocatedLessonSchema(BaseModel):
    aula_id: str
    disciplina_id: str
    disciplina_nome: str
    tipo: str = "TEORICA"
    dia: str
    inicio: str
    fim: str
    start_min: int
    end_min: int
    professores: list[str] = Field(default_factory=list)
    turmas: list[str] = Field(default_factory=list)
    salas: list[str] = Field(default_factory=list)
    is_pinned: bool = False
    regime: str = "ANUAL"
    semestre: str = "AMBOS"
    semestre_alocado: int = 1


class TimetableVersionSummary(BaseModel):
    id: uuid.UUID
    version_number: int
    label: str
    estatisticas: dict[str, Any]
    created_at: datetime
    is_active: bool


class TimetableResponse(BaseModel):
    version_id: uuid.UUID | None
    version_number: int | None
    label: str | None
    alocacoes: list[AllocatedLessonSchema] = Field(default_factory=list)
    salas_teoricas_designadas: dict[str, str] = Field(default_factory=dict)
    estatisticas: dict[str, Any] = Field(default_factory=dict)
    mensagens_diagnostico: list[str] = Field(default_factory=list)
    conflitos: dict[str, list[SlotConflictInfoSchema]] = Field(default_factory=dict)


class EvaluateMoveRequest(BaseModel):
    aula_id: str
    dia: str
    inicio: str
    fim: str
    salas_override: list[str] = Field(default_factory=list)
    ignore_aula_ids: list[str] = Field(default_factory=list)


class MoveLessonRequest(BaseModel):
    aula_id: str
    dia: str
    inicio: str
    fim: str
    salas: list[str] | None = None
    pin_after_move: bool = False


class EvaluateSwapRequest(BaseModel):
    aula_a_id: str
    aula_b_id: str


class SwapLessonRequest(BaseModel):
    aula_a_id: str
    aula_b_id: str
    pin_both: bool = False


class ChangeRoomRequest(BaseModel):
    aula_id: str
    salas: list[str]
    swap_with_aula_id: str | None = None
    pin_after: bool = False


class AllocateManualRequest(BaseModel):
    aula_id: str
    disciplina_id: str
    dia: str
    inicio: str
    fim: str
    salas: list[str] = Field(default_factory=list)
    semestre_alocado: int = 1
    is_pinned: bool = True


class PinLessonRequest(BaseModel):
    pinned: bool


class PinAllRequest(BaseModel):
    pinned: bool
