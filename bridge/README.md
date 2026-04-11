# bridge

The bridge is the small service that sits between the 3DS app and Hermes.

Its job is to keep the handheld side simple.

## Current status

The bridge skeleton is in place:
- FastAPI app
- env-based config loading
- auth token support
- app entrypoint

## Environment variables

- `HERMES_3DS_BRIDGE_HOST`
- `HERMES_3DS_BRIDGE_PORT`
- `HERMES_3DS_BRIDGE_AUTH_TOKEN`

## Local run

```bash
python -m venv .venv
. .venv/bin/activate
pip install -e './bridge[dev]'
export HERMES_3DS_BRIDGE_AUTH_TOKEN='change-me'
hermes-3ds-bridge
```

The API endpoints for health and chat are the next step.
