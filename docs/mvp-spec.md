# MVP spec

## Goal

Ship a first public version of `hermes-agent-3ds` that lets a user open an app on a modded 3DS, connect to a configured bridge, send a message, and read a useful Hermes reply.

## Product summary

V1 is a thin client.

The 3DS app is responsible for:
- input
- settings
- request/response UI
- basic local persistence

The bridge is responsible for:
- talking to Hermes
- shaping responses for the 3DS
- handling backend complexity

## Primary user story

> I open the app on my modded 3DS, send a short message to Hermes, and get a clean readable reply back without needing to fight the setup.

## V1 scope

### Included
- launchable 3DS homebrew app
- single configured bridge target
- settings screen for host, port, and device token or shared key
- one active chat thread in memory
- software keyboard prompt entry
- send message / receive reply flow
- loading, success, and error states
- response rendering designed for small screens
- config saved on SD card
- basic install docs for end users

### Not included
- running Hermes directly on the 3DS
- multiple saved chats
- user accounts
- direct log-in to external services from the 3DS
- voice, images, file upload, or streaming media
- plugin system
- online bridge hosting as a requirement
- complex theme system
- full realtime streaming as a required feature

## UX requirements

### Core flow
1. User launches the app
2. App loads local config
3. User opens or confirms bridge settings
4. User presses a button to write a message
5. Software keyboard opens
6. App sends the request
7. App shows a loading state
8. App displays the reply
9. User can send another message or clear the current thread

### Screen model

#### Top screen
Primary output area.

Used for:
- current conversation view
- status text
- errors
- branding / title treatment

#### Bottom screen
Primary action area.

Used for:
- button hints
- settings navigation
- short menus
- input prompts and interaction guidance

### Writing style in-app
- short
- clear
- human
- not verbose
- not overly technical

### Visual style targets
- Hermes / Nous-inspired mood
- retro terminal flavor
- pastel colors
- a little ASCII charm
- clean hierarchy over decoration

## Functional requirements

### 3DS app

#### Settings
The app must allow the user to set and save:
- bridge host or IP
- bridge port
- local auth token or shared secret
- optional device name later, but not required for v1

#### Chat request
The app must send:
- plain text user message
- lightweight device/app metadata
- current short local conversation context if enabled

#### Chat response
The app must handle:
- normal text reply
- friendly error reply
- no-response timeout case
- oversized reply handling via truncation, paging, or shortening

#### Local persistence
The app must persist:
- bridge settings
- minimal preferences

The app does not need to persist full conversation history in v1.

### Bridge

#### Minimum responsibilities
- authenticate incoming 3DS request
- validate request payload
- forward message to Hermes/backend runtime
- receive result
- return a 3DS-friendly payload
- shorten or trim overly long output

#### Response contract
The bridge must return:
- `ok`
- `reply`
- `error` when relevant
- optional metadata like `truncated`

## Non-functional requirements

### Performance
- app should feel responsive on real hardware
- request/response loop should avoid unnecessary animation or heavy rendering
- UI should remain usable on Old 3DS, even if New 3DS is smoother

### Reliability
- bad config should fail clearly
- network timeout should fail clearly
- bridge offline state should be obvious
- malformed responses should fail safely

### Security
- no direct third-party provider secrets stored on the 3DS
- bridge token should be scoped to this app
- local-network-first deployment is the default recommendation
- SSL/TLS complexity should be handled by the bridge side wherever possible

## Proposed v1 API shape

### `POST /api/v1/chat`
Request:
```json
{
  "token": "device-token",
  "message": "Summarize my last task.",
  "context": [
    {"role": "user", "content": "Hi"},
    {"role": "assistant", "content": "Hello"}
  ],
  "client": {
    "platform": "3ds",
    "app_version": "0.1.0"
  }
}
```

Success response:
```json
{
  "ok": true,
  "reply": "Here is the short version...",
  "truncated": false
}
```

Error response:
```json
{
  "ok": false,
  "error": "Bridge unreachable or request failed."
}
```

### `GET /api/v1/health`
Used for setup validation.

Example response:
```json
{
  "ok": true,
  "service": "hermes-agent-3ds-bridge"
}
```

## Acceptance criteria

V1 is done when:
- a modded 3DS can launch the app successfully
- a user can configure a local bridge target without editing source code
- the app can send a message and receive a reply from the bridge
- normal network failures are understandable
- docs are simple enough for a non-dev user to follow
- the app feels lightweight and readable on real hardware

## Nice-to-have if cheap
- reply paging
- connection test button in settings
- simple first-run setup screen
- release builds in both `.3dsx` and `.cia` form

## Explicit v1 rule

If a feature makes install, support, or reliability noticeably worse, it does not belong in v1.
