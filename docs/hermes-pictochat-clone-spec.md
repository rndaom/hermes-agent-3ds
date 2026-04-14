# Hermes Agent 3DS PictoChat Clone Spec

> Source-of-truth UI spec for the handheld client. This replaces the older dark-mode PictoChat spec.

## Goal

Make Hermes Agent on 3DS feel like a real PictoChat sibling:
- PictoChat-first
- DS firmware-first
- Hermes-aware
- light, paper-based, and tactile
- stable on original / Old 3DS hardware
- readable before decorative

The app should look like it belongs on Nintendo handheld hardware and happens to talk to the Hermes gateway.

## Explicit reset from the previous spec

The older dark-mode direction is no longer the source of truth.

This spec replaces it with:
- light ruled-paper surfaces instead of graphite slabs
- DS firmware gradients instead of dark console chrome
- room and note metaphors instead of relay-deck framing
- PictoChat-style chips, rails, and message cards as the primary language

## Research basis

This spec is grounded in four inputs.

### 1. User-provided design system
- Figma: `https://www.figma.com/design/DlaQhALYHx9t7q6z6bQ4YT/Nintendo-DS-Design-System-V1.3.3--Community-?node-id=135-51118&t=iGZYxl8UkyOEqG4k-1`

### 2. Local reference boards in `/home/rndaom/Downloads/Assets/`
- `Pictochat Elements.png`
- `Grid & Background.png`
- `Frame Gradients.png`
- `Buttons.png`
- `Icons.png`
- `Navigation & Calibration.png`
- `Theme Tokens.png`
- `Semantic Tokens.png`
- `Typeface.png`
- `Keyboard.png`
- `Nintendo DS Design System V1.3.3 (Community).png`

### 3. PictoChat behavior references
- PictoChat room metaphors, colored user chips, ruled note cards, and keyboard / drawing input concepts
- PictoChat’s board-like top screen and tool-driven lower screen
- PictoChat’s system-alert styling and room-first information hierarchy

### 4. Repo and runtime audit
- the client already has a stable Citro2D/Citro3D renderer
- the Hermes transport stack is native V2 and should stay intact
- the current code already supports:
  - message history
  - scrollable message history
  - room selection via conversation IDs
  - settings persistence
  - graphical transient flows
  - native Hermes slash-command dispatch for selected commands

## Product fantasy

Hermes Agent 3DS should feel like:
- PictoChat, but connected to Hermes instead of nearby DS consoles
- a room notebook for quick prompts and replies
- a tiny handheld relay appliance
- a playful Nintendo UI wrapped around a serious gateway function

The emotional target is:
- bright
- neat
- tactile
- lightweight
- a little toy-like
- never noisy

## Primary metaphors

### Hermes conversations are rooms
- `conversation_id` is still the real transport identity
- the handheld UI can surface them as sessions when that reads more naturally on-screen
- the active conversation should stay visible in the lower-screen header

### Messages are notes
- text send action becomes `Text Prompt`
- voice send action becomes `Audio Prompt`
- rendered messages should look like ruled PictoChat note cards

### Gateway state is relay state
- network and transport status should read as relay / link / notice copy
- Hermes-specific wording should stay concise and secondary to the PictoChat shell

## Visual pillars

### 1. Ruled paper first
Borrow directly from `Grid & Background.png` and `Pictochat Elements.png`:
- warm off-white backgrounds
- light gray notebook rules
- occasional vertical guide lines
- thin frame outlines

### 2. DS firmware chrome
Borrow directly from `Frame Gradients.png` and `Navigation & Calibration.png`:
- slim top rails
- top-to-bottom gradient bars
- soft rectangular control surfaces
- tiny utility chips and footer hint buttons

### 3. User-selected DS theme identity
Borrow directly from `Pictochat Elements.png`, `Buttons.png`, and `Theme Tokens.png`:
- one global shell accent chosen by the user
- colored sender chips
- the chosen DS theme appears in rails, focus rings, and controls
- neutral paper surfaces under the accent colors
- rooms do not get their own colors by default

## Platform constraints

### Screen geometry
- top screen: `400x240`
- bottom screen: `320x240`

### Hardware target
- original / Old 3DS is the minimum target
- no dependence on stereoscopic 3D
- no dependence on wide mode
- no expensive shader path required
- no animation-heavy shell

### Rendering stack
Keep using:
- `gfxInitDefault()`
- `gfxSet3D(false)`
- `C3D_Init(...)`
- `C2D_Init(...)`
- `C2D_Prepare()`
- `C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT)`
- `C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT)`

Renderer ownership stays the same:
- `app_gfx` owns frame presentation
- `app_ui` owns composition
- normal screens must not mix `PrintConsole` rendering with Citro2D rendering

## Typography and text rules

