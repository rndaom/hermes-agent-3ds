import os

import pytest

from hermes_3ds_bridge.config import BridgeSettings


def test_load_settings_reads_environment_values(monkeypatch):
    monkeypatch.setenv("HERMES_3DS_BRIDGE_HOST", "0.0.0.0")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_PORT", "9999")
    monkeypatch.setenv("HERMES_3DS_BRIDGE_AUTH_TOKEN", "secret-token")

    settings = BridgeSettings.from_env()

    assert settings.host == "0.0.0.0"
    assert settings.port == 9999
    assert settings.auth_token == "secret-token"


def test_load_settings_requires_auth_token(monkeypatch):
    monkeypatch.delenv("HERMES_3DS_BRIDGE_AUTH_TOKEN", raising=False)

    with pytest.raises(ValueError, match="AUTH_TOKEN"):
        BridgeSettings.from_env()
