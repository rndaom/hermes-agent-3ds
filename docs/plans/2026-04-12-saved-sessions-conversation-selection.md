# Hermes Agent 3DS Saved Sessions + Conversation Selection Implementation Plan

> **For Hermes:** Use the existing native V2 gateway/session model. Do not invent a separate 3DS-only persistence model for transcripts.

**Goal:** Add Old-3DS-friendly saved sessions and conversation selection so the 3DS behaves like a real Hermes gateway surface, with host-side session truth and handheld-side lightweight cached metadata.

**Architecture:** Hermes remains the source of truth for sessions/transcripts. `hermes-agent` will expose small 3DS-specific conversation/session list/select endpoints that reuse `SessionStore` + `SessionDB`. `hermes-agent-3ds` will add a conversation picker UI, persist lightweight recent-conversation metadata on SD, and route typed/voice requests through the selected `conversation_id` instead of hardcoded `main`.

**Tech Stack:** Python (`aiohttp`, existing gateway/session/store code), C/libctru 3DS client, SD-backed ini/json persistence, existing V2 HTTP transport.

---

## Task 1: Add focused failing/expectation tests for 3DS conversation/session APIs

**Objective:** Lock the required host-side behavior before implementation.

**Files:**
- Modify: `hermes-agent/tests/gateway/test_3ds_adapter.py`
- Modify: `hermes-agent/tests/test_hermes_state.py`

**Implementation targets:**
- Add coverage for:
  - `GET /api/v2/conversations`
  - `GET /api/v2/conversations/{conversation_id}/sessions`
  - `POST /api/v2/conversations/{conversation_id}/select`
- Verify device scoping (`user_id == device_id`) so one 3DS cannot see another device's saved sessions.
- Verify current conversation rows come from active `SessionStore` entries and expose title/preview metadata.
- Verify select/resume reopens the target session and evicts stale cached agents.

---

## Task 2: Extend Hermes session DB queries for device-scoped saved-session listing

**Objective:** Make `SessionDB` capable of listing/resolving saved sessions for one 3DS device instead of all 3DS sessions.

**Files:**
- Modify: `hermes-agent/hermes_state.py`

**Implementation targets:**
- Extend `get_session_by_title`, `resolve_session_by_title`, and `list_sessions_rich` to accept optional `user_id` filtering.
- Add optional `search` support for future filtering without extra schema changes.
- Keep default behavior backward-compatible for existing CLI/gateway callers.

---

## Task 3: Extract reusable runner helpers for 3DS session browsing/resume

**Objective:** Reuse the real Hermes gateway session-switch path instead of duplicating session logic in the adapter.

**Files:**
- Modify: `hermes-agent/gateway/run.py`

**Implementation targets:**
- Add helper(s) that:
  - list saved sessions for a source/device
  - resume/select a target session for a specific `session_key`
- Ensure the shared selection path:
  - flushes current-session memories
  - clears `_running_agents`
  - evicts `_agent_cache`
  - calls `SessionDB.reopen_session(target_id)`
  - returns title/message-count metadata for the UI
- Keep `/resume` behavior working through the same path.

---

## Task 4: Expose 3DS conversation/session control-plane endpoints

**Objective:** Let the handheld browse current conversation slots and switch a slot to a saved session, matching gateway semantics.

**Files:**
- Modify: `hermes-agent/gateway/platforms/threeds.py`
- Modify: `hermes-agent/gateway/run.py`

**Implementation targets:**
- Inject `gateway_runner` into the 3DS adapter at creation time.
- Add endpoints:
  - `GET /api/v2/conversations`
  - `GET /api/v2/conversations/{conversation_id}/sessions`
  - `POST /api/v2/conversations/{conversation_id}/select`
- Conversation list should use current 3DS `SessionStore` entries for the device.
- Session list should use device-scoped `SessionDB` history.
- Responses should stay small and bounded for Old 3DS.

---

## Task 5: Add lightweight 3DS local conversation cache/persistence

**Objective:** Keep a bounded local list of recent conversations and the active selection on SD, even if the host is temporarily unavailable.

**Files:**
- Modify: `hermes-agent-3ds/client/include/app_config.h`
- Modify: `hermes-agent-3ds/client/source/app_config.c`
- Create: `hermes-agent-3ds/client/include/session_store.h`
- Create: `hermes-agent-3ds/client/source/session_store.c`
- Modify: `hermes-agent-3ds/client/Makefile`
- Modify: `hermes-agent-3ds/client/tests/test_settings_config.py`

