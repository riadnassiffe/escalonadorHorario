from __future__ import annotations

from typing import Literal
from pydantic import BaseModel, EmailStr, Field, field_validator, model_validator

DayOfWeek = Literal["SEG", "TER", "QUA", "QUI", "SEX"]
ShiftType = Literal["MANHA", "TARDE", "NOITE", "INTEGRAL"]
RegimeType = Literal["ANUAL", "SEMESTRAL"]
SemestreType = Literal["1", "2", "AMBOS"]


class TimeSlotSchema(BaseModel):
    inicio: str = Field(pattern=r"^\d{2}:\d{2}$")
    fim: str = Field(pattern=r"^\d{2}:\d{2}$")


class JornadaTurmaSchema(BaseModel):
    slots: list[TimeSlotSchema] = Field(default_factory=list)


def _normalize_optional_email(v: object) -> object:
    if isinstance(v, str):
        stripped = v.strip().lower()
        return stripped or None
    return v


class ProfessorBase(BaseModel):
    code: str | None = None
    nome: str = Field(min_length=1, max_length=200)
    email: EmailStr | None = None
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

    @field_validator("email", mode="before")
    @classmethod
    def clean_email(cls, v: object) -> object:
        return _normalize_optional_email(v)


class ProfessorCreate(ProfessorBase):
    pass


class ProfessorUpdate(ProfessorBase):
    pass


class ProfessorResponse(ProfessorBase):
    id: str
    code: str
    email: str | None = None
    has_user_account: bool = False
    must_change_password: bool = False


class ProfessorCreateResponse(ProfessorResponse):
    initial_password: str | None = None
    account_created: bool = False
    email_sent: bool = False
    delivery_note: str | None = None


class ProvisionAccessRequest(BaseModel):
    email: EmailStr | None = None
    reset_password: bool = True

    @field_validator("email", mode="before")
    @classmethod
    def clean_email(cls, v: object) -> object:
        return _normalize_optional_email(v)


class ProfessorCredentialResponse(BaseModel):
    code: str
    nome: str
    email: str
    initial_password: str | None = None
    account_created: bool = False
    password_reset: bool = False
    email_sent: bool = False
    delivery_note: str | None = None


class BatchProvisionRequest(BaseModel):
    only_missing: bool = True


class BatchProvisionResponse(BaseModel):
    credentials: list[ProfessorCredentialResponse]
    total_processed: int
    created_count: int
    reset_count: int
    skipped_count: int


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


class TurmaCopyRequest(BaseModel):
    nome: str = Field(min_length=1, max_length=200)
    code: str | None = None
    copy_disciplinas: bool = False
    disciplina_codes: list[str] | None = None
    sala_teorica_id: str | None = None
    salas_teoricas_permitidas: list[str] | None = None


class TurmaCopyResponse(BaseModel):
    turma: TurmaResponse
    copied_disciplinas_count: int = 0
    copied_disciplinas: list[DisciplinaResponse] = Field(default_factory=list)


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
    salas_alternativas: list[list[str]] = Field(default_factory=list)
    exigir_aulas_consecutivas: bool = False
    max_aulas_por_dia_turma: int = Field(default=2, ge=0)

    @field_validator("salas_alternativas", mode="before")
    @classmethod
    def normalize_salas_alternativas(cls, v: object) -> list[list[str]]:
        if not isinstance(v, list):
            return []
        cleaned: list[list[str]] = []
        for combo in v:
            if not isinstance(combo, list):
                continue
            items = [s.strip() for s in combo if isinstance(s, str) and s.strip()]
            if items:
                cleaned.append(items)
        return cleaned

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
