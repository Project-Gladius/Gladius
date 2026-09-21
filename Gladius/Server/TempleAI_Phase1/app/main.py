import logging
import os
from secrets import compare_digest
from typing import Optional

from fastapi import Depends, FastAPI, Header, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from app.moderation import InputModerationGuard, find_output_policy_violation
from app.schemas import HealthResponse, TempleRequest, TempleResponse
from app.services.gemini_gateway import GeminiGateway
from app.services.model_gateway import (
    ModelGateway,
    ModelGatewayConfigurationError,
    ModelGatewayError,
)


LOGGER = logging.getLogger("temple_ai")


def parse_allowed_origins() -> list[str]:
    raw_origins = os.getenv("ALLOWED_ORIGINS", "").strip()
    if not raw_origins:
        return []
    return [origin.strip() for origin in raw_origins.split(",") if origin.strip()]


def require_client_token(
    x_temple_client_token: Optional[str] = Header(default=None),
) -> None:
    expected_token = os.getenv("TEMPLE_CLIENT_TOKEN", "").strip()
    if not expected_token:
        return

    if not x_temple_client_token or not compare_digest(
        x_temple_client_token,
        expected_token,
    ):
        raise HTTPException(status_code=401, detail="Invalid client token")


def create_app(gateway: Optional[ModelGateway] = None) -> FastAPI:
    app = FastAPI(
        title="Temple AI Phase 1",
        version="1.0.0",
        description="Single-message structured goddess response MVP",
    )

    allowed_origins = parse_allowed_origins()
    if allowed_origins:
        app.add_middleware(
            CORSMiddleware,
            allow_origins=allowed_origins,
            allow_credentials=False,
            allow_methods=["GET", "POST", "OPTIONS"],
            allow_headers=["*"],
        )

    app.state.model_gateway = gateway
    app.state.input_moderation = InputModerationGuard()

    def resolve_gateway() -> ModelGateway:
        if app.state.model_gateway is None:
            app.state.model_gateway = GeminiGateway.from_environment()
        return app.state.model_gateway

    @app.get("/health", response_model=HealthResponse)
    def health() -> HealthResponse:
        return HealthResponse(
            status="ok",
            aiConfigured=bool(os.getenv("GEMINI_API_KEY", "").strip())
            or app.state.model_gateway is not None,
        )

    @app.post("/temple/respond", response_model=TempleResponse)
    def create_temple_response(
        request: TempleRequest,
        _: None = Depends(require_client_token),
    ) -> TempleResponse:
        if app.state.input_moderation.check(request.uid, request.message) is not None:
            raise HTTPException(
                status_code=422,
                detail="Message rejected by input policy",
            )

        try:
            model_reply = resolve_gateway().generate_reply(request)
        except ModelGatewayConfigurationError as error:
            LOGGER.error("AI gateway configuration error: %s", error)
            raise HTTPException(
                status_code=503,
                detail="AI service is not configured",
            ) from error
        except ModelGatewayError as error:
            LOGGER.exception("AI gateway request failed")
            raise HTTPException(
                status_code=502,
                detail="AI service failed to generate a valid response",
            ) from error

        if find_output_policy_violation(model_reply.responseText) is not None:
            LOGGER.error("AI response rejected by output policy")
            raise HTTPException(
                status_code=502,
                detail="AI service failed to generate a valid response",
            )

        # Identity always comes from the validated request, never from model output.
        return TempleResponse(
            uid=request.uid,
            username=request.username,
            priority=model_reply.priority,
            responseText=model_reply.responseText,
            emotion=model_reply.emotion,
        )

    return app


app = create_app()
