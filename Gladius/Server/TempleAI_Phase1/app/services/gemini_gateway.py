import os

from pydantic import ValidationError

from app.prompts import build_temple_prompt
from app.schemas import ModelReply, TempleRequest
from app.services.model_gateway import (
    ModelGateway,
    ModelGatewayConfigurationError,
    ModelGatewayError,
)


class GeminiGateway(ModelGateway):
    def __init__(self, api_key: str, model: str) -> None:
        if not api_key:
            raise ModelGatewayConfigurationError(
                "GEMINI_API_KEY environment variable is missing"
            )

        try:
            from google import genai
        except ImportError as error:
            raise ModelGatewayConfigurationError(
                "google-genai package is not installed"
            ) from error

        self._client = genai.Client(api_key=api_key)
        self._model = model

    @classmethod
    def from_environment(cls) -> "GeminiGateway":
        return cls(
            api_key=os.getenv("GEMINI_API_KEY", "").strip(),
            model=os.getenv("GEMINI_MODEL", "gemini-3.7-flash").strip(),
        )

    def generate_reply(self, request: TempleRequest) -> ModelReply:
        try:
            interaction = self._client.interactions.create(
                model=self._model,
                input=build_temple_prompt(request),
                response_format={
                    "type": "text",
                    "mime_type": "application/json",
                    "schema": ModelReply.model_json_schema(),
                },
            )
            return ModelReply.model_validate_json(interaction.output_text)
        except ValidationError as error:
            raise ModelGatewayError("AI returned an invalid response shape") from error
        except ModelGatewayError:
            raise
        except Exception as error:
            raise ModelGatewayError("Gemini request failed") from error

