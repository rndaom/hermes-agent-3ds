import pytest

from hermes_3ds_bridge.auth import verify_bearer_token
from hermes_3ds_bridge.config import BridgeSettings


@pytest.fixture
def settings():
    return BridgeSettings(host="127.0.0.1", port=8787, auth_token="secret-token")


def test_verify_bearer_token_accepts_matching_token(settings):
    assert verify_bearer_token("Bearer secret-token", settings) is True


def test_verify_bearer_token_rejects_wrong_token(settings):
    assert verify_bearer_token("Bearer wrong-token", settings) is False


def test_verify_bearer_token_rejects_missing_bearer_prefix(settings):
    assert verify_bearer_token("secret-token", settings) is False
