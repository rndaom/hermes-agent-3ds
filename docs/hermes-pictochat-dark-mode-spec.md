# Hermes Agent 3DS — dark-mode PictoChat visual spec

> Source-of-truth UI spec for the handheld client. This supersedes the previous pixel-RPG direction for the main app UX.

## Goal

Make Hermes Agent on 3DS feel like a native dark-mode Nintendo handheld messenger:
- Hermes-branded
- PictoChat-informed
- DS firmware-informed
- stable on original / Old 3DS hardware
- readable first
- visually real, not console-chrome pretending to be a GUI

## Non-goals

This spec explicitly rejects:
- Pokemon-inspired UI language
- Nous branding or themes
- any shipped light theme or light-mode implementation target
- giant terminal slabs on black backgrounds
- modern mobile-chat styling with retro paint on top
- heavy animation, particle effects, blur, or alpha-rich transitions
- mixing multiple rendering models on the same main app screens

## Research basis

This spec is grounded in four inputs:

1. Figma source provided by the user
- Nintendo DS Design System V1.3.3 (Community)

2. Exported design boards reviewed from `~/Downloads`
- `Pictochat Elements.png`
- `Theme Tokens.png`
- `Buttons.png`
- `Button Gradients.png`
- `Keyboard.png`
- `Grid & Background.png`
- `Typeface.png`
- `Icons.png`
- `Theme Select.png`
- `Frame Gradients.png`
- `Global Tokens.png`
- `Semantic Tokens.png`
- `Nintendo DS Design System V1.3.3 (Community).png`
- `Navigation & Calibration.png`

3. Official / current 3DS graphics stack references
- `citro2d` docs: https://citro2d.devkitpro.org/
- `libctru` gfx docs: https://libctru.devkitpro.org/gfx_8h.html
- local installed examples under `/opt/devkitpro/examples/3ds`
- local installed toolchain versions on this machine:
  - `libctru 2.7.0-1`
  - `citro2d 1.7.0-1`
  - `citro3d 1.7.1-2`
  - `3ds-examples 20240917-1`
  - `tex3ds 2.3.0-4`
  - `picasso 2.7.2-3`

4. Current repo audit
- active graphical UI lives in `client/source/app_ui.c` + `client/source/app_gfx.c`
- current implementation already uses Citro2D/Citro3D
- approval and voice transient flows now render through the same graphical renderer language
- current implementation is closer to the target, but still visually incomplete for the desired PictoChat-first finish

## Core product fantasy

Hermes Agent 3DS should feel like:
- a dark handheld messaging utility
- a compact relay terminal built into Nintendo hardware
- a tactile appliance
- a tool for short-form communication, not a desktop chat app shrunk down badly

The emotional target is:
- calm
- compact
- clever
- structured
- a little playful
- never noisy

## Visual identity in one sentence

Dark graphite handheld system chrome + PictoChat-style note/message cards + DS firmware framing + tiny pixel-crisp utility icons.

## The three strongest reference pillars

### 1. PictoChat message DNA
Preserve these ideas:
- thin framed message cards
- attached sender/room chip at the card edge
- compact system notices
- note-card / ruled-paper ancestry
- color-coded participant or room identity

### 2. DS firmware chrome
Preserve these ideas:
- framed top and bottom rails
- visible modular paneling
- restrained gradients instead of pure flat slabs
- neutral buttons living on top of colored structural chrome
- dense but orderly spacing

### 3. Hermes branding
Preserve these ideas:
- relay / link / thread / session language
- subtle courier / signal / switchboard motifs
- serious utility tone with a little charm
- no fantasy RPG framing as the primary brand language

## Platform and hardware facts

### Screen geometry
For high-level Citro2D UI layout, design for:
- top screen: `400x240`
- bottom screen: `320x240`

Important low-level note from `libctru`:
- the 3DS LCDs are physically portrait displays rotated 90 degrees
- low-level framebuffer APIs expose that reality
- for normal Citro2D UI work, use the conventional landscape coordinate spaces above

### Old 3DS baseline
All visual decisions must assume the minimum target is an original / Old 3DS.

That means:
- no always-on animated background layers
- no shader-heavy effects as a requirement
- no expensive overdraw just for style
- no gratuitous fullscreen fades during normal navigation
- stable redraws matter more than ornament
- text clarity matters more than novelty

