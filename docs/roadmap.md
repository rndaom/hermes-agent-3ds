# Roadmap

## current baseline

- [x] native V2 health check path
- [x] native V2 message send path
- [x] native V2 event polling
- [x] approval interaction flow
- [x] stale-event skipping via cursor + `reply_to`
- [x] SD-backed host / port / token / device ID config
- [x] Homebrew Launcher `.3dsx` packaging
- [x] Old 3DS-friendly paging and UI layout
- [x] remove bundled v1 bridge code from this repo
- [x] remove v1 chat transport from the client path

## next

- [ ] finish / restore Hermes-side native 3DS gateway support in `hermes-agent`
- [ ] real-hardware validation against the cleaned V2-only host path
- [ ] saved sessions / conversation selection
- [ ] nicer UI polish
- [ ] microphone input option on 3DS with bridge-side speech-to-text
- [ ] replace placeholder app art/icon with the provided portrait image asset

## maybe later

- [ ] theme packs
- [ ] richer media support
- [ ] notification/polling ideas
