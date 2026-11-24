from __future__ import annotations

from datetime import datetime, timedelta, timezone
import re
import uuid

from fastapi import APIRouter, Depends, HTTPException, status
from sqlalchemy import select, update
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import settings
from app.core.database import get_db
from app.core.security import (
    create_access_token,
    create_refresh_token,
    decode_token,
    generate_reset_token,
    get_current_user,
    hash_password,
    hash_reset_token,
    require_roles,
    verify_password,
)
from app.models.auth import Institution, PasswordResetToken, User, utcnow
from app.schemas.auth import (
    ChangePasswordRequest,
    ForgotPasswordRequest,
    ForgotPasswordResponse,
    LoginRequest,
    MessageResponse,
    RefreshRequest,
    RegisterRequest,
    ResetPasswordRequest,
    TokenResponse,
    UserCreateRequest,
    UserResponse,
)
from app.services.email_service import send_password_reset_email

router = APIRouter(prefix="/auth", tags=["auth"])


def _slugify(name: str) -> str:
    base = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-") or "inst"
    return f"{base}-{uuid.uuid4().hex[:6]}"


def _as_utc(dt: datetime) -> datetime:
    return dt.replace(tzinfo=timezone.utc) if dt.tzinfo is None else dt.astimezone(timezone.utc)


async def _to_user_response(db: AsyncSession, user: User) -> UserResponse:
    inst = await db.get(Institution, user.institution_id)
    return UserResponse(
        id=user.id,
        institution_id=user.institution_id,
        institution_name=inst.name if inst else "",
        email=user.email,
        full_name=user.full_name,
        role=user.role,
        must_change_password=user.must_change_password,
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


@router.post("/forgot-password", response_model=ForgotPasswordResponse)
async def forgot_password(req: ForgotPasswordRequest, db: AsyncSession = Depends(get_db)):
    generic_msg = "Se o e-mail estiver cadastrado, enviaremos as instruções para redefinição de senha."
    user = (
        await db.execute(select(User).where(User.email == req.email.lower(), User.is_active.is_(True)))
    ).scalar_one_or_none()
    if user is None:
        return ForgotPasswordResponse(message=generic_msg, email_sent=False)

    now = utcnow()
    await db.execute(
        update(PasswordResetToken)
        .where(PasswordResetToken.user_id == user.id, PasswordResetToken.used_at.is_(None))
        .values(used_at=now)
    )

    raw_token, token_hash = generate_reset_token()
    reset_row = PasswordResetToken(
        user_id=user.id,
        token_hash=token_hash,
        expires_at=now + timedelta(minutes=settings.PASSWORD_RESET_EXPIRE_MINUTES),
    )
    db.add(reset_row)
    await db.commit()

    reset_url = f"{settings.FRONTEND_URL.rstrip('/')}/reset-password?token={raw_token}"
    sent, _ = await send_password_reset_email(user.email, user.full_name, reset_url)

    dev_mode = settings.APP_ENV.lower() == "development" and not sent
    return ForgotPasswordResponse(
        message=generic_msg,
        email_sent=sent,
        dev_reset_token=raw_token if dev_mode else None,
        dev_reset_url=reset_url if dev_mode else None,
    )


@router.post("/reset-password", response_model=MessageResponse)
async def reset_password(req: ResetPasswordRequest, db: AsyncSession = Depends(get_db)):
    token_hash = hash_reset_token(req.token.strip())
    reset_row = (
        await db.execute(select(PasswordResetToken).where(PasswordResetToken.token_hash == token_hash))
    ).scalar_one_or_none()

    now = utcnow()
    if reset_row is None or reset_row.used_at is not None or _as_utc(reset_row.expires_at) <= now:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Token de redefinição inválido ou expirado.")

    user = await db.get(User, reset_row.user_id)
    if user is None or not user.is_active:
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Usuário inválido ou inativo.")

    user.password_hash = hash_password(req.new_password)
    user.must_change_password = False
    reset_row.used_at = now
    await db.execute(
        update(PasswordResetToken)
        .where(PasswordResetToken.user_id == user.id, PasswordResetToken.used_at.is_(None))
        .values(used_at=now)
    )
    await db.commit()
    return MessageResponse(ok=True, message="Senha redefinida com sucesso.")


@router.post("/change-password", response_model=UserResponse)
async def change_password(
    req: ChangePasswordRequest,
    current_user: User = Depends(get_current_user),
    db: AsyncSession = Depends(get_db),
):
    if not verify_password(req.current_password, current_user.password_hash):
        raise HTTPException(status_code=status.HTTP_400_BAD_REQUEST, detail="Senha atual incorreta.")
    if req.current_password == req.new_password:
        raise HTTPException(
            status_code=status.HTTP_400_BAD_REQUEST,
            detail="A nova senha deve ser diferente da senha atual.",
        )

    current_user.password_hash = hash_password(req.new_password)
    current_user.must_change_password = False
    await db.commit()
    await db.refresh(current_user)
    return await _to_user_response(db, current_user)