### Wide mode and stereoscopic 3D
Do not depend on either.

Spec rule:
- `gfxSet3D(false)`
- no requirement for wide mode
- no feature that only looks correct with stereoscopic depth

## Rendering architecture requirements

## Approved rendering stack
Use:
- `gfxInitDefault()`
- `gfxSet3D(false)`
- `C3D_Init(...)`
- `C2D_Init(...)`
- `C2D_Prepare()`
- `C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT)`
- `C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT)`

Reason:
- this is the current practical official path for real graphical 3DS homebrew UIs
- it is already installed and working locally
- it aligns with local examples in `/opt/devkitpro/examples/3ds`

## Renderer ownership rule
`app_gfx` must be the sole owner of frame presentation for the normal app UI.

Required rule:
- no `gfxFlushBuffers()` / `gfxSwapBuffers()` / ad-hoc presentation calls outside the renderer for normal GUI screens
- no mixed presentation model between `C3D_FrameBegin/End` and console-driven manual swaps on the same main UI screens

This is the single most important stability rule in this spec.

## PrintConsole rule
Do not mix `PrintConsole` rendering with Citro2D on the same normal app screens.

Allowed:
- temporary developer-only fallback paths during migration
- emergency debug modes hidden behind compile-time or developer-only switches

Not allowed in the final user-facing main UX:
- home screen in Citro2D, then approval flow on the same screens via console
- home/settings/conversations in Citro2D with voice-recording shells rendered by `printf`

Transient screens must eventually share the same renderer language.

## Text rendering rule
Primary text path should use Citro2D text.

Relevant official capabilities:
- Citro2D supports text using the system font
- local examples show `C2D_TextBuf`, `C2D_TextParse`, `C2D_TextOptimize`, `C2D_DrawText`, and `C2D_TextGetDimensions`
- custom font loading is possible with `C2D_FontLoad(...)`
- system font loading may require `cfguInit()` when explicitly loading system fonts

Spec requirement:
- all bounded text layout must be implemented via pixel-width measurement and explicit truncation / ellipsis rules, not by assuming character count equals visual width
- if clipping/scissoring is added, it is a secondary safety net, not a substitute for width budgeting
- cache or reuse text buffers intelligently
- do not reparsed everything redundantly if there is a cheaper stable path
- all long text fields must have pixel-width budgets, clipping, or ellipsis rules

## Asset pipeline rule
Preferred production asset path for this repo should respect the current build system first.

First-pass approved path:
- use the existing `client/gfx/*.t3s` -> `tex3ds` -> `.t3x` pipeline already supported by `client/Makefile`
- use embedded/generated assets for the first polished UI pass

Optional later path:
- ROMFS-backed assets if the project intentionally decides to move there later
- if ROMFS is adopted, the implementation must explicitly wire `romfsInit()` / `romfsExit()`, asset paths, packaging, and runtime loading rules

Shader rule:
- `picasso` is optional only for a deliberate future shader path
- the first polished pass must not require custom shaders

## Current repo problems this spec must fix

The current graphical pass is not the source of truth.
This spec exists partly to replace ambiguity.

### Known current issues from repo audit
- some cards and action wells still read as generic dark UI instead of a stronger PictoChat note-card system
- some action/help rows still need better control-label accuracy and tighter composition
- long text containment still needs hardware validation for extreme thread titles and status strings
- the first polished asset-backed crest/icon pass is still pending

This spec treats those as defects, not as precedent.

## Current implemented milestone and live hardware feedback

### What is now finished
The following are now implemented and were validated in the repo and deployed to the real Old 3DS build:
- Citro2D/Citro3D graphical UI is the active main-screen path
- `main.c` no longer boots the main UI through `consoleInit(...)`
- renderer ownership cleanup for normal GUI screens is complete
- approval prompt now renders graphically
- mic recording prompt now renders graphically
- the app is deployed over FTPD to `sdmc:/3ds/hermes-agent-3ds/`
- the user gave strong positive first-impression feedback on the deployed graphical build

### What the latest live hardware feedback says
This feedback is now part of the source of truth and should directly guide the next visual pass.

1. The current build is a strong baseline
- The user described the current build as "super cool already" and "astounding work"
- This means the current graphical direction is approved as a foundation, not to be discarded

