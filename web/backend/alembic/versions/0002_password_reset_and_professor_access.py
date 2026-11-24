"""Password reset tokens and professor account access

Revision ID: 0002_password_reset_and_professor_access
Revises: 0001_initial_schema
Create Date: 2026-11-23 18:55:00

"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa

revision: str = "0002_password_reset_and_professor_access"
down_revision: Union[str, None] = "0001_initial_schema"
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    op.add_column(
        "users",
        sa.Column("must_change_password", sa.Boolean(), nullable=False, server_default=sa.false()),
    )
    op.add_column(
        "projects",
        sa.Column("allow_professor_preferences_edit", sa.Boolean(), nullable=False, server_default=sa.true()),
    )

    op.create_table(
        "password_reset_tokens",
        sa.Column("id", sa.Uuid(as_uuid=True), primary_key=True),
        sa.Column(
            "user_id",
            sa.Uuid(as_uuid=True),
            sa.ForeignKey("users.id", ondelete="CASCADE"),
            nullable=False,
        ),
        sa.Column("token_hash", sa.String(64), nullable=False),
        sa.Column("expires_at", sa.DateTime(timezone=True), nullable=False),
        sa.Column("used_at", sa.DateTime(timezone=True), nullable=True),
        sa.Column("created_at", sa.DateTime(timezone=True), nullable=False),
    )
    op.create_index("ix_password_reset_tokens_user_id", "password_reset_tokens", ["user_id"])
    op.create_index("ix_password_reset_tokens_token_hash", "password_reset_tokens", ["token_hash"], unique=True)

    with op.batch_alter_table("professores") as batch_op:
        batch_op.add_column(sa.Column("email", sa.String(255), nullable=True))
        batch_op.add_column(sa.Column("user_id", sa.Uuid(as_uuid=True), nullable=True))
        batch_op.create_index("ix_professores_email", ["email"])
        batch_op.create_index("ix_professores_user_id", ["user_id"])
        batch_op.create_foreign_key(
            "fk_professores_user_id_users",
            "users",
            ["user_id"],
            ["id"],
            ondelete="SET NULL",
        )


def downgrade() -> None:
    with op.batch_alter_table("professores") as batch_op:
        batch_op.drop_constraint("fk_professores_user_id_users", type_="foreignkey")
        batch_op.drop_index("ix_professores_user_id")
        batch_op.drop_index("ix_professores_email")
        batch_op.drop_column("user_id")
        batch_op.drop_column("email")

    op.drop_index("ix_password_reset_tokens_token_hash", table_name="password_reset_tokens")
    op.drop_index("ix_password_reset_tokens_user_id", table_name="password_reset_tokens")
    op.drop_table("password_reset_tokens")

    op.drop_column("projects", "allow_professor_preferences_edit")
    op.drop_column("users", "must_change_password")
