from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_app_config_supports_native_v2_urls_and_device_identity():
    header = (CLIENT_DIR / "include" / "app_config.h").read_text()
    source = (CLIENT_DIR / "source" / "app_config.c").read_text()

    assert "HERMES_APP_MESSAGES_URL_MAX" in header
    assert "HERMES_APP_EVENTS_URL_MAX" in header
    assert "HERMES_APP_VOICE_URL_MAX" in header
    assert "HERMES_APP_INTERACTION_URL_MAX" in header
    assert "HERMES_APP_DEVICE_ID_MAX" in header
    assert "device_id" in header
    assert "hermes_app_config_build_messages_url" in header
    assert "hermes_app_config_build_events_url" in header
    assert "hermes_app_config_build_voice_url" in header
    assert "hermes_app_config_build_interaction_url" in header

    assert '"/api/v2/health"' in source
    assert '"/api/v1/health"' not in source
    assert '"/api/v2/messages"' in source
    assert '"/api/v2/events"' in source
    assert '"/api/v2/voice"' in source
    assert '"/api/v2/conversations"' in source
    assert '"/api/v2/interactions/"' in source
    assert "device_id=" in source


def test_bridge_v2_module_exists_for_native_gateway_protocol():
    header_path = CLIENT_DIR / "include" / "bridge_v2.h"
    source_path = CLIENT_DIR / "source" / "bridge_v2.c"

    assert header_path.exists(), "client/include/bridge_v2.h should exist"
    assert source_path.exists(), "client/source/bridge_v2.c should exist"

    header = header_path.read_text()
    source = source_path.read_text()

    assert "BridgeV2MessageResult" in header
    assert "BridgeV2EventPollResult" in header
    assert "BridgeV2InteractionResult" in header
    assert "u32 cursor;" in header
    assert "reply_to_message_id" in header
    assert "bridge_v2_send_message" in header
    assert "bridge_v2_send_voice_message" in header
    assert "bridge_v2_poll_events" in header
    assert "bridge_v2_submit_interaction" in header

    assert "Authorization: Bearer " in source
    assert "bridge_v2_send_message" in source or '"/api/v2/messages"' in source
    assert "bridge_v2_send_voice_message" in source or '"/api/v2/voice"' in source
    assert "bridge_v2_list_conversations" in source
    assert "device_id" in source
    assert "conversation_id" in source
    assert "audio/wav" in source
    assert "extract_json_u32(response_body, \"cursor\"" in source
    assert "cursor" in source
    assert "wait" in source
    assert "approval.request" in source
    assert "message.created" in source
    assert "message.updated" in source
    assert "reply_to" in source


def test_bridge_v2_decodes_common_json_unicode_punctuation_for_3ds_console():
    source = (CLIENT_DIR / "source" / "bridge_v2.c").read_text()

    assert "case 'u':" in source
    assert "hex_digit_value" in source
    assert "append_codepoint_fallback" in source
    assert "0x2019" in source
    assert "0x2014" in source
    assert "0x2026" in source


def test_makefile_picks_up_bridge_v2_sources_automatically():
    makefile = (CLIENT_DIR / "Makefile").read_text()
    assert "$(wildcard $(dir)/*.c)" in makefile
