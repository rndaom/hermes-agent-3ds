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

    assert header_path.exists(), (
        "client/include/bridge_chat.h should remain as shared chat result types"
    )
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
    assert "bridge_v2_send_image_message" in request_c
    assert (
        "parse_content_length_header"
        in (CLIENT_DIR / "source" / "bridge_v2.c").read_text()
    )
    assert (
        "response_complete(response, response_used)"
        in (CLIENT_DIR / "source" / "bridge_v2.c").read_text()
    )
    assert (
        "getpeername(socket_fd" in (CLIENT_DIR / "source" / "bridge_v2.c").read_text()
    )
    assert (CLIENT_DIR / "nintendo_ds_bios_ascii.txt").exists()
    assert "config->active_conversation_id" in request_c
    assert "message_buffer" in request_c
    assert "bridge_v2_list_conversations" in conv_c
    assert "bridge_v2_poll_events" in request_c
    assert "bridge_v2_submit_interaction" in request_c
    assert "hermes_app_config_build_interaction_url" in request_c
    assert "reply_to_message_id" in request_c
    assert "Approval required" in request_c
    assert "Command denied." in request_c
    assert "KEY_UP" in home_c
    assert "KEY_TOUCH" in home_c
    assert "KEY_CPAD_UP" in home_c
    assert "KEY_CPAD_DOWN" in home_c
    assert "KEY_LEFT" in home_c
    assert "KEY_RIGHT" in home_c
    assert "KEY_L" in home_c
    assert "KEY_R" in home_c
    assert "history_scroll" in main_c
    assert "hermes_app_ui_home_history_max_scroll" in home_c
    assert "KEY_DOWN" in conv_c
    assert "Write a note" in input_c
    assert "Record mic" in request_c or "mic" in request_c.lower()
    assert "Picture prompt" in request_c or "picture" in request_c.lower()
    assert "bridge_v2_send_voice_message" in request_c
    assert "config->active_conversation_id" in request_c
    assert "wav_data" in request_c
    assert "bmp_data" in request_c
    assert "MICU_StartSampling" in main_c or "voice_input_record_prompt" in request_c
    assert "picture_input_capture_prompt" in request_c
    assert (
        "Last message" in main_c
        or "draw_message_card" in (CLIENT_DIR / "source" / "app_ui.c").read_text()
    )
    assert '"YOU"' in (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert '"HERMES"' in (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert "swkbdInputText" in input_c
    assert "hermes_app_home_handle_input" in main_c
    assert "hermes_app_requests_handle_text" in home_c
    assert "hermes_app_requests_handle_voice" in home_c
    assert "hermes_app_requests_handle_picture" in home_c
    assert "hermes_app_requests_handle_clear_command" in home_c
    assert "APP_REQUEST_REPLY_WAIT_TIMEOUT_MS" in request_c
    assert "60ULL * 60ULL * 1000ULL" in request_c
    assert "Timed out waiting for Hermes reply." in request_c
    assert "Sending note to Hermes..." in request_c
    assert "Sending mic note to Hermes..." in request_c
    assert "Sending picture note to Hermes..." in request_c
    assert (
        "status.updated" in request_c
        or "status.updated" in (CLIENT_DIR / "source" / "bridge_v2.c").read_text()
    )
    assert '"/api/v1/chat"' not in main_c


def test_main_c_wraps_and_scrolls_message_history_for_small_screens():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    request_c = (CLIENT_DIR / "source" / "app_requests.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "KEY_CPAD_UP" in home_c
    assert "KEY_CPAD_DOWN" in home_c
    assert "KEY_LEFT" in home_c
    assert "KEY_RIGHT" in home_c
    assert "KEY_L" in home_c
    assert "KEY_R" in home_c
    assert "history_scroll" in main_c
    assert "hermes_app_ui_home_history_max_scroll" in home_c
    assert "wrap_text_for_pixels" in ui_c
    assert "HOME_LOG_VIEW_H" in ui_c
    assert "home_history_content_height" in ui_c
    assert "HOME_LOG_LINE_STEP" in ui_c
    assert "hermes_app_ui_home_history_upsert(APP_UI_MESSAGE_HERMES" in request_c
    assert "message_buffer[0] == '/'" in request_c
    assert "hermes_app_ui_home_history_max_scroll(status_line)" in request_c
    assert "text_is_fresh_session_command" in request_c
    assert "submit_hidden_reset_command" in request_c
    assert "prompt_picker_choice" in request_c
    assert "prompt_v2_interaction_choice" in request_c
    assert "interaction_required" in request_c
    assert "INTERACTION_VISIBLE_OPTION_COUNT 8U" in request_c
    assert '"Use Pad/touch. L/R page %lu/%lu. A choose, B cancel."' in request_c
    assert "hermes_app_requests_handle_reasoning_command" in request_c
    assert "hermes_app_requests_handle_fast_command" in request_c
    assert "THINKING" in ui_c
    assert "WORKING" in ui_c
    assert "g_home_history[index].height" in ui_c
    assert "entry->height" in ui_c
    assert "message_card_intersects_view" in ui_c
    assert "y < HOME_LOG_CLIP_BOTTOM && y + height > HOME_LOG_CLIP_TOP" in ui_c
    assert "HOME_WRAP_MAX_LINES 256" in ui_c
    assert "HOME_HISTORY_TEXT_MAX" in ui_c


def test_ui_wraps_truncated_lines_on_utf8_boundaries_before_graphical_fit():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "utf8_prefix_boundary" in ui_c
    assert "fit_prefix_to_width" in ui_c
    assert "append_truncation_ellipsis" in ui_c


def test_voice_input_waits_for_a_release_before_allowing_stop():
    voice_input_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()

    assert "hidKeysHeld() & KEY_A" in voice_input_c
    assert "waiting_for_a_release" in voice_input_c
    assert "KEY_UP" not in voice_input_c


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


def test_picture_input_uses_graphical_capture_render_helpers_without_console_printf():
    picture_input_c = (CLIENT_DIR / "source" / "picture_input.c").read_text()
    picture_input_h = (CLIENT_DIR / "include" / "picture_input.h").read_text()
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()

    assert "PictureInputPreview" in picture_input_h
    assert "picture_input_capture_prompt" in picture_input_h
    assert "picture_input_decode_bmp_preview" in picture_input_h
    assert "camInit()" in picture_input_c
    assert "CAMU_StartCapture" in picture_input_c
    assert "CAMU_SetReceiving" in picture_input_c
    assert "compute_picture_tone_map" in picture_input_c
    assert "build_processed_frame_from_camera" in picture_input_c
    assert "svcSleepThread(PICTURE_INPUT_CAPTURE_WARMUP_NS)" in picture_input_c
    assert "hermes_app_ui_render_picture_capture" in picture_input_c
    assert "hermes_app_ui_render_picture_review" in picture_input_c
    assert "hermes_app_ui_render_picture_capture" in ui_h
    assert "hermes_app_ui_render_picture_review" in ui_h
    assert "KEY_X" in picture_input_c
    assert "consoleSelect" not in picture_input_c
    assert "consoleClear" not in picture_input_c


def test_picture_preview_uses_display_transfer_for_gpu_texture_upload():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    picture_input_c = (CLIENT_DIR / "source" / "picture_input.c").read_text()

    assert "C3D_SyncDisplayTransfer" in ui_c
    assert "GX_TRANSFER_OUT_TILED(1)" in ui_c
    assert "preview->subtexture.top = 1.0f;" in ui_c
    assert "preview->subtexture.bottom = 0.0f;" in ui_c
    assert (
        "source_y0 = ((size_t)y * (size_t)frame->height) / PICTURE_INPUT_PREVIEW_HEIGHT;"
        in picture_input_c
    )
    assert "sample00 = frame->rgba8_data" in picture_input_c
    assert "C3D_TexUpload(&g_home_media_texture, rgba8_data)" not in ui_c


def test_picture_input_decodes_rgb565_red_from_high_bits_and_blue_from_low_bits():
    picture_input_c = (CLIENT_DIR / "source" / "picture_input.c").read_text()

    assert "u8 red = expand_5_to_8((u16)((pixel >> 11) & 0x1F));" in picture_input_c
    assert "u8 blue = expand_5_to_8((u16)(pixel & 0x1F));" in picture_input_c
    assert (
        "out_rgba[0] = tone_map_channel(expand_5_to_8((u16)((pixel >> 11) & 0x1F)), tone);"
        in picture_input_c
    )
    assert (
        "out_rgba[2] = tone_map_channel(expand_5_to_8((u16)(pixel & 0x1F)), tone);"
        in picture_input_c
    )


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
