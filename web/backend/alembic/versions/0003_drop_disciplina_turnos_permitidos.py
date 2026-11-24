"""Drop turnos_permitidos from disciplinas

Revision ID: 0003_drop_disciplina_turnos_permitidos
Revises: 0002_password_reset_and_professor_access
Create Date: 2026-11-24 17:45:00

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa
from sqlalchemy.dialects import postgresql

revision: str = "0003_drop_disciplina_turnos_permitidos"
down_revision: Union[str, None] = "0002_password_reset_and_professor_access"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def json_col() -> sa.JSON:
    return sa.JSON().with_variant(postgresql.JSONB(astext_type=sa.Text()), "postgresql")


def upgrade() -> None:
    with op.batch_alter_table("disciplinas") as batch_op:
        batch_op.drop_column("turnos_permitidos")


def downgrade() -> None:
    with op.batch_alter_table("disciplinas") as batch_op:
        batch_op.add_column(
            sa.Column("turnos_permitidos", json_col(), nullable=False, server_default='["MANHA"]')
        )
