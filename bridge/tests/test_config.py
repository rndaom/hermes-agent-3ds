import os

import pytest

from hermes_3ds_bridge.config import BridgeSettings


def test_load_settings_reads_environment_values(monkeypatch):
    monkeypatch.setenv("HERMES_3DS_BRIDGE_HOST", "0.0.0.0")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_PORT", "9999")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_AUTH_TOKEN", "secret-token")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_HERMES_COMMAND", "/usr/local/bin/hermes")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_HERMES_TIMEOUT_SECONDS", "45")

    settings = BridgeSettings.from_env()

    assert settings.host == "0.0.0.0"
    assert settings.port == 9999
    assert settings.auth_token == "secret-token"
    assert settings.hermes_command == "/usr/local/bin/hermes"
    assert settings.hermes_timeout_seconds == 45


def test_load_settings_requires_auth_token(monkeypatch):
    monkeypatch.delenv("HERMES_3DS_BRIDGE_AUTH_TOKEN", raising=False)

    with pytest.raises(ValueError, match="AUTH_TOKEN"):
        BridgeSettings.from_env()
