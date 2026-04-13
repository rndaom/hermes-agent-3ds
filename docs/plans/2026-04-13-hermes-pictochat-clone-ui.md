# Hermes Agent 3DS PictoChat Clone UI Plan

> Use `docs/hermes-pictochat-clone-spec.md` as the source of truth.

## Goal

Replace the previous dark-mode shell with a PictoChat-clone handheld UI while keeping the Hermes V2 gateway behavior intact.

## Locked scope

- preserve the transport stack
- preserve SD-backed config and room persistence
- preserve reply paging, approvals, and mic capture
- replace the user-facing shell, wording, and layout system

## Visual direction

- light ruled-paper surfaces
- DS gradient header rails
- room-colored note chips
- PictoChat-style note board on the top screen
- DS tool tray on the bottom screen

## Repo surfaces to update

### Docs
- `README.md`
- `client/README.md`
- `docs/design-language.md`
- `docs/current-status.md`
- `docs/install.md`
- historical notes that pointed at the removed dark-mode spec

### Source
- `client/source/app_gfx.c`
- `client/source/app_ui.c`
- `client/source/app_home.c`
- `client/source/app_conversations.c`
- `client/source/app_input.c`
- `client/source/app_requests.c`

### Tests
- `client/tests/test_ui_pictochat_spec.py`
- `client/tests/test_actual_gui_renderer.py`
- `client/tests/test_release_prep.py`
- `client/tests/test_ui_live_feedback.py`
- `client/tests/test_ui_phase2.py`
- `client/tests/test_ui_phase3.py`
- `client/tests/test_ui_refresh.py`
- any related test that locked the old labels

## Build and validation

Run:

```bash
.venv/bin/python -m pytest client/tests -q
make -C client clean && make -C client
make -C client release RELEASE_VERSION=local
```

## Deployment target

Copy to:

```text
sdmc:/3ds/hermes-agent-3ds/
```

Expected deployed files:
- `hermes-agent-3ds.3dsx`
- `hermes-agent-3ds.smdh`

## Completion criteria

- docs reference only the new PictoChat clone spec
- the old dark-mode spec and plan are gone
- the home UI now uses `Write Note`, `Check Relay`, `Rooms`, `Setup`, `Mic Note`, and `Clear Board`
- the room picker is presented as the `Room Book`
- setup reads as a paper config sheet
- approval and mic screens match the same shell
- tests pass
- release artifact builds
- the new build is pushed and deployed to the real 3DS
