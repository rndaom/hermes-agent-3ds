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
    assert "BRIDGE_CHAT_IO_TIMEOUT_SECONDS" in source
    assert "#define BRIDGE_CHAT_IO_TIMEOUT_SECONDS 180" in source
    assert "Timed out waiting for a Hermes reply." in source
    assert "extract_json_string(response_body, \"reply\"" in source
    assert "extract_json_string(response_body, \"error\"" in source
    assert '"\\\"truncated\\\":true"' in source



def test_main_c_offers_message_prompt_and_reply_rendering():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert '"bridge_chat.h"' in main_c
    assert '"bridge_v2.h"' in main_c
    assert "BridgeChatResult" in main_c
    assert "BridgeV2MessageResult" in main_c
    assert "BridgeV2EventPollResult" in main_c
    assert "hermes_app_config_build_messages_url" in main_c
    assert "hermes_app_config_build_events_url" in main_c
    assert "bridge_v2_send_message" in main_c
    assert "bridge_v2_poll_events" in main_c
    assert "bridge_v2_submit_interaction" in main_c
    assert "hermes_app_config_build_interaction_url" in main_c
    assert "Approval required" in main_c
    assert "Command denied." in main_c
    assert "KEY_B" in main_c
    assert "Write a message" in main_c or "Ask Hermes" in main_c
    assert "Last message" in main_c
    assert "Last reply" in main_c
    assert "swkbdInputText" in main_c
    assert "Checking bridge..." in main_c


def test_main_c_wraps_and_pages_long_reply_text_for_small_screens():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert "wrap_text_for_console" in main_c
    assert "render_wrapped_page" in main_c
    assert "KEY_L" in main_c
    assert "KEY_R" in main_c
    assert "reply_page" in main_c
    assert "Page %lu/%lu" in main_c


def test_main_c_uses_both_top_and_bottom_screens_for_ui():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert "PrintConsole" in main_c
    assert "consoleInit(GFX_TOP" in main_c
    assert "consoleInit(GFX_BOTTOM" in main_c
    assert "consoleSelect(&top_console)" in main_c
    assert "consoleSelect(&bottom_console)" in main_c
    assert "render_home_top_screen" in main_c
    assert "render_home_bottom_screen" in main_c
