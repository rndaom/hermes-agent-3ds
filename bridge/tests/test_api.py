from fastapi.testclient import TestClient

from hermes_3ds_bridge.app import create_app
from hermes_3ds_bridge.config import BridgeSettings


class StubResponder:
    def __init__(self, reply: str):
        self.reply = reply
        self.calls = []

    def generate_reply(self, *, message: str, context: list[dict], client: dict | None) -> str:
        self.calls.append({"message": message, "context": context, "client": client})
        return self.reply


def make_client(reply: str = "Hello from Hermes", max_reply_chars: int = 80):
    settings = BridgeSettings(
        host="127.0.0.1",
        port=8787,
        auth_token="secret-token",
        max_reply_chars=max_reply_chars,
    )
    responder = StubResponder(reply)
    app = create_app(settings, responder=responder)
    return TestClient(app), responder


def test_health_endpoint_returns_service_status():
    client, _ = make_client()

    response = client.get("/api/v1/health")

    assert response.status_code == 200
    assert response.json() == {
        "ok": True,
        "service": "hermes-agent-3ds-bridge",
        "version": "0.1.0",
    }


def test_chat_endpoint_rejects_unauthorized_requests():
    client, _ = make_client()

    response = client.post("/api/v1/chat", json={"message": "hello", "token": "wrong-token"})

    assert response.status_code == 401
    assert response.json() == {"ok": False, "error": "Unauthorized."}


def test_chat_endpoint_rejects_blank_messages():
    client, _ = make_client()

    response = client.post(
        "/api/v1/chat",
        json={"message": "   ", "token": "secret-token"},
    )

    assert response.status_code == 400
    assert response.json() == {"ok": False, "error": "Message is required."}


def test_chat_endpoint_returns_reply_and_passes_context_to_responder():
    client, responder = make_client(reply="Short reply")

    response = client.post(
        "/api/v1/chat",
        json={
            "message": "Summarize this",
            "token": "secret-token",
            "context": [{"role": "user", "content": "Earlier message"}],
            "client": {"platform": "3ds", "app_version": "0.1.0"},
        },
    )

    assert response.status_code == 200
    assert response.json() == {
        "ok": True,
        "reply": "Short reply",
        "truncated": False,
    }
    assert responder.calls == [
        {
            "message": "Summarize this",
            "context": [{"role": "user", "content": "Earlier message"}],
            "client": {"platform": "3ds", "app_version": "0.1.0"},
        }
    ]


def test_chat_endpoint_truncates_overlong_replies():
    client, _ = make_client(reply="x" * 120, max_reply_chars=32)

    response = client.post(
        "/api/v1/chat",
        json={"message": "hello", "token": "secret-token"},
    )

    assert response.status_code == 200
    assert response.json() == {
        "ok": True,
        "reply": "x" * 31 + "…",
        "truncated": True,
    }
