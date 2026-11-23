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
            {"inicio": "07:30", "fim": "08:20"},
            {"inicio": "08:20", "fim": "09:10"},
            {"inicio": "09:25", "fim": "10:15"},
            {"inicio": "10:15", "fim": "11:05"},
            {"inicio": "11:05", "fim": "11:55"},
        ],
        "TARDE": [
            {"inicio": "13:30", "fim": "14:20"},
            {"inicio": "14:20", "fim": "15:10"},
            {"inicio": "15:25", "fim": "16:15"},
            {"inicio": "16:15", "fim": "17:05"},
            {"inicio": "17:05", "fim": "17:55"},
        ],
        "NOITE": [
            {"inicio": "19:00", "fim": "19:50"},
            {"inicio": "19:50", "fim": "20:40"},
            {"inicio": "20:55", "fim": "21:45"},
            {"inicio": "21:45", "fim": "22:35"},
        ],
        "INTEGRAL": [
            {"inicio": "07:30", "fim": "08:20"},
            {"inicio": "08:20", "fim": "09:10"},
            {"inicio": "09:25", "fim": "10:15"},
            {"inicio": "10:15", "fim": "11:05"},
            {"inicio": "13:30", "fim": "14:20"},
            {"inicio": "14:20", "fim": "15:10"},
            {"inicio": "15:25", "fim": "16:15"},
            {"inicio": "16:15", "fim": "17:05"},
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

    modelo_preferencia_dias: Mapped[str] = mapped_column(
        String(32), default="POR_DIA_TRABALHADO", nullable=False
    )
    preenchimento_integral_jornada: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    distribuir_disciplinas_na_semana: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)
    max_aulas_disciplina_dia_turma: Mapped[int] = mapped_column(Integer, default=2, nullable=False)

    tempo_limite_segundos: Mapped[float] = mapped_column(Float, default=30.0, nullable=False)
    sem_limite_tempo: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    max_memoria_mb: Mapped[int] = mapped_column(Integer, default=2048, nullable=False)
    sem_limite_memoria: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    num_threads: Mapped[int] = mapped_column(Integer, default=4, nullable=False)
    log_search_progress: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    usar_distribuicao_como_entrada: Mapped[bool] = mapped_column(Boolean, default=True, nullable=False)

    slot_templates: Mapped[dict] = mapped_column(JSONBCompat, default=default_slot_templates, nullable=False)

    project: Mapped[Project] = relationship(back_populates="config")
