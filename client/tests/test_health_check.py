from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_bridge_health_check_source_files_exist():
    assert (CLIENT_DIR / "include" / "bridge_health.h").exists()
    assert (CLIENT_DIR / "source" / "bridge_health.c").exists()


def test_bridge_health_defaults_target_the_native_v2_health_endpoint():
    header = (CLIENT_DIR / "include" / "bridge_health.h").read_text()
    app_config_header = (CLIENT_DIR / "include" / "app_config.h").read_text()
    app_config_source = (CLIENT_DIR / "source" / "app_config.c").read_text()

    assert "DEFAULT_BRIDGE_HEALTH_URL" in header
    assert "/api/v2/health" in header
    assert "/api/v1/health" not in header
    assert "hermes_app_config_build_health_url" in app_config_header
    assert '"/api/v2/health"' in app_config_source
    assert '"/api/v1/health"' not in app_config_source


def test_health_check_implementation_uses_wifi_and_timeout_based_socket_networking():
    source = (CLIENT_DIR / "source" / "bridge_health.c").read_text()
    assert "ACU_GetWifiStatus" in source
    assert "socInit" in source
    assert "connect(" in source
    assert "select(" in source
    assert "send(" in source
    assert "recv(" in source
    assert "Bridge timed out." in source
    assert "socket_errno" in source
    assert "socket_stage" in source


def test_main_c_offers_a_button_driven_native_v2_gateway_health_check_ui():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    home_h = (CLIENT_DIR / "include" / "app_home.h").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    request_c = (CLIENT_DIR / "source" / "app_requests.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    assert "Press A to check Hermes gateway." in ui_c
    assert "AppHomeContext" in home_h
    assert "Checking Hermes gateway..." in home_c
    assert "Reply received over native v2." in request_c
    assert "KEY_A" in home_c
    assert "bridge_health_check_run" in home_c
    assert "bridge_v2_get_capabilities" not in main_c
    assert "Native v2 responded even though v1 health failed." not in main_c
