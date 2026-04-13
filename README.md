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

### 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

- move to `Settings` with the D-pad / Circle Pad
- press `A` to open settings
- set `Host`
- set `Port`
- set `Token`
- set `Device ID`
- press `X` again to save
- press `B` to return home

### 5) Test it

- D-pad / Circle Pad `UP/DOWN` — choose a home action
- `A` — run the selected action
- D-pad / Circle Pad `LEFT/RIGHT` — page long replies

## Controls

### Home
- UP/DOWN — move through the home action list
- LEFT/RIGHT — page long replies
- A — run the selected action
- START — exit

### Conversations
- UP/DOWN — select a conversation slot
- A — activate the highlighted conversation
- X — create a new conversation ID
- Y — sync recent conversation slots from Hermes
- B — return home
- START — exit

### Settings
- UP/DOWN — select field
- A — edit field
- X — save settings
- Y — restore defaults
- B — return home

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
- [`docs/hermes-pictochat-dark-mode-spec.md`](docs/hermes-pictochat-dark-mode-spec.md)
- [`docs/plans/2026-04-12-hermes-pictochat-dark-mode-ui.md`](docs/plans/2026-04-12-hermes-pictochat-dark-mode-ui.md)
- [`CONTRIBUTING.md`](CONTRIBUTING.md)

## License

MIT
