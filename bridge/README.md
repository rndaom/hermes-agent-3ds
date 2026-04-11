# bridge

The bridge is the small service that sits between the 3DS app and Hermes.

Its job is to keep the handheld side simple.

## Current status

The bridge now has:
- FastAPI app
- env-based config loading
- auth token support
- `GET /api/v1/health`
- `POST /api/v1/chat`
- reply truncation for 3DS-sized output
- a real Hermes CLI connector behind the chat endpoint
- a real 3DS client target for setup and hardware testing

The chat endpoint now forwards requests into Hermes through the local `hermes chat -q` CLI flow and returns a cleaned text reply.

## Environment variables

- `HERMES_3DS_BRIDGE_HOST`
- `HERMES_3DS_BRIDGE_PORT`
- `HERMES_3DS_BRIDGE_AUTH_TOKEN`
- `HERMES_3DS_BRIDGE_MAX_REPLY_CHARS`
- `HERMES_3DS_BRIDGE_HERMES_COMMAND`
- `HERMES_3DS_BRIDGE_HERMES_TIMEOUT_SECONDS`

## Local run

```bash
python -m venv .venv
. .venv/bin/activate
pip install -e './bridge[dev]'
export HERMES_3DS_BRIDGE_AUTH_TOKEN='***'
hermes-3ds-bridge
```

## Endpoints

### `GET /api/v1/health`
Returns a simple setup check payload.

### `POST /api/v1/chat`
Accepts JSON like:

```json
{
  "token": "***",
  "message": "hello from my 3ds"
}
```

Returns JSON like:

```json
{
  "ok": true,
  "reply": "Hi from Hermes on your 3DS!",
  "truncated": false
}
```

The bridge cleans up Hermes CLI chrome like `session_id:` lines before returning text to the 3DS.
