import logging
import os
from secrets import compare_digest
from threading import Lock
from typing import Optional

from dotenv import load_dotenv
from app.external_data import external_data_dir

load_dotenv(external_data_dir() / "TempleAI.env", override=False)

from fastapi import Depends, FastAPI, Header, HTTPException
from fastapi.middleware.cors import CORSMiddleware

from app.moderation import InputModerationGuard, find_output_policy_violation
from app.schemas import (
    HealthResponse,
    TempleBatchItemResponse,
    TempleBatchRequest,
    TempleBatchResponse,
    TempleRequest,
    TempleResponse,
)
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
        description="Structured single and batched goddess responses",
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
    app.state.batch_results = {}
    app.state.batch_lock = Lock()

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

    @app.post("/temple/respond-batch", response_model=TempleBatchResponse)
    def create_temple_batch_response(
        request: TempleBatchRequest,
        _: None = Depends(require_client_token),
    ) -> TempleBatchResponse:
        fingerprint = request.model_dump_json()

        # Serialize batch generation so a retry cannot trigger duplicate AI billing.
        with app.state.batch_lock:
            cached = app.state.batch_results.get(request.batchId)
            if cached is not None:
                cached_fingerprint, cached_response = cached
                if cached_fingerprint != fingerprint:
                    raise HTTPException(
                        status_code=409,
                        detail="batchId was already used with different content",
                    )
                return cached_response

            accepted_messages = []
            # The same player may send several messages per batch; the prompt lowers
            # the priority of their repeats instead of rejecting them.
            seen_uids: set[str] = set()
            for message in request.messages:
                if message.uid in seen_uids:
                    continue
                seen_uids.add(message.uid)
                if (
                    app.state.input_moderation.check(
                        message.userId,
                        message.message,
                    )
                    is not None
                ):
                    continue
                accepted_messages.append(message)

            if not accepted_messages:
                raise HTTPException(status_code=422, detail="No acceptable messages")

            filtered_request = request.model_copy(
                update={"messages": accepted_messages}
            )
            try:
                model_replies = resolve_gateway().generate_batch_replies(
                    filtered_request
                )
            except ModelGatewayConfigurationError as error:
                LOGGER.error("AI gateway configuration error: %s", error)
                raise HTTPException(
                    status_code=503,
                    detail="AI service is not configured",
                ) from error
            except ModelGatewayError as error:
                LOGGER.exception("AI gateway batch request failed")
                raise HTTPException(
                    status_code=502,
                    detail="AI service failed to generate valid responses",
                ) from error

            if len(model_replies) != len(accepted_messages):
                LOGGER.error("AI batch response count did not match request count")
                raise HTTPException(
                    status_code=502,
                    detail="AI service failed to generate valid responses",
                )

            public_responses = []
            for message, reply in zip(accepted_messages, model_replies):
                if find_output_policy_violation(reply.responseText) is not None:
                    LOGGER.error("AI batch item rejected by output policy")
                    raise HTTPException(
                        status_code=502,
                        detail="AI service failed to generate valid responses",
                    )
                public_responses.append(
                    TempleBatchItemResponse(
                        uid=message.uid,
                        username=message.username,
                        priority=reply.priority,
                        responseText=reply.responseText,
                        emotion=reply.emotion,
                    )
                )

            response = TempleBatchResponse(
                schemaVersion=1,
                batchId=request.batchId,
                responses=public_responses,
                acceptedCount=len(public_responses),
                rejectedCount=len(request.messages) - len(public_responses),
            )
            app.state.batch_results[request.batchId] = (fingerprint, response)
            return response

    return app


app = create_app()
