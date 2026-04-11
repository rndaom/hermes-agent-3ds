from fastapi.testclient import TestClient

from hermes_3ds_bridge.app import create_app
from hermes_3ds_bridge.config import BridgeSettings
from hermes_3ds_bridge.responder import HermesTimeoutError


class TimeoutResponder:
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, str] | None) -> str:
        raise HermesTimeoutError("Hermes took too long on the host. Try a shorter request.")


class FailingResponder:
    def generate_reply(self, *, message: str, context: list[dict[str, str]], client: dict[str, str] | None) -> str:
        raise RuntimeError("boom")



def test_chat_endpoint_returns_504_for_host_hermes_timeout():
    settings = BridgeSettings(host="127.0.0.1", port=8787, auth_token="secret-token")
    app = create_app(settings, responder=TimeoutResponder())
    client = TestClient(app)

    response = client.post(
        "/api/v1/chat",
        json={"token": "secret-token", "message": "top 3 headlines in news today"},
    )

    assert response.status_code == 504
    assert response.json() == {
        "ok": False,
        "error": "Hermes took too long on the host. Try a shorter request.",
    }



def test_chat_endpoint_keeps_generic_502_for_other_bridge_failures():
    settings = BridgeSettings(host="127.0.0.1", port=8787, auth_token="secret-token")
    app = create_app(settings, responder=FailingResponder())
    client = TestClient(app)

    response = client.post(
        "/api/v1/chat",
        json={"token": "secret-token", "message": "hello"},
    )

    assert response.status_code == 502
    assert response.json() == {
        "ok": False,
        "error": "Bridge request failed.",
    }
