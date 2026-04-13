from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_home_ui_cleanup_removes_telemetry_strip_and_uses_single_thread_status_box():
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    health_h = (CLIENT_DIR / "include" / "bridge_health.h").read_text()

    assert "AppUiHomeTelemetry" not in ui_h
    assert "home_telemetry" not in ui_c
    assert "home_telemetry" not in main_c
    assert 'app_gfx_panel_inset(10.0f, 10.0f, 380.0f, 64.0f' in ui_c
    assert '"ROOM"' in ui_c
    assert '"LINK"' in ui_c
    assert '"MODEL"' not in ui_c
    assert '"CONTEXT"' not in ui_c
    assert '"SESSION"' not in ui_c
    assert "model_name" in health_h
    assert 'draw_header("HERMES AGENT", "RELAY DECK")' not in ui_c


def test_home_command_menu_has_real_navigation_state_and_unlabeled_rows():
    home_h = (CLIENT_DIR / "include" / "app_home.h").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "typedef enum HomeCommand" in home_h
    assert "HOME_COMMAND_ASK" in home_h
    assert "HOME_COMMAND_CHECK" in home_h
    assert "HOME_COMMAND_THREADS" in home_h
    assert "HOME_COMMAND_CONFIG" in home_h
    assert "HOME_COMMAND_MIC" in home_h
    assert "HOME_COMMAND_CLEAR" in home_h
    assert "HOME_COMMAND_EXIT" in home_h
    assert "HomeCommand* command_selection" in home_h
    assert "KEY_DOWN" in home_c
    assert "KEY_UP" in home_c
    assert "KEY_LEFT" in home_c
    assert "KEY_RIGHT" in home_c
    assert "KEY_CPAD_DOWN" in home_c
    assert "KEY_CPAD_UP" in home_c
    assert "KEY_CPAD_LEFT" in home_c
    assert "KEY_CPAD_RIGHT" in home_c
    assert "KEY_SELECT" not in home_c
    assert "execute_selected_home_command" in home_c
    assert "command_selection != NULL" in home_c
    assert 'draw_menu_row(16.0f, 40.0f, 124.0f, "Ask Hermes", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 64.0f, 124.0f, "Check Link", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 88.0f, 124.0f, "Conversations", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 112.0f, 124.0f, "Settings", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 136.0f, 124.0f, "Mic Input", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 160.0f, 124.0f, "Clear Reply", selected_command' in ui_c
    assert 'draw_menu_row(16.0f, 184.0f, 124.0f, "Exit", selected_command' in ui_c
    assert '"B Ask Hermes"' not in ui_c
    assert '"A Check Link"' not in ui_c
    assert '"SELECT Threads"' not in ui_c


def test_home_reply_layout_uses_pixel_width_wrapping_not_console_character_width():
    gfx_h = (CLIENT_DIR / "include" / "app_gfx.h").read_text()
    gfx_c = (CLIENT_DIR / "source" / "app_gfx.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "app_gfx_measure_text" in gfx_h
    assert "app_gfx_measure_text(" in gfx_c
    assert "wrap_text_for_pixels" in ui_c
    assert "HOME_WRAP_WIDTH" not in ui_c
