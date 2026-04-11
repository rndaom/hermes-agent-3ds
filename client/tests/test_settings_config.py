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
    assert "hermes_app_config_build_health_url" in header


def test_app_config_source_reads_and_writes_host_port_and_token():
    source = (CLIENT_DIR / "source" / "app_config.c").read_text()
    assert "mkdir(" in source
    assert "fopen(" in source
    assert "fgets(" in source
    assert "fprintf(" in source
    assert '"host=%s\\n"' in source
    assert '"port=%u\\n"' in source
    assert '"token=%s\\n"' in source
    assert "DEFAULT_BRIDGE_HOST" in source
    assert "DEFAULT_BRIDGE_PORT" in source


def test_main_c_loads_saved_config_and_uses_it_for_health_checks():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    assert '"app_config.h"' in main_c
    assert "HermesAppConfig" in main_c
    assert "hermes_app_config_load" in main_c
    assert "hermes_app_config_save" in main_c
    assert "hermes_app_config_build_health_url" in main_c
    assert "bridge_health_check_run(DEFAULT_BRIDGE_HEALTH_URL" not in main_c


def test_main_c_has_settings_screen_navigation_and_keyboard_editing():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    assert "APP_SCREEN_SETTINGS" in main_c
    assert "KEY_X" in main_c
    assert "KEY_UP" in main_c
    assert "KEY_DOWN" in main_c
    assert "swkbdInit" in main_c
    assert "swkbdInputText" in main_c
    assert "Host" in main_c
    assert "Port" in main_c
    assert "Token" in main_c
    assert "X: settings" in main_c or "Settings" in main_c
    assert "Save settings" in main_c or "X: save" in main_c
