from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]
REPO_ROOT = CLIENT_DIR.parent


def test_app_config_only_supports_native_v2_endpoint_urls():
    header = (CLIENT_DIR / "include" / "app_config.h").read_text()
    source = (CLIENT_DIR / "source" / "app_config.c").read_text()

    assert "HERMES_APP_CHAT_URL_MAX" not in header
    assert "hermes_app_config_build_chat_url" not in header
    assert '"/api/v1/chat"' not in source
    assert '"/api/v2/messages"' in source
    assert '"/api/v2/events"' in source
    assert '"/api/v2/interactions/"' in source


def test_legacy_v1_chat_transport_module_is_removed():
    header_path = CLIENT_DIR / "include" / "bridge_chat.h"
    source_path = CLIENT_DIR / "source" / "bridge_chat.c"

    assert header_path.exists(), "client/include/bridge_chat.h should remain as shared chat result types"
    assert not source_path.exists(), "client/source/bridge_chat.c should be removed"

    header = header_path.read_text()
    assert "BridgeChatResult" in header
    assert "bridge_chat_run" not in header


def test_legacy_python_bridge_repo_folder_is_removed():
    assert not (REPO_ROOT / "bridge").exists()


def test_main_c_offers_message_prompt_and_reply_rendering_over_native_v2_only():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert '"bridge_chat.h"' in main_c
    assert '"bridge_v2.h"' in main_c
    assert "BridgeChatResult" in main_c
    assert "BridgeV2MessageResult" in main_c
    assert "BridgeV2EventPollResult" in main_c
    assert "BridgeV2ConversationListResult" in main_c
    assert "complete_v2_roundtrip" in main_c
    assert "poll_for_v2_reply" in main_c
    assert "hermes_app_config_build_messages_url" in main_c
    assert "hermes_app_config_build_events_url" in main_c
    assert "hermes_app_config_build_conversations_url" in main_c
    assert "bridge_v2_send_message" in main_c
    assert "bridge_v2_send_message(messages_url, config.token, config.device_id, config.active_conversation_id" in main_c
    assert "bridge_v2_list_conversations" in main_c
    assert "bridge_v2_poll_events" in main_c
    assert "bridge_v2_submit_interaction" in main_c
    assert "hermes_app_config_build_interaction_url" in main_c
    assert "reply_to_message_id" in main_c
    assert "Approval required" in main_c
    assert "Command denied." in main_c
    assert "KEY_B" in main_c
    assert "KEY_UP" in main_c
    assert "KEY_SELECT" in main_c
    assert "Write a message" in main_c or "Ask Hermes" in main_c
    assert "Record mic" in main_c or "mic" in main_c.lower()
    assert "bridge_v2_send_voice_message" in main_c
    assert "bridge_v2_send_voice_message(voice_url, config.token, config.device_id, config.active_conversation_id" in main_c
    assert "MICU_StartSampling" in main_c or "voice_input_record_prompt" in main_c
    assert "Last message" in main_c
    assert "Last reply" in main_c
    assert "swkbdInputText" in main_c
    assert "Asking Hermes over v2..." in main_c
    assert '"/api/v1/chat"' not in main_c


def test_main_c_wraps_and_pages_long_reply_text_for_small_screens():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert "wrap_text_for_console" in main_c
    assert "render_wrapped_page" in main_c
    assert "KEY_L" in main_c
    assert "KEY_R" in main_c
    assert "reply_page" in main_c
    assert "Page %lu/%lu" in main_c


def test_voice_input_waits_for_up_release_before_allowing_stop():
    voice_input_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()

    assert "hidKeysHeld() & KEY_UP" in voice_input_c
    assert "waiting_for_up_release" in voice_input_c


def test_voice_input_avoids_full_console_redraws_every_recording_frame():
    voice_input_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()

    assert "render_recording_shell" in voice_input_c
    assert "update_recording_status" in voice_input_c
    assert 'printf("\\x1b[5;1HTime:' in voice_input_c
    assert 'printf("\\x1b[6;1HAudio:' in voice_input_c
    assert "current_tenths != last_render_tenths" in voice_input_c



def test_main_c_uses_both_top_and_bottom_screens_for_ui():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert "PrintConsole" in main_c
    assert "consoleInit(GFX_TOP" in main_c
    assert "consoleInit(GFX_BOTTOM" in main_c
    assert "consoleSelect(&top_console)" in main_c
    assert "consoleSelect(&bottom_console)" in main_c
    assert "render_home_top_screen" in main_c
    assert "render_home_bottom_screen" in main_c
