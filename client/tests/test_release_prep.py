from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"


def test_home_bottom_screen_uses_action_deck_and_minimal_thread_summary():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()
    ui_c = (CLIENT_DIR / "source" / "app_ui.c").read_text()

    assert 'printf("Health:' not in main_c
    assert 'printf("Reply page:' not in main_c
    assert '"ACTION DECK"' in ui_c
    assert '"THREAD"' in ui_c
    assert '"LINK"' in ui_c
    assert 'app_gfx_text(18.0f, 170.0f, 0.30f, 0.30f, UI_SIGNAL_HI, "THREAD")' in ui_c
    assert 'app_gfx_text(18.0f, 196.0f, 0.30f, 0.30f, UI_SIGNAL_HI, "LINK")' in ui_c


def test_makefile_supports_packaged_release_zip_output():
    makefile = (CLIENT_DIR / "Makefile").read_text()

    assert ".PHONY: all clean release package" in makefile
    assert "release: all" in makefile
    assert "release/$(TARGET)-$(RELEASE_VERSION)" in makefile
    assert "python3 -m zipfile -c" in makefile
    assert "$(TARGET).3dsx" in makefile


def test_readme_includes_real_end_user_install_and_native_gateway_steps():
    readme = (REPO_DIR / "README.md").read_text()

    assert "## Quick start" in readme
    assert "sdmc:/3ds/hermes-agent-3ds/" in readme
    assert "native 3DS gateway" in readme
    assert "/api/v2/health" in readme
    assert "Open **Hermes Agent 3DS**" in readme
    assert "A — run the selected action" in readme or "A` — run the selected action" in readme
