from __future__ import annotations

import re
import uuid

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.database import get_db
from app.core.security import (
    create_access_token,
    create_refresh_token,
    decode_token,
    get_current_user,
    hash_password,
    require_roles,
    verify_password,
)
from app.models.auth import Institution, User
from app.schemas.auth import (
    LoginRequest,
    RefreshRequest,
    RegisterRequest,
    TokenResponse,
    UserCreateRequest,
    UserResponse,
)

router = APIRouter(prefix="/auth", tags=["auth"])


def _slugify(name: str) -> str:
    base = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") or "inst"
    return f"{base}-{uuid.uuid4().hex[:6]}"


async def _to_user_response(db: AsyncSession, user: User) -> UserResponse:
    inst = await db.get(Institution, user.institution_id)
    return UserResponse(
        id=user.id,
        institution_id=user.institution_id,
        institution_name=inst.name if inst else "",
        email=user.email,
        full_name=user.full_name,
        role=user.role,
        is_active=user.is_active,
        created_at=user.created_at,
    )


@router.post("/register", response_model=TokenResponse, status_code=status.HTTP_201_CREATED)
async def register(req: RegisterRequest, db: AsyncSession = Depends(get_db)):
    existing = await db.execute(select(User).where(User.email == req.email.lower()))
    if existing.scalar_one_or_none() is not None:
        raise HTTPException(status_code=400, detail="E-mail já cadastrado.")

    inst = Institution(name=req.institution_name, slug=_slugify(req.institution_name))
    db.add(inst)
    await db.flush()

    user = User(
        institution_id=inst.id,
        email=req.email.lower(),
        full_name=req.full_name,
        password_hash=hash_password(req.password),
        role="ADMIN",
    )
    db.add(user)
    await db.commit()
    await db.refresh(user)

    return TokenResponse(
        access_token=create_access_token(user.id, inst.id, user.role),
        refresh_token=create_refresh_token(user.id, inst.id, user.role),
        user=await _to_user_response(db, user),
    )


@router.post("/login", response_model=TokenResponse)
async def login(req: LoginRequest, db: AsyncSession = Depends(get_db)):
    res = await db.execute(select(User).where(User.email == req.email.lower(), User.is_active.is_(True)))
    user = res.scalar_one_or_none()
    if user is None or not verify_password(req.password, user.password_hash):
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Credenciais inválidas.")

    return TokenResponse(
        access_token=create_access_token(user.id, user.institution_id, user.role),
        refresh_token=create_refresh_token(user.id, user.institution_id, user.role),
        user=await _to_user_response(db, user),
    )


@router.post("/refresh", response_model=TokenResponse)
async def refresh_token(req: RefreshRequest, db: AsyncSession = Depends(get_db)):
    payload = decode_token(req.refresh_token)
    if payload.get("type") != "refresh":
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Refresh token inválido.")
    user_id = uuid.UUID(str(payload["sub"]))
    user = await db.get(User, user_id)
    if user is None or not user.is_active:
        raise HTTPException(status_code=status.HTTP_401_UNAUTHORIZED, detail="Usuário inválido.")

    return TokenResponse(
        access_token=create_access_token(user.id, user.institution_id, user.role),
        refresh_token=create_refresh_token(user.id, user.institution_id, user.role),
        user=await _to_user_response(db, user),
    )


@router.get("/me", response_model=UserResponse)
async def me(current_user: User = Depends(get_current_user), db: AsyncSession = Depends(get_db)):
    return await _to_user_response(db, current_user)


@router.post("/users", response_model=UserResponse, status_code=status.HTTP_201_CREATED)
async def create_institution_user(
    req: UserCreateRequest,
    admin_user: User = Depends(require_roles("ADMIN")),
    db: AsyncSession = Depends(get_db),
):
    existing = await db.execute(select(User).where(User.email == req.email.lower()))
    if existing.scalar_one_or_none() is not None:
        raise HTTPException(status_code=400, detail="E-mail já cadastrado.")

    user = User(
        institution_id=admin_user.institution_id,
        email=req.email.lower(),
        full_name=req.full_name,
        password_hash=hash_password(req.password),
        role=req.role,
    )
    db.add(user)
    await db.commit()
    await db.refresh(user)
    return await _to_user_response(db, user)
