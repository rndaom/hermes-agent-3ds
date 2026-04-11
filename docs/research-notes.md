# Research notes

This document captures the main findings behind the v1 plan.

## Sources reviewed

- [3DS Hacks Guide](https://3ds.hacks.guide/)
- [devkitPro Getting Started](https://devkitpro.org/wiki/Getting_Started)
- [devkitPro/libctru](https://github.com/devkitPro/libctru)
- [devkitPro/3ds-examples](https://github.com/devkitPro/3ds-examples)
- [devkitPro/citro2d](https://github.com/devkitPro/citro2d)
- [Ollama3DS](https://github.com/Dzhmelyk135/Ollama3DS)
- [TriCord](https://github.com/2b-zipper/TriCord)
- GBAtemp threads and related 3DS homebrew discussion

## What matters most

### 1) The base idea is viable

3DS Hacks Guide makes two useful things clear:
- homebrew apps still have Internet access
- Nintendo's official online shutdown does not block normal homebrew networking

That means a networked Hermes client is still realistic on modded hardware.

### 2) The standard toolchain is still the right one

devkitPro and libctru are the normal path.

The stack we should assume for development is:
- `devkitARM`
- `libctru`
- optional `citro2d` for cleaner UI work

devkitPro's `3ds-dev` package group is still the expected install path for developers.

### 3) Official examples already cover the primitives we need

`3ds-examples` includes working examples for:
- HTTP
- HTTP POST
- sockets
- SSLC
- software keyboard
- input handling
- SD card / romfs
- graphics

This is important because v1 does not need novel low-level tricks. The primitives already exist in sample form.

### 4) There is strong prior art for a thin 3DS network client

#### Ollama3DS
Closest proof-of-concept to this project.

Useful takeaways:
- 3DS can act as a chat frontend to a stronger machine on local Wi‑Fi
- simple HTTP request/response is enough for a usable first version
- in-app settings stored on SD card is a good fit
- the 3DS software keyboard is good enough for short prompt entry
- small response buffers and long replies are a real concern

Notable details from Ollama3DS:
- config path: `/3ds/ollama3ds/config.ini`
- defaults are stored locally and editable in-app
- chat history is intentionally small
- README notes a `4 KB` response buffer limit in that implementation

#### TriCord
Useful as a cautionary and architectural reference.

Useful takeaways:
- a polished multi-screen networked 3DS app is possible
- Citro2D/Citro3D can support a richer custom UI
- WebSockets and modern auth flows are much more complex than plain HTTP
- SSL verification and network edge cases can become user-facing support problems
- Old 3DS hardware is noticeably more constrained than New 3DS

TriCord also shows a practical split between:
- config saved on SD card
- network client layer
- UI screen manager
- app-specific logic

### 5) HTTP is the right default for v1

The official examples, Ollama3DS, and the extra complexity visible in TriCord all point in the same direction:

For v1, the 3DS app should talk to a bridge using **simple HTTP JSON**.

That keeps the 3DS side lighter by avoiding:
- direct provider auth
- WebSocket protocol handling
- modern TLS complexity against many third-party APIs
- large realtime state sync

### 6) The bridge should absorb modern complexity

The bridge service should handle:
- Hermes integration
- request shaping
- response shortening
- auth between the 3DS app and backend
- future compatibility changes

The 3DS client should stay focused on:
- input
- settings
- sending one request
- rendering one response clearly

## Constraints to design around

### Hardware and UX constraints
- text entry is slower than on a phone or PC
- screens are small
- long replies become unpleasant quickly
- Old 3DS hardware is meaningfully weaker
- memory limits are real

### Networking constraints
- HTTPS/TLS can be painful on older hardware and older client stacks
- retry and timeout behavior matters
- local-network-first design is safer and easier than wide-open Internet deployment

### Product constraints
- setup has to stay simple
- docs should avoid sounding like a developer-only toy
- the app should feel lightweight, not overloaded

## Practical conclusions

### Good v1 choices
- one bridge target
- one chat session
- one message in, one response out
- settings stored on SD card
- short, paged, or trimmed replies
- local network setup first
- HTTP JSON API

### Bad v1 choices
- direct provider APIs from the 3DS
- WebSockets as a requirement
- multiple accounts
- complex session sync
- giant theming system
- rich media focus
- long streaming output as a first dependency

## Final research conclusion

A Hermes 3DS client is realistic if it is treated as a **thin handheld frontend**.

The technical path with the best odds of shipping is:
- 3DS homebrew app built with `libctru`
- simple custom UI, likely with `citro2d`
- SD-backed local config
- local HTTP bridge on a stronger machine
- deliberately small v1 scope
