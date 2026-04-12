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


def test_app_ui_has_phase3_split_layout_and_pixel_crest_helpers():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "render_split_line" in ui_c
    assert "render_pixel_crest_line" in ui_c
    assert "crest_lines" in ui_c
    assert "Use COMMAND DECK below." in ui_c


def test_phase3_followup_uses_explicit_top_screen_cursor_layout():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "print_at(" in ui_c
    assert "clear_text_area(" in ui_c
    assert "\\x1b[%d;%dH" in ui_c
    assert "[ MESSENGER LINK ]" in ui_c
    assert "[ RELAY CREST ]" in ui_c
    assert "[ SESSION ]" in ui_c
