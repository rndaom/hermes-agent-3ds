# Architecture

## Overview

`hermes-agent-3ds` is now the **handheld client** for the native 3DS gateway flow.

```text
┌─────────────────────┐      HTTP JSON      ┌──────────────────────────┐
│  Nintendo 3DS app   │  ───────────────▶   │   hermes-agent gateway   │
│  UI + cfg + input   │  ◀───────────────   │   platform = 3ds         │
└─────────────────────┘                     └──────────────────────────┘
```

This repo owns the left side. `hermes-agent` should own the right side.

## Repo split

### `hermes-agent-3ds`
Owns:
- 3DS UI
- settings and SD persistence
- active conversation selection + recent-conversation cache
- HTTP client transport
- approval UX
- packaging / deploy artifacts

### `hermes-agent`
Owns:
- 3DS gateway platform registration
- auth/token handling
- session plumbing
- message dispatch
- event mailbox / long-poll transport
- approval requests and responses

## Design principles

### 1) Thin client first
The 3DS is a compact frontend, not the backend.

### 2) Gateway-native, not bridge-specific
The handheld should talk to Hermes through the same gateway model as other platforms.

### 3) Old-3DS-friendly UX
Keep payloads small, layouts readable, and control flow simple.

### 4) Match replies reliably
The client should only accept final replies that match the current user message.

## Expected host-side API

The native 3DS gateway should expose:
- `GET /api/v2/health` with `token`, `device_id`, and active `conversation_id` query parameters
- `GET /api/v2/conversations`
- `POST /api/v2/messages`
- `POST /api/v2/image`
- `POST /api/v2/voice`
- `GET /api/v2/events`
- `GET /api/v2/media/{media_id}`
- `POST /api/v2/interactions/{request_id}/respond`

## Device config

Persist on SD card:
- host
- port
- token
- device ID
- active conversation ID
- recent conversation IDs

Path:
- `sdmc:/3ds/hermes-agent-3ds/config.ini`

## Conversation flow

1. The 3DS keeps a small local list of recent `conversation_id` values.
2. The user picks one from the handheld conversation screen or creates a new ID.
3. Hermes derives the real session from `device_id + conversation_id`, just like other gateway platforms derive it from chat/thread identity.
4. When online, the client can sync recent conversation slots from `GET /api/v2/conversations`.

## Message flow

1. User writes a message on the 3DS
2. Client sends `POST /api/v2/messages` for the active `conversation_id`
3. Gateway acknowledges with `message_id`, `conversation_id`, and `cursor`
4. Client begins polling `GET /api/v2/events` from that cursor
5. Client ignores stale or unrelated events
6. Client accepts a final reply only when `reply_to == message_id`
7. Client renders the reply

## Approval flow

1. Gateway emits `approval.request`
2. 3DS prompts the user for a choice
3. Client sends `POST /api/v2/interactions/{request_id}/respond`
4. Client resumes event polling for the final reply

## Reliability notes

The important V2 behaviors are:
- cursor handoff from message ack to first poll
- `reply_to` matching
- ignoring tool progress / unrelated assistant events
- waiting long enough for real Hermes replies on slower paths

## UI composition

### Top screen
- room board header
- room and relay status cards
- recent note history
- latest Hermes reply with paging indicators

### Bottom screen
- tool tray
- selected-tool detail
- footer hints
- setup / room-book action legends

## What is intentionally gone

- repo-local standalone Python bridge
- v1 chat transport
- v1-first architecture assumptions
