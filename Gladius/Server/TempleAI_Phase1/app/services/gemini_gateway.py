import logging
import os
import time
from collections.abc import Callable
from typing import TypeVar

from pydantic import BaseModel

from app.prompts import build_temple_batch_prompt, build_temple_prompt
from app.schemas import ModelBatchReply, ModelReply, TempleBatchRequest, TempleRequest
from app.services.fallback_reply import (
    build_batch_fallback_replies,
    build_fallback_reply,
)
from app.services.model_gateway import (
    ModelGateway,
    ModelGatewayConfigurationError,
    ModelGatewayError,
)


LOGGER = logging.getLogger("temple_ai.gemini")
ResponseModel = TypeVar("ResponseModel", bound=BaseModel)


class GeminiGateway(ModelGateway):
    def __init__(
        self,
        api_key: str,
        models: tuple[str, ...],
        timeout_ms: int = 20_000,
        failure_cooldown_seconds: float = 60.0,
    ) -> None:
        if not api_key:
            raise ModelGatewayConfigurationError(
                "GEMINI_API_KEY environment variable is missing"
            )
        if not models:
            raise ModelGatewayConfigurationError("No Gemini model is configured")

        try:
            from google import genai
            from google.genai import types
        except ImportError as error:
            raise ModelGatewayConfigurationError(
                "google-genai package is not installed"
            ) from error

        retry_options = types.HttpRetryOptions(attempts=1)
        self._client = genai.Client(
            api_key=api_key,
            http_options=types.HttpOptions(
                timeout=timeout_ms,
                retry_options=retry_options,
            ),
        )
        self._types = types
        self._models = models
        self._timeout_ms = timeout_ms
        self._failure_cooldown_seconds = failure_cooldown_seconds
        self._model_retry_after: dict[str, float] = {}

    @classmethod
    def from_environment(cls) -> "GeminiGateway":
        primary_model = os.getenv("GEMINI_MODEL", "gemini-3.8-flash").strip()
        fallback_models = tuple(
            model.strip()
            for model in os.getenv(
                "GEMINI_FALLBACK_MODELS",
                "gemini-3.7-flash",
            ).split(",")
            if model.strip()
        )
        models = tuple(dict.fromkeys((primary_model, *fallback_models)))
        timeout_ms = int(os.getenv("GEMINI_TIMEOUT_MS", "20000"))
        failure_cooldown_seconds = float(
            os.getenv("GEMINI_FAILURE_COOLDOWN_SECONDS", "60")
        )
        return cls(
            api_key=os.getenv("GEMINI_API_KEY", "").strip(),
            models=models,
            timeout_ms=timeout_ms,
            failure_cooldown_seconds=failure_cooldown_seconds,
        )

    def generate_reply(self, request: TempleRequest) -> ModelReply:
        return self._generate_structured(
            prompt=build_temple_prompt(request),
            response_type=ModelReply,
            max_output_tokens=256,
            fallback=lambda: build_fallback_reply(request),
        )

    def generate_batch_replies(
        self,
        request: TempleBatchRequest,
    ) -> list[ModelReply]:
        batch_reply = self._generate_structured(
            prompt=build_temple_batch_prompt(request),
            response_type=ModelBatchReply,
            max_output_tokens=4096,
            expected_count=len(request.messages),
            fallback=lambda: ModelBatchReply(
                responses=build_batch_fallback_replies(request)
            ),
        )
        return batch_reply.responses

    def _generate_structured(
        self,
        *,
        prompt: str,
        response_type: type[ResponseModel],
        max_output_tokens: int,
        fallback: Callable[[], ResponseModel],
        expected_count: int | None = None,
    ) -> ResponseModel:
        last_error: Exception | None = None
        now = time.monotonic()

        for model in self._models:
            if self._model_retry_after.get(model, 0.0) > now:
                continue
            try:
                response = self._client.models.generate_content(
                    model=model,
                    contents=prompt,
                    config=self._types.GenerateContentConfig(
                        response_mime_type="application/json",
                        response_json_schema=response_type.model_json_schema(),
                        max_output_tokens=max_output_tokens,
                        thinking_config=self._types.ThinkingConfig(
                            thinking_level="low"
                        ),
                        automatic_function_calling=(
                            self._types.AutomaticFunctionCallingConfig(disable=True)
                        ),
                        http_options=self._types.HttpOptions(
                            timeout=self._timeout_ms,
                            retry_options=self._types.HttpRetryOptions(attempts=1),
                        ),
                    ),
                )
                if not response.text:
                    raise ModelGatewayError("Gemini returned an empty response")

                reply = response_type.model_validate_json(response.text)
                if (
                    expected_count is not None
                    and isinstance(reply, ModelBatchReply)
                    and len(reply.responses) != expected_count
                ):
                    raise ModelGatewayError(
                        "Gemini returned a different number of batch responses"
                    )
                self._model_retry_after.pop(model, None)
                LOGGER.info("Gemini response generated with model %s", model)
                return reply
            except Exception as error:
                last_error = error
                self._model_retry_after[model] = (
                    time.monotonic() + self._failure_cooldown_seconds
                )
                LOGGER.warning("Gemini model %s failed: %s", model, error)

        if last_error is not None:
            LOGGER.error("All Gemini models failed; using local safe fallback")
        else:
            LOGGER.warning("Gemini models are cooling down; using local safe fallback")
        return fallback()
