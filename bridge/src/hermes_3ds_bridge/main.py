from __future__ import annotations

import uvicorn

from hermes_3ds_bridge.app import create_app
from hermes_3ds_bridge.config import BridgeSettings



def run() -> None:
    settings = BridgeSettings.from_env()
    app = create_app(settings)
    uvicorn.run(app, host=settings.host, port=settings.port)
