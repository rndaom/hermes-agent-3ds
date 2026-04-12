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
    home_h = (CLIENT_DIR / "include" / "app_home.h").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    conv_h = (CLIENT_DIR / "include" / "app_conversations.h").read_text()
    conv_c = (CLIENT_DIR / "source" / "app_conversations.c").read_text()
    request_h = (CLIENT_DIR / "include" / "app_requests.h").read_text()
    request_c = (CLIENT_DIR / "source" / "app_requests.c").read_text()
    input_c = (CLIENT_DIR / "source" / "app_input.c").read_text()

    assert '"bridge_chat.h"' in main_c
    assert '"app_home.h"' in main_c
    assert '"app_conversations.h"' in main_c
    assert '"app_requests.h"' in home_c
    assert '"bridge_v2.h"' in request_h
    assert "BridgeChatResult" in main_c
    assert "AppHomeContext" in home_h
    assert "hermes_app_home_handle_input" in home_h
    assert "BridgeV2MessageResult" in request_h
    assert "BridgeV2EventPollResult" in request_h
    assert "BridgeV2ConversationListResult" in conv_h
    assert "complete_v2_roundtrip" in request_c
    assert "poll_for_v2_reply" in request_c
    assert "hermes_app_config_build_messages_url" in request_c
    assert "hermes_app_config_build_events_url" in request_c
    assert "hermes_app_config_build_conversations_url" in conv_c
    assert "bridge_v2_send_message" in request_c
    assert "config->active_conversation_id" in request_c
    assert "message_buffer" in request_c
    assert "bridge_v2_list_conversations" in conv_c
    assert "bridge_v2_poll_events" in request_c
    assert "bridge_v2_submit_interaction" in request_c
    assert "hermes_app_config_build_interaction_url" in request_c
    assert "reply_to_message_id" in request_c
    assert "Approval required" in request_c
    assert "Command denied." in request_c
    assert "KEY_B" in home_c
    assert "KEY_UP" in home_c
    assert "KEY_SELECT" in home_c
    assert "KEY_DOWN" in conv_c
    assert "Write a message" in input_c
    assert "Record mic" in request_c or "mic" in request_c.lower()
    assert "bridge_v2_send_voice_message" in request_c
    assert "config->active_conversation_id" in request_c
    assert "wav_data" in request_c
    assert "MICU_StartSampling" in main_c or "voice_input_record_prompt" in request_c
    assert "Last message" in main_c or "Last message" in (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert "HERMES REPLY" in (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert "swkbdInputText" in input_c
    assert "hermes_app_home_handle_input" in main_c
    assert "hermes_app_requests_handle_text" in home_c
    assert "hermes_app_requests_handle_voice" in home_c
    assert "Asking Hermes over v2..." in request_c
    assert "Sending mic input to Hermes..." in request_c
    assert '"/api/v1/chat"' not in main_c


def test_main_c_wraps_and_pages_long_reply_text_for_small_screens():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "KEY_L" in home_c
    assert "KEY_R" in home_c
    assert "reply_page" in main_c
    assert "hermes_app_ui_reply_page_count" in home_c
    assert "wrap_text_for_console" in ui_c
    assert "page_count_for_lines" in ui_c
    assert "reply_start = reply_page * HOME_REPLY_LINES_PER_PAGE" in ui_c


def test_voice_input_waits_for_up_release_before_allowing_stop():
    voice_input_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()

    assert "hidKeysHeld() & KEY_UP" in voice_input_c
    assert "waiting_for_up_release" in voice_input_c


def test_voice_input_uses_graphical_recording_render_helpers_without_console_printf():
    voice_input_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()

    assert "hermes_app_ui_render_voice_recording" in voice_input_c
    assert "hermes_app_ui_render_voice_recording" in ui_h
    assert "render_recording_shell" not in voice_input_c
    assert "update_recording_status" not in voice_input_c
    assert "consoleSelect" not in voice_input_c
    assert "consoleClear" not in voice_input_c
    assert "current_tenths != last_render_tenths" in voice_input_c



def test_main_c_uses_both_top_and_bottom_screens_for_ui():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()

    assert "PrintConsole" not in main_c
    assert "consoleInit(GFX_TOP" not in main_c
    assert "consoleInit(GFX_BOTTOM" not in main_c
    assert "hermes_app_ui_render" in main_c
    assert "hermes_app_ui_render" in ui_h
    assert "render_home_graphical" in ui_c
    assert "app_gfx_begin_top" in ui_c
    assert "app_gfx_begin_bottom" in ui_c
