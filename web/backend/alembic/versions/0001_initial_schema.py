"""Initial schema

Revision ID: 0001_initial_schema
Revises:
Create Date: 2026-11-23 16:20:00

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

revision: str = "0001_initial_schema"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def json_col() -> sa.JSON:
    return sa.JSON().with_variant(postgresql.JSONB(astext_type=sa.Text()), "postgresql")


def upgrade() -> None:
    op.create_table(
        "institutions",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("name", sa.String(200), nullable=False),
        sa.Column("slug", sa.String(120), nullable=False, unique=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_institutions_slug", "institutions", ["slug"], unique=True)

    op.create_table(
        "users",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("institution_id", sa.Uuid(as_uuid=True), sa.ForeignKey("institutions.id", ondelete="CASCADE"), nullable=False),
        sa.Column("email", sa.String(255), nullable=False, unique=True),
        sa.Column("full_name", sa.String(200), nullable=False),
        sa.Column("password_hash", sa.String(255), nullable=False),
        sa.Column("role", sa.String(32), nullable=False),
        sa.Column("is_active", sa.Boolean(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_users_institution_id", "users", ["institution_id"])
    op.create_index("ix_users_email", "users", ["email"], unique=True)

    op.create_table(
        "projects",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("institution_id", sa.Uuid(as_uuid=True), sa.ForeignKey("institutions.id", ondelete="CASCADE"), nullable=False),
        sa.Column("created_by", sa.Uuid(as_uuid=True), sa.ForeignKey("users.id", ondelete="SET NULL"), nullable=True),
        sa.Column("name", sa.String(200), nullable=False),
        sa.Column("description", sa.Text(), nullable=False),
        sa.Column("academic_year", sa.String(32), nullable=False),
        sa.Column("active_timetable_version_id", sa.Uuid(as_uuid=True), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("updated_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_projects_institution_id", "projects", ["institution_id"])

    op.create_table(
        "optimization_configs",
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), primary_key=True),
        sa.Column("algoritmo", sa.String(32), nullable=False),
        sa.Column("escopo_semestre", sa.String(32), nullable=False),
        sa.Column("sincronizar_anuais_entre_semestres", sa.Boolean(), nullable=False),
        sa.Column("lambda_janelas", sa.Integer(), nullable=False),
        sa.Column("lambda_dias", sa.Integer(), nullable=False),
        sa.Column("lambda_preferencias", sa.Integer(), nullable=False),
        sa.Column("lambda_preferencia_dias", sa.Integer(), nullable=False),
        sa.Column("lambda_preferencia_turnos", sa.Integer(), nullable=False),
        sa.Column("lambda_lacunas_turma", sa.Integer(), nullable=False),
        sa.Column("modelo_preferencia_dias", sa.String(32), nullable=False),
        sa.Column("preenchimento_integral_jornada", sa.Boolean(), nullable=False),
        sa.Column("distribuir_disciplinas_na_semana", sa.Boolean(), nullable=False),
        sa.Column("max_aulas_disciplina_dia_turma", sa.Integer(), nullable=False),
        sa.Column("tempo_limite_segundos", sa.Float(), nullable=False),
        sa.Column("sem_limite_tempo", sa.Boolean(), nullable=False),
        sa.Column("max_memoria_mb", sa.Integer(), nullable=False),
        sa.Column("sem_limite_memoria", sa.Boolean(), nullable=False),
        sa.Column("num_threads", sa.Integer(), nullable=False),
        sa.Column("log_search_progress", sa.Boolean(), nullable=False),
        sa.Column("usar_distribuicao_como_entrada", sa.Boolean(), nullable=False),
        sa.Column("slot_templates", json_col(), nullable=False),
    )

    op.create_table(
        "professores",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("code", sa.String(64), nullable=False),
        sa.Column("nome", sa.String(200), nullable=False),
        sa.Column("preferencia_dias", json_col(), nullable=False),
        sa.Column("disponibilidade", json_col(), nullable=False),
        sa.Column("turnos_pretendidos", json_col(), nullable=False),
        sa.Column("carga_maxima_diaria_minutos", sa.Integer(), nullable=False),
        sa.Column("max_aulas_por_dia", sa.Integer(), nullable=False),
        sa.Column("peso_janela", sa.Integer(), nullable=False),
        sa.UniqueConstraint("project_id", "code", name="uq_professor_project_code"),
    )
    op.create_index("ix_professores_project_id", "professores", ["project_id"])

    op.create_table(
        "rooms",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("code", sa.String(64), nullable=False),
        sa.Column("nome", sa.String(200), nullable=False),
        sa.Column("tipo", sa.String(32), nullable=False),
        sa.Column("capacidade", sa.Integer(), nullable=False),
        sa.Column("recursos", json_col(), nullable=False),
        sa.UniqueConstraint("project_id", "code", name="uq_room_project_code"),
    )
    op.create_index("ix_rooms_project_id", "rooms", ["project_id"])

    op.create_table(
        "turmas",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("code", sa.String(64), nullable=False),
        sa.Column("nome", sa.String(200), nullable=False),
        sa.Column("turno", sa.String(32), nullable=False),
        sa.Column("dias_letivos", json_col(), nullable=False),
        sa.Column("jornada_padrao", json_col(), nullable=False),
        sa.Column("excecoes_por_dia", json_col(), nullable=False),
        sa.Column("sala_teorica_id", sa.String(64), nullable=True),
        sa.Column("salas_teoricas_permitidas", json_col(), nullable=False),
        sa.UniqueConstraint("project_id", "code", name="uq_turma_project_code"),
    )
    op.create_index("ix_turmas_project_id", "turmas", ["project_id"])

    op.create_table(
        "disciplinas",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("code", sa.String(64), nullable=False),
        sa.Column("nome", sa.String(200), nullable=False),
        sa.Column("tipo", sa.String(32), nullable=False),
        sa.Column("duracao_minutos", sa.Integer(), nullable=False),
        sa.Column("aulas_semanais", sa.Integer(), nullable=False),
        sa.Column("regime", sa.String(32), nullable=False),
        sa.Column("semestre", sa.String(32), nullable=False),
        sa.Column("professores", json_col(), nullable=False),
        sa.Column("turmas", json_col(), nullable=False),
        sa.Column("turnos_permitidos", json_col(), nullable=False),
        sa.Column("salas_alternativas", json_col(), nullable=False),
        sa.Column("exigir_aulas_consecutivas", sa.Boolean(), nullable=False),
        sa.Column("max_aulas_por_dia_turma", sa.Integer(), nullable=False),
        sa.UniqueConstraint("project_id", "code", name="uq_disciplina_project_code"),
    )
    op.create_index("ix_disciplinas_project_id", "disciplinas", ["project_id"])

    op.create_table(
        "timetable_versions",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("version_number", sa.Integer(), nullable=False),
        sa.Column("label", sa.String(200), nullable=False),
        sa.Column("salas_teoricas_designadas", json_col(), nullable=False),
        sa.Column("estatisticas", json_col(), nullable=False),
        sa.Column("mensagens_diagnostico", json_col(), nullable=False),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("created_by", sa.Uuid(as_uuid=True), sa.ForeignKey("users.id", ondelete="SET NULL"), nullable=True),
    )
    op.create_index("ix_timetable_versions_project_id", "timetable_versions", ["project_id"])

    op.create_table(
        "allocated_lessons",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("version_id", sa.Uuid(as_uuid=True), sa.ForeignKey("timetable_versions.id", ondelete="CASCADE"), nullable=False),
        sa.Column("aula_id", sa.String(96), nullable=False),
        sa.Column("disciplina_id", sa.String(64), nullable=False),
        sa.Column("disciplina_nome", sa.String(200), nullable=False),
        sa.Column("tipo", sa.String(32), nullable=False),
        sa.Column("dia", sa.String(8), nullable=False),
        sa.Column("start_min", sa.Integer(), nullable=False),
        sa.Column("end_min", sa.Integer(), nullable=False),
        sa.Column("professores", json_col(), nullable=False),
        sa.Column("turmas", json_col(), nullable=False),
        sa.Column("salas", json_col(), nullable=False),
        sa.Column("is_pinned", sa.Boolean(), nullable=False),
        sa.Column("regime", sa.String(32), nullable=False),
        sa.Column("semestre", sa.String(32), nullable=False),
        sa.Column("semestre_alocado", sa.Integer(), nullable=False),
    )
    op.create_index("ix_allocated_lessons_version_id", "allocated_lessons", ["version_id"])
    op.create_index("ix_allocated_version_dia_start", "allocated_lessons", ["version_id", "dia", "start_min"])

    op.create_table(
        "optimization_jobs",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column("project_id", sa.Uuid(as_uuid=True), sa.ForeignKey("projects.id", ondelete="CASCADE"), nullable=False),
        sa.Column("created_by", sa.Uuid(as_uuid=True), sa.ForeignKey("users.id", ondelete="SET NULL"), nullable=True),
        sa.Column("celery_task_id", sa.String(128), nullable=True),
        sa.Column("status", sa.String(32), nullable=False),
        sa.Column("algorithm", sa.String(32), nullable=False),
        sa.Column("config_snapshot", json_col(), nullable=False),
        sa.Column("progress_ratio", sa.Float(), nullable=False),
        sa.Column("progress_message", sa.String(500), nullable=False),
        sa.Column("logs", json_col(), nullable=False),
        sa.Column("result_version_id", sa.Uuid(as_uuid=True), sa.ForeignKey("timetable_versions.id", ondelete="SET NULL"), nullable=True),
        sa.Column("error_detail", sa.Text(), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("started_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("finished_at", sa.DateTime(timezone=True), nullable=True),
    )
    op.create_index("ix_optimization_jobs_project_id", "optimization_jobs", ["project_id"])
    op.create_index("ix_optimization_jobs_celery_task_id", "optimization_jobs", ["celery_task_id"])


def downgrade() -> None:
    op.drop_table("optimization_jobs")
    op.drop_table("allocated_lessons")
    op.drop_table("timetable_versions")
    op.drop_table("disciplinas")
    op.drop_table("turmas")
    op.drop_table("rooms")
    op.drop_table("professores")
    op.drop_table("optimization_configs")
    op.drop_table("projects")
    op.drop_table("users")
    op.drop_table("institutions")
