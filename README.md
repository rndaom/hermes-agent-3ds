# hermes-agent-3ds

```text
   .-.
  (o o)  Hermes, but tiny.
  | O \
   \   \
    `~~~'
```

A Nintendo 3DS client for Hermes Agent.

The app runs on a modded 3DS. Hermes runs on your PC. The 3DS talks to the **native 3DS gateway in `hermes-agent`** over HTTP.

## Current architecture

This repo is now the **3DS client only**.

- `hermes-agent-3ds` = handheld app, settings, UI, input, transport client
- `hermes-agent` = Hermes-side 3DS gateway platform, sessions, approvals, message flow

This repo is V2-only now. The old bundled bridge path is gone.

## Build prerequisites

Install the current devkitPro 3DS toolchain before building:

- follow the official devkitPro getting-started guide: https://devkitpro.org/wiki/Getting_Started
- make sure `DEVKITARM` is set in your shell
- install the 3DS stack this repo uses: `devkitARM`, `libctru`, `citro2d`, `citro3d`

The local machine used for this repo also has the reference examples under `/opt/devkitpro/examples/3ds`.

## Quick start

### 1) Build the 3DS app

From `client/`:

```bash
make clean && make
make release RELEASE_VERSION=local
```

### 2) Copy it to your SD card

Install to:

```text
sdmc:/3ds/hermes-agent-3ds/
```

Expected files:

```text
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.3dsx
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.smdh
```

### 3) Run Hermes with the native 3DS gateway enabled

The target host is the Hermes-side 3DS platform in the main `hermes-agent` repo.

The 3DS client expects the gateway to support the active handheld flow:

- `GET /api/v2/health`
- `GET /api/v2/conversations`
- `POST /api/v2/messages`
- `POST /api/v2/voice`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`

The health check request is not just a bare route hit. The client currently calls `GET /api/v2/health` with `token`, `device_id`, and the active `conversation_id` as query parameters.

### 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

- move to `Setup` with the D-pad / Circle Pad
- press `A` to open setup
- set `Host`
- set `Port`
- set `Token`
- set `Device ID`
- set `Theme`
- set `Mode`
- press `X` again to save
- press `B` to return home

### 5) Test it

- run `.venv/bin/python -m pytest client/tests -q`
- D-pad / Circle Pad `UP/DOWN` — choose a home action
- `A` — run the selected action
- D-pad / Circle Pad `LEFT/RIGHT` — page long replies

## Controls

### Home
- UP/DOWN or Circle Pad UP/DOWN — move through the home action list
- LEFT/RIGHT or Circle Pad LEFT/RIGHT — page long replies
- A — run the selected action
- Touch the bottom-screen action buttons — select and run that action
- START — exit

### Rooms
- UP/DOWN or Circle Pad UP/DOWN — select a conversation slot
- A — activate the highlighted conversation
- X — create a new conversation ID
- Y — sync recent conversation slots from Hermes
- B — return home
- START — exit

### Setup
- UP/DOWN or Circle Pad UP/DOWN — select field
- A — edit field, cycle theme, or toggle light/dark mode
- X — save settings
- Y — restore defaults
- B — return home

### Mic Session
- UP — stop recording and send the captured audio
- B — cancel the recording
- START — abort the recording

## FAQ

### Does Hermes run directly on the 3DS?
No. The 3DS is the client. Hermes runs on stronger hardware.

### Do I need a modded 3DS?
Yes.

### Is V1 still supported?
No. V1 is gone. This repo is V2-only.

### Where is the host-side gateway code?
In `hermes-agent`, where 3DS should behave like a normal Hermes gateway platform.

## Links

- [`docs/install.md`](docs/install.md)
- [`docs/current-status.md`](docs/current-status.md)
- [`docs/architecture.md`](docs/architecture.md)
- [`docs/roadmap.md`](docs/roadmap.md)
- [`docs/design-language.md`](docs/design-language.md)
- [`docs/hermes-pictochat-clone-spec.md`](docs/hermes-pictochat-clone-spec.md)
- [`docs/plans/2026-04-13-hermes-pictochat-clone-ui.md`](docs/plans/2026-04-13-hermes-pictochat-clone-ui.md)
- [`CONTRIBUTING.md`](CONTRIBUTING.md)

## License

MIT
