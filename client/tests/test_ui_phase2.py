from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"
DOCS_DIR = REPO_DIR / "docs"


def test_phase2_ui_doc_describes_ascii_framed_handheld_pass():
    doc_path = DOCS_DIR / "ui-phase2-hermes-handheld.md"
    assert doc_path.exists(), "docs/ui-phase2-hermes-handheld.md should exist"

    doc = doc_path.read_text()
    assert "Phase 2" in doc
    assert "ASCII-framed" in doc or "ASCII framed" in doc
    assert "Old 3DS" in doc
    assert "panel" in doc.lower()
    assert "crest" in doc.lower() or "relay" in doc.lower()


def test_app_ui_uses_phase2_framed_sections_and_relay_language():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "render_panel_title" in ui_c
    assert "HERMES RELAY" in ui_c
    assert "LINK STATE" in ui_c
    assert "THREAD ARCHIVE" in ui_c
    assert "LINK SETTINGS" in ui_c
    assert "HERMES REPLY" in ui_c
