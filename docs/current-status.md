# Current status

Last major verified milestone: **the V2-only 3DS client is validated end-to-end against the native 3DS gateway in `hermes-agent`, including real Old 3DS hardware.**

## What is done

### 3DS client
- devkitPro toolchain is installed and working on the Arch PC
- `.3dsx` builds successfully
- app appears in Homebrew Launcher on the real modded 3DS
- app launches and exits cleanly with `START`
- SD config load/save exists at `sdmc:/3ds/hermes-agent-3ds/config.ini`
- settings screen supports:
  - host
  - port
  - token
  - device ID
- settings fields are editable with the 3DS software keyboard
- active conversation + recent conversation list are persisted on SD
- top/bottom-screen UI split is implemented
- top-screen transcript history scrolls with `L/R`
- bottom-screen `LEFT/RIGHT` switches between the tool tray and the slash-command page
- conversation picker exists from the home action list for switching/syncing conversation slots
- native V2 chat flow is implemented with:
  - `POST /api/v2/messages`
  - `POST /api/v2/voice`
  - `GET /api/v2/events`
  - approval handling through `POST /api/v2/interactions/{request_id}/respond`
- microphone input is implemented on-device and routed through host-side speech-to-text with `UP`
- the mic recording screen now uses graphical in-app rendering instead of the old console shell
- approval prompts now use graphical in-app rendering instead of the old console shell
- renderer ownership cleanup is complete for normal GUI screens
- stale-event filtering is in place using:
  - ack cursor handoff
  - matching `reply_to`
  - longer wait window for final replies
- streamed partial replies now render from `message.updated` events when the gateway emits them
- a PictoChat-clone Hermes baseline is now the active visual direction for the handheld client
- the home screen now targets a ruled-paper room board on top and a tool tray on the bottom
- the home action list now uses D-pad / Circle Pad selection with A-to-confirm instead of labeled home-screen hotkeys
- the home screen now includes a native Hermes slash-command page for `/reset` and `/compress`
- reply, thread/link, and token/detail text containment now uses pixel-width wrapping and tighter panel sizing instead of overflowing fixed rows

### Host-side integration
- native 3DS gateway support now exists in `hermes-agent`
- the gateway exposes the expected V2 surface used by the handheld client:
  - `GET /api/v2/health` with `token`, `device_id`, and active `conversation_id` query parameters
  - `GET /api/v2/conversations`
  - `POST /api/v2/messages`
  - `POST /api/v2/voice`
  - `GET /api/v2/events`
  - `POST /api/v2/interactions/{request_id}/respond`
- live local integration has been validated against the Hermes-side gateway
- the latest app build has been deployed over FTPD to the real Old 3DS for current hardware verification

### Repo cleanup
- this repo is now client-only
- bundled bridge code is gone
- old V1 chat transport code is gone from the client path
- docs are aligned around the V2-only split:
  - `hermes-agent-3ds` = handheld client
  - `hermes-agent` = host-side 3DS gateway platform

## Current target architecture

This repo is the handheld client only.

Host-side gateway ownership belongs in `hermes-agent`, where 3DS behaves like any other Hermes gateway platform.

## Current known-good validation

- `.venv/bin/python -m pytest client/tests -q`
- `make -C client clean && make -C client`
- `make -C client release RELEASE_VERSION=local`
- live V2 gateway checks for:
  - `/api/v2/health` with query-string identity parameters
  - `/api/v2/conversations`
  - `/api/v2/messages`
  - `/api/v2/voice`
  - `/api/v2/events`
- real-hardware deployment over FTPD to the Old 3DS

Latest built artifact:
- `client/hermes-agent-3ds.3dsx`

## What is intentionally gone

- the old standalone repo-local bridge flow
- `/api/v1/chat` as the client chat path
- V1-first setup and architecture assumptions

## Recommended next steps

1. decide whether to import sprite-backed DS icons and chrome through `client/gfx/` or keep the procedural shell for Old-3DS simplicity
2. add richer media handling such as camera upload, inline generated image rendering, and generated audio playback
3. add more native slash commands to the bottom-screen command page
4. continue tightening any remaining text-fit or readability issues found on hardware
