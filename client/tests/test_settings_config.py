from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_app_config_module_exists_with_sdmc_config_path_and_defaults():
    header_path = CLIENT_DIR / "include" / "app_config.h"
    source_path = CLIENT_DIR / "source" / "app_config.c"

    assert header_path.exists(), "client/include/app_config.h should exist"
    assert source_path.exists(), "client/source/app_config.c should exist"

    header = header_path.read_text()
    assert "sdmc:/3ds/hermes-agent-3ds/config.ini" in header
    assert "HermesAppConfig" in header
    assert "DEFAULT_BRIDGE_HOST" in header
    assert "DEFAULT_BRIDGE_PORT" in header
    assert "hermes_app_config_load" in header
    assert "hermes_app_config_save" in header
    assert "active_conversation_id" in header
    assert "recent_conversations" in header
    assert "hermes_app_config_build_health_url" in header
    assert "hermes_app_config_build_conversations_url" in header


def test_app_config_source_reads_and_writes_host_port_token_device_id_and_conversations():
    source = (CLIENT_DIR / "source" / "app_config.c").read_text()
    assert "mkdir(" in source
    assert "fopen(" in source
    assert "fgets(" in source
    assert "fprintf(" in source
    assert '"host=%s\\n"' in source
    assert '"port=%u\\n"' in source
    assert '"token=%s\\n"' in source
    assert '"device_id=%s\\n"' in source
    assert '"active_conversation_id=%s\\n"' in source
    assert '"recent_conversations="' in source
    assert "DEFAULT_BRIDGE_HOST" in source
    assert "DEFAULT_BRIDGE_PORT" in source
    assert "DEFAULT_CONVERSATION_ID" in source


def test_main_c_loads_saved_config_and_uses_it_for_health_checks():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    assert '"app_config.h"' in main_c
    assert "HermesAppConfig" in main_c
    assert "hermes_app_config_load" in main_c
    assert "hermes_app_config_save" in main_c
    assert "hermes_app_config_build_health_url" in main_c
    assert "bridge_health_check_run(DEFAULT_BRIDGE_HEALTH_URL" not in main_c


def test_main_c_has_settings_and_conversation_picker_navigation():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    conv_header = (CLIENT_DIR / "include" / "app_conversations.h").read_text()
    conv_c = (CLIENT_DIR / "source" / "app_conversations.c").read_text()
    input_header = (CLIENT_DIR / "include" / "app_input.h").read_text()
    input_c = (CLIENT_DIR / "source" / "app_input.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert "APP_SCREEN_SETTINGS" in main_c
    assert "APP_SCREEN_CONVERSATIONS" in main_c
    assert '"app_conversations.h"' in main_c
    assert "KEY_X" in main_c
    assert "KEY_UP" in main_c
    assert "KEY_SELECT" in main_c
    assert "KEY_DOWN" in conv_c
    assert "swkbdInit" in input_c
    assert "swkbdInputText" in input_c
    assert "SettingsField" in input_header
    assert "settings_field_label" in input_header
    assert "AppConversationState" in conv_header
    assert "Host" in input_c
    assert "Port" in input_c
    assert "Token" in input_c
    assert "Device ID" in input_c
    assert "Conversation picker opened" in conv_c
    assert "bridge_v2_list_conversations" in conv_c
    assert "active_conversation_id" in conv_c
    assert "SELECT conv" in ui_c or "Conversations" in ui_c
    assert "Save settings" in main_c or "X: save" in main_c
