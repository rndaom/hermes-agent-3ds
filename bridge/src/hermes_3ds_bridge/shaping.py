from __future__ import annotations


def shape_reply(reply: str, *, max_chars: int) -> tuple[str, bool]:
    normalized = reply.strip()
    if len(normalized) <= max_chars:
        return normalized, False

    return normalized[: max_chars - 1].rstrip() + "…", True
