from __future__ import annotations

import subprocess
from typing import Any, Protocol

from hermes_3ds_bridge.config import BridgeSettings


class Responder(Protocol):
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str: ...


class EchoResponder:
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str:
        return f"Bridge is up, but Hermes is not connected yet. You said: {message}"


class HermesCLIResponder:
    def __init__(self, settings: BridgeSettings):
        self.settings = settings

    def _build_prompt(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str:
        lines = [
            "You are replying through a Nintendo 3DS bridge.",
            "Keep the response concise, plain text, and readable on a very small screen.",
            "Avoid markdown tables and avoid unnecessary verbosity.",
            "Do not perform dangerous side-effecting actions unless the user explicitly asks.",
            "",
        ]

        if client:
            lines.append(f"Client metadata: {client}")
            lines.append("")

        if context:
            lines.append("Conversation context:")
            for item in context:
                role = item.get("role", "unknown").strip() or "unknown"
                content = item.get("content", "").strip()
                lines.append(f"- {role}: {content}")
            lines.append("")

        lines.append("Current user message:")
        lines.append(message.strip())
        lines.append("")
        lines.append("Reply directly to the current user message.")
        return "\n".join(lines)

    def _clean_reply(self, raw_reply: str) -> str:
        cleaned_lines: list[str] = []

        for line in raw_reply.splitlines():
            stripped = line.strip()
            if not stripped:
                cleaned_lines.append("")
                continue
            if stripped.startswith("session_id:"):
                continue
            if stripped.startswith("╭") or stripped.startswith("╰"):
                continue
            cleaned_lines.append(line)

        cleaned = "\n".join(cleaned_lines).strip()
        return cleaned

    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, Any] | None) -> str:
        prompt = self._build_prompt(message=message, context=context, client=client)
        command = [
            self.settings.hermes_command,
            "chat",
            "-q",
            prompt,
            "-Q",
            "--source",
            "tool",
        ]
        completed = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=self.settings.hermes_timeout_seconds,
            check=False,
        )

        if completed.returncode != 0:
            stderr = (completed.stderr or "").strip()
            raise RuntimeError(stderr or f"Hermes command failed with exit code {completed.returncode}.")

        reply = self._clean_reply(completed.stdout or "")
        if not reply:
            raise RuntimeError("Hermes returned an empty response.")

        return reply
