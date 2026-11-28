"""Drop global max_aulas_disciplina_dia_turma from optimization_configs

Revision ID: 0005_drop_global_max_aulas_disciplina_dia
Revises: 0004_aulas_consecutivas_config
Create Date: 2026-11-28 10:45:00

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

revision: str = "0005_drop_global_max_aulas_disciplina_dia"
down_revision: Union[str, None] = "0004_aulas_consecutivas_config"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.execute(
        sa.text(
            """
            UPDATE disciplinas
            SET max_aulas_por_dia_turma = COALESCE(
                (
                    SELECT optimization_configs.max_aulas_disciplina_dia_turma
                    FROM optimization_configs
                    WHERE optimization_configs.project_id = disciplinas.project_id
                ),
                2
            )
            WHERE max_aulas_por_dia_turma <= 0
            """
        )
    )
    with op.batch_alter_table("optimization_configs") as batch_op:
        batch_op.drop_column("max_aulas_disciplina_dia_turma")


def downgrade() -> None:
    with op.batch_alter_table("optimization_configs") as batch_op:
        batch_op.add_column(
            sa.Column("max_aulas_disciplina_dia_turma", sa.Integer(), nullable=False, server_default="2")
        )