2. Top rail content needs to become more useful
- The current subtitle / secondary top-screen label (`RELAY DECK`) should be replaced
- Preferred replacement content:
  - current model name, for example `gpt-5.4`
  - context bar / context usage indicator
  - session length or similar session/runtime status

3. Active-thread utility panel needs containment and better composition
- text currently spills outside the active-thread box in some cases
- there is too much unused empty space on the right side of that box
- the next pass should rebalance the panel so the available width is actually used

4. Crest panel placeholder is acceptable for now
- The current rough placeholder shape in the relay/crest panel is acceptable temporarily
- The user expects this area to hold a real future custom crest / image / icon treatment later
- This is a future art pass item, not an immediate blocker

5. Reply card containment is still not good enough
- long reply text can visually cross the note lines / exceed the intended box boundaries
- reply text still wraps too early and leaves too much unused width on the right side of the reply card
- the next pass must use more of the real horizontal width of the card

6. Command menu should support true D-pad navigation in addition to hotkeys
- hotkeys stay
- D-pad navigation should be added for the command list itself
- pressing the selected command via confirm/select action should work
- this should make the command deck feel like a real handheld menu, not just a list of labeled shortcuts

7. Command/gateway text still needs containment work
- command menu and gateway box content can still render outside their containing boxes in edge cases
- bounded text rules must continue to be applied until no user-facing overflow remains

## Dark-mode-only visual language

## Base surfaces
The shell is dark-first.

Use these material families:
- app background: deep graphite / blue-black
- raised panel: dark slate
- inset well: darker charcoal
- chrome rail: dark tinted gradient strip
- text field / compose field: dark paper / dark LCD well
- status plaque: near-black with restrained colored trim

Do not use:
- large bright white surfaces
- pure black everywhere
- glossy mobile glassmorphism

## Structural texture
Allowed subtle textures:
- faint horizontal scanlines
- faint modular grid / graph-paper underlay
- restrained dither in gradients where it helps the handheld feel
- 1px inset lines

Not allowed:
- noisy wallpaper textures
- strong repeating backgrounds fighting legibility

## Corners and geometry
Primary geometry should be:
- squared or tightly rounded
- compact
- framed
- modular

Do not use:
- giant bubble corners
- soft modern SaaS cards
- floating edge-less panes

## Palette system

## Global neutral palette
Recommended default neutrals:
- `bg.app`: `#171a22`
- `bg.top`: `#1b1f29`
- `bg.bottom`: `#202532`
- `surface.1`: `#242a37`
- `surface.2`: `#2d3445`
- `surface.3`: `#394359`
- `border.soft`: `#55617a`
- `border.strong`: `#93a1bb`
- `text.primary`: `#f1f3f7`
- `text.secondary`: `#c5cfdb`
- `text.muted`: `#8b97ab`
- `text.inverse`: `#171a22`

## Hermes default accent family
Default Hermes theme should feel like a relay deck, not neon cyberpunk.

Recommended defaults:
- `accent.primary`: `#5c83b1`  // slate/relay blue
- `accent.primary-hi`: `#82a8d6`
- `accent.secondary`: `#62d1cf` // signal cyan
- `accent.secondary-hi`: `#9de8e4`
- `accent.system`: `#d6a85d` // muted amber for system rails and warnings
- `accent.warning`: `#cf8746`
- `accent.error`: `#d65d68`
- `accent.success`: `#69bf79`

## Optional theme families
The DS reference strongly supports personalization.
The production UI may offer multiple accent themes, but all must remain dark-shell variants.

Approved optional families:
- blue-gray
- amber-brown
- red
- orange
- yellow-amber
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

Rule:
- themes recolor chrome, focus, identity chips, and small rails
- themes do not recolor all text or flood full surfaces with saturation

## Semantic vs identity color
Keep these separate:
- identity/theme color = room/user/chrome preference
- semantic color = success/warning/error/info

A blue room theme must not weaken the readability of error/warning states.

## Typography

## Typography philosophy
The app should look pixel-aware without making messages painful to read.

Use a split system:
- headers, labels, counters, chips, and small system headings: pixel-ish / bitmap-ish / crisp handheld display style
- longer message content and dense metadata: highly readable compact UI text using the same general visual family or a cleaner companion

