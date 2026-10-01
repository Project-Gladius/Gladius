import pytest
from pydantic import ValidationError

from app.schemas import Emotion, ModelReply, TempleRequest


def test_request_strips_surrounding_whitespace() -> None:
    request = TempleRequest(
        uid="  user-1  ",
        username="  별빛여행자  ",
        message="  여신님, 들리시나요?  ",
    )

    assert request.uid == "user-1"
    assert request.username == "별빛여행자"
    assert request.message == "여신님, 들리시나요?"


def test_request_rejects_blank_message() -> None:
    with pytest.raises(ValidationError):
        TempleRequest(uid="user-1", username="tester", message="   ")


def test_model_reply_rejects_unknown_emotion() -> None:
    with pytest.raises(ValidationError):
        ModelReply(
            priority=50,
            responseText="응답",
            emotion="invented-emotion",
        )


def test_model_reply_accepts_allowed_emotion() -> None:
    result = ModelReply(
        priority=100,
        responseText="그대의 목소리를 들었노라.",
        emotion=Emotion.NEUTRAL,
    )

    assert result.emotion == Emotion.NEUTRAL
