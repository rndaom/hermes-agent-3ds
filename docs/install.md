# Install and first-run guide

This guide is for a **modded Nintendo 3DS** using the Homebrew Launcher build.

If your console is not already modded, use the community guide first:

- https://3ds.hacks.guide/

## What you need

- a modded 3DS on Wi-Fi
- this repo on a PC that can run Hermes
- the 3DS and PC on the same LAN
- a bridge auth token you choose yourself

## 1) Build the app

From `client/`:

```bash
make clean && make
make release RELEASE_VERSION=local
```

The packaged output will be:

```text
client/release/hermes-agent-3ds-local.zip
```

## 2) Copy the app to the SD card

Inside the zip, copy the `3ds/hermes-agent-3ds/` folder onto your SD card so you end up with:

```text
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.3dsx
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.smdh
```

## 3) Start the bridge

From the repo root:

```bash
python -m venv bridge/.venv
. bridge/.venv/bin/activate
pip install -e './bridge[dev]'
export HERMES_3DS_BRIDGE_AUTH_TOKEN='change-me'
hermes-3ds-bridge
```

Alternative entrypoint:

```bash
python -m hermes_3ds_bridge.main
```

By default the bridge listens on port `8787`.

## 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

On the home screen:
- press `X` to open settings
- set `Host` to your PC's LAN IP
- set `Port` to the bridge port
- set `Token` to the same value as `HERMES_3DS_BRIDGE_AUTH_TOKEN`
- press `X` to save
- press `B` to return home

## 5) Test the connection

On the home screen:
- press `A` to run a health check
- if that succeeds, press `B` and send a test message
- use `L/R` if the reply spans multiple pages

## Common issues

### Health check fails immediately
- confirm the PC bridge is running
- confirm the 3DS and PC are on the same network
- confirm the host/port are correct in settings

### Unauthorized / token error
- the token saved on the 3DS must exactly match `HERMES_3DS_BRIDGE_AUTH_TOKEN`

### App launches but settings are wrong
- open settings with `X`
- update host/port/token
- save again with `X`

## Current release note

The current user-facing release path is `.3dsx` for Homebrew Launcher. A `.cia` build can be added later if the install flow stays clean and reliable.