## Type hierarchy
Recommended roles:
- `display`: splash titles only
- `screen_title`: top/bottom rail titles
- `panel_title`: section labels like `THREAD`, `REPLY`, `GATEWAY`
- `chip_label`: sender, room, status chips
- `body`: message content, previews, setting values
- `meta`: timestamps, state hints, counts, footer hints

## First-pass font decision
For the first compliant implementation pass:
- use the current Citro2D text path with one readable system-font-backed body style for all dense content
- approximate the pixel-handheld look through sizing, caps usage, spacing, rails, chips, and iconography rather than by forcing a novelty font everywhere
- if a bundled pixel display font is added later, restrict it to `screen_title`, `panel_title`, and `chip_label`

## First-pass size targets
These are the default scale targets for the first stable implementation.
- `screen_title`: about `0.50f` to `0.56f` Citro2D scale
- `panel_title`: about `0.40f` to `0.46f`
- `chip_label`: about `0.32f` to `0.38f`
- `body`: about `0.30f` to `0.36f`
- `meta`: about `0.28f` to `0.32f`

Implementation rule:
- line height and page size must be derived from these chosen scales and the layout metrics table above, not from ad-hoc trial values scattered through the code

## Text rules
- short labels
- all caps for system headings is allowed
- body text should not be all caps
- high x-height and strong spacing over thin elegant typography
- no fake hacker-terminal font
- no novelty JRPG font as the main body type

## Iconography

## Icon style
Icons must be:
- pixel-crisp
- low-detail but high-silhouette
- readable at tiny sizes
- primarily monochrome/light-on-dark by default
- accent-tinted only when active, selected, or category-coded

## Approved icon motifs
- relay node
- mail/message card
- link chain / link beam
- settings gear simplified to handheld-era form
- mic / waveform / input
- arrows / paging / thread navigation
- room tile / notebook tab / card stack
- sync / pending / approval / alert

## Explicitly avoid
- mascot faces as core brand iconography
- fantasy creature silhouettes
- Pokemon-like stamp sets
- modern featherweight outline icon packs

## Component system

## Screen rails
Every screen gets:
- top structural rail with title and compact status/identity info
- bottom structural rail or command deck framing

Dark-mode rail recipe:
- 2-step vertical gradient
- 1px top highlight or inner line
- 1px outer frame
- restrained accent stripe or inner rail

## Message card
Primary top-screen message component.

Anatomy:
- dark neutral card body
- thin accent border
- attached sender/thread chip at the top edge
- optional faint ruled internal lines
- optional small action pill or page tag

Rules:
- card body remains mostly neutral
- accent indicates identity, not full fill
- long text wraps within a fixed pixel width
- ellipsis or paging is mandatory when content exceeds capacity

## System plaque
Used for:
- searching
- syncing
- network discovery
- approval pending
- voice capture states

Anatomy:
- near-black body
- amber or accent trim
- tiny pixel icon + compact status label
- optional small telemetry bars or dots

## Button
Buttons are not giant modern tap pills.

Construction:
- tight rounded-rect or rectangular body
- 1px outer border
- 1px inset line or highlight
- restrained gradient or tone step
- small left icon cell optional
- strong selected/focus state independent of theme color

## Focus state
Focus/selection must not depend only on fill color.

Approved selection cues:
- brighter border
- inset accent line
- corner brackets
- high-contrast inner fill change
- optional selection bar for menu rows

Not sufficient by itself:
- “it gets a little bluer”

## Panel and layout language
Use:
- framed compartments
- visible sectioning
- small modular wells
- grouped controls
- predictable rhythm

Avoid:
- floating panes with no screen logic
- over-framing every single atom

## Screen composition

## Canonical layout metrics
These are the default geometry targets for the first compliant visual pass.
If the implementation deviates, it should do so intentionally and update this table.

### Top screen (`400x240`)
- outer gutter: `10px`
- top rail height: `28px`
- gap below rail: `10px`
- upper utility band height: `72px`
- gap between utility band and reply card: `8px`
- reply card height target: `100px` minimum
- default panel radius: `2px` to `4px` equivalent visual softness only

### Bottom screen (`320x240`)
- outer gutter: `8px`
- top rail height: `28px`
- gap below rail: `8px`
- main command block width target: `180px` to `192px`
- command row height: `16px` to `20px`
- command row vertical gap: `2px` to `4px`
- right utility well width target: `104px` to `116px`

