from __future__ import annotations

from typing import Any

from pydantic import BaseModel, Field


class ChatContextMessage(BaseModel):
    role: str
    content: str


class ChatRequest(BaseModel):
    token: str | None = None
    message: str
    context: list[ChatContextMessage] = Field(default_factory=list)
    client: dict[str, Any] | None = None


class ChatSuccessResponse(BaseModel):
    ok: bool = True
    reply: str
    truncated: bool = False


class ErrorResponse(BaseModel):
    ok: bool = False
    error: str


class HealthResponse(BaseModel):
    ok: bool = True
    service: str
    version: str
