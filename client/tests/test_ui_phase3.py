from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"
DOCS_DIR = REPO_DIR / "docs"


def test_phase3_ui_doc_describes_pixel_art_composition_pass():
    doc_path = DOCS_DIR / "ui-phase3-hermes-handheld.md"
    assert doc_path.exists(), "docs/ui-phase3-hermes-handheld.md should exist"

    doc = doc_path.read_text()
    assert "Phase 3" in doc
    assert "pixel art" in doc.lower()
    assert "top screen" in doc.lower()
    assert "right-side crest" in doc.lower() or "right side crest" in doc.lower()
    assert "Old 3DS" in doc


def test_app_ui_has_phase3_graphical_two_panel_layout_and_crest_block():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "render_home_graphical" in ui_c
    assert "app_gfx_panel_inset(12.0f, 48.0f, 226.0f, 76.0f" in ui_c
    assert "app_gfx_panel_inset(248.0f, 48.0f, 140.0f, 76.0f" in ui_c
    assert "relay crest" in ui_c
    assert "HERMES REPLY" in ui_c


def test_phase3_followup_uses_explicit_graphical_panel_layout():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "draw_header(" in ui_c
    assert "draw_bottom_header(" in ui_c
    assert "app_gfx_begin_top" in ui_c
    assert "app_gfx_begin_bottom" in ui_c
    assert "app_gfx_panel_inset(12.0f, 132.0f, 376.0f, 96.0f" in ui_c
