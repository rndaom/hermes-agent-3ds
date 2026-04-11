from fastapi.testclient import TestClient

from hermes_3ds_bridge.app import create_app
from hermes_3ds_bridge.config import BridgeSettings
from hermes_3ds_bridge.responder import HermesCLIResponder


def test_create_app_exposes_service_metadata():
    settings = BridgeSettings(host="127.0.0.1", port=8787, auth_token="secret-token")
    app = create_app(settings)
    client = TestClient(app)

    response = client.get("/")

    assert response.status_code == 200
    assert response.json() == {
        "service": "hermes-agent-3ds-bridge",
        "status": "starting",
        "version": "0.1.0",
    }


def test_create_app_uses_hermes_cli_responder_by_default():
    settings = BridgeSettings(host="127.0.0.1", port=8787, auth_token="secret-token")
    app = create_app(settings)

    assert isinstance(app.state.responder, HermesCLIResponder)
