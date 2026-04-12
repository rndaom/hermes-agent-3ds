# Hermes Agent 3DS Conversation Slot Alignment Plan

Status: implemented for the intended v2 gateway model; advanced saved-session restore is deferred.

## Goal

Keep the 3DS aligned with the normal Hermes gateway model:
- the PC-side Hermes gateway is the always-on service
- the 3DS app is a thin handheld client
- `conversation_id` acts like the handheld-facing chat/thread slot identity
- Hermes remains the source of truth for transcripts and session state

## What shipped

The current implementation already covers the core gateway-friendly behavior:
- `GET /api/v2/conversations`
- `POST /api/v2/messages`
- `POST /api/v2/voice`
- `GET /api/v2/events`
- `POST /api/v2/interactions/{request_id}/respond`
- SD-backed `active_conversation_id`
- SD-backed recent conversation ID cache
- on-device conversation picker
- typed and voice requests routed through the selected conversation slot

In practice this means:
1. the 3DS keeps a small local list of recent conversation IDs
2. the user selects or creates a conversation slot on-device
3. Hermes derives the live session from `device_id + conversation_id`
4. switching slots resumes the corresponding Hermes conversation naturally

## What is intentionally deferred

The earlier draft plan proposed a second layer of saved-session restore endpoints:
- `GET /api/v2/conversations/{conversation_id}/sessions`
- `POST /api/v2/conversations/{conversation_id}/select`

That is not required for the current gateway vision and is intentionally deferred.

Why defer it:
- it adds extra control-plane complexity beyond what other gateway clients need
- it duplicates behavior that the current `device_id + conversation_id` model already handles cleanly
- it would increase UI complexity on Old 3DS hardware without improving the core chat surface enough to justify it yet

If richer historical restore is ever needed later, it should be treated as a separate enhancement, not part of the baseline v2 conversation-slot model.

## Cleanup follow-up completed after landing

The follow-up cleanup pass for this feature should keep the repo aligned with the shipped behavior:
- remove unused client-side capabilities plumbing if the handheld does not call it
- keep docs focused on conversation slots, not broader saved-session restore semantics
- refactor duplicated typed/voice request flow in `main.c` into shared helpers
- keep host/client docs in sync with the actual live endpoints and controls

## Validation

Current validation loop:
- `python3 -m pytest client/tests -q`
- `make -C client clean && make -C client`
- live gateway checks for:
  - `/api/v2/health`
  - `/api/v2/conversations`
  - `/api/v2/messages`
  - `/api/v2/voice`
  - `/api/v2/events`
- real-hardware deployment over FTPD to the Old 3DS

## Bottom line

For the intended product shape, the correct mental model is:
- Hermes gateway on PC = always-on backend
- 3DS app = simple handheld gateway client
- conversation picker = slot/thread selection
- no extra saved-session restore layer is required for baseline v2
