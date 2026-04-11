# Architecture

## Overview

`hermes-agent-3ds` is a two-part system:

1. a Nintendo 3DS homebrew client
2. a bridge service that connects the client to Hermes

```text
┌─────────────────────┐      HTTP JSON      ┌──────────────────────────┐
│  Nintendo 3DS app   │  ───────────────▶   │  hermes-agent-3ds bridge │
│  input + UI + cfg   │  ◀───────────────   │  auth + shaping + Hermes │
└─────────────────────┘                     └──────────────────────────┘
```

## Why this shape

This architecture keeps the 3DS side simple.

The handheld app should not own:
- provider-specific auth
- modern API complexity
- heavy response shaping
- broad backend compatibility work

The bridge exists to absorb that complexity so the app can stay small and stable.

## Design principles

### 1) Thin client first
The 3DS should behave like a compact frontend, not the brain.

### 2) HTTP before anything fancier
Plain request/response HTTP is the default for v1.

### 3) Local network first
The best first setup is 3DS on Wi‑Fi talking to a bridge running on the same LAN.

### 4) Short replies win
The bridge should optimize for readability on small hardware.

### 5) Keep replacement surfaces small
The bridge should be the only piece that needs to know much about Hermes runtime details.

## 3DS app architecture

## Suggested stack
- `devkitARM`
- `libctru`
- `citro2d` for the custom 2D UI
- optional `citro3d` only as a dependency of `citro2d`, not as a product goal

## App layers

### UI layer
Responsible for:
- top and bottom screen rendering
- button hints
- loading and error states
- settings screen
- conversation view

### Input layer
Responsible for:
- hardware buttons
- software keyboard invocation
- scrolling and paging controls

### App state layer
Responsible for:
- current screen
- current message draft
- current in-memory chat context
- current request state
- loaded config

### Config layer
Responsible for:
- reading config from SD card
- writing config to SD card
- applying defaults on first run

Suggested config path:
- `sdmc:/3ds/hermes-agent-3ds/config.ini`

### Network layer
Responsible for:
- bridge health checks
- sending JSON requests
- parsing JSON responses
- timeout and retry behavior

## Bridge architecture

## Responsibilities
The bridge should expose a very small API surface.

### Required endpoints
- `GET /api/v1/health`
- `POST /api/v1/chat`

### Internal bridge modules

#### Auth
- verifies device token or shared secret
- rejects malformed or unauthorized requests

#### Request adapter
- validates incoming client JSON
- normalizes context
- applies basic guardrails like length limits

#### Hermes connector
- sends the request to Hermes
- receives final text output
- hides Hermes-specific wiring from the 3DS client

#### Response shaper
- trims or summarizes long output
- marks whether text was truncated
- converts backend failures into short user-friendly errors

## Data flow

### Chat request flow
1. User writes a message on the 3DS
2. Client serializes request JSON
3. Client sends `POST /api/v1/chat`
4. Bridge authenticates request
5. Bridge forwards message to Hermes
6. Hermes produces a reply
7. Bridge shortens the reply if needed
8. Bridge returns response JSON
9. Client renders the result

### Health check flow
1. User opens settings or uses test connection
2. Client sends `GET /api/v1/health`
3. Bridge returns a minimal success payload
4. Client shows pass/fail state

## State model

### On device
Persist only what the app needs to boot cleanly:
- host
- port
- token
- maybe a few basic preferences later

Keep chat history in memory only for v1.

### In bridge
The bridge can stay stateless for v1 except for:
- token validation config
- optional logging
- Hermes connection config

Stateless is preferred because it keeps deployment and debugging simple.

## Networking choices

## Why not WebSockets for v1
WebSockets are possible on 3DS, but they add cost:
- more protocol complexity
- more TLS edge cases
- more reconnect state
- more support burden

TriCord is a good example of why this is better saved for later.

## Why HTTP JSON
HTTP JSON fits the real job:
- send one prompt
- get one answer
- handle a few simple errors

That is enough for a first usable release.

## Response shaping rules

The bridge should aim for:
- short first paragraph
- limited total output size
- plain text only in v1
- no markdown features that look bad on 3DS

Suggested defaults:
- prefer one compact reply over long multi-section output
- hard-limit reply size on the bridge
- include `truncated: true` when shortened

## UI composition

### Top screen
- title bar
- conversation output
- network/error status line

### Bottom screen
- actions
- settings list
- page controls
- simple interaction prompts

## Recommended v1 screen list
- splash / startup
- main chat screen
- settings screen
- error / status overlay
- optional about screen later

## Failure handling

The client should handle these cases directly:
- no saved config
- cannot resolve host
- cannot connect to bridge
- request timeout
- invalid bridge response
- unauthorized token

The user should always get a short plain-language error.

## Release shape

### User-facing output
Plan for both:
- `.3dsx` for Homebrew Launcher users
- `.cia` if the build pipeline later supports a cleaner install path

### Developer-facing build path
Follow standard devkitPro setup.

## Recommended implementation order

### Phase 1: app bootstrap
- create buildable homebrew app
- initialize graphics/input
- create screen loop
- load and save config

### Phase 2: bridge contract
- build `health` and `chat` endpoints
- define JSON schema
- add auth and length limits

### Phase 3: end-to-end chat
- software keyboard input
- request send
- response render
- clear error handling

### Phase 4: polish
- better typography and spacing
- improved small-screen formatting
- packaging and install docs

## Architectural conclusion

The safest and cleanest v1 is:
- custom 3DS app
- small local config
- HTTP JSON bridge
- stateless backend layer
- aggressive simplicity

That gives the project the best chance of becoming a real public release instead of a neat but fragile demo.
