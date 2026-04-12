from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
DOCS_DIR = REPO_DIR / "docs"


def test_pictochat_dark_mode_spec_exists_and_locks_visual_direction():
    doc_path = DOCS_DIR / "hermes-pictochat-dark-mode-spec.md"
    assert doc_path.exists(), "docs/hermes-pictochat-dark-mode-spec.md should exist"

    doc = doc_path.read_text()
    assert "PictoChat" in doc
    assert "dark-mode" in doc.lower()
    assert "Old 3DS" in doc
    assert "Citro2D" in doc
    assert "PrintConsole" in doc
    assert "Do not mix `PrintConsole`" in doc
    assert "400x240" in doc
    assert "320x240" in doc


def test_design_language_points_to_pictochat_hermes_spec():
    doc = (DOCS_DIR / "design-language.md").read_text()
    assert "dark-mode Hermes + PictoChat + DS firmware messenger" in doc
    assert "docs/hermes-pictochat-dark-mode-spec.md" in doc
    assert "No Pokemon theme. No Nous theme." in doc


def test_pictochat_ui_plan_doc_exists_with_renderer_and_transient_flow_work():
    doc_path = DOCS_DIR / "plans" / "2026-04-12-hermes-pictochat-dark-mode-ui.md"
    assert doc_path.exists(), "docs/plans/2026-04-12-hermes-pictochat-dark-mode-ui.md should exist"

    doc = doc_path.read_text()
    assert "app_requests.c" in doc
    assert "voice_input.c" in doc
    assert "app_gfx" in doc
    assert "Old 3DS" in doc
    assert "reply pagination" in doc.lower()
