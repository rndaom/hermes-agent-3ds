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

    assert "app_gfx_panel_inset" in ui_c
    assert "relay crest" in ui_c
    assert "LINK STATUS" in ui_c
    assert "THREAD ARCHIVE" in ui_c
    assert "LINK SETTINGS" in ui_c
    assert "HERMES REPLY" in ui_c


def test_phase2_followup_uses_wider_graphical_panels_and_less_duplicate_home_info():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "app_gfx_panel_inset(12.0f, 132.0f, 376.0f, 96.0f" in ui_c
    assert 'printf("ACTIVE THREAD\\n%s\\n", conversation_label);' not in ui_c
    assert 'printf("Gateway:' not in ui_c
