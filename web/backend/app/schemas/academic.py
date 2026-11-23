from __future__ import annotations

from typing import Literal
from pydantic import BaseModel, Field, model_validator

DayOfWeek = Literal["SEG", "TER", "QUA", "QUI", "SEX"]
ShiftType = Literal["MANHA", "TARDE", "NOITE", "INTEGRAL"]
RegimeType = Literal["ANUAL", "SEMESTRAL"]
SemestreType = Literal["1", "2", "AMBOS"]


class TimeSlotSchema(BaseModel):
    inicio: str = Field(pattern=r"^\d{2}:\d{2}$")
    fim: str = Field(pattern=r"^\d{2}:\d{2}$")


class JornadaTurmaSchema(BaseModel):
    slots: list[TimeSlotSchema] = Field(default_factory=list)


class ProfessorBase(BaseModel):
    code: str | None = None
    nome: str = Field(min_length=1, max_length=200)
    preferencia_dias: dict[str, int] = Field(
        default_factory=lambda: {"SEG": 5, "TER": 5, "QUA": 5, "QUI": 5, "SEX": 5}
    )
    disponibilidade: dict[str, list[TimeSlotSchema]] = Field(default_factory=dict)
    turnos_pretendidos: dict[str, list[str]] = Field(
        default_factory=lambda: {d: ["MANHA", "TARDE", "NOITE"] for d in ["SEG", "TER", "QUA", "QUI", "SEX"]}
    )
    carga_maxima_diaria_minutos: int = Field(default=0, ge=0)
    max_aulas_por_dia: int = Field(default=0, ge=0)
    peso_janela: int = Field(default=8, ge=0, le=10)


class ProfessorCreate(ProfessorBase):
    pass


class ProfessorUpdate(ProfessorBase):
    pass


class ProfessorResponse(ProfessorBase):
    id: str
    code: str


class RoomBase(BaseModel):
    code: str | None = None
    nome: str = Field(min_length=1, max_length=200)
    tipo: str = "TEORICA"
    capacidade: int = Field(default=40, ge=1)
    recursos: list[str] = Field(default_factory=list)


class RoomCreate(RoomBase):
    pass


class RoomUpdate(RoomBase):
    pass


class RoomResponse(RoomBase):
    id: str
    code: str


class TurmaBase(BaseModel):
    code: str | None = None
    nome: str = Field(min_length=1, max_length=200)
    turno: str = "MANHA"
    dias_letivos: list[str] = Field(default_factory=lambda: ["SEG", "TER", "QUA", "QUI", "SEX"])
    jornada_padrao: JornadaTurmaSchema = Field(default_factory=JornadaTurmaSchema)
    excecoes_por_dia: dict[str, JornadaTurmaSchema] = Field(default_factory=dict)
    sala_teorica_id: str | None = None
    salas_teoricas_permitidas: list[str] = Field(default_factory=list)


class TurmaCreate(TurmaBase):
    pass


class TurmaUpdate(TurmaBase):
    pass


class TurmaResponse(TurmaBase):
    id: str
    code: str


class DisciplinaBase(BaseModel):
    code: str | None = None
    nome: str = Field(min_length=1, max_length=200)
    tipo: str = "TEORICA"
    duracao_minutos: int = Field(default=50, ge=5)
    aulas_semanais: int = Field(default=4, ge=1)
    regime: str = "ANUAL"
    semestre: str = "AMBOS"
    professores: list[str] = Field(default_factory=list)
    turmas: list[str] = Field(default_factory=list)
    turnos_permitidos: list[str] = Field(default_factory=lambda: ["MANHA"])
    salas_alternativas: list[list[str]] = Field(default_factory=list)
    exigir_aulas_consecutivas: bool = False
    max_aulas_por_dia_turma: int = Field(default=2, ge=0)

    @model_validator(mode="after")
    def sync_semestre_with_regime(self) -> "DisciplinaBase":
        if self.regime == "ANUAL":
            self.semestre = "AMBOS"
        return self


class DisciplinaCreate(DisciplinaBase):
    pass


class DisciplinaUpdate(DisciplinaBase):
    pass


class DisciplinaResponse(DisciplinaBase):
    id: str
    code: str
