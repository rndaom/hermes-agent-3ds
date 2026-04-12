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

Legacy v1 bridge code is gone from this repo.

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

The 3DS client expects the gateway to expose:

- `GET /api/v2/health`
- `GET /api/v2/capabilities`
- `POST /api/v2/messages`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`

### 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

- `X` — open settings
- set `Host`
- set `Port`
- set `Token`
- set `Device ID`
- press `X` again to save
- press `B` to return home

### 5) Test it

- `A` — check Hermes gateway health
- `B` — ask Hermes
- `L/R` — page long replies

## Controls

### Home
- A — check Hermes gateway health
- B — ask Hermes
- X — open settings
- Y — clear status
- L/R — page long replies
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

### Is v1 still supported?
No. This repo is V2-only now.

### Where is the host-side gateway code?
In `hermes-agent`, where 3DS should behave like a normal Hermes gateway platform.

## Links

- [`docs/install.md`](docs/install.md)
- [`docs/current-status.md`](docs/current-status.md)
- [`docs/architecture.md`](docs/architecture.md)
- [`docs/roadmap.md`](docs/roadmap.md)
- [`docs/design-language.md`](docs/design-language.md)
- [`CONTRIBUTING.md`](CONTRIBUTING.md)

## License

MIT
