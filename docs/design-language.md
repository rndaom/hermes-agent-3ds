# Design language

The app should feel like Hermes translated onto real Nintendo handheld hardware.

## Mood

- dark-mode first
- calm
- compact
- tactile
- structured
- a little playful, never noisy

## Visual cues

- dark graphite and slate surfaces over pure black slabs
- framed handheld chrome inspired by PictoChat and DS firmware
- thin borders, inset lines, and restrained gradients
- small pixel-crisp labels and icons
- subtle ruled/grid textures where they help the handheld feel
- readable message cards with attached identity chips

## UI rules

- one clear focus per screen
- top screen = conversation/status stage
- bottom screen = command/input deck
- keep button hints obvious
- keep text short and width-bounded
- never assume the user wants to read a wall of text on a 3DS
- selection state must be obvious and must reflect real navigation state
- do not mix `PrintConsole` UI and graphical UI on the same normal app screens

## Writing tone

- human
- concise
- helpful
- a little personality is good
- fake corporate polish is bad

## Brand note

This is a Hermes handheld messenger.
No Pokemon theme. No Nous theme. No generic terminal skin.

## Current visual direction

The current source-of-truth direction is:
- dark-mode Hermes + PictoChat + DS firmware messenger
- compact note-card message framing
- dark framed rails and command decks
- color used as accent and identity, not as full-surface flood
- Old-3DS-safe graphical UI via Citro2D/Citro3D

See `docs/hermes-pictochat-dark-mode-spec.md` for the current spec.
Historical docs such as `docs/pixel-rpg-visual-direction.md` remain in the repo for reference only.
