from __future__ import annotations

import uuid
from typing import Literal

from pydantic import BaseModel, Field, field_validator

from app.schemas.academic import DayOfWeek, TimeSlotSchema
from app.schemas.timetable import AllocatedLessonSchema
from app.services.time_utils import ALL_DAYS

ALLOWED_SHIFTS = ("MANHA", "TARDE", "NOITE")


class ProfessorProfileSummary(BaseModel):
    professor_id: uuid.UUID
    project_id: uuid.UUID
    project_name: str
    academic_year: str
    allow_preferences_edit: bool
    code: str
    nome: str
    email: str | None = None
    preferencia_dias: dict[str, int]
    turnos_pretendidos: dict[str, list[str]]
    disponibilidade: dict[str, list[TimeSlotSchema]]
    peso_janela: int
    carga_maxima_diaria_minutos: int
    max_aulas_por_dia: int


class ProfessorPreferencesUpdate(BaseModel):
    preferencia_dias: dict[DayOfWeek, int]
    turnos_pretendidos: dict[DayOfWeek, list[Literal["MANHA", "TARDE", "NOITE"]]]
    disponibilidade: dict[DayOfWeek, list[TimeSlotSchema]] | None = None
    peso_janela: int | None = Field(default=None, ge=0, le=10)

    @field_validator("preferencia_dias")
    @classmethod
    def validate_pref_dias(cls, v: dict[DayOfWeek, int]) -> dict[DayOfWeek, int]:
        for day, weight in v.items():
            if weight < 0 or weight > 10:
                raise ValueError(f"Preferência para {day} deve estar entre 0 e 10.")
        return v

    @field_validator("turnos_pretendidos")
    @classmethod
    def deduplicate_shifts(
        cls, v: dict[DayOfWeek, list[Literal["MANHA", "TARDE", "NOITE"]]]
    ) -> dict[DayOfWeek, list[Literal["MANHA", "TARDE", "NOITE"]]]:
        normalized: dict[DayOfWeek, list[Literal["MANHA", "TARDE", "NOITE"]]] = {}
        for day, shifts in v.items():
            seen = set(shifts)
            normalized[day] = [s for s in ALLOWED_SHIFTS if s in seen]  # type: ignore[misc]
        return normalized

    @field_validator("disponibilidade")
    @classmethod
    def validate_disponibilidade(
        cls, v: dict[DayOfWeek, list[TimeSlotSchema]] | None
    ) -> dict[DayOfWeek, list[TimeSlotSchema]] | None:
        if v is None:
            return None
        for day, slots in v.items():
            for slot in slots:
                if slot.inicio >= slot.fim:
                    raise ValueError(f"Faixa inválida em {day}: início ({slot.inicio}) deve ser menor que fim ({slot.fim}).")
        return v


class ProfessorTimetableResponse(BaseModel):
    project_id: uuid.UUID
    project_name: str
    professor_code: str
    professor_nome: str
    version_number: int | None = None
    version_label: str | None = None
    alocacoes: list[AllocatedLessonSchema]
    turmas_map: dict[str, str]
    salas_map: dict[str, str]


__all__ = [
    "ALL_DAYS",
    "ProfessorProfileSummary",
    "ProfessorPreferencesUpdate",
    "ProfessorTimetableResponse",
]
