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


def test_app_ui_uses_phase2_message_cards_and_bottom_summary_language():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "app_gfx_panel_inset" in ui_c
    assert '"YOU"' in ui_c
    assert '"HERMES"' in ui_c
    assert '"THREAD"' in ui_c
    assert '"LINK"' in ui_c
    assert "THREAD ARCHIVE" in ui_c
    assert "LINK SETTINGS" in ui_c
    assert "relay crest" not in ui_c


def test_phase2_followup_uses_touch_sized_home_buttons_and_message_cards():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert 'app_gfx_panel_inset(8.0f, 8.0f, 384.0f, 224.0f' in ui_c
    assert 'draw_action_button(16.0f, 44.0f, 136.0f, 28.0f, "Ask Hermes"' in ui_c
    assert 'draw_action_button(168.0f, 116.0f, 136.0f, 28.0f, "Clear Reply"' in ui_c
    assert 'draw_message_card(' in ui_c
    assert 'printf("Gateway:' not in ui_c
