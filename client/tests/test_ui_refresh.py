from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"
DOCS_DIR = REPO_DIR / "docs"


def test_phase1_ui_refresh_doc_defines_hermes_handheld_direction():
    doc_path = DOCS_DIR / "ui-phase1-hermes-handheld.md"
    assert doc_path.exists(), "docs/ui-phase1-hermes-handheld.md should exist"

    doc = doc_path.read_text()
    assert "Hermes Handheld" in doc
    assert "dark charcoal" in doc
    assert "amber" in doc.lower()
    assert "top screen" in doc.lower()
    assert "bottom screen" in doc.lower()
    assert "Old 3DS" in doc


def test_app_ui_uses_home_action_list_and_clean_thread_summary():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "ACTIONS" in ui_c
    assert "ACTIVE THREAD" in ui_c
    assert '"ROOM"' in ui_c
    assert '"LINK"' in ui_c
    assert "HERMES REPLY" in ui_c
    assert "THREAD ARCHIVE" in ui_c
    assert "SYSTEM CONFIG" in ui_c
    assert '"MODEL"' not in ui_c
    assert '"CONTEXT"' not in ui_c
    assert '"SESSION"' not in ui_c
