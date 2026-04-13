from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_home_ui_cleanup_removes_telemetry_strip_and_moves_thread_status_to_bottom_summary():
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    health_h = (CLIENT_DIR / "include" / "bridge_health.h").read_text()

    assert "AppUiHomeTelemetry" not in ui_h
    assert "home_telemetry" not in ui_c
    assert "home_telemetry" not in main_c
    assert "draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f" in ui_c
    assert "draw_ruled_paper(8.0f, 156.0f, 304.0f, 52.0f" not in ui_c
    assert "home_status_summary(" in ui_c
    assert '"TOOL TRAY"' in ui_c
    assert '"PAGE"' not in ui_c
    assert '"Touch OK"' not in ui_c
    assert '"ACTIVE THREAD"' not in ui_c
    assert '"MODEL"' not in ui_c
    assert '"CONTEXT"' not in ui_c
    assert '"SESSION"' in ui_c
    assert "model_name" in health_h
    assert '"RELAY DECK"' not in ui_c


def test_home_command_menu_has_touch_support_and_reply_paging_on_pad_circle_and_shoulders():
    home_h = (CLIENT_DIR / "include" / "app_home.h").read_text()
    home_c = (CLIENT_DIR / "source" / "app_home.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "typedef enum HomeCommand" in home_h
    assert "HOME_COMMAND_TEXT" in home_h
    assert "HOME_COMMAND_CHECK" in home_h
    assert "HOME_COMMAND_SESSIONS" in home_h
    assert "HOME_COMMAND_SETTINGS" in home_h
    assert "HOME_COMMAND_AUDIO" in home_h
    assert "HOME_COMMAND_CLEAR" not in home_h
    assert "HomeCommand* command_selection" in home_h
    assert "KEY_DOWN" in home_c
    assert "KEY_UP" in home_c
    assert "KEY_CPAD_DOWN" in home_c
    assert "KEY_CPAD_UP" in home_c
    assert "hermes_app_ui_home_history_max_scroll" in home_c
    assert "history_scroll" in home_c
    assert "KEY_TOUCH" in home_c
    assert "hidTouchRead" in home_c
    assert "home_command_from_touch" in home_c
    assert "execute_selected_home_command" in home_c
    assert "command_selection != NULL" in home_c
    assert 'draw_action_button(16.0f, 44.0f, 136.0f, 28.0f, "Text Prompt"' in ui_c
    assert 'draw_action_button(168.0f, 44.0f, 136.0f, 28.0f, "Check Relay"' in ui_c
    assert 'draw_action_button(16.0f, 80.0f, 136.0f, 28.0f, "Sessions"' in ui_c
    assert 'draw_action_button(168.0f, 80.0f, 136.0f, 28.0f, "Settings"' in ui_c
    assert 'draw_action_button(92.0f, 116.0f, 136.0f, 28.0f, "Audio Prompt"' in ui_c
    assert 'draw_hint_button(206.0f, 214.0f, 98.0f, "START Exit"' in ui_c
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
