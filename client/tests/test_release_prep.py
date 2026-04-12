from pathlib import Path


REPO_DIR = Path(__file__).resolve().parents[2]
CLIENT_DIR = REPO_DIR / "client"


def test_home_bottom_screen_avoids_duplicate_output_sections():
    main_c = (CLIENT_DIR / "source" / "main.c").read_text()

    assert 'printf("Health:' not in main_c
    assert 'printf("Reply page:' not in main_c
    assert 'printf("Gateway:' in main_c
    assert 'printf("Token:' in main_c


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
    assert "A — check Hermes gateway health" in readme
