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
- basic reply truncation

Right now the chat endpoint uses a temporary local responder so the API can be exercised before the real Hermes connector is wired in.

## Environment variables

- `HERMES_3DS_BRIDGE_HOST`
- `HERMES_3DS_BRIDGE_PORT`
- `HERMES_3DS_BRIDGE_AUTH_TOKEN`
- `HERMES_3DS_BRIDGE_MAX_REPLY_CHARS`

## Local run

```bash
python -m venv .venv
. .venv/bin/activate
pip install -e './bridge[dev]'
export HERMES_3DS_BRIDGE_AUTH_TOKEN='change-me'
hermes-3ds-bridge
```

## Endpoints

### `GET /api/v1/health`
Returns a simple setup check payload.

### `POST /api/v1/chat`
Accepts JSON like:

```json
{
  "token": "change-me",
  "message": "hello from my 3ds"
}
```

Returns JSON like:

```json
{
  "ok": true,
  "reply": "Bridge is up, but Hermes is not connected yet. You said: hello from my 3ds",
  "truncated": false
}
```

The real Hermes connector is the next step.
