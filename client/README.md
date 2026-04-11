# client

This directory contains the Nintendo 3DS homebrew app.

## Current status

The client now has:
- devkitPro-style project layout
- build Makefile
- packaged `.3dsx` release zip target
- simple Old 3DS-friendly console UI
- app metadata + icon placeholder
- bridge health-check flow over HTTP
- SD-backed config load/save
- settings screen for host, port, and token
- software keyboard editing for settings fields
- first chat request / reply flow over `POST /api/v1/chat`
- in-app rendering for the last message and last reply

Current controls:

### Home
- `A` — run bridge health check
- `B` — Ask Hermes / open the message keyboard
- `X` — open settings
- `Y` — clear the current status
- `L/R` — page long replies
- `START` — exit back to Homebrew Launcher

### Settings
- `UP/DOWN` — select host / port / token
- `A` — edit selected field
- `X` — save settings to SD
- `Y` — restore defaults
- `B` — return home

Config is stored at:

```text
sdmc:/3ds/hermes-agent-3ds/config.ini
```

The saved token is reused in chat requests.

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

## Default bridge target

First-launch defaults still point at:

```text
http://10.75.76.156:8787/api/v1/health
```

Those defaults now live in `include/app_config.h` and are only used until the user saves their own settings.
