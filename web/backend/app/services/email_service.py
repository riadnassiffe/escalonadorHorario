from __future__ import annotations

import asyncio
from email.message import EmailMessage
import logging
import smtplib

from app.core.config import settings

logger = logging.getLogger("app.email")


def is_smtp_configured() -> bool:
    return bool(settings.SMTP_HOST and settings.SMTP_HOST.strip())


def is_deliverable_email(email: str) -> bool:
    normalized = email.strip().lower()
    return bool(normalized) and not normalized.endswith((".internal", ".local"))


def _send_smtp_sync(to_email: str, subject: str, body_text: str) -> None:
    msg = EmailMessage()
    msg["From"] = settings.SMTP_FROM
    msg["To"] = to_email
    msg["Subject"] = subject
    msg.set_content(body_text)

    host = str(settings.SMTP_HOST).strip()
    port = int(settings.SMTP_PORT)
    if settings.SMTP_USE_SSL:
        with smtplib.SMTP_SSL(host, port, timeout=15) as server:
            if settings.SMTP_USER and settings.SMTP_PASSWORD:
                server.login(settings.SMTP_USER, settings.SMTP_PASSWORD)
            server.send_message(msg)
    else:
        with smtplib.SMTP(host, port, timeout=15) as server:
            if settings.SMTP_USE_TLS:
                server.starttls()
            if settings.SMTP_USER and settings.SMTP_PASSWORD:
                server.login(settings.SMTP_USER, settings.SMTP_PASSWORD)
            server.send_message(msg)


async def send_email(to_email: str, subject: str, body_text: str) -> tuple[bool, str | None]:
    if not is_deliverable_email(to_email):
        logger.info("E-mail interno automático (%s); envio SMTP ignorado. Assunto: %s", to_email, subject)
        return (
            False,
            "E-mail interno automático; copie e entregue as credenciais manualmente ou informe um e-mail real.",
        )

    if not is_smtp_configured():
        logger.info(
            "SMTP não configurado — e-mail simulado para %s | Assunto: %s\n%s",
            to_email,
            subject,
            body_text,
        )
        return False, "SMTP não configurado; entregue as credenciais manualmente."

    try:
        await asyncio.to_thread(_send_smtp_sync, to_email, subject, body_text)
        return True, None
    except Exception as exc:
        logger.warning("Falha ao enviar e-mail para %s via SMTP: %s", to_email, exc)
        return False, f"Falha ao enviar e-mail via SMTP: {exc}"


async def send_password_reset_email(to_email: str, full_name: str, reset_url: str) -> tuple[bool, str | None]:
    subject = f"{settings.APP_NAME} — Redefinição de senha"
    body = (
        f"Olá, {full_name}.\n\n"
        "Recebemos uma solicitação para redefinir sua senha de acesso.\n"
        f"Acesse o link abaixo para criar uma nova senha (válido por {settings.PASSWORD_RESET_EXPIRE_MINUTES} minutos):\n\n"
        f"{reset_url}\n\n"
        "Se você não solicitou esta alteração, ignore este e-mail.\n"
    )
    return await send_email(to_email, subject, body)


async def send_professor_welcome_email(
    to_email: str,
    full_name: str,
    institution_name: str,
    initial_password: str,
    login_url: str,
) -> tuple[bool, str | None]:
    subject = f"{settings.APP_NAME} — Acesso ao Portal do Professor"
    body = (
        f"Olá, {full_name}.\n\n"
        f"Seu acesso ao sistema de horários da instituição {institution_name} foi gerado.\n\n"
        f"Link de acesso: {login_url}\n"
        f"E-mail/Login: {to_email}\n"
        f"Senha inicial temporária: {initial_password}\n\n"
        "No primeiro acesso será solicitada a definição de uma nova senha pessoal.\n"
    )
    return await send_email(to_email, subject, body)
