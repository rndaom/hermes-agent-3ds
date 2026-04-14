from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
DOCS_DIR = REPO_DIR / "docs"


def test_pictochat_clone_spec_exists_and_replaces_the_dark_mode_version():
    doc_path = DOCS_DIR / "hermes-pictochat-clone-spec.md"
    assert doc_path.exists(), "docs/hermes-pictochat-clone-spec.md should exist"
    assert not (DOCS_DIR / "hermes-pictochat-dark-mode-spec.md").exists()

    doc = doc_path.read_text()
    assert "PictoChat" in doc
    assert "Text Prompt" in doc
    assert "Check Relay" in doc
    assert "session" in doc.lower() or "room" in doc.lower()
    assert "Old 3DS" in doc
    assert "Citro2D" in doc
    assert "PrintConsole" in doc
    assert "400x240" in doc
    assert "320x240" in doc


def test_design_language_points_to_pictochat_clone_spec():
    doc = (DOCS_DIR / "design-language.md").read_text()
    assert "PictoChat-clone Hermes messenger" in doc
    assert "docs/hermes-pictochat-clone-spec.md" in doc
    assert "No Pokemon theme. No Nous theme." in doc


def test_pictochat_clone_ui_plan_exists_with_renderer_and_room_book_work():
    doc_path = DOCS_DIR / "plans" / "2026-04-13-hermes-pictochat-clone-ui.md"
    assert doc_path.exists(), (
        "docs/plans/2026-04-13-hermes-pictochat-clone-ui.md should exist"
    )

    doc = doc_path.read_text()
    assert "app_ui.c" in doc
    assert "app_requests.c" in doc
    assert "app_gfx" in doc
    assert "slash-command" in doc.lower() or "slash command" in doc.lower()
    assert "Text Prompt" in doc
