# hermes-agent-3ds

```text
   .-.
  (o o)  Hermes, but tiny.
  | O \
   \   \
    `~~~'
```

A Nintendo 3DS client for Hermes Agent.

The app runs on a modded 3DS. Hermes runs on your PC or server. The 3DS talks to Hermes through a small bridge service.

## Quick start

This project is for a **modded Nintendo 3DS**. If your system is not already modded, start with:

- https://3ds.hacks.guide/

### 1) Install the 3DS app

Current release shape is a Homebrew Launcher build.

Copy the app to:

```text
sdmc:/3ds/hermes-agent-3ds/
```

The folder should contain at least:

```text
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.3dsx
```

Then launch **Hermes Agent 3DS** from the Homebrew Launcher.

### 2) Run the bridge on your PC

From this repo:

```bash
python -m venv bridge/.venv
. bridge/.venv/bin/activate
pip install -e './bridge[dev]'
export HERMES_3DS_BRIDGE_AUTH_TOKEN='change-me'
hermes-3ds-bridge
```

If you prefer the module entrypoint directly, this also works:

```bash
python -m hermes_3ds_bridge.main
```

The bridge should be reachable from the same LAN as the 3DS.

### 3) Connect for the first time

Open the app on your 3DS.

- `X` — open settings
- set the bridge host
- set the bridge port
- set the auth token to match `HERMES_3DS_BRIDGE_AUTH_TOKEN`
- press `X` again to save settings
- press `B` to return home
- press `A` — run bridge health check

If the health check succeeds, press `B` on the home screen and send a test message to Hermes.

## Controls

### Home
- A — run bridge health check
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

### Will this be easy to set up?
That's the goal. The app is being built around a simple LAN bridge flow and short install steps.

### Is this only for one specific Hermes setup?
No. The long-term goal is to support a simple bridge model that can work with normal Hermes usage.

### Is the app released yet?
The core v1 flow is now implemented and hardware-validated on a real modded 3DS. The repo includes working build, bridge, and setup instructions for local testing and first-release prep.

## Links

- [`docs/install.md`](docs/install.md)
- [`docs/roadmap.md`](docs/roadmap.md)
- [`docs/design-language.md`](docs/design-language.md)
- [`bridge/README.md`](bridge/README.md)
- [`CONTRIBUTING.md`](CONTRIBUTING.md)

## License

MIT
