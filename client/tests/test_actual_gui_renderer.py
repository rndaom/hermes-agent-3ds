from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"


def test_makefile_links_citro2d_and_citro3d_for_graphical_ui():
    makefile = (CLIENT_DIR / "Makefile").read_text()

    assert "-lcitro2d" in makefile
    assert "-lcitro3d" in makefile


def test_app_gfx_module_exists_with_citro2d_screen_targets_and_panel_primitives():
    header = (CLIENT_DIR / "include" / "app_gfx.h").read_text()
    source = (CLIENT_DIR / "source" / "app_gfx.c").read_text()

    assert "app_gfx_init" in header
    assert "app_gfx_begin_frame" in header
    assert "app_gfx_end_frame" in header
    assert "app_gfx_panel" in header
    assert "C2D_CreateScreenTarget" in source
    assert "C2D_DrawRectSolid" in source
    assert "C2D_DrawText" in source


def test_app_ui_exposes_graphical_ui_lifecycle_without_printconsole_render_args():
    header = (CLIENT_DIR / "include" / "app_ui.h").read_text()

    assert "bool hermes_app_ui_init(void);" in header
    assert "void hermes_app_ui_exit(void);" in header
    assert "void hermes_app_ui_render(" in header
    assert "PrintConsole* top_console" not in header
    assert "PrintConsole* bottom_console" not in header


def test_main_uses_graphical_ui_init_and_exit_hooks():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert '"app_gfx.h"' in main_c
    assert "hermes_app_ui_init()" in main_c
    assert "hermes_app_ui_exit()" in main_c
    assert "C3D_Init" not in main_c
