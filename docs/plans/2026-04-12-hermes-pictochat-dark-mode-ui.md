# Hermes Agent 3DS PictoChat Dark-Mode UI Implementation Plan

> For Hermes: use subagent-driven-development for code execution, but treat `docs/hermes-pictochat-dark-mode-spec.md` as the visual source of truth.

Goal: replace the current mixed graphical/console handheld UI with a stable dark-mode Hermes + PictoChat visual app on Old 3DS hardware.

Architecture:
- Keep networking, V2 gateway behavior, settings persistence, thread persistence, and input flow intact.
- Unify all user-facing rendering behind the existing Citro2D/Citro3D renderer seam in `app_gfx.*`.
- Restyle the app around dark framed rails, note-card messaging, compact command decks, and bounded text layout.

Tech stack:
- `libctru 2.7.0`
- `citro2d 1.7.0`
- `citro3d 1.7.1`
- existing `app_gfx.*`, `app_ui.*`, `voice_input.*`, `app_requests.*`
- optional ROMFS sprite assets via `tex3ds` later

Primary spec:
- `docs/hermes-pictochat-dark-mode-spec.md`

---

## Task 1: Lock the source of truth in docs and tests

Objective: make the repo point at the new visual direction before more code changes land.

Files:
- Modify: `docs/design-language.md`
- Modify: `README.md`
- Modify: `client/README.md`
- Modify: `docs/current-status.md`
- Modify: `docs/plans/2026-04-12-actual-3ds-gui.md`
- Create: `client/tests/test_ui_pictochat_spec.py`

Step 1: Add the spec link to design docs
- Update `docs/design-language.md` so its “Current visual direction” points at `docs/hermes-pictochat-dark-mode-spec.md`.
- Keep historical docs like `docs/pixel-rpg-visual-direction.md`, but stop presenting them as current.

Step 2: Add the spec link to the README
- Add the new spec to the docs link section.

Step 3: Align older adjacent docs
- Update `client/README.md` so it does not describe the client as console-first.
- Update `docs/current-status.md` so the new visual rewrite is clearly an active next step rather than implying UI work is complete.
- Mark `docs/plans/2026-04-12-actual-3ds-gui.md` as historical/superseded rather than current guidance.

Step 4: Add a focused regression test
- Assert that:
  - `docs/hermes-pictochat-dark-mode-spec.md` exists
  - it mentions `PictoChat`
  - it mentions `Old 3DS`
  - it mentions `Citro2D`
  - it mentions the renderer-mixing prohibition
  - `docs/design-language.md` points at the new spec

Step 5: Run the focused test

Run:
```bash
.venv/bin/python -m pytest client/tests/test_ui_pictochat_spec.py -q
```

Step 6: Commit
```bash
git add docs/design-language.md README.md client/README.md docs/current-status.md docs/plans/2026-04-12-actual-3ds-gui.md docs/hermes-pictochat-dark-mode-spec.md client/tests/test_ui_pictochat_spec.py
git commit -m "docs: add pictochat dark-mode 3ds ui spec"
```

---

## Task 2: Remove mixed frame-presentation and UI ownership

Objective: make `app_gfx` the only presentation owner for normal GUI screens and make `app_ui` the visual composition owner for all user-facing flows.

Files:
- Modify: `client/source/app_gfx.c`
- Modify: `client/source/main.c`
- Modify: `client/source/app_requests.c`
- Modify: `client/source/voice_input.c`
- Modify: `client/include/app_home.h`
- Modify: `client/include/app_requests.h`
- Modify: `client/include/voice_input.h`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Audit all direct swap/flush calls
- Search for:
  - `gfxFlushBuffers`
  - `gfxSwapBuffers`
  - `C3D_FrameBegin`
  - `C3D_FrameEnd`
- Confirm only `app_gfx.c` owns the normal GUI frame lifecycle after this task.

Step 2: Remove public `PrintConsole*` leakage from transient-flow APIs
- Update public headers and contexts so user-facing transient flows no longer require console handles as their rendering contract.
- Route transient composition through `app_ui` and renderer primitives instead.

Step 3: Remove direct GUI-screen presentation from transient modules
- `voice_input.c` and `app_requests.c` should stop presenting frames directly for normal GUI usage.
- If a temporary console-only fallback remains during migration, isolate it behind a dedicated debug-only path.

Step 4: Update tests
- Add assertions that no normal GUI path in `main.c`, `app_requests.c`, or `voice_input.c` calls `gfxSwapBuffers` directly.
- Add assertions that public transient-flow headers no longer expose `PrintConsole*` as normal user-facing rendering dependencies.
- Keep `app_gfx.c` as the allowed owner.

