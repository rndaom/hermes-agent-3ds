# Hermes Agent 3DS — pixel RPG visual direction

Goal: move the app toward a classic handheld monster-RPG feel without copying Pokemon-specific characters, symbols, layouts, or branding.

## One-line direction

Make Hermes Agent feel like a small, polished late-GBA / early-DS pixel adventure tool:
- cozy
- readable
- game-like
- menu-driven
- clearly Hermes

Not parody. Not fan game. Not literal Pokemon UI cloning.

## What we want to capture

We want the aesthetic language of classic handheld creature-RPGs:
- chunky pixel framing
- friendly menu boxes
- high-contrast outlines
- soft limited palette
- clean status panels
- map/menu/dialog rhythm
- tiny emblem art with lots of character
- UI that feels like a game menu, not a dev console

## What we must avoid

Do not reuse or closely imitate:
- Pokeballs
- Pokemon creatures or silhouettes
- Pokedex framing
- badge shapes
- battle HUD layouts copied 1:1
- Pokemon fonts or wordmarks
- exact border patterns from Pokemon games
- professor / trainer / gym visual language

The target is: "inspired by handheld JRPG UI craft".
Not: "Hermes Pokemon hack".

## Hermes-specific fantasy

The app fantasy should be:
- Hermes is a relay traveler / courier terminal
- the top screen feels like a field device status screen
- the bottom screen feels like a command menu from a handheld RPG
- replies feel like dialogue boxes from an adventure game
- thread switching feels like opening a party / journal / comms roster

This makes the app feel game-like while still fitting the product.

## Core visual pillars

### 1. Pixel-framed panels
Panels should look hand-built from tiles, not like terminal boxes.

Rules:
- 2px to 3px outer dark outline
- lighter inner line
- flat fill with minimal shading
- corners should feel like sprite corners, not modern rounded UI cards
- borders should be slightly ornamental but still cheap to render

### 2. Warm handheld palette
Use a limited palette that reads well on an Old 3DS.

Suggested starting palette:
- Ink: `#1c1d25`
- Deep panel: `#2d3347`
- Slate panel: `#43506a`
- Mist panel: `#6e7b91`
- Light text: `#e8e6d9`
- Soft cream: `#f4f1e6`
- Hermes teal: `#58c7c2`
- Signal cyan: `#8ae7e3`
- Gold accent: `#d9b45f`
- Warning amber: `#c9824a`
- Success green: `#78b96f`
- Reply lavender-gray: `#8f88a9`

Notes:
- avoid pure black everywhere
- avoid neon hacker green
- keep backgrounds dark but slightly colored
- use teal/gold as Hermes identity accents

### 3. Dialogue-box presentation
Hermes replies should feel like RPG dialogue windows.

Rules:
- reply box gets the strongest frame on the top screen
- text sits on a calm flat background
- page indicator should feel like in-world UI chrome, not debug text
- long replies should still prioritize readability over decoration

### 4. Sprite-like emblem language
Instead of Pokemon creatures, use Hermes-native pixel motifs:
- relay crest
- courier bird silhouette
- signal tower
- star map node
- messenger cartridge / deck icon
- link crystal / relay shard

These can be tiny 16x16 or 24x24 icons.

### 5. Menu rhythm
The app should feel navigated through a game menu system.

Rules:
- each screen has one dominant title
- selected items should use a clear cursor marker or highlighted strip
- command lists should feel like classic RPG menus, not desktop settings pages
- spacing should create strong rows and sections

## Screen direction

## Top screen = story/status stage

The top screen should feel like the main scene window.

Preferred structure:

```text
+================================================+
| HERMES AGENT                      LINK  OK      |
| Relay Deck                                      |
+==========================+=====================+
| Active Thread            | Crest / pixel icon  |
| gateway-main             | small Hermes motif  |
| status: connected        |                     |
| relay: healthy           |                     |
+==========================+=====================+
| HERMES REPLY                                   |
| "Ready for your next message."                |
|                                                |
|                                            1/3 |
+================================================+
```

Design intent:
- use the full width
- left side is utility info
- right side is emblem / decorative identity
- lower half belongs to the reply/dialog area

## Bottom screen = RPG command menu

The bottom screen should feel like a command menu from a handheld RPG.

Preferred structure:

```text
+======================================+
| COMMAND DECK                         |
+======================================+
| > Ask Hermes                         |
|   Check Link                         |
|   Threads                            |
|   Config                             |
|   Mic Input                          |
|   Clear Reply                        |
+----------------------+---------------+
| Gateway              | Token         |
| 192.168.x.x:8787     | configured    |
+----------------------+---------------+
```

