from __future__ import annotations

from dataclasses import dataclass
import re
import uuid

from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from app.core.config import settings
from app.core.security import generate_temporary_password, hash_password
from app.models.academic import ProfessorModel
from app.models.auth import Institution, User
from app.models.project import Project
from app.services.email_service import send_professor_welcome_email


@dataclass
class ProvisionResult:
    code: str
    nome: str
    email: str
    user_id: uuid.UUID
    initial_password: str | None
    account_created: bool
    password_reset: bool
    email_sent: bool
    delivery_note: str | None


def build_default_professor_email(code: str, institution_slug: str, existing_emails: set[str]) -> str:
    code_part = re.sub(r"[^a-z0-9._-]+", "", code.lower()).strip("._-") or "prof"
    slug_part = re.sub(r"[^a-z0-9-]+", "-", institution_slug.lower()).strip("-")[:45].strip("-") or "instituicao"
    domain = f"{slug_part}.internal"

    candidate = f"{code_part}@{domain}"
    if candidate not in existing_emails:
        return candidate

    idx = 2
    while True:
        alt = f"{code_part}-{idx}@{domain}"
        if alt not in existing_emails:
            return alt
        idx += 1


async def provision_professor_account(
    db: AsyncSession,
    project: Project,
    prof: ProfessorModel,
    *,
    reset_if_exists: bool = False,
) -> ProvisionResult:
    inst = await db.get(Institution, project.institution_id)
    inst_name = inst.name if inst else "Instituição"
    inst_slug = inst.slug if inst else "instituicao"
    login_url = f"{settings.FRONTEND_URL.rstrip('/')}/login"

    if prof.user_id is not None:
        user = await db.get(User, prof.user_id)
        if user is not None:
            prof.email = user.email
            if not reset_if_exists:
                return ProvisionResult(
                    code=prof.code,
                    nome=prof.nome,
                    email=user.email,
                    user_id=user.id,
                    initial_password=None,
                    account_created=False,
                    password_reset=False,
                    email_sent=False,
                    delivery_note="Professor já possui conta vinculada.",
                )

            temp_pw = generate_temporary_password()
            user.password_hash = hash_password(temp_pw)
            user.must_change_password = True
            user.is_active = True
            email_sent, note = await send_professor_welcome_email(
                user.email, prof.nome, inst_name, temp_pw, login_url
            )
            return ProvisionResult(
                code=prof.code,
                nome=prof.nome,
                email=user.email,
                user_id=user.id,
                initial_password=temp_pw,
                account_created=False,
                password_reset=True,
                email_sent=email_sent,
                delivery_note=note,
            )
        prof.user_id = None

    if prof.email and prof.email.strip():
        target_email = prof.email.strip().lower()
    else:
        existing_emails = set(
            e.lower() for e in (await db.execute(select(User.email))).scalars().all()
        )
        target_email = build_default_professor_email(prof.code, inst_slug, existing_emails)

    existing_user = (await db.execute(select(User).where(User.email == target_email))).scalar_one_or_none()
    if existing_user is not None:
        if existing_user.institution_id != project.institution_id:
            raise ValueError("Este e-mail já está vinculado a outra instituição.")

        prof.user_id = existing_user.id
        prof.email = existing_user.email
        if reset_if_exists:
            temp_pw = generate_temporary_password()
            existing_user.password_hash = hash_password(temp_pw)
            existing_user.must_change_password = True
            existing_user.is_active = True
            email_sent, note = await send_professor_welcome_email(
                existing_user.email, prof.nome, inst_name, temp_pw, login_url
            )
            return ProvisionResult(
                code=prof.code,
                nome=prof.nome,
                email=existing_user.email,
                user_id=existing_user.id,
                initial_password=temp_pw,
                account_created=False,
                password_reset=True,
                email_sent=email_sent,
                delivery_note=note,
            )

        return ProvisionResult(
            code=prof.code,
            nome=prof.nome,
            email=existing_user.email,
            user_id=existing_user.id,
            initial_password=None,
            account_created=False,
            password_reset=False,
            email_sent=False,
            delivery_note="Conta existente na instituição vinculada ao professor.",
        )

    temp_pw = generate_temporary_password()
    user = User(
        institution_id=project.institution_id,
        email=target_email,
        full_name=prof.nome,
        password_hash=hash_password(temp_pw),
        role="PROFESSOR",
        must_change_password=True,
        is_active=True,
    )
    db.add(user)
    await db.flush()

    prof.user_id = user.id
    prof.email = target_email
    email_sent, note = await send_professor_welcome_email(
        target_email, prof.nome, inst_name, temp_pw, login_url
    )
    return ProvisionResult(
        code=prof.code,
        nome=prof.nome,
        email=target_email,
        user_id=user.id,
        initial_password=temp_pw,
        account_created=True,
        password_reset=False,
        email_sent=email_sent,
        delivery_note=note,
    )
