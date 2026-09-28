import pytest

from app.schemas import Emotion, TempleRequest
from app.services.fallback_reply import build_fallback_reply


@pytest.mark.parametrize(
    ("message", "emotion"),
    [
        ("!축복 전투에서 살아남고 싶어요", Emotion.BESTOW),
        ("오늘 전투가 너무 힘들었어요", Emotion.SORROWFUL),
        ("떠난 동료가 그리워요", Emotion.SORROWFUL),
        ("여신님은 아름다우세요", Emotion.PLEASED),
        ("제 목소리가 들리시나요?", Emotion.PLEASED),
    ],
)
def test_fallback_reply_is_safe_and_contextual(
    message: str,
    emotion: Emotion,
) -> None:
    reply = build_fallback_reply(
        TempleRequest(uid="u-1", username="tester", message=message)
    )

    assert reply.emotion == emotion
    assert len(reply.responseText) <= 70


def test_different_questions_receive_contextual_answers() -> None:
    questions = (
        "제가 뭘 조심해야 할까요?",
        "왜 신전 밖으로 나갈 수 없나요?",
        "제국을 원망하시나요?",
        "왜 인간의 어머니가 되셨나요?",
        "오늘 전투가 너무 힘들었어요.",
    )
    replies = {
        build_fallback_reply(
            TempleRequest(uid=str(index), username="tester", message=question)
        ).responseText
        for index, question in enumerate(questions)
    }

    assert len(replies) == len(questions)
    assert "목소리를 들었노라" not in replies
