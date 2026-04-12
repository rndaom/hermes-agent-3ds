from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"
DOCS_DIR = REPO_DIR / "docs"


def test_pixel_rpg_visual_direction_doc_exists_and_mentions_non_infringing_goal():
    doc_path = DOCS_DIR / "pixel-rpg-visual-direction.md"
    assert doc_path.exists(), "docs/pixel-rpg-visual-direction.md should exist"

    doc = doc_path.read_text()
    assert "pixel RPG" in doc or "pixel-rpg" in doc.lower()
    assert "classic handheld" in doc.lower()
    assert "Pokemon-specific" in doc or "pokemon-specific" in doc.lower()
    assert "relay crest" in doc.lower()
    assert "command menu" in doc.lower()


def test_app_ui_uses_pixel_rpg_home_screen_language_and_menu_rows():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "RELAY DECK" in ui_c
    assert "COMMAND MENU" in ui_c
    assert "Ask Hermes" in ui_c
    assert "Check Link" in ui_c
    assert "Mic Input" in ui_c
    assert "Clear Reply" in ui_c


def test_app_ui_uses_pixel_rpg_thread_and_options_labels():
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert "THREAD LOG" in ui_c
    assert "OPTIONS MENU" in ui_c
    assert "Current Channel" in ui_c
    assert "Link Status" in ui_c
