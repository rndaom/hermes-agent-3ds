from __future__ import annotations

from typing import Any, Protocol


class Responder(Protocol):
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str: ...


class EchoResponder:
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str:
        return f"Bridge is up, but Hermes is not connected yet. You said: {message}"
