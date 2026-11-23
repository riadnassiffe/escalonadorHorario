from __future__ import annotations

import uuid
from datetime import datetime
from typing import Literal

from pydantic import BaseModel, EmailStr, Field


class RegisterRequest(BaseModel):
    institution_name: str = Field(min_length=2, max_length=200)
    full_name: str = Field(min_length=2, max_length=200)
    email: EmailStr
    password: str = Field(min_length=6, max_length=128)


class LoginRequest(BaseModel):
    email: EmailStr
    password: str


class RefreshRequest(BaseModel):
    refresh_token: str


class UserCreateRequest(BaseModel):
    full_name: str = Field(min_length=2, max_length=200)
    email: EmailStr
    password: str = Field(min_length=6, max_length=128)
    role: Literal["ADMIN", "COORDENADOR", "VISUALIZADOR"] = "COORDENADOR"


class UserResponse(BaseModel):
    id: uuid.UUID
    institution_id: uuid.UUID
    institution_name: str
    email: EmailStr
    full_name: str
    role: str
    is_active: bool
    created_at: datetime


class TokenResponse(BaseModel):
    access_token: str
    refresh_token: str
    token_type: str = "bearer"
    user: UserResponse
