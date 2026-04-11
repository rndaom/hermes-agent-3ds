from __future__ import annotations

import subprocess

import pytest

from hermes_3ds_bridge.config import BridgeSettings
from hermes_3ds_bridge.responder import HermesCLIResponder


@pytest.fixture
def settings() -> BridgeSettings:
    return BridgeSettings(
        host="127.0.0.1",
        port=8787,
        auth_token="secret-token",
    )


def test_hermes_cli_responder_invokes_hermes_chat_with_compact_3ds_prompt(monkeypatch, settings):
    captured: dict[str, object] = {}

    def fake_run(command: list[str], capture_output: bool, text: bool, timeout: int, check: bool):
        captured["command"] = command
        captured["capture_output"] = capture_output
        captured["text"] = text
        captured["timeout"] = timeout
        captured["check"] = check
        return subprocess.CompletedProcess(command, 0, stdout="Hermes says hi\n", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    responder = HermesCLIResponder(settings)
    reply = responder.generate_reply(
        message="Summarize the build",
        context=[
            {"role": "user", "content": "Earlier question"},
            {"role": "assistant", "content": "Earlier answer"},
        ],
        client={"platform": "3ds", "app_version": "0.1.0"},
    )

    assert reply == "Hermes says hi"
    assert captured["command"][:3] == ["hermes", "chat", "-q"]
    assert "--source" in captured["command"]
    assert "tool" in captured["command"]
    assert "-Q" in captured["command"]
    prompt = captured["command"][captured["command"].index("-q") + 1]
    assert "Nintendo 3DS" in prompt
    assert "Earlier question" in prompt
    assert "Earlier answer" in prompt
    assert "Summarize the build" in prompt
    assert captured["timeout"] == settings.hermes_timeout_seconds


def test_hermes_cli_responder_raises_for_nonzero_exit(monkeypatch, settings):
    def fake_run(command: list[str], capture_output: bool, text: bool, timeout: int, check: bool):
        return subprocess.CompletedProcess(command, 1, stdout="", stderr="boom")

    monkeypatch.setattr(subprocess, "run", fake_run)

    responder = HermesCLIResponder(settings)
    with pytest.raises(RuntimeError, match="boom"):
        responder.generate_reply(message="hello", context=[], client=None)


def test_hermes_cli_responder_strips_cli_chrome_from_stdout(monkeypatch, settings):
    def fake_run(command: list[str], capture_output: bool, text: bool, timeout: int, check: bool):
        return subprocess.CompletedProcess(
            command,
            0,
            stdout="╭─ ⚕ Hermes ─────────╮\nHi from Hermes.\n\nsession_id: abc123\n",
            stderr="",
        )

    monkeypatch.setattr(subprocess, "run", fake_run)

    responder = HermesCLIResponder(settings)
    assert responder.generate_reply(message="hello", context=[], client=None) == "Hi from Hermes."


def test_hermes_cli_responder_raises_for_empty_stdout(monkeypatch, settings):
    def fake_run(command: list[str], capture_output: bool, text: bool, timeout: int, check: bool):
        return subprocess.CompletedProcess(command, 0, stdout="   \n", stderr="")

    monkeypatch.setattr(subprocess, "run", fake_run)

    responder = HermesCLIResponder(settings)
    with pytest.raises(RuntimeError, match="empty"):
        responder.generate_reply(message="hello", context=[], client=None)
