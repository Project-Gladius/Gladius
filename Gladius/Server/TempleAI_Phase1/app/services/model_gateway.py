from abc import ABC, abstractmethod

from app.schemas import ModelReply, TempleRequest


class ModelGatewayError(RuntimeError):
    """Base error for provider configuration, transport, and response failures."""


class ModelGatewayConfigurationError(ModelGatewayError):
    """Raised when the provider cannot start because configuration is missing."""


class ModelGateway(ABC):
    @abstractmethod
    def generate_reply(self, request: TempleRequest) -> ModelReply:
        raise NotImplementedError

