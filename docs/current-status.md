# Current status

Last major verified milestone: **the client is now V2-only and targets the native 3DS gateway architecture in `hermes-agent`.**

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
- top/bottom-screen UI split is implemented
- long replies page cleanly with `L/R`
- native V2 chat flow is implemented with:
  - `POST /api/v2/messages`
  - `GET /api/v2/events`
  - approval handling through `POST /api/v2/interactions/{request_id}/respond`
- stale-event filtering is in place using:
  - ack cursor handoff
  - matching `reply_to`
  - longer wait window for final replies

### Repo cleanup
- legacy bundled Python bridge has been removed from this repo
- legacy v1 chat transport code has been removed from the client build path
- docs are being aligned around the V2-only architecture

## Current target architecture

This repo is now the handheld client only.

Host-side gateway ownership belongs in `hermes-agent`, where 3DS should behave like any other Hermes gateway platform.

Expected host-side API surface:
- `GET /api/v2/health`
- `GET /api/v2/capabilities`
- `POST /api/v2/messages`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`

## Current known-good local validation

- `python3 -m pytest client/tests -q`
- `make -C client clean && make -C client`

Latest built artifact:
- `client/hermes-agent-3ds.3dsx`

## What is intentionally gone

- standalone repo-local v1 bridge flow
- `/api/v1/chat` as the client chat path
- v1-first docs and setup guidance

## Recommended next steps

1. finish or restore the Hermes-side native 3DS gateway branch in `hermes-agent`
2. validate the V2-only client against that gateway on real hardware
3. add the next user-facing improvements:
   - microphone input option with bridge-side STT
   - portrait artwork/app icon
   - session UX polish
