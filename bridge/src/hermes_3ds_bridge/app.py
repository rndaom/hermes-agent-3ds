from __future__ import annotations

from fastapi import FastAPI, Header
from fastapi.responses import JSONResponse

from hermes_3ds_bridge.auth import verify_bearer_token, verify_request_token
from hermes_3ds_bridge.config import BridgeSettings
from hermes_3ds_bridge.models import ChatRequest, ChatSuccessResponse, ErrorResponse, HealthResponse
from hermes_3ds_bridge.responder import HermesCLIResponder, Responder
from hermes_3ds_bridge.shaping import shape_reply



def create_app(settings: BridgeSettings | None = None, responder: Responder | None = None) -> FastAPI:
    resolved_settings = settings or BridgeSettings.from_env()
    resolved_responder = responder or HermesCLIResponder(resolved_settings)

    app = FastAPI(
        title=resolved_settings.service_name,
        version=resolved_settings.version,
    )
    app.state.settings = resolved_settings
    app.state.responder = resolved_responder

    @app.get("/")
    async def root() -> dict[str, str]:
        return {
            "service": resolved_settings.service_name,
            "status": "starting",
            "version": resolved_settings.version,
        }

    @app.get("/api/v1/health", response_model=HealthResponse)
    async def health() -> HealthResponse:
        return HealthResponse(
            service=resolved_settings.service_name,
            version=resolved_settings.version,
        )

    @app.post("/api/v1/chat", response_model=ChatSuccessResponse | ErrorResponse)
    async def chat(
        payload: ChatRequest,
        authorization: str | None = Header(default=None),
    ) -> ChatSuccessResponse | JSONResponse:
        authorized = verify_bearer_token(authorization, resolved_settings) or verify_request_token(
            payload.token,
            resolved_settings,
        )
        if not authorized:
            return JSONResponse(
                status_code=401,
                content=ErrorResponse(error="Unauthorized.").model_dump(),
            )

        if not payload.message.strip():
            return JSONResponse(
                status_code=400,
                content=ErrorResponse(error="Message is required.").model_dump(),
            )

        try:
            context: list[dict[str, str]] = [item.model_dump() for item in payload.context]
            reply = resolved_responder.generate_reply(
                message=payload.message.strip(),
                context=context,
                client=payload.client,
            )
        except Exception:
            return JSONResponse(
                status_code=502,
                content=ErrorResponse(error="Bridge request failed.").model_dump(),
            )

        if not reply.strip():
            return JSONResponse(
                status_code=502,
                content=ErrorResponse(error="Bridge request failed.").model_dump(),
            )

        shaped_reply, truncated = shape_reply(reply, max_chars=resolved_settings.max_reply_chars)
        return ChatSuccessResponse(reply=shaped_reply, truncated=truncated)

    return app