Step 5: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
```

Step 6: Commit
```bash
git add client/source/app_gfx.c client/source/main.c client/source/app_requests.c client/source/voice_input.c client/include/app_home.h client/include/app_requests.h client/include/voice_input.h client/tests/test_actual_gui_renderer.py
git commit -m "refactor: centralize 3ds gui frame presentation"
```

---

## Task 3: Add bounded text-layout primitives

Objective: stop uncontrolled string overdraw and lock visible line budgets using actual Citro2D text metrics.

Files:
- Modify: `client/include/app_gfx.h`
- Modify: `client/source/app_gfx.c`
- Modify: `client/source/app_ui.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Add measurement and ellipsis helpers
Examples:
- `app_gfx_measure_text(...)`
- `app_gfx_text_fit(...)`
- helper(s) that shorten strings to fit a target pixel width with ellipsis

Step 2: Define width budgets for all variable-length fields
At minimum:
- active thread label
- status line
- host value
- token summary
- device ID
- conversation title
- conversation preview
- reply page label
- approval/request summary
- command preview text
- voice-input status copy
- elapsed timer labels

Step 3: Replace raw unbounded draws in `app_ui.c`
- No direct long string draw should remain without truncation, paging, or width-fit intent.
- Do not rely on character count for proportional text layout.

Step 4: Add tests
- Assert new helpers exist.
- Assert `app_ui.c` uses them for variable-length fields.

Step 5: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
.venv/bin/python -m pytest client/tests -q
```

Step 6: Commit
```bash
git add client/include/app_gfx.h client/source/app_gfx.c client/source/app_ui.c client/tests/test_actual_gui_renderer.py
git commit -m "fix: add bounded text layout for graphical ui"
```

---

## Task 4: Fix reply pagination from shared layout constants

Objective: remove the current page/line mismatch in the graphical reply view and tie pagination to one shared layout definition.

Files:
- Modify: `client/source/app_ui.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Define one reply-layout source of truth
- Derive page size from shared values:
  - reply-card height
  - reserved title/meta area
  - body font scale
  - body line height
- Do not keep a page-count constant that can drift away from what is drawn.

Step 2: Update the graphical home screen reply block
- Make the top-screen reply card draw the same number of lines it claims to paginate.
- If the visual card cannot fit the original count, change the shared layout definition and update tests accordingly.

Step 3: Add tests
- Assert that the reply-page constant/layout definition and rendered-line usage stay aligned.

Step 4: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
```

Step 5: Commit
```bash
git add client/source/app_ui.c client/tests/test_actual_gui_renderer.py
git commit -m "fix: align graphical reply pagination with rendered lines"
```

---

## Task 5: Replace the placeholder palette with PictoChat dark-mode tokens

Objective: retheme the shell away from the current RPG/gold-teal placeholder look and toward the approved Hermes + PictoChat dark-mode system.

Files:
- Modify: `client/source/app_ui.c`
- Optionally create: `client/include/app_theme.h`
- Optionally create: `client/source/app_theme.c`
- Test: `client/tests/test_ui_pictochat_spec.py`

Step 1: Extract UI colors into named tokens
At minimum:
- app backgrounds
- panel levels
- border levels
- text levels
- default Hermes accent family
- semantic colors

Step 2: Apply the new token set to all primary screens
- top rail
- bottom rail
- cards/panels
- selection bars
- chips and emphasis text

Step 3: Match the spec’s usage rules
- keep content surfaces mostly neutral
- use accents on chrome, chips, focus, and identity elements
- do not saturate whole message surfaces

Step 4: Lock the first-pass typography decision
- keep system-font-backed readable body text for this pass
- if a future bundled display font is desired, track it as a later task rather than sneaking it into the palette pass

Step 5: Add tests
- Assert the new spec-oriented vocabulary or token file exists.
- Avoid overfitting exact hex strings everywhere in tests; prefer a token-module existence test if extracted.

Step 6: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_ui_pictochat_spec.py -q
.venv/bin/python -m pytest client/tests -q
```

Step 7: Commit
```bash
git add client/source/app_ui.c client/include/app_theme.h client/source/app_theme.c client/tests/test_ui_pictochat_spec.py
git commit -m "feat: apply hermes pictochat dark-mode ui tokens"
```

---

## Task 6: Convert the home screen into the spec’s note-card messenger layout

Objective: make the home screen feel like the target product, not a placeholder dashboard.

Files:
- Modify: `client/source/app_ui.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Update top-screen structure
Required top-screen blocks:
- top rail
- thread/link summary block
- main reply card with note-card framing
- proper page indicator

Step 2: Update bottom-screen structure
Required bottom-screen blocks:
- command deck header
- real action list with selection state
- compact utility summary wells

Step 3: Add identity-chip / card-edge treatment where appropriate
- thread chip or message/source chip
- compact system plaque for link state if needed

Step 4: Ensure selection is real
- bottom-screen highlight must correspond to actual navigable state, not a hardcoded decorative row

Step 5: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
.venv/bin/python -m pytest client/tests -q
```

Step 6: Commit
```bash
git add client/source/app_ui.c client/tests/test_actual_gui_renderer.py
git commit -m "feat: restyle home screen as pictochat hermes messenger"
```

---

## Task 7: Restyle settings and conversations to the same system

Objective: eliminate cross-screen style drift.

