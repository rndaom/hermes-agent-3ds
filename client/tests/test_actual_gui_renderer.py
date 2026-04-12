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
    assert "app_gfx_text_fit" in header
    assert "C2D_CreateScreenTarget" in source
    assert "C2D_DrawRectSolid" in source
    assert "C2D_DrawText" in source
    assert "C2D_TextGetDimensions" in source


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
    assert "consoleInit(GFX_TOP" not in main_c
    assert "consoleInit(GFX_BOTTOM" not in main_c
    assert "PrintConsole" not in main_c


def test_transient_ui_paths_do_not_expose_printconsole_or_manual_buffer_swaps():
    home_h = (CLIENT_DIR / "include" / "app_home.h").read_text()
    request_h = (CLIENT_DIR / "include" / "app_requests.h").read_text()
    voice_h = (CLIENT_DIR / "include" / "voice_input.h").read_text()
    request_c = (CLIENT_DIR / "source" / "app_requests.c").read_text()
    voice_c = (CLIENT_DIR / "source" / "voice_input.c").read_text()
    ui_h = (CLIENT_DIR / "include" / "app_ui.h").read_text()

    assert "PrintConsole" not in home_h
    assert "PrintConsole" not in request_h
    assert "PrintConsole" not in voice_h
    assert "gfxSwapBuffers" not in request_c
    assert "gfxFlushBuffers" not in request_c
    assert "gfxSwapBuffers" not in voice_c
    assert "gfxFlushBuffers" not in voice_c
    assert "hermes_app_ui_render_approval_prompt" in ui_h
    assert "hermes_app_ui_render_voice_recording" in ui_h
