# Current status

Last major verified milestone: **the packaged v1 flow now passes real 3DS hardware validation end-to-end, including health checks, chat, long replies, paging, timeout handling, and duplicate-reply cleanup**.

## What is done

### Bridge
- FastAPI bridge exists and runs locally.
- `GET /api/v1/health` works.
- `POST /api/v1/chat` exists with auth, validation, and reply shaping.
- The bridge now forwards chat requests into the local Hermes CLI.
- Hermes CLI output is cleaned before being returned to the 3DS.
- Local bridge test suite passes.

### 3DS client
- devkitPro toolchain is installed and working on the Arch PC.
- `.3dsx` builds successfully.
- App appears in Homebrew Launcher on the user's real modded 3DS.
- App launches and exits cleanly with `START`.
- Health-check UI works on real hardware.
- Pressing `A` performs a successful bridge health check and renders:
  - service name
  - version
  - HTTP status
- SD config load/save now exists at `sdmc:/3ds/hermes-agent-3ds/config.ini`.
- A settings screen now exists for:
  - host
  - port
  - token
- Settings fields are editable with the 3DS software keyboard.
- Real hardware checks already confirmed:
  - settings page opens correctly
  - host / port / token display correctly
  - saved values persist after reopening
  - saved settings are reused by health checks
- Health checks now use the loaded/saved bridge config instead of a hardcoded URL.
- Chat flow is now implemented in the client build:
  - software keyboard prompt entry for a message
  - `POST /api/v1/chat`
  - token included in the request body
  - last message / last reply rendered on-screen
  - friendly error rendering for failed chat requests
- A first small-screen formatting pass is now implemented in the client build:
  - wrapped message/reply text instead of fixed preview-only truncation
  - reply paging with `L/R`
  - compact home-screen layout for longer Hermes replies
- A dual-screen UI pass is now implemented in the client build:
  - top screen focuses on reply/status output
  - bottom screen focuses on actions, metadata, and page hints
- The bottom screen has now been trimmed to avoid duplicating status/output that already appears on the top screen.
- A release packaging target now exists in `client/Makefile`:
  - `make release RELEASE_VERSION=<name>`
  - outputs a zip with the Homebrew Launcher folder layout
- End-user install docs now exist in:
  - `README.md`
  - `docs/install.md`

### Deployment / device workflow
- FTP deploy to the 3DS works when FTPD is open and the SD card is mounted correctly.
- The app is currently deployed at:
  - `sd:/3ds/hermes-agent-3ds/hermes-agent-3ds.3dsx`
- First-launch defaults still point at:
  - `http://10.75.76.156:8787/api/v1/health`
- The current 3DS config token has been synced to the running bridge token.

## Important debugging findings
- Earlier FTPD failures were caused by the 3DS not exposing the SD card correctly until the SD was reinserted.
- The main health-check bug was in the 3DS client networking code, not the PC bridge.
- A nonblocking `connect()` + `getsockopt(... SO_ERROR ...)` path falsely failed on 3DS.
- Removing the `SO_ERROR` check fixed the real-hardware health check.
- Diagnostic UI fields (`last rc`, `socket errno`, `stage`) are still present for now.
- Hermes CLI quiet mode still emitted decorative chrome / `session_id:` lines, so the bridge now strips those before returning text to the 3DS.
- Real Hermes replies can take much longer than the first placeholder responder, so the 3DS client chat receive timeout was raised from 8 seconds to 180 seconds.
- Some Hermes CLI replies repeated the same content block twice; the bridge now collapses repeated consecutive leading reply blocks before returning text to the 3DS.

## Current known-good behavior
On the real 3DS, the app already shows:
- `Bridge answered successfully.`
- `Bridge OK`
- `service: hermes-agent-3ds-bridge`
- `version: 0.1.0`
- `http: 200`

On real hardware, the current app already supports:
- loading defaults when no config file exists
- opening the settings screen
- editing host / port / token in-app
- saving those settings back to SD
- reopening settings and seeing persisted values
- reusing saved settings for health checks

On desktop-verified and real-hardware live bridge tests, chat now also supports:
- entering a prompt with the software keyboard
- sending chat requests to `/api/v1/chat`
- forwarding the prompt into real Hermes
- returning a cleaned text reply like `Hi from Hermes on your 3DS!`
- surfacing HTTP / auth / timeout failures as short errors

The latest real-hardware release-validation pass confirmed:
- packaged install/update flow works on the real 3DS
- health check passes
- short chat passes
- long/slower chat passes
- reply paging passes
- duplicate response blocks are no longer shown

## Recommended next step
Core v1 scope is now complete. The next work should come from the post-v1 follow-ups below, with timeout/session behavior tracked separately in GitHub issue #10.

## New requested follow-ups
- add microphone input as an option/button so the 3DS can send speech to the bridge for STT
- replace the placeholder app image/icon with the provided portrait artwork
- keep host-side Hermes capabilities available through the bridge flow
