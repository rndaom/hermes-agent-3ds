# client

This directory contains the Nintendo 3DS homebrew app.

## Current status

The client now has:
- devkitPro-style project layout
- build Makefile
- packaged `.3dsx` release zip target
- graphical Citro2D/Citro3D UI on the main home, settings, and conversation screens
- SD-backed config load/save
- settings screen for host, port, token, and device ID
- SD-backed active conversation + recent conversation cache
- native V2 gateway health check over HTTP
- native V2 conversation-slot sync via `GET /api/v2/conversations`
- native V2 message send / event poll flow
- on-device approval handling
- in-app rendering for the last message and last reply
- reply paging for long output

Current design work is now driven by:
- `docs/hermes-pictochat-dark-mode-spec.md`
- `docs/plans/2026-04-12-hermes-pictochat-dark-mode-ui.md`

The remaining UI job is to keep tightening the dark-mode Hermes + PictoChat visual pass and move more of the shell toward asset-backed handheld chrome where it helps readability.

Current controls:

### Home
- `UP/DOWN` or Circle Pad `UP/DOWN` — move through the home action list
- `LEFT/RIGHT` or Circle Pad `LEFT/RIGHT` — page long replies
- `A` — run the selected action
- touch the bottom-screen action buttons — select and run that action
- `START` — exit back to Homebrew Launcher

### Conversations
- `UP/DOWN` or Circle Pad `UP/DOWN` — select a conversation slot
- `A` — activate the highlighted conversation
- `X` — create a new conversation ID
- `Y` — sync recent conversation slots from Hermes
- `B` — return home

### Settings
- `UP/DOWN` or Circle Pad `UP/DOWN` — select host / port / token / device ID
- `A` — edit selected field
- `X` — save settings to SD
- `Y` — restore defaults
- `B` — return home

### Mic Session
- `UP` — stop recording and send the captured audio
- `B` — cancel the recording
- `START` — abort the recording

Config is stored at:

```text
sdmc:/3ds/hermes-agent-3ds/config.ini
```

That file now also persists:
- the active `conversation_id`
- a small recent-conversation list used by the on-device picker

## Build

Install the current devkitPro 3DS toolchain first:

- official guide: https://devkitpro.org/wiki/Getting_Started
- required stack for this app: `devkitARM`, `libctru`, `citro2d`, `citro3d`
- `DEVKITARM` must be set in your shell

Then run:

```bash
make clean && make
```

That produces the `.3dsx` build.

To create a release zip with the Homebrew Launcher folder layout:

```bash
make release RELEASE_VERSION=local
```

That produces:

```text
release/hermes-agent-3ds-local.zip
```

## Default gateway target

First-launch defaults still point at:

```text
http://10.75.76.156:8787/api/v2/health
```

Those defaults live in `include/app_config.h` and are only used until the user saves their own settings.

## Protocol note

This client is V2-only.

It expects the Hermes-side 3DS gateway to implement the active client flow:
- `/api/v2/health`
- `/api/v2/conversations`
- `/api/v2/messages`
- `/api/v2/voice`
- `/api/v2/events`
- `/api/v2/interactions/{request_id}/respond`

The current health-check request includes `token`, `device_id`, and the active `conversation_id` as query parameters.
