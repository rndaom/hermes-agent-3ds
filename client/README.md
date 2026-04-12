# client

This directory contains the Nintendo 3DS homebrew app.

## Current status

The client now has:
- devkitPro-style project layout
- build Makefile
- packaged `.3dsx` release zip target
- simple Old 3DS-friendly console UI
- SD-backed config load/save
- settings screen for host, port, token, and device ID
- native V2 gateway health check over HTTP
- native V2 message send / event poll flow
- on-device approval handling
- in-app rendering for the last message and last reply
- reply paging for long output

Current controls:

### Home
- `A` — check Hermes gateway health
- `B` — Ask Hermes / open the message keyboard
- `X` — open settings
- `Y` — clear the current status
- `L/R` — page long replies
- `START` — exit back to Homebrew Launcher

### Settings
- `UP/DOWN` — select host / port / token / device ID
- `A` — edit selected field
- `X` — save settings to SD
- `Y` — restore defaults
- `B` — return home

Config is stored at:

```text
sdmc:/3ds/hermes-agent-3ds/config.ini
```

## Build

Install the normal 3DS devkitPro toolchain, then run:

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

It expects the Hermes-side 3DS gateway to implement:
- `/api/v2/health`
- `/api/v2/capabilities`
- `/api/v2/messages`
- `/api/v2/events`
- `/api/v2/interactions/{request_id}/respond`
