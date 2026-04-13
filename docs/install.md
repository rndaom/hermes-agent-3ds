# Install and first-run guide

This guide is for a **modded Nintendo 3DS** using the Homebrew Launcher build.

If your console is not already modded, use:

- https://3ds.hacks.guide/

## What you need

- a modded 3DS on Wi-Fi
- this repo on a PC for building the client
- the current devkitPro 3DS toolchain installed on that PC
- a Hermes checkout with the native 3DS gateway enabled
- the 3DS and PC on the same LAN
- a token you configure on the Hermes-side 3DS gateway
- a stable `device_id` for this handheld

For the toolchain, use the official devkitPro getting-started guide and make sure the 3DS stack used by this repo is available:

- https://devkitpro.org/wiki/Getting_Started
- `devkitARM`
- `libctru`
- `citro2d`
- `citro3d`

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

Run Hermes from the main `hermes-agent` repo with the 3DS gateway enabled on your PC. The 3DS client expects this active handheld flow:

- `GET /api/v2/health`
- `GET /api/v2/conversations`
- `POST /api/v2/messages`
- `POST /api/v2/voice`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`

The handheld health check currently calls `GET /api/v2/health` with `token`, `device_id`, and the active `conversation_id` in the query string.

By default the local setup here uses port `8787`.

## 4) Configure the app on the 3DS

Open **Hermes Agent 3DS** in Homebrew Launcher.

On the home screen:
- move to `Setup` with the D-pad / Circle Pad
- press `A` to open setup
- set `Host` to your PC's LAN IP
- set `Port` to the gateway port
- set `Token` to the same value configured in Hermes
- set `Device ID` to a stable per-device value
- press `X` to save
- press `B` to return home

## 5) Test the connection

On the home screen:
- move to `Check Relay` with the D-pad / Circle Pad and press `A`
- if that succeeds, move to `Rooms` and press `A` to open the room book
- use the D-pad or Circle Pad there to move through saved conversation slots
- press `X` there if you want to create a new conversation ID, or `Y` to sync recent conversation slots from Hermes
- return home, move to `Write Note`, and press `A` to send a test message in the active conversation
- or move to `Mic Note` and press `A` to start recording, then press `UP` to stop and send it through host-side speech-to-text
- press `B` during mic capture to cancel, or `START` to abort that recording session
- use D-pad / Circle Pad `LEFT/RIGHT` if the reply spans multiple pages

## Common issues

### Health check fails immediately
- confirm Hermes is running with the native 3DS gateway enabled
- confirm the 3DS and PC are on the same network
- confirm the host/port are correct in settings

### Unauthorized / token error
- the token saved on the 3DS must exactly match the Hermes-side 3DS gateway token

### App says V2 config is incomplete
- open `Setup` from the home action list
- set `Device ID`
- save again with `X`

### No assistant reply arrived yet
- verify the Hermes-side 3DS gateway is returning matching `reply_to` values
- verify the gateway supports long-poll events and interaction responses

## Current release note

The current user-facing release path is `.3dsx` for Homebrew Launcher. A `.cia` build can be added later if the install flow stays clean and reliable.

## Verification

From the repo root, the current local verification commands are:

```bash
.venv/bin/python -m pytest client/tests -q
make -C client clean && make -C client
```