Files:
- Modify: `client/source/app_ui.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Settings screen
- make labels, value wells, and selection states match the spec
- keep editable value wells clearly separated from field labels

Step 2: Conversation screen
- use list rows with bounded title + preview
- active row highlight must be real
- add thread/archive identity cues without clutter

Step 3: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
.venv/bin/python -m pytest client/tests -q
```

Step 4: Commit
```bash
git add client/source/app_ui.c client/tests/test_actual_gui_renderer.py
git commit -m "feat: unify settings and conversation screens with ui spec"
```

---

## Task 8: Convert approval and voice-input flows off console UI

Objective: remove the last major user-facing style break and stability risk.

Files:
- Modify: `client/source/app_requests.c`
- Modify: `client/source/voice_input.c`
- Modify: `client/source/app_ui.c`
- Modify: `client/include/app_ui.h`
- Modify: `client/include/app_gfx.h`
- Modify: `client/source/app_gfx.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Add the minimum overlay/shell primitives needed
Examples:
- centered modal panel
- compact status plaque
- action strip
- meter or pulse bar helper if needed

Step 2: Approval flow
- graphical request summary
- graphical approve/reject actions
- composition should route through `app_ui` / renderer primitives rather than direct console drawing

Step 3: Voice-input flow
- graphical recording shell
- elapsed time/status
- stop/cancel affordances
- low redraw complexity
- composition should route through `app_ui` / renderer primitives rather than direct console drawing

Step 4: Add tests
- Assert these files no longer rely on user-facing `PrintConsole` rendering for normal app flow.

Step 5: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests/test_actual_gui_renderer.py -q
.venv/bin/python -m pytest client/tests -q
source /etc/profile.d/devkit-env.sh && make -C client clean && make -C client
```

Step 6: Commit
```bash
git add client/source/app_requests.c client/source/voice_input.c client/source/app_ui.c client/include/app_ui.h client/include/app_gfx.h client/source/app_gfx.c client/tests/test_actual_gui_renderer.py
git commit -m "feat: convert transient handheld flows to graphical ui"
```

---

## Task 9: Add optional asset support for icons and rails

Objective: move from placeholder rectangles into a polished but cheap-to-render visual system.

Files:
- Create: `client/gfx/` assets as needed
- Modify: `client/Makefile`
- Modify: `client/include/app_gfx.h`
- Modify: `client/source/app_gfx.c`
- Test: `client/tests/test_actual_gui_renderer.py`

Step 1: Add minimal assets only
Suggested first asset set:
- relay icon / crest tile
- small mic icon
- thread/archive icon
- status glyph set
- optional rail or border tile strip

Step 2: Wire assets through the existing `gfx/*.t3s` pipeline
- keep asset count minimal
- use sprite sheet / atlas where practical
- only move to ROMFS later if there is a clear reason and dedicated follow-up work

Step 3: Replace placeholder “relay crest” block and raw geometry-only stand-ins

Step 4: Verify
Run:
```bash
.venv/bin/python -m pytest client/tests -q
source /etc/profile.d/devkit-env.sh && make -C client clean && make -C client
```

Step 5: Commit
```bash
git add client/Makefile client/gfx client/include/app_gfx.h client/source/app_gfx.c client/tests/test_actual_gui_renderer.py
git commit -m "feat: add handheld ui icon assets for graphical shell"
```

---

## Task 10: Real-hardware acceptance on Old 3DS

Objective: prove the spec survives real hardware and not just local builds.

Files:
- No required source changes unless bugs are found.
- Record notes in `docs/current-status.md` if a milestone is fully verified.

Step 1: Build and deploy
Run:
```bash
source /etc/profile.d/devkit-env.sh && make -C client clean && make -C client
```
Deploy over FTPD to:
```text
sdmc:/3ds/hermes-agent-3ds/
```

Step 2: Check visual integrity
Verify on real hardware:
- no corrupted or stray colored pixels
- no flicker when moving between home/settings/conversations
- no style jump into raw console UI during approval or voice input

Step 3: Check text safety
Verify:
- long reply pages show every line in order
- long host/token/device/thread strings stay clipped or ellipsized cleanly
- page indicator matches actual visible content

Step 4: Check interaction integrity
Verify:
- menu highlight follows real selection
- software keyboard handoff returns to the correct shell cleanly
- approval flow is readable
- voice-input shell remains stable during recording

Step 5: If accepted, update status docs
- note the spec milestone in `docs/current-status.md`

Step 6: Commit
```bash
git add docs/current-status.md
git commit -m "docs: record verified pictochat dark-mode ui milestone"
```

---

## Acceptance criteria

This plan is complete only when:
- the app has one clear dark-mode Hermes + PictoChat visual identity
- all normal user-facing screens use the same graphical renderer language
- no mixed GUI/console normal-screen rendering remains
- pagination, clipping, and focus state are correct
- the result is verified on original / Old 3DS hardware

## Implementation guardrails

Always keep these in view:
- no feature work mixed into the UI pass unless required
- no redesign that turns the app into Pokemon, Nous, or a generic mobile chat clone
- no overbuilt animation
- no unbounded text fields
- no direct frame presentation outside the renderer
- no assuming emulator success equals hardware success
