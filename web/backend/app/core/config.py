from __future__ import annotations

from pathlib import Path
from pydantic_settings import BaseSettings, SettingsConfigDict


def _default_solver_cli_bin() -> str:
    # Resolve ../../build/escalonador_solver_cli relative to web/backend/app/core/config.py
    repo_root = Path(__file__).resolve().parents[4]
    candidate = repo_root / "build" / "escalonador_solver_cli"
    if candidate.exists():
        return str(candidate)
    return "escalonador_solver_cli"


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_file=".env", env_file_encoding="utf-8", extra="ignore")

    APP_NAME: str = "Escalonador Inteligente de Horários API"
    APP_ENV: str = "development"
    API_V1_PREFIX: str = "/api/v1"
    FRONTEND_URL: str = "http://localhost:5173"

    DATABASE_URL: str = "postgresql+asyncpg://escalonador:escalonador@localhost:5432/escalonador"
    SYNC_DATABASE_URL: str | None = None
    REDIS_URL: str = "redis://localhost:6379/0"

    JWT_SECRET_KEY: str = "dev-secret-change-me-in-production-32bytes"
    JWT_ALGORITHM: str = "HS256"
    ACCESS_TOKEN_EXPIRE_MINUTES: int = 60
    REFRESH_TOKEN_EXPIRE_DAYS: int = 7
    PASSWORD_RESET_EXPIRE_MINUTES: int = 30

    SMTP_HOST: str | None = None
    SMTP_PORT: int = 587
    SMTP_USER: str | None = None
    SMTP_PASSWORD: str | None = None
    SMTP_FROM: str = "noreply@escalonador.internal"
    SMTP_USE_TLS: bool = True
    SMTP_USE_SSL: bool = False

    SOLVER_CLI_BIN: str = _default_solver_cli_bin()
    CORS_ORIGINS: list[str] = ["http://localhost:5173", "http://localhost:3000", "http://127.0.0.1:5173"]

    @property
    def sync_database_url(self) -> str:
        if self.SYNC_DATABASE_URL:
            return self.SYNC_DATABASE_URL
        url = self.DATABASE_URL
        if url.startswith("postgresql+asyncpg://"):
            return url.replace("postgresql+asyncpg://", "postgresql+psycopg://", 1)
        if url.startswith("sqlite+aiosqlite://"):
            return url.replace("sqlite+aiosqlite://", "sqlite://", 1)
        return url


settings = Settings()
