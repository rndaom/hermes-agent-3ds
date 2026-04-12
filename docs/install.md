# Install and first-run guide

This guide is for a **modded Nintendo 3DS** using the Homebrew Launcher build.

If your console is not already modded, use:

- https://3ds.hacks.guide/

## What you need

- a modded 3DS on Wi-Fi
- this repo on a PC for building the client
- a Hermes checkout with the native 3DS gateway enabled
- the 3DS and PC on the same LAN
- a token you configure on the Hermes-side 3DS gateway
- a stable `device_id` for this handheld

## 1) Build the app

From `client/`:

```bash
make clean && make
make release RELEASE_VERSION=local
```

Packaged output:

```text
client/release/hermes-agent-3ds-local.zip
```

## 2) Copy the app to the SD card

Copy the `3ds/hermes-agent-3ds/` folder onto your SD card so you end up with:

```text
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.3dsx
sdmc:/3ds/hermes-agent-3ds/hermes-agent-3ds.smdh
```

## 3) Start Hermes with native 3DS gateway support

This repo no longer ships a standalone bridge.

Run Hermes from the main `hermes-agent` repo with the 3DS gateway enabled on your PC. The 3DS client expects these endpoints:

- `GET /api/v2/health`
- `GET /api/v2/capabilities`
- `POST /api/v2/messages`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`

By default the local setup here uses port `8787`.

## 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

On the home screen:
- press `X` to open settings
- set `Host` to your PC's LAN IP
- set `Port` to the gateway port
- set `Token` to the same value configured in Hermes
- set `Device ID` to a stable per-device value
- press `X` to save
- press `B` to return home

## 5) Test the connection

On the home screen:
- press `A` to run a health check
- if that succeeds, press `B` and send a test message
- use `L/R` if the reply spans multiple pages

## Common issues

### Health check fails immediately
- confirm Hermes is running with the native 3DS gateway enabled
- confirm the 3DS and PC are on the same network
- confirm the host/port are correct in settings

### Unauthorized / token error
- the token saved on the 3DS must exactly match the Hermes-side 3DS gateway token

### App says V2 config is incomplete
- open settings with `X`
- set `Device ID`
- save again with `X`

### No assistant reply arrived yet
- verify the Hermes-side 3DS gateway is returning matching `reply_to` values
- verify the gateway supports long-poll events and interaction responses

## Current release note

The current user-facing release path is `.3dsx` for Homebrew Launcher. A `.cia` build can be added later if the install flow stays clean and reliable.
