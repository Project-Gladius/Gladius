import hashlib
from dataclasses import dataclass

from app.moderation import normalize_for_policy_check
from app.prompts import build_user_message_orders
from app.schemas import Emotion, ModelReply, TempleBatchRequest, TempleRequest


@dataclass(frozen=True)
class FallbackRule:
    terms: tuple[str, ...]
    priority: int
    emotion: Emotion
    responses: tuple[str, ...]


FALLBACK_RULES = (
    FallbackRule(
        terms=("조심", "주의", "경계", "살아남", "생존"),
        priority=85,
        emotion=Emotion.NEUTRAL,
        responses=(
            "승리보다 먼저 퇴로를 살피거라. 지친 몸으로 무리하게 추격하지 말거라.",
            "마수의 움직임보다 그대의 호흡을 먼저 살피거라. 조급함이 가장 큰 적이란다.",
            "눈앞의 기회에 마음을 빼앗기지 말거라. 살아 돌아오는 것이 먼저란다.",
        ),
    ),
    FallbackRule(
        terms=("강화", "무기", "공격력", "체력", "치명타", "선택"),
        priority=80,
        emotion=Emotion.NEUTRAL,
        responses=(
            "그대가 자주 무너지는 순간을 돌아보거라. 부족한 곳을 메우는 힘이 먼저란다.",
            "강한 일격보다 살아남을 힘이 필요할 때도 있단다. 그대의 약점을 먼저 보거라.",
            "선택은 그대의 몫이란다. 다음 싸움에서 가장 두려운 순간을 기준으로 삼거라.",
        ),
    ),
    FallbackRule(
        terms=("누구", "이름", "정체"),
        priority=75,
        emotion=Emotion.NEUTRAL,
        responses=(
            "나는 인간의 모신 아우렐리아란다. 지친 그대가 쉬어갈 자리를 지키고 있지.",
            "나의 이름은 아우렐리아. 오래도록 인간의 성장과 상처를 지켜본 모신이란다.",
        ),
    ),
    FallbackRule(
        terms=("왜안싸", "직접싸", "나갈수", "갇혔", "묶였", "신전밖"),
        priority=90,
        emotion=Emotion.SORROWFUL,
        responses=(
            "제국의 결계가 나를 이 신전에 묶어 두었단다. 마음만은 그대와 함께하리라.",
            "나 또한 그대 곁에 서고 싶구나. 하지만 이 신전의 문턱을 넘을 수 없단다.",
        ),
    ),
    FallbackRule(
        terms=("제국", "황제", "배신", "원망"),
        priority=85,
        emotion=Emotion.UPSET,
        responses=(
            "제국의 선택은 나를 아프게 했단다. 그래도 그들 역시 내가 품은 인간이란다.",
            "나를 묶은 제국을 원망하면서도 미워할 수는 없구나. 그것이 어미의 벌인가 보다.",
            "제국은 인간을 지켰고 또한 상처 입혔단다. 어느 한쪽만이 진실은 아니란다.",
        ),
    ),
    FallbackRule(
        terms=("콜로세움", "검투", "경기장", "관중"),
        priority=90,
        emotion=Emotion.SORROWFUL,
        responses=(
            "그대가 청해야 할 것은 더 강한 힘보다 싸우지 않아도 되는 내일일지 모른단다.",
            "모래 위의 싸움은 백성이 전쟁을 잊기 위해 만들어졌단다. 그 짐을 그대가 지고 있구나.",
        ),
    ),
    FallbackRule(
        terms=("파토스", "대전쟁", "마족", "마신", "악시움"),
        priority=80,
        emotion=Emotion.SORROWFUL,
        responses=(
            "파토스의 피로가 악시움을 긴 전쟁으로 몰아넣었단다. 그 상처는 아직 끝나지 않았지.",
            "인간과 마족은 너무 오래 서로를 잃어 왔단다. 승리만으로는 그 상처가 낫지 않는구나.",
        ),
    ),
    FallbackRule(
        terms=("어머니", "모신", "인간을사랑", "인간이좋"),
        priority=80,
        emotion=Emotion.PLEASED,
        responses=(
            "스스로 배우고 다시 일어서는 인간의 모습을 사랑했단다. 그래서 나는 어머니가 되었지.",
            "약하면서도 실패에서 배우는 인간은 아름다웠단다. 그 성장을 지키고 싶었지.",
        ),
    ),
    FallbackRule(
        terms=("과거", "기억", "옛날", "처음"),
        priority=75,
        emotion=Emotion.SORROWFUL,
        responses=(
            "돌을 다시 깎으며 스스로 배우던 인간을 기억한단다. 작은 성장이 내 마음을 움직였지.",
            "가진 것이 없어도 가장 좋은 것을 내밀던 인간들을 기억한단다. 참으로 그립구나.",
        ),
    ),
    FallbackRule(
        terms=("죽은", "죽음", "떠난", "그리워", "동료"),
        priority=85,
        emotion=Emotion.SORROWFUL,
        responses=(
            "떠나간 이들을 나 또한 기억한단다. 그 마음을 홀로 품지 말거라.",
            "그들의 이름을 모두 알지는 못해도 희생을 잊지는 않는단다. 잠시 함께 애도하자꾸나.",
        ),
    ),
    FallbackRule(
        terms=("힘들", "아프", "상처", "지쳤", "피곤"),
        priority=80,
        emotion=Emotion.SORROWFUL,
        responses=(
            "그대가 견딘 고통을 알고 있단다. 이곳에서 잠시 쉬어가거라.",
            "상처 입은 몸으로 강한 척하지 않아도 된단다. 지금은 숨을 고르거라.",
            "오늘의 싸움은 끝났단다. 무거운 짐을 잠시 내 앞에 내려놓거라.",
        ),
    ),
    FallbackRule(
        terms=("두려", "무서", "겁나", "불안"),
        priority=85,
        emotion=Emotion.SORROWFUL,
        responses=(
            "두려움은 살아 돌아오고 싶은 마음의 증거란다. 부끄러워하지 말거라.",
            "용기란 두려움이 없는 것이 아니란다. 두려워도 한 걸음 내딛는 것이지.",
        ),
    ),
    FallbackRule(
        terms=("전투", "싸움", "마수", "이길", "승리"),
        priority=80,
        emotion=Emotion.NEUTRAL,
        responses=(
            "승리를 장담할 수는 없단다. 호흡을 지키고 무리한 일격을 삼가거라.",
            "마수의 힘보다 빈틈을 살피거라. 살아 돌아온 뒤의 싸움도 남아 있단다.",
            "동료의 움직임을 믿고 홀로 앞서지 말거라. 함께 돌아오는 것이 진정한 승리란다.",
        ),
    ),
    FallbackRule(
        terms=("아름답", "아름다", "예쁘", "감사", "고마", "좋아"),
        priority=70,
        emotion=Emotion.PLEASED,
        responses=(
            "그대의 따뜻한 마음을 기쁘게 받으마. 무사히 돌아와 다행이구나.",
            "그리 말해 주니 오래된 신전에도 온기가 도는구나. 고맙단다.",
        ),
    ),
    FallbackRule(
        terms=("들리", "목소리", "계세요", "안녕", "반가"),
        priority=65,
        emotion=Emotion.PLEASED,
        responses=(
            "잘 들리고 있단다. 무사히 내게 돌아온 그대의 목소리가 반갑구나.",
            "나는 이곳에 있단다. 그대의 목소리도 마음도 모두 닿고 있구나.",
        ),
    ),
)

