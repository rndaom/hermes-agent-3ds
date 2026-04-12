from pathlib import Path


CLIENT_DIR = Path(__file__).resolve().parents[1]


def test_makefile_exists_with_3ds_metadata():
    makefile = CLIENT_DIR / "Makefile"
    assert makefile.exists(), "client/Makefile should exist"

    content = makefile.read_text()
    assert "APP_TITLE" in content
    assert "APP_DESCRIPTION" in content
    assert "APP_AUTHOR" in content
    assert "Hermes Agent 3DS" in content


def test_main_c_bootstraps_a_basic_old_3ds_friendly_loop():
    main_c = CLIENT_DIR / "source" / "main.c"
    assert main_c.exists(), "client/source/main.c should exist"

    content = main_c.read_text()
    assert "gfxInitDefault" in content
    assert "consoleInit" in content
    assert "aptMainLoop" in content
    assert "KEY_START" in content
    assert "Old 3DS" in content


def test_icon_and_include_dir_exist():
    assert (CLIENT_DIR / "icon.png").exists(), "client/icon.png should exist"
    assert (CLIENT_DIR / "include").exists(), "client/include should exist"
