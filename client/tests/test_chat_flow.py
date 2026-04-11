from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_app_config_supports_chat_endpoint_urls():
    header = (CLIENT_DIR / "include" / "app_config.h").read_text()
    source = (CLIENT_DIR / "source" / "app_config.c").read_text()

    assert "HERMES_APP_CHAT_URL_MAX" in header
    assert "hermes_app_config_build_chat_url" in header
    assert '"/api/v1/chat"' in source



def test_bridge_chat_module_exists_and_posts_json_to_chat_endpoint():
    header_path = CLIENT_DIR / "include" / "bridge_chat.h"
    source_path = CLIENT_DIR / "source" / "bridge_chat.c"

    assert header_path.exists(), "client/include/bridge_chat.h should exist"
    assert source_path.exists(), "client/source/bridge_chat.c should exist"

    header = header_path.read_text()
    source = source_path.read_text()

    assert "BridgeChatResult" in header
    assert "BRIDGE_CHAT_REPLY_MAX" in header
    assert "bridge_chat_run" in header

    assert '"POST %s HTTP/1.1\\r\\n"' in source
    assert '"Content-Type: application/json\\r\\n"' in source
    assert 'message' in source
    assert 'token' in source
    assert 'context' in source
    assert 'platform' in source
    assert 'app_version' in source
    assert "recv(" in source
    assert "extract_json_string(response_body, \"reply\"" in source
    assert "extract_json_string(response_body, \"error\"" in source
    assert '"\\\"truncated\\\":true"' in source



def test_main_c_offers_message_prompt_and_reply_rendering():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert '"bridge_chat.h"' in main_c
    assert "BridgeChatResult" in main_c
    assert "hermes_app_config_build_chat_url" in main_c
    assert "bridge_chat_run" in main_c
    assert "KEY_B" in main_c
    assert "Write a message" in main_c or "Ask Hermes" in main_c
    assert "Last message" in main_c
    assert "Last reply" in main_c
    assert "swkbdInputText" in main_c
    assert "Checking bridge..." in main_c
