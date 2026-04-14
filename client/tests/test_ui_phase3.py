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


def test_app_ui_has_phase3_message_log_home_layout():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "render_home_graphical" in ui_c
    assert "draw_message_card" in ui_c
    assert "draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f" in ui_c
    assert "draw_ruled_paper(8.0f, 156.0f, 304.0f, 52.0f" not in ui_c
    assert 'draw_action_button(16.0f, 44.0f, 136.0f, 28.0f, "Text Prompt"' in ui_c
    assert 'draw_action_button(168.0f, 44.0f, 136.0f, 28.0f, "Clear Screen"' in ui_c
    assert '"Touch OK"' not in ui_c
    assert '"ACTIVE THREAD"' not in ui_c


def test_phase3_followup_keeps_explicit_graphical_panel_layout():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "draw_bottom_header(" in ui_c
    assert "app_gfx_begin_top" in ui_c
    assert "app_gfx_begin_bottom" in ui_c
    assert "draw_top_header(" in ui_c
    assert "draw_ruled_paper(8.0f, 8.0f, 384.0f, 224.0f" in ui_c
    assert "draw_ruled_paper(8.0f, 156.0f, 304.0f, 52.0f" not in ui_c
    assert 'draw_hint_button(114.0f, 214.0f, 80.0f, "A Select"' in ui_c
