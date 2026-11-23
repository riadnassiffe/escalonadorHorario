from __future__ import annotations

from collections.abc import AsyncGenerator
from sqlalchemy import JSON, create_engine
from sqlalchemy.dialects.postgresql import JSONB
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from sqlalchemy.orm import DeclarativeBase, Session, sessionmaker

from app.core.config import settings

JSONBCompat = JSON().with_variant(JSONB(), "postgresql")


class Base(DeclarativeBase):
    pass


async_engine = create_async_engine(settings.DATABASE_URL, echo=False, future=True)
AsyncSessionLocal = async_sessionmaker(async_engine, class_=AsyncSession, expire_on_commit=False)

sync_engine = create_engine(settings.sync_database_url, echo=False, future=True)
SyncSessionLocal = sessionmaker(sync_engine, class_=Session, expire_on_commit=False)


async def get_db() -> AsyncGenerator[AsyncSession, None]:
    async with AsyncSessionLocal() as session:
        yield session
