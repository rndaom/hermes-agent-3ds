# Design language

The app should feel like Hermes translated into a real PictoChat-era Nintendo handheld app.

## Mood

- light paper first
- tactile
- friendly
- compact
- structured
- playful, never noisy

## Visual cues

- ruled paper and soft off-white surfaces
- DS firmware rails and slim frame gradients
- color-coded room chips and sender tags
- thin borders, notebook rules, and touch-friendly buttons
- small labels and clear utility copy
- note cards that feel lifted from PictoChat

## UI rules

- one clear focus per screen
- top screen = room board
- bottom screen = tool tray
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
- PictoChat-clone Hermes messenger
- light ruled-paper message framing
- DS firmware rails and tool trays
- bright room-color accents over neutral paper surfaces
- Old-3DS-safe graphical UI via Citro2D/Citro3D

See `docs/hermes-pictochat-clone-spec.md` for the current spec.
Historical docs such as `docs/pixel-rpg-visual-direction.md` remain in the repo for reference only.