### Text visibility budgets
These are default production targets, not suggestions.
- top reply card visible reply lines: `4` by default on the first stable graphical pass
- top reply card visible last-message lines: `2`
- conversation preview lines per row: `1`
- settings value lines per field: `1`
- status plaque lines: `2` max

If later art or font changes require different numbers, update this table and the pagination logic together.

## Control mapping
Default control mapping remains explicit and first-class.

### Home
- `UP/DOWN` = move through the home action list
- `Circle Pad UP/DOWN` = move through the home action list
- `LEFT/RIGHT` = reply page
- `Circle Pad LEFT/RIGHT` = reply page
- `A` = run the selected home action
- touch = tap a bottom-screen home action button
- `START` = exit

### Threads
- `UP/DOWN` = move selection
- `Circle Pad UP/DOWN` = move selection
- `A` = use selected thread
- `X` = create thread
- `Y` = sync threads
- `B` = return home
- `START` = exit

### Settings
- `UP/DOWN` = move field selection
- `Circle Pad UP/DOWN` = move field selection
- `A` = edit selected field
- `X` = save settings
- `Y` = restore defaults
- `B` = return home
- `START` = exit

### Approval flow
- `A` = allow once
- `X` = allow for the current session
- `Y` = always allow
- `B` = deny
- `START` = cancel the prompt only if it is safe to leave it

### Voice-input flow
- `UP` = stop/send once recording is armed
- `B` = cancel
- `START` = abort only if it is mapped to the same safe cancel path

Touch input already exists for the home action deck. None of these mappings may become touch-only.

## Compose and keyboard policy
For this UI rewrite, the project should keep the built-in software keyboard for message compose and field editing.

Decision for this spec:
- built-in `swkbd` remains the compose/edit input path for v1 of the visual refresh
- custom on-device keyboard art is deferred
- the app shell must still clearly frame which field/message is being edited before entering `swkbd` and after returning from it

This keeps the visual rewrite focused and Old-3DS-safe.

## Top screen role
Top screen is the main conversation and status stage.

Required structure:
1. top rail
- app title
- active mode or screen name
- compact connection/thread indicator

2. upper utility region
- active thread / room / conversation identity
- connection or relay state
- tiny emblem or utility icon block

3. primary message/reply stage
- last Hermes reply
- last sent message context if needed
- reply page indicator
- optional message-state chip like `SYNCED`, `WAITING`, `APPROVAL`

Top-screen rule:
- reply content owns the largest and most readable area
- status metadata must never crowd out reply readability

## Bottom screen role
Bottom screen is the command and compose deck.

Required structure:
1. bottom rail / header
- screen-appropriate action deck title

2. main command region
- stylus- and button-friendly action rows
- real current selection state, not decorative fake highlight

3. compact utility region
- gateway summary
- token/device summary if relevant
- page hint / input hint

Bottom-screen rule:
- action affordances must be obvious even before reading detailed help text

## Home screen
Top screen:
- active thread block
- relay/link health block
- main reply card

Bottom screen:
- ask Hermes
- check link
- threads
- config
- mic input
- clear reply

App exit remains on `START` rather than a dedicated home action row.

## Settings screen
Top screen:
- settings categories and value wells
- selected field must be visually obvious
- current value well separate from label region

Bottom screen:
- edit
- save
- restore defaults
- back/home
- `START` exit hint

## Conversation screen
Top screen:
- thread archive list
- compact preview snippets
- active thread clearly marked
- page/scroll indicator if needed

Bottom screen:
- use thread
- create thread
- sync threads
- back/home
- `START` exit hint

## Voice input screen
This screen must be graphical too.

Top screen:
- recording state plaque
- elapsed time
- level or pulse indicator if cheap enough
- status message

Bottom screen:
- stop/send
- cancel
- short input hints

Rule:
- no raw console fallback shell in the final UX

## Approval/request screen
This screen must share the same visual language.

Top screen:
- request summary
- what Hermes wants to do
- current target or command preview

Bottom screen:
- approve
- reject
- back
- short button hints

Rule:
- do not dump raw debug text and call it a UX

## Keyboard and compose design

The DS reference strongly favors explicit keyboard zoning.

