from __future__ import annotations

from dataclasses import dataclass
import os


ENV_PREFIX = "HERMES_3DS_BRIDGE_"


@dataclass(slots=True)
class BridgeSettings:
    host: str
    port: int
    auth_token: str
    service_name: str = "hermes-agent-3ds-bridge"
    version: str = "0.1.0"
    max_reply_chars: int = 1200

    @classmethod
    def from_env(cls) -> "BridgeSettings":
        host = os.getenv(f"{ENV_PREFIX}HOST", "127.0.0.1")
        port = int(os.getenv(f"{ENV_PREFIX}PORT", "8787"))
        auth_token = os.getenv(f"{ENV_PREFIX}AUTH_TOKEN")
        max_reply_chars = int(os.getenv(f"{ENV_PREFIX}MAX_REPLY_CHARS", "1200"))

        if not auth_token:
            raise ValueError("HERMES_3DS_BRIDGE_AUTH_TOKEN is required.")

        return cls(
            host=host,
            port=port,
            auth_token=auth_token,
            max_reply_chars=max_reply_chars,
        )