**Implementation targets:**
- Persist `active_conversation_id` in `config.ini`.
- Store recent conversation metadata in a separate SD file (small bounded JSON/ini-style cache is fine).
- Cache fields:
  - `conversation_id`
  - `label/title`
  - `preview`
  - `updated_at`
- Keep transcript/history host-side only.

---

## Task 6: Extend V2 client transport for conversation browsing/selecting

**Objective:** Add the missing HTTP client helpers for the new host endpoints.

**Files:**
- Modify: `hermes-agent-3ds/client/include/bridge_v2.h`
- Modify: `hermes-agent-3ds/client/source/bridge_v2.c`
- Modify: `hermes-agent-3ds/client/tests/test_v2_protocol.py`

**Implementation targets:**
- Add small result structs + helpers for:
  - list conversations
  - list sessions for a conversation
  - select/resume a session into a conversation slot
- Keep parser logic bounded and conservative.
- Reuse existing authorization and URL-building patterns.

---

## Task 7: Add 3DS conversation picker UI and route all chat/voice through active conversation

**Objective:** Make saved sessions/conversation selection usable on-device without hurting the existing home flow.

**Files:**
- Modify: `hermes-agent-3ds/client/source/main.c`
- Modify: `hermes-agent-3ds/client/tests/test_chat_flow.py`

**Implementation targets:**
- Add `APP_SCREEN_CONVERSATIONS`.
- Show the active conversation on the home screen.
- Suggested controls:
  - `SELECT` = open Conversations
  - `UP/DOWN` = move selection
  - `A` = activate conversation
  - `X` = create a new conversation slot locally
  - `Y` = sync/refresh from host
  - `B` = back
- Replace hardcoded `"main"` in typed and voice paths with `active_conversation_id`.
- Keep approval flow intact.

---

## Task 8: Add saved-session restore flow inside the conversation picker

**Objective:** Let the user resume a previously saved Hermes session into the selected conversation slot, mirroring gateway `/resume` semantics.

**Files:**
- Modify: `hermes-agent-3ds/client/source/main.c`
- Modify: `hermes-agent-3ds/client/source/bridge_v2.c`

**Implementation targets:**
- Add a simple action to fetch and list saved sessions for the highlighted conversation.
- Let the user choose one and call the host-side select/resume endpoint.
- Update local cache + active conversation metadata after a successful restore.

---

## Task 9: Update docs in both repos

**Objective:** Document the new behavior and controls so the architecture stays aligned.

**Files:**
- Modify: `hermes-agent/website/docs/user-guide/messaging/3ds.md`
- Modify: `hermes-agent/website/docs/user-guide/messaging/index.md` (if needed for comparison text)
- Modify: `hermes-agent-3ds/client/README.md`
- Modify: `hermes-agent-3ds/docs/architecture.md`
- Modify: `hermes-agent-3ds/docs/current-status.md`
- Modify: `hermes-agent-3ds/docs/roadmap.md`

**Implementation targets:**
- Document conversation picker controls.
- Document `conversation_id` as the 3DS-facing conversation slot identity.
- Document that Hermes remains source of truth for transcripts and saved sessions.

---

## Task 10: Validate, review, deploy, and push

**Objective:** Verify the feature across both repos and leave `main` clean and updated.

**Files:**
- No new product files; commit/test/deploy workflow only.

**Verification commands:**
- In `hermes-agent`:
  - `source venv/bin/activate && python -m pytest tests/gateway/test_3ds_adapter.py tests/test_hermes_state.py -q`
- In `hermes-agent-3ds`:
  - `python3 -m pytest client/tests -q`
  - `make -C client clean && make -C client`
- If network/hardware validation is needed:
  - verify `GET /api/v2/conversations`
  - verify `GET /api/v2/conversations/{conversation_id}/sessions`
  - verify `POST /api/v2/conversations/{conversation_id}/select`
  - deploy updated `.3dsx` over FTPD and test on the Old 3DS

**Commit hygiene:**
- Commit `hermes-agent` and `hermes-agent-3ds` separately.
- Keep unrelated repo changes out of both commits.