Required composition rules:
- compose preview field is visually separate from keys/actions
- regular keys are compact and uniform
- action keys are wider and more prominent
- bottom action row handles mode switching and send/confirm behavior
- symbols / presets / modes should be explicit, not hidden behind mystery icons

If built-in software keyboard remains in use:
- wrap the transition in the same graphical app shell
- clearly show what field is being edited before and after the handoff

## Motion and effects

Allowed:
- simple highlight change on selection
- tiny state blink for recording or sync
- one cheap transition for screen swaps if stable

Avoid:
- constant motion on both screens
- alpha-rich particle effects
- per-frame decorative shimmer across big areas
- transitions that cost clarity or stability on Old 3DS

## Performance and safety budgets

These are hard constraints for implementation.

### Rendering discipline
- avoid renderer mixing
- avoid redundant redraw chains
- avoid reparsing unchanged long text every frame if cacheable
- avoid drawing off-panel text with no clipping budget

### UI budget discipline
Every field needs one of:
- fixed max width + ellipsis
- fixed visible rows + paging
- fixed visible rows + truncation marker

No field may be left “unbounded.”

This includes transient flows and not just the main screens:
- approval/request summary
- command preview text
- voice-input status copy
- elapsed timer labels
- software-keyboard return summaries
- any system error banner shown on top or bottom screen

### Asset discipline
- keep icon sizes small and reusable
- prefer sprite atlases / shared texture assets
- only add decorative assets that materially improve clarity or identity
- decorative rails, grids, and note textures should preferably be atlas-backed or otherwise reused; do not rebuild large decorative layers from many tiny dynamic primitives every frame on Old 3DS

### Touch and input discipline
- D-pad/button navigation must remain first-class
- touch can enhance the bottom screen later, but must not become the only viable path

## Required implementation constraints for this repo

The following are mandatory for future code work:

1. `app_gfx` becomes the sole presentation owner for GUI screens
2. `app_ui` becomes the sole source of truth for visual composition
3. `voice_input.c` graphical flows must migrate off raw `PrintConsole`
4. approval/request prompts must migrate off raw `PrintConsole`
5. reply pagination logic must match actual visible rows
6. all long text fields get clipping or ellipsis rules
7. menu highlight state must reflect real navigation state
8. all primary screens share the same dark-mode PictoChat/Hermes language

## Migration phases

### Phase A — source-of-truth spec and token cleanup
- land this spec
- update `docs/design-language.md`
- keep historical docs for reference, but mark this spec as current

### Phase B — renderer discipline
- unify graphical presentation ownership
- remove mixed normal-screen console rendering
- add clipping/truncation helpers and field budgets

### Phase C — shell conversion
- restyle home/settings/conversations to match this spec exactly
- fix pagination and fake highlights

### Phase D — transient screen conversion
- graphical voice-input shell
- graphical approval/request shell
- consistent bottom-screen command deck

### Phase E — asset polish
- icon sheet
- theme variants
- optional border tiles / crest art
- subtle textures only after stability is proven

## Validation checklist

A build is not accepted against this spec until it passes:

1. `python3 -m pytest client/tests -q`
2. `source /etc/profile.d/devkit-env.sh && make -C client clean && make -C client`
3. real hardware verification on original / Old 3DS

Real hardware acceptance must explicitly test:
- long reply paging
- long conversation titles
- long host/token/device strings
- settings editing
- thread switching
- voice-input shell
- approval prompt shell
- repeated navigation without visual corruption

## Review checklist for future UI changes

Every UI PR or implementation pass should answer:
- Does it still look like a handheld messenger rather than a generic app?
- Does it preserve dark-mode PictoChat card language?
- Are accents structural, not overwhelming?
- Are all text fields width-bounded?
- Is selection state independent of theme color?
- Is the render path unified?
- Is it safe on Old 3DS?

## Anti-regression summary

Never regress to:
- Pokemon-like game UI
- Nous-themed styling
- light-mode-first surfaces
- giant terminal boxes
- mixed console/GPU UI on the same normal screens
- placeholder fake highlight states
- unbounded text drawing

## Final direction statement

Hermes Agent 3DS should look and behave like a real dark-mode Nintendo handheld messaging utility: framed, compact, note-card-driven, pixel-crisp, themeable in restrained ways, and technically disciplined enough to stay stable on original Old 3DS hardware.