Use the system font for this pass, but style it to echo the DS reference:
- small scales
- uppercase labels for chrome
- short titles
- bounded text widths everywhere

Required text behavior:
- use pixel-width measurement
- use truncation or wrapping budgets for every variable-length field
- keep long Hermes replies readable via transcript scrolling
- do not assume character count equals visible width

## Core screen layouts

## Home screen

### Top screen
The top screen is the room board.

Required structure:
- DS-style colored header rail
- compact Hermes / relay-status header
- stacked ruled message notes
- hard cutoff below the header rail so cards never bleed into chrome

Behavior rules:
- show a scrollable note transcript with the newest messages at the bottom by default
- allow streamed Hermes partial replies to update the current note in place
- show a friendly empty-state note when nothing has been sent yet

### Bottom screen
The bottom screen is the tool tray.

Required structure:
- header rail
- multiple touch-sized tool pages
- tool page plus a Hermes slash-command page
- footer hint buttons

Required home tools:
- `Text Prompt`
- `Check Relay`
- `Sessions`
- `Settings`
- `Audio Prompt`
- `Picture Prompt`

Required slash-command entries:
- `Reset Session` -> native `/reset`
- `Compress` -> native `/compress`

## Settings screen

The setup screen should feel like a configuration sheet, not a dark control panel.

Required structure:
- header rail with PictoChat shell title
- six ruled config rows:
  - host
  - port
  - token
  - device ID
  - theme
  - mode
- clear selected-row highlight
- status strip for save / validation feedback
- lower-screen legend for `A`, `X`, `Y`, `B`, and `START`

## Sessions screen

The conversation picker becomes the `Sessions` page.

Required structure:
- recent rooms rendered as consistent ruled rows on paper
- active selection highlight tied to the current global theme
- preview text if Hermes supplied it
- lower-screen legend for:
  - `A Use session`
  - `X New session`
  - `Y Sync sessions`
  - `B Back`

## Approval screen

This should look like a DS system alert layered into the PictoChat shell.

Required structure:
- alert banner treatment inspired by the black/orange alert asset in `Navigation & Calibration.png`
- short explanation of the pending Hermes approval
- lower-screen option grid for:
  - once
  - session
  - always
  - deny
- explicit `START` cancel copy

## Mic Session screen

This should feel like a note-capture variant of PictoChat.

Required structure:
- recording status banner
- simple time and audio counters
- progress or capture-state framing on paper
- lower-screen legend for `A`, `B`, and `START`

## Color system

### Base neutrals
- warm off-white background
- pale paper fill
- light gray notebook rules
- medium gray outlines
- dark charcoal text

### Accent family
Accent colors should come from the DS theme picker palette references. Support the full 16-color set from `Theme Select.png`:
- blue
- brown
- red
- pink
- orange
- yellow
- lime
- green
- forest
- teal
- sky
- royal
- navy
- purple
- magenta
- fuchsia

Mode support:
- each theme needs light and dark variants
- dark mode is a user-selected shell mode, not a separate visual direction

Usage rule:
- accents belong on rails, chips, highlights, and active controls
- most message surfaces stay paper-colored
- avoid flooding full screens with saturated color

## UX rules

### Keep the controls real
- D-pad and Circle Pad remain first-class
- touch remains supported for the home tool tray
- `START` exits the app from main screens
- `L/R` scroll the top-screen transcript
- `LEFT/RIGHT` switch between lower-screen pages

### Keep Hermes clear, but quiet
- Hermes still needs to be visible in the copy
- do not bury transport failures
- do not let Hermes jargon dominate the screen chrome

### Keep room state obvious
- the active room should always be visible
- the active global theme should be visible in the shell
- room changes should clear stale visible message history, as the current app already does

## Implementation guidance

### Preserve these modules
- `client/source/bridge_v2.c`
- `client/source/bridge_health.c`
- `client/source/app_requests.c` transport semantics
- `client/source/app_config.c`

### Primary rewrite surface
- `client/source/app_ui.c`
- user-facing copy in `app_home.c`, `app_conversations.c`, `app_input.c`, and `app_requests.c`

### Asset strategy for this pass
First pass is allowed to stay procedural:
- Citro2D shapes
- DS-inspired gradients
- ruled-paper lines
- color tokens sampled from the reference boards

Future pass:
- import sprite-backed icons and chrome through the existing `client/gfx/*.t3s` pipeline if it meaningfully improves fidelity

## Acceptance checklist

This spec is satisfied when:
- the previous dark-mode PictoChat spec is gone
- the new source of truth is this PictoChat clone spec
- the home screen reads like a PictoChat room board
- the bottom screen reads like a DS tool tray
- rooms are the visible metaphor for Hermes conversations
- setup and approval screens follow the same visual system
- the app still builds, passes tests, and deploys on real hardware
