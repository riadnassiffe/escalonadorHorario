from __future__ import annotations

from datetime import datetime
import uuid

from sqlalchemy import Boolean, DateTime, Float, ForeignKey, Index, Integer, String, Text, Uuid
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base, JSONBCompat
from app.models.auth import utcnow


class TimetableVersion(Base):
    __tablename__ = "timetable_versions"

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    version_number: Mapped[int] = mapped_column(Integer, default=1, nullable=False)
    label: Mapped[str] = mapped_column(String(200), default="Grade", nullable=False)
    salas_teoricas_designadas: Mapped[dict] = mapped_column(JSONBCompat, default=dict, nullable=False)
    estatisticas: Mapped[dict] = mapped_column(JSONBCompat, default=dict, nullable=False)
    mensagens_diagnostico: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow, nullable=False)
    created_by: Mapped[uuid.UUID | None] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True
    )

    project = relationship("Project", back_populates="timetable_versions")
    alocacoes: Mapped[list["AllocatedLessonModel"]] = relationship(
        back_populates="version", cascade="all, delete-orphan"
    )


class AllocatedLessonModel(Base):
    __tablename__ = "allocated_lessons"
    __table_args__ = (Index("ix_allocated_version_dia_start", "version_id", "dia", "start_min"),)

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    version_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("timetable_versions.id", ondelete="CASCADE"), nullable=False, index=True
    )
    aula_id: Mapped[str] = mapped_column(String(96), nullable=False)
    disciplina_id: Mapped[str] = mapped_column(String(64), nullable=False)
    disciplina_nome: Mapped[str] = mapped_column(String(200), nullable=False)
    tipo: Mapped[str] = mapped_column(String(32), default="TEORICA", nullable=False)
    dia: Mapped[str] = mapped_column(String(8), nullable=False)
    start_min: Mapped[int] = mapped_column(Integer, nullable=False)
    end_min: Mapped[int] = mapped_column(Integer, nullable=False)
    professores: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    turmas: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    salas: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    is_pinned: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    regime: Mapped[str] = mapped_column(String(32), default="ANUAL", nullable=False)
    semestre: Mapped[str] = mapped_column(String(32), default="AMBOS", nullable=False)
    semestre_alocado: Mapped[int] = mapped_column(Integer, default=1, nullable=False)

    version: Mapped[TimetableVersion] = relationship(back_populates="alocacoes")


class OptimizationJob(Base):
    __tablename__ = "optimization_jobs"

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    created_by: Mapped[uuid.UUID | None] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True
    )
    celery_task_id: Mapped[str | None] = mapped_column(String(128), nullable=True, index=True)
    status: Mapped[str] = mapped_column(String(32), default="PENDING", nullable=False)
    algorithm: Mapped[str] = mapped_column(String(32), default="CP_SAT", nullable=False)
    config_snapshot: Mapped[dict] = mapped_column(JSONBCompat, default=dict, nullable=False)
    progress_ratio: Mapped[float] = mapped_column(Float, default=0.0, nullable=False)
    progress_message: Mapped[str] = mapped_column(String(500), default="Aguardando na fila...", nullable=False)
    logs: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    result_version_id: Mapped[uuid.UUID | None] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("timetable_versions.id", ondelete="SET NULL"), nullable=True
    )
    error_detail: Mapped[str | None] = mapped_column(Text, nullable=True)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=utcnow, nullable=False)
    started_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    finished_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    project = relationship("Project", back_populates="optimization_jobs")
