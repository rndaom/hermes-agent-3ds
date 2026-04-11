# hermes-agent-3ds

```text
   .-.
  (o o)  Hermes, but tiny.
  | O \
   \   \
    `~~~'
```

A Nintendo 3DS client for Hermes Agent.

The goal is simple: open an app on a modded 3DS, talk to Hermes, get useful replies back.

This repo is for the **3DS app** and the public project docs around it. The heavy lifting still happens on a PC or server.

## Status

**Early project setup.**

We're starting with a clean v1:
- one chat screen
- one bridge/server target
- simple setup
- small, readable replies
- a UI that feels like Hermes on old handheld hardware

## What this is

`hermes-agent-3ds` is a thin client.

The 3DS is the frontend.
Hermes runs somewhere stronger.

Think of it like a tiny handheld gateway for Hermes, in the same spirit as talking to Hermes through Discord or Telegram.

## Why

Because it's funny, charming, and honestly kind of perfect.
Ancient handheld meets modern agent.

## V1 goals

- launch from the 3DS home menu
- connect to a local bridge service
- send a message
- receive a response
- keep the UI fast, simple, and readable
- keep setup friendly for non-dev users

## Design direction

The look and feel should lean into:
- Nous / Hermes energy
- retro anime terminal vibes
- pastel colors
- tasteful ASCII touches
- clean screens over clutter

More on that in [`docs/design-language.md`](docs/design-language.md).

## Repo guide

- [`docs/vision.md`](docs/vision.md) — what the project is trying to be
- [`docs/roadmap.md`](docs/roadmap.md) — current plan
- [`docs/design-language.md`](docs/design-language.md) — visual and tone direction
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — how to help

## Contributing

Issues and PRs are welcome. If you want to help, small focused contributions are best.

If you're opening an issue, please keep it clear and practical.

## License

MIT
