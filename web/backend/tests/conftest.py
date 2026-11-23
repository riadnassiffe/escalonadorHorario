from __future__ import annotations

from collections.abc import AsyncGenerator
from pathlib import Path
import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient
from sqlalchemy import create_engine
from sqlalchemy.ext.asyncio import AsyncSession, async_sessionmaker, create_async_engine
from sqlalchemy.orm import Session, sessionmaker

from app.core.database import Base, get_db
from app.main import app as fastapi_app
import app.models  # noqa: F401


@pytest.fixture()
def db_paths(tmp_path: Path) -> tuple[str, str]:
    db_file = tmp_path / "test_escalonador.db"
    return f"sqlite+aiosqlite:///{db_file}", f"sqlite:///{db_file}"


@pytest_asyncio.fixture()
async def db_engines(db_paths: tuple[str, str]):
    async_url, sync_url = db_paths
    async_eng = create_async_engine(async_url, future=True)
    sync_eng = create_engine(sync_url, future=True)

    async with async_eng.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)

    async_sf = async_sessionmaker(async_eng, class_=AsyncSession, expire_on_commit=False)
    sync_sf = sessionmaker(sync_eng, class_=Session, expire_on_commit=False)

    yield async_sf, sync_sf

    await async_eng.dispose()
    sync_eng.dispose()


@pytest_asyncio.fixture()
async def client(db_engines) -> AsyncGenerator[AsyncClient, None]:
    async_sf, _ = db_engines

    async def override_get_db() -> AsyncGenerator[AsyncSession, None]:
        async with async_sf() as session:
            yield session

    fastapi_app.dependency_overrides[get_db] = override_get_db
    transport = ASGITransport(app=fastapi_app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac
    fastapi_app.dependency_overrides.clear()


async def register_and_login(
    client: AsyncClient,
    institution_name: str = "Escola Modelo",
    full_name: str = "Admin Teste",
    email: str = "admin@escola.edu.br",
    password: str = "senha1234",
) -> dict:
    resp = await client.post(
        "/api/v1/auth/register",
        json={
            "institution_name": institution_name,
            "full_name": full_name,
            "email": email,
            "password": password,
        },
    )
    assert resp.status_code == 201, resp.text
    return resp.json()
