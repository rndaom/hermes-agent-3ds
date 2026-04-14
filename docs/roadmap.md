# Roadmap

## current baseline

- [x] native V2 health check path
- [x] native V2 message send path
- [x] native V2 picture send path
- [x] native V2 event polling
- [x] native V2 media fetch path for picture previews
- [x] approval interaction flow
- [x] stale-event skipping via cursor + `reply_to`
- [x] SD-backed host / port / token / device ID config
- [x] Homebrew Launcher `.3dsx` packaging
- [x] Old 3DS-friendly paging and UI layout
- [x] remove bundled V1 bridge code from this repo
- [x] remove V1 chat transport from the client path
- [x] land Hermes-side native 3DS gateway support in `hermes-agent`
- [x] validate the V2-only client against that gateway on real hardware

## next

- [x] conversation slot selection + recent conversation sync
- [x] UI polish pass for readability and flow
- [x] microphone input option on 3DS with host-side speech-to-text
- [x] picture input option on 3DS with outer-camera still capture
- [x] replace placeholder app art/icon with the provided portrait image asset
- [x] native slash-command page for `/reset` and `/compress`
- [x] streamed partial reply rendering for 3DS conversations
- [x] inline picture preview rendering for Hermes conversation events

## maybe later

- [ ] richer picture controls such as preview/retake before send
- [ ] generated-audio playback from Hermes replies
- [ ] Hermes activity emoticons / expressive status indicators
- [ ] expanded bottom-screen slash-command surface
- [ ] sprite-backed DS icons and chrome assets
- [ ] theme packs
- [ ] richer media support
- [ ] notification/polling ideas
