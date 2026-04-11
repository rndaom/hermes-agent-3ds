# Implementation plan

This is the working task list for v1.

## Phase 1 — bridge contract

### Task 1: Define the request and response schema
- finalize `GET /api/v1/health`
- finalize `POST /api/v1/chat`
- lock required fields and error shape

### Task 2: Create the bridge service skeleton
- choose the bridge runtime
- create the app entrypoint
- add config loading
- add local auth token support

### Task 3: Implement the health endpoint
- return a simple success payload
- make it useful for setup testing

### Task 4: Implement the chat endpoint
- validate input
- forward a message to Hermes
- return a short text reply
- return simple errors

### Task 5: Add response shaping
- trim overly long replies
- mark truncated responses
- keep output 3DS-friendly

## Phase 2 — 3DS app foundation

### Task 6: Bootstrap the 3DS project
- create the buildable homebrew app
- set up the devkitPro build flow
- add app metadata and assets placeholders

### Task 7: Build the core app loop
- initialize graphics and input
- create a basic screen loop
- add a clean startup state

### Task 8: Add local config support
- load config from SD card
- save config to SD card
- apply defaults on first launch

### Task 9: Build the settings screen
- edit host
- edit port
- edit token
- add a simple connection test

## Phase 3 — end-to-end chat

### Task 10: Add prompt input
- open the 3DS software keyboard
- capture user text
- reject empty messages

### Task 11: Add bridge networking
- call the health endpoint
- call the chat endpoint
- parse JSON responses
- handle timeout and offline states

### Task 12: Build the chat screen
- show sent message
- show reply
- show loading state
- show readable errors

### Task 13: Add small-screen formatting
- wrap long lines cleanly
- keep replies compact
- add paging or truncation handling if needed

## Phase 4 — public release prep

### Task 14: Package installable builds
- produce `.3dsx`
- add `.cia` later if clean and low-friction

### Task 15: Write end-user install docs
- how to install the app
- how to run the bridge
- how to connect for the first time

### Task 16: Test on real hardware
- verify setup on a real modded 3DS
- test normal failure cases
- tighten rough edges before first release

## Build order

If we are executing this now, the recommended order is:
1. bridge skeleton
2. bridge API
3. 3DS app bootstrap
4. settings + config
5. end-to-end chat
6. install docs
7. release prep

## Rule for v1

If a task adds major complexity without improving the basic chat flow, defer it.