BLESSING_RESPONSES = (
    "그대의 상처가 온전히 아물기를. 다시 일어설 힘을 내리노라.",
    "그대의 생명이 더욱 굳세어지기를. 내 축복이 함께하리라.",
    "그대의 손끝이 흔들리지 않기를. 단단한 기본기를 축복하노라.",
    "결정적인 순간 그대의 일격이 빛나기를. 내 힘을 보태리라.",
)

DEFAULT_RESPONSES = (
    "그 물음은 가볍지 않구나. 그대가 무엇을 두려워하는지 먼저 들려주겠니.",
    "답을 서두르지 말거라. 그대가 처한 일을 조금 더 들려준다면 함께 헤아려 보마.",
    "그대의 뜻을 함부로 짐작하고 싶지 않구나. 마음속 이야기를 조금 더 들려주거라.",
)


def _choose(message: str, responses: tuple[str, ...]) -> str:
    digest = hashlib.blake2s(message.encode("utf-8"), digest_size=2).digest()
    index = int.from_bytes(digest, "big") % len(responses)
    return responses[index]


def build_fallback_reply(request: TempleRequest) -> ModelReply:
    message = normalize_for_policy_check(request.message)

    if message.startswith("축복") or request.message.lstrip().startswith("!축복"):
        return ModelReply(
            priority=90,
            responseText=_choose(message, BLESSING_RESPONSES),
            emotion=Emotion.BESTOW,
        )

    for rule in FALLBACK_RULES:
        if any(term in message for term in rule.terms):
            return ModelReply(
                priority=rule.priority,
                responseText=_choose(message, rule.responses),
                emotion=rule.emotion,
            )

    return ModelReply(
        priority=60,
        responseText=_choose(message, DEFAULT_RESPONSES),
        emotion=Emotion.NEUTRAL,
    )


# Priority removed for each repeated message from the same player in one batch,
# mirroring the repeated-player rule in the batch prompt.
REPEAT_PRIORITY_PENALTY = 20


def build_batch_fallback_replies(request: TempleBatchRequest) -> list[ModelReply]:
    replies: list[ModelReply] = []
    for item, order in zip(request.messages, build_user_message_orders(request)):
        reply = build_fallback_reply(
            TempleRequest(
                uid=item.uid,
                username=item.username,
                message=item.message,
            )
        )
        if order > 1:
            penalty = REPEAT_PRIORITY_PENALTY * (order - 1)
            reply = reply.model_copy(
                update={"priority": max(0, reply.priority - penalty)}
            )
        replies.append(reply)
    return replies
