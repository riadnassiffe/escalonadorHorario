from __future__ import annotations

from datetime import datetime
import uuid

from sqlalchemy import Boolean, DateTime, Float, ForeignKey, Integer, String, Text, Uuid
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base, JSONBCompat
from app.models.auth import utcnow


def default_slot_templates() -> dict:
    return {
        "MANHA": [
            {"inicio": "08:00", "fim": "08:45"},
            {"inicio": "08:45", "fim": "09:30"},
            {"inicio": "09:45", "fim": "10:30"},
            {"inicio": "10:30", "fim": "11:15"},
            {"inicio": "11:15", "fim": "12:00"},
        ],
        "TARDE": [
            {"inicio": "13:15", "fim": "14:00"},
            {"inicio": "14:00", "fim": "14:45"},
            {"inicio": "14:45", "fim": "15:30"},
            {"inicio": "15:45", "fim": "16:30"},
            {"inicio": "16:30", "fim": "17:15"},
        ],
        "NOITE": [
            {"inicio": "18:30", "fim": "19:20"},
            {"inicio": "19:20", "fim": "20:10"},
            {"inicio": "20:20", "fim": "21:10"},
            {"inicio": "21:10", "fim": "22:00"},
        ],
        "INTEGRAL": [
            {"inicio": "08:00", "fim": "08:45"},
            {"inicio": "08:45", "fim": "09:30"},
            {"inicio": "09:45", "fim": "10:30"},
            {"inicio": "10:30", "fim": "11:15"},
            {"inicio": "11:15", "fim": "12:00"},
            {"inicio": "13:15", "fim": "14:00"},
            {"inicio": "14:00", "fim": "14:45"},
            {"inicio": "14:45", "fim": "15:30"},
            {"inicio": "15:45", "fim": "16:30"},
            {"inicio": "16:30", "fim": "17:15"},
        ],
    }


class Project(Base):
    __tablename__ = "projects"

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    institution_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("institutions.id", ondelete="CASCADE"), nullable=False, index=True
    )
    created_by: Mapped[uuid.UUID | None] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True
    )
    name: Mapped[str] = mapped_column(String(200), nullable=False)
    description: Mapped[str] = mapped_column(Text, default="", nullable=False)
    academic_year: Mapped[str] = mapped_column(String(32), default="2026", nullable=False)
    active_timetable_version_id: Mapped[uuid.UUID | None] = mapped_column(Uuid(as_uuid=True), nullable=True)
    allow_professor_preferences_edit: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow, nullable=False)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), default=utcnow, onupdate=utcnow, nullable=False
    )

    institution = relationship("Institution", back_populates="projects")
    config: Mapped["OptimizationConfigModel"] = relationship(
        back_populates="project", uselist=False, cascade="all, delete-orphan"
    )
    professores = relationship("ProfessorModel", back_populates="project", cascade="all, delete-orphan")
    salas = relationship("RoomModel", back_populates="project", cascade="all, delete-orphan")
    turmas = relationship("TurmaModel", back_populates="project", cascade="all, delete-orphan")
    disciplinas = relationship("DisciplinaModel", back_populates="project", cascade="all, delete-orphan")
    timetable_versions = relationship("TimetableVersion", back_populates="project", cascade="all, delete-orphan")
    optimization_jobs = relationship("OptimizationJob", back_populates="project", cascade="all, delete-orphan")


class OptimizationConfigModel(Base):
    __tablename__ = "optimization_configs"

    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), primary_key=True
    )
    algoritmo: Mapped[str] = mapped_column(String(32), default="CP_SAT", nullable=False)
    escopo_semestre: Mapped[str] = mapped_column(String(32), default="TODAS_SEM_FILTRO", nullable=False)
    sincronizar_anuais_entre_semestres: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)

    lambda_janelas: Mapped[int] = mapped_column(Integer, default=8, nullable=False)
    lambda_dias: Mapped[int] = mapped_column(Integer, default=5, nullable=False)
    lambda_preferencias: Mapped[int] = mapped_column(Integer, default=3, nullable=False)
    lambda_preferencia_dias: Mapped[int] = mapped_column(Integer, default=7, nullable=False)
    lambda_preferencia_turnos: Mapped[int] = mapped_column(Integer, default=8, nullable=False)
    lambda_lacunas_turma: Mapped[int] = mapped_column(Integer, default=2, nullable=False)
    lambda_aulas_consecutivas: Mapped[int] = mapped_column(Integer, default=8, nullable=False)

    modelo_preferencia_dias: Mapped[str] = mapped_column(
        String(32), default="POR_DIA_TRABALHADO", nullable=False
    )
    preenchimento_integral_jornada: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    distribuir_disciplinas_na_semana: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    exigir_aulas_consecutivas_rigido: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)

    tempo_limite_segundos: Mapped[float] = mapped_column(Float, default=30.0, nullable=False)
    sem_limite_tempo: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    max_memoria_mb: Mapped[int] = mapped_column(Integer, default=2048, nullable=False)
    sem_limite_memoria: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    num_threads: Mapped[int] = mapped_column(Integer, default=4, nullable=False)
    log_search_progress: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    usar_distribuicao_como_entrada: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)

    slot_templates: Mapped[dict] = mapped_column(JSONBCompat, default=default_slot_templates, nullable=False)

    project: Mapped[Project] = relationship(back_populates="config")
