from __future__ import annotations

from fastapi import FastAPI

from hermes_3ds_bridge.config import BridgeSettings



def create_app(settings: BridgeSettings | None = None) -> FastAPI:
    resolved_settings = settings or BridgeSettings.from_env()

    app = FastAPI(
        title=resolved_settings.service_name,
        version=resolved_settings.version,
    )
    app.state.settings = resolved_settings

    @app.get("/")
    async def root() -> dict[str, str]:
        return {
            "service": resolved_settings.service_name,
            "status": "starting",
            "version": resolved_settings.version,
        }

    return app
