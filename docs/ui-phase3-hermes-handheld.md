# Hermes Handheld — Phase 3 UI pass

Goal: expand the top screen composition so it uses more of the available width and add the first true pixel art style pass without moving the whole app to a heavier renderer yet.

## Direction

Phase 3 is a pixel art composition pass.

It keeps the text-first UI foundations from earlier phases, but adds:
- a wider top-screen composition
- a right-side crest motif
- split-line layout so status and branding use both sides of the top screen
- less duplicated information between top and bottom screens

## Top screen

The top screen should no longer feel like everything is crammed in the top-left corner.

Instead it should use:
- a wide framed Hermes header
- relay crest branding
- split lines with left-side status text and a right-side crest motif
- full-width reply section underneath

The right-side crest should act like a pixel art emblem panel.

## Bottom screen

The bottom screen stays action-focused.

It should keep:
- the command deck
- gateway summary
- token summary

It should avoid repeating active thread data that already lives on the top screen.

## Pixel art rule

For this phase, pixel art means small, cheap, readable handheld decoration:
- a right-side crest motif
- clean geometric framing
- no heavy animation
- no expensive rendering path that risks Old 3DS stability

## Old 3DS rule

This pass must remain Old 3DS safe:
- static decoration only
- text readability first
- cheap redraw cost
- no full visual overhaul that risks existing UX

## Next phase after this

If this composition pass feels good on hardware, the next real graphics step can be:
- Citro2D top-screen panels
- a real crest sprite
- shaded panel fills
- touch-friendly bottom buttons
