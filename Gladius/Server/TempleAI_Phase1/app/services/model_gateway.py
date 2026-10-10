from abc import ABC, abstractmethod

from app.schemas import ModelReply, TempleBatchRequest, TempleRequest


class ModelGatewayError(RuntimeError):
    """Base error for provider configuration, transport, and response failures."""


class ModelGatewayConfigurationError(ModelGatewayError):
    """Raised when the provider cannot start because configuration is missing."""


class ModelGateway(ABC):
    @abstractmethod
    def generate_reply(self, request: TempleRequest) -> ModelReply:
        raise NotImplementedError

    def generate_batch_replies(
        self,
        request: TempleBatchRequest,
    ) -> list[ModelReply]:
        return [
            self.generate_reply(
                TempleRequest(
                    uid=item.uid,
                    username=item.username,
                    message=item.message,
                )
            )
            for item in request.messages
        ]

