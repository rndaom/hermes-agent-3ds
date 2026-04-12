from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
DOCS_DIR = REPO_DIR / "docs"
CLIENT_DIR = REPO_DIR / "client"


def test_pixel_rpg_visual_direction_doc_exists_and_mentions_non_infringing_goal():
    doc_path = DOCS_DIR / "pixel-rpg-visual-direction.md"
    assert doc_path.exists(), "docs/pixel-rpg-visual-direction.md should exist"

    doc = doc_path.read_text()
    assert "classic handheld" in doc.lower()
    assert "Pokemon-specific" in doc or "pokemon-specific" in doc.lower()
    assert "relay crest" in doc.lower()


def test_actual_gui_plan_doc_exists_for_moving_off_console_ui():
    doc_path = DOCS_DIR / "plans" / "2026-04-12-actual-3ds-gui.md"
    assert doc_path.exists(), "docs/plans/2026-04-12-actual-3ds-gui.md should exist"

    doc = doc_path.read_text()
    assert "PrintConsole" in doc
    assert "consoleInit" in doc
    assert "framebuffer" in doc.lower() or "citro2d" in doc.lower()
    assert "Old 3DS" in doc


def test_main_c_still_uses_the_graphical_renderer_while_historical_gui_doc_remains_for_reference():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert "hermes_app_ui_init()" in main_c
    assert "hermes_app_ui_exit()" in main_c
    assert "consoleInit(GFX_TOP, &top_console);" not in main_c
    assert "consoleInit(GFX_BOTTOM, &bottom_console);" not in main_c
