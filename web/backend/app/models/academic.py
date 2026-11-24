from __future__ import annotations

import uuid

from sqlalchemy import Boolean, ForeignKey, Integer, String, UniqueConstraint, Uuid
from sqlalchemy.orm import Mapped, mapped_column, relationship

from app.core.database import Base, JSONBCompat


class ProfessorModel(Base):
    __tablename__ = "professores"
    __table_args__ = (UniqueConstraint("project_id", "code", name="uq_professor_project_code"),)

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    code: Mapped[str] = mapped_column(String(64), nullable=False)
    nome: Mapped[str] = mapped_column(String(200), nullable=False)
    email: Mapped[str | None] = mapped_column(String(255), nullable=True, index=True)
    user_id: Mapped[uuid.UUID | None] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("users.id", ondelete="SET NULL"), nullable=True, index=True
    )
    preferencia_dias: Mapped[dict] = mapped_column(
        JSONBCompat, default=lambda: {"SEG": 5, "TER": 5, "QUA": 5, "QUI": 5, "SEX": 5}, nullable=False
    )
    disponibilidade: Mapped[dict] = mapped_column(JSONBCompat, default=dict, nullable=False)
    turnos_pretendidos: Mapped[dict] = mapped_column(
        JSONBCompat,
        default=lambda: {d: ["MANHA", "TARDE", "NOITE"] for d in ["SEG", "TER", "QUA", "QUI", "SEX"]},
        nullable=False,
    )
    carga_maxima_diaria_minutos: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    max_aulas_por_dia: Mapped[int] = mapped_column(Integer, default=0, nullable=False)
    peso_janela: Mapped[int] = mapped_column(Integer, default=8, nullable=False)

    project = relationship("Project", back_populates="professores")
    user = relationship("User")


class RoomModel(Base):
    __tablename__ = "rooms"
    __table_args__ = (UniqueConstraint("project_id", "code", name="uq_room_project_code"),)

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    code: Mapped[str] = mapped_column(String(64), nullable=False)
    nome: Mapped[str] = mapped_column(String(200), nullable=False)
    tipo: Mapped[str] = mapped_column(String(32), default="TEORICA", nullable=False)
    capacidade: Mapped[int] = mapped_column(Integer, default=40, nullable=False)
    recursos: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)

    project = relationship("Project", back_populates="salas")


class TurmaModel(Base):
    __tablename__ = "turmas"
    __table_args__ = (UniqueConstraint("project_id", "code", name="uq_turma_project_code"),)

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    code: Mapped[str] = mapped_column(String(64), nullable=False)
    nome: Mapped[str] = mapped_column(String(200), nullable=False)
    turno: Mapped[str] = mapped_column(String(32), default="MANHA", nullable=False)
    dias_letivos: Mapped[list] = mapped_column(
        JSONBCompat, default=lambda: ["SEG", "TER", "QUA", "QUI", "SEX"], nullable=False
    )
    jornada_padrao: Mapped[dict] = mapped_column(JSONBCompat, default=lambda: {"slots": []}, nullable=False)
    excecoes_por_dia: Mapped[dict] = mapped_column(JSONBCompat, default=dict, nullable=False)
    sala_teorica_id: Mapped[str | None] = mapped_column(String(64), nullable=True)
    salas_teoricas_permitidas: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)

    project = relationship("Project", back_populates="turmas")


class DisciplinaModel(Base):
    __tablename__ = "disciplinas"
    __table_args__ = (UniqueConstraint("project_id", "code", name="uq_disciplina_project_code"),)

    id: Mapped[uuid.UUID] = mapped_column(Uuid(as_uuid=True), primary_key=True, default=uuid.uuid4)
    project_id: Mapped[uuid.UUID] = mapped_column(
        Uuid(as_uuid=True), ForeignKey("projects.id", ondelete="CASCADE"), nullable=False, index=True
    )
    code: Mapped[str] = mapped_column(String(64), nullable=False)
    nome: Mapped[str] = mapped_column(String(200), nullable=False)
    tipo: Mapped[str] = mapped_column(String(32), default="TEORICA", nullable=False)
    duracao_minutos: Mapped[int] = mapped_column(Integer, default=50, nullable=False)
    aulas_semanais: Mapped[int] = mapped_column(Integer, default=4, nullable=False)
    regime: Mapped[str] = mapped_column(String(32), default="ANUAL", nullable=False)
    semestre: Mapped[str] = mapped_column(String(32), default="AMBOS", nullable=False)
    professores: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    turmas: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    salas_alternativas: Mapped[list] = mapped_column(JSONBCompat, default=list, nullable=False)
    exigir_aulas_consecutivas: Mapped[bool] = mapped_column(Boolean, default=False, nullable=False)
    max_aulas_por_dia_turma: Mapped[int] = mapped_column(Integer, default=2, nullable=False)

    project = relationship("Project", back_populates="disciplinas")