Design intent:
- make actions feel like menu choices, not raw button legends
- physical button hints can live in a thin footer row
- the menu itself should be readable even before reading the controls

## Conversation screen direction

Conversation selection should feel like a party roster / comms log.

Use:
- left cursor arrow or block pointer
- one thread per row
- active thread highlight bar
- small status glyphs for synced, pending, or missing
- preview text trimmed to one short line

Avoid:
- dense debug metadata
- too many numeric fields on one screen

## Settings screen direction

Settings should feel like an equipment/options menu.

Use:
- labeled rows
- selected row highlight
- editable value box
- subtle icon by each field if cheap enough
- grouped sections: network, identity, audio

## Aesthetic references to emulate

Emulate these qualities:
- GBA/DS handheld JRPG menus
- classic adventure dialogue boxes
- overworld pause-menu clarity
- pixel iconography with strong silhouettes
- light ornamental framing around practical utility UI

Do not emulate:
- battle scenes directly
- creature encounter UI
- trainer cards
- copycat franchise iconography

## Motion and effects

Old 3DS-safe only.

Allowed:
- simple selection blink
- one or two-frame cursor shimmer
- panel swap transitions if very cheap
- tiny idle sparkle on crest only if redraw cost is safe

Avoid:
- full-screen animated backgrounds
- particle-heavy effects
- alpha-heavy transitions
- constant motion on both screens

## Typography feel

Typography should read like a retro handheld localizer box:
- short labels
- all-caps for system headings
- title case or sentence case for reply copy
- compact but not cramped
- no fake gritty hacker styling

Good labels:
- RELAY DECK
- COMMAND DECK
- THREAD LOG
- LINK STATUS
- HERMES REPLY
- AUDIO INPUT
- SYSTEM CONFIG

## Immediate art targets

First-pass asset set:
- 24x24 Hermes relay crest sprite
- 16x16 status icons: online, offline, syncing, mic, settings
- panel border tiles or border drawing rules
- highlight bar treatment for selected menu rows
- dialogue box treatment for reply pane
- top-screen background texture pattern at very low contrast

## Suggested implementation order

### Phase A — style lock
Do first without changing renderer too much.
- finalize palette
- finalize border language
- finalize top/bottom screen composition
- finalize selected-row treatment
- finalize icon list

### Phase B — text-first fake mock in current console UI
Use the existing console layout to prove the composition.
- stronger framed reply box
- menu-style command deck rows
- pointer-style selection markers
- more game-like section names
- temporary ASCII stand-ins for crest/icons

### Phase C — sprite-backed UI pass
After the composition feels right on hardware.
- move crest to real pixel sprite
- move icons to small sprite sheet
- add tile-like panel framing
- keep redraw cost predictable

### Phase D — polish
- subtle highlight animation
- cleaner page indicator
- optional patterned panel fills

## Mapping current Hermes UI to the new fantasy

Current term -> pixel RPG framing
- Messenger Deck -> Relay Deck
- Messenger Link -> Link Status
- Active Thread -> Active Thread or Current Channel
- Hermes Reply -> Hermes Reply
- Thread Deck -> Thread Log
- System Config -> System Config

Recommended note:
Keep `Hermes Agent` as the master brand.
The game-like labels should sit underneath it, not replace it.

## Non-infringing signature idea

The signature visual motif should be a Hermes relay sigil:
- diamond core
- wing-like side fins
- tiny antenna spark on top
- square pixel silhouette

This gives us a mascot-quality identity without needing a character.

## Definition of done for the first visual pass

We are successful when:
- the app no longer feels like a debug console
- the screens feel like a handheld RPG tool/menu system
- a user can describe it as "Pokemon-ish" or "classic handheld RPG" without seeing any copied franchise elements
- the UI still reads cleanly on an Old 3DS
- the rendering remains lightweight

## Recommended next file touches

When implementation starts, the first files to touch should be:
- `docs/design-language.md`
- `client/source/app_ui.c`
- `client/tests/test_ui_refresh.py`
- `assets/` for crest/icon mockups

## First concrete implementation slice

The best first slice is:
1. keep the current rendering path
2. restyle the top screen as a framed dialogue/status layout
3. turn the bottom screen into a true menu list with a cursor row
4. add temporary ASCII pixel stand-ins for the crest and icons
5. test on real Old 3DS hardware before adding true sprite assets
