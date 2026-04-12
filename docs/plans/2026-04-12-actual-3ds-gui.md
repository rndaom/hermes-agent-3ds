# Actual 3DS GUI Implementation Plan

> Historical note: this document reflects the earlier transition plan away from the console-only UI. It is now superseded by `docs/hermes-pictochat-dark-mode-spec.md` and `docs/plans/2026-04-12-hermes-pictochat-dark-mode-ui.md`.

> For Hermes: replace the current console-only presentation with a real graphical GUI, Old-3DS-safe, while keeping the existing app behavior and navigation logic.

Goal: move Hermes Agent 3DS off the current black-background PrintConsole UI and into a real pixel-art handheld GUI.

Architecture:
- Keep app state, networking, settings, and conversation logic as-is.
- Split rendering into a dedicated GUI layer with a small draw API.
- Start with a lightweight software/framebuffer renderer that works with current dependencies, then optionally add a Citro2D backend later.

Tech stack:
- existing libctru / gfxInitDefault path
- current app state modules
- raw framebuffer drawing via gfxGetFramebuffer / screen buffers
- optional future Citro2D backend once the dependency is available

Current repo facts at the time this historical plan was written:
- `client/source/main.c` currently initializes `consoleInit(GFX_TOP, &top_console);` and `consoleInit(GFX_BOTTOM, &bottom_console);`
- `client/source/app_ui.c` is entirely `PrintConsole*` based
- `client/Makefile` links only `-lctru -lm`
- this machine currently does not have Citro2D headers or lib installed

Success criteria:
- no pure text console look on the main screens
- non-black colored backgrounds and framed panels
- real highlight bars / boxes instead of ASCII alignment tricks
- Hermes reply area looks like a dialogue window
- bottom screen actions look like a menu, not terminal text
- still readable and performant on an original Old 3DS

## Task 1: Lock the rendering path
Objective: choose the first real GUI backend that can be implemented immediately.

Decision:
- use a custom framebuffer renderer first
- do not block on installing Citro2D
- keep a backend seam so Citro2D can replace or augment it later

Files:
- Modify: `client/source/main.c`
- Create: `client/include/app_gfx.h`
- Create: `client/source/app_gfx.c`
- Create: `client/tests/test_actual_gui_plan.py`

Implementation notes:
- `app_gfx.*` should expose primitives for:
  - clear screen with color
  - fill rect
  - draw rect border
  - draw panel frame
  - draw selection bar
  - draw simple bitmap text
- top and bottom screens should be addressed independently

## Task 2: Add a tiny bitmap font and panel primitives
Objective: make it possible to draw actual UI widgets without PrintConsole.

Files:
- Create: `client/include/app_gfx_font.h`
- Create: `client/source/app_gfx_font.c`
- Modify: `client/source/app_gfx.c`

Implementation notes:
- keep font tiny and fixed-width
- enough glyphs for uppercase, lowercase, numbers, punctuation used by Hermes UI
- optimize for clarity, not style
- use 1-bit or simple mask-based glyphs

## Task 3: Render a true graphical home screen
Objective: replace the top/bottom home console screens with real colored GUI panels.

Files:
- Modify: `client/include/app_ui.h`
- Modify: `client/source/app_ui.c`
- Modify: `client/source/main.c`
- Test: `client/tests/test_ui_actual_gui.py`

Top screen target:
- colored background
- framed title bar
- left status panel
- right crest/emblem panel
- framed dialogue/reply box

Bottom screen target:
- framed command menu
- selected row highlight bar
- gateway/token summary boxes
- footer hint strip

## Task 4: Convert settings and conversation screens
Objective: make all primary screens use the new GUI language.

Files:
- Modify: `client/source/app_ui.c`
- Test: `client/tests/test_settings_config.py`
- Test: `client/tests/test_ui_actual_gui.py`

Settings target:
- options menu rows
- active row highlight
- value box styling

Conversation target:
- thread list with highlight bar
- active thread badge/glyph
- preview row styling

## Task 5: Keep behavior, remove console dependence from the main loop
Objective: preserve interactions while no longer relying on console layout.

Files:
- Modify: `client/source/main.c`
- Modify: `client/include/app_ui.h`
- Modify: `client/source/app_ui.c`

Implementation notes:
- no more `PrintConsole*` parameters in the long term
- UI render should take app state, not console handles
- continue using existing input handling and bridge code unchanged

## Task 6: Verify on hardware
Objective: confirm the new GUI is actually what the user asked for.

Validation:
- `python -m pytest client/tests -q`
- `source /etc/profile.d/devkit-env.sh && make -C client clean && make -C client`
- deploy to `sd:/3ds/hermes-agent-3ds/`
- verify on Old 3DS:
  - no text-console look
  - good contrast
  - no obvious tearing/flicker
  - menu highlight readable
  - reply box readable

## Important constraints
- avoid heavy animation
- avoid alpha-heavy blending everywhere
- prefer flat fills and sharp borders
- use pixel-art panel language, not anti-aliased modern UI
- keep redraw work predictable

## Immediate next move
1. add `app_gfx.*` with framebuffer primitives
2. add failing tests for the new renderer files and non-console GUI path
3. land the first real graphical home screen before touching settings/conversations
