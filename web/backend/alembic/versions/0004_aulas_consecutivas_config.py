"""Add aulas_consecutivas config fields to optimization_configs

Revision ID: 0004_aulas_consecutivas_config
Revises: 0003_drop_disciplina_turnos_permitidos
Create Date: 2026-11-27 15:00:00

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

revision: str = "0004_aulas_consecutivas_config"
down_revision: Union[str, None] = "0003_drop_disciplina_turnos_permitidos"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    with op.batch_alter_table("optimization_configs") as batch_op:
        batch_op.add_column(
            sa.Column("lambda_aulas_consecutivas", sa.Integer(), nullable=False, server_default="8")
        )
        batch_op.add_column(
            sa.Column("exigir_aulas_consecutivas_rigido", sa.Boolean(), nullable=False, server_default=sa.true())
        )


def downgrade() -> None:
    with op.batch_alter_table("optimization_configs") as batch_op:
        batch_op.drop_column("exigir_aulas_consecutivas_rigido")
        batch_op.drop_column("lambda_aulas_consecutivas")
