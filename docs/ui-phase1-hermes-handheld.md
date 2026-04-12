# Hermes Handheld — Phase 1 UI refresh

Goal: move the 3DS app from plain debug-console screens toward a Hermes-branded handheld UI without adding heavy graphics work yet.

## Direction

Phase 1 uses a Hermes Handheld hybrid:
- Hermes CLI mood and hierarchy on the top screen
- Nintendo-like command deck layout on the bottom screen
- text-first rendering so it stays safe on Old 3DS hardware

## Visual identity

- background mood: dark charcoal instead of harsh pure black
- accent mood: amber and warm orange
- body text: light neutral text with strong section labels
- style: terminal-inspired, but arranged like a handheld utility app

## Top screen

The top screen should feel like the Hermes CLI condensed into a small handheld display.

Sections:
- Hermes brand header
- current status line
- messenger link diagnostics
- active thread label
- latest user message
- latest Hermes reply with paging

## Bottom screen

The bottom screen should feel like a simple command deck.

Sections:
- action grid / control list
- gateway summary
- active thread summary
- token/config summary

This keeps the bottom screen practical and readable while still feeling like a real app.

## Old 3DS constraints

- keep it text-first for now
- avoid full-screen redraw-heavy animation
- prefer strong hierarchy over decoration
- use small ASCII or pixel-art touches later, not everywhere at once
- do not rely on pure black + tiny amber text alone for the whole experience

## Phase 1 deliverables

1. Brand all screens consistently as Hermes Agent
2. Replace generic control blocks with deck-style labels
3. Improve section naming: messenger link, active thread, Hermes reply
4. Make settings and conversations feel like dedicated handheld screens
5. Keep the implementation simple enough to preserve current performance and reliability
