# Hermes Handheld — Phase 2 UI pass

> Historical note: this document captures an earlier UI phase and remains in the repo for reference only. The current visual source of truth is `docs/hermes-pictochat-clone-spec.md`.

Goal: keep the Phase 1 text-first layout, but give it stronger handheld personality with ASCII-framed chrome that still runs cleanly on an Old 3DS.

## Direction

Phase 2 is an ASCII-framed handheld pass.

It keeps the same two-screen structure, but adds:
- framed Hermes headers
- a compact relay crest line
- panel titles for major sections
- stronger visual separation between controls, link state, thread state, and reply state

## Why this phase exists

Phase 1 fixed naming and hierarchy.
Phase 2 should make the app feel more intentional without jumping to heavy graphics yet.

That means:
- no risky renderer rewrite yet
- no heavy sprite pipeline yet
- no extra animation pressure on Old 3DS hardware
- just better chrome, framing, and identity

## Visual rules

- use ASCII framed headers as the main screen chrome
- use a compact crest line like a relay sigil, not giant art
- treat section headings as panels, not plain text labels
- keep message and reply readability first
- never let decoration crowd out functional text

## Screen goals

### Home
- framed Hermes header
- compact relay crest under the header
- panel for link state
- panel for active thread
- panel for Hermes reply
- boxed command deck on the bottom screen

### Settings
- framed Hermes header
- compact relay crest
- panel title for link settings
- boxed config deck on the bottom screen

### Conversations
- framed Hermes header
- compact relay crest
- panel title for thread archive
- boxed thread deck on the bottom screen

## Old 3DS constraint

This phase must remain Old 3DS safe:
- text-first
- static layout
- no redraw-heavy animation
- decoration must be cheap to render

## What comes after this

If Phase 2 feels good on hardware, the next step can be the first real graphic pass:
- tiny pixel crest sprite
- panel fills / background tone work
- touch-friendly bottom-screen buttons
- more game-like selection styling
