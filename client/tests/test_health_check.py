from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_bridge_health_check_source_files_exist():
    assert (CLIENT_DIR / "include" / "bridge_health.h").exists()
    assert (CLIENT_DIR / "source" / "bridge_health.c").exists()


def test_bridge_health_defaults_target_the_health_endpoint():
    header = (CLIENT_DIR / "include" / "bridge_health.h").read_text()
    assert "DEFAULT_BRIDGE_HEALTH_URL" in header
    assert "/api/v1/health" in header
    assert "http://" in header


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


def test_main_c_offers_a_button_driven_bridge_health_check_ui():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    assert "Press A to check bridge health." in main_c
    assert "Checking bridge..." in main_c
    assert "Bridge OK" in main_c
    assert "Bridge check failed" in main_c
    assert "last rc:" in main_c
    assert "socket errno:" in main_c
    assert "stage:" in main_c
    assert "KEY_A" in main_c
