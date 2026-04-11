from __future__ import annotations

from secrets import compare_digest

from hermes_3ds_bridge.config import BridgeSettings


BEARER_PREFIX = "Bearer "


def verify_bearer_token(authorization_header: str | None, settings: BridgeSettings) -> bool:
    if not authorization_header or not authorization_header.startswith(BEARER_PREFIX):
        return False

    provided_token = authorization_header[len(BEARER_PREFIX):].strip()
    if not provided_token:
        return False

    return compare_digest(provided_token, settings.auth_token)


def verify_request_token(body_token: str | None, settings: BridgeSettings) -> bool:
    if not body_token:
        return False

    return compare_digest(body_token, settings.auth_token)
