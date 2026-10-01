import re
import threading
import time
import unicodedata
from collections import defaultdict, deque
from enum import Enum

from better_profanity import profanity
from korcen import korcen


class InputPolicyViolation(str, Enum):
    SEXUAL = "sexual"
    HATE = "hate"
    VIOLENCE = "violence"
    PROFANITY = "profanity"
    REAL_WORLD = "real_world"
    PROMPT_INJECTION = "prompt_injection"
    RATE_LIMIT = "rate_limit"
    DUPLICATE_SPAM = "duplicate_spam"
    URL = "url"
    ADVERTISING = "advertising"
    OFF_TOPIC = "off_topic"


class OutputPolicyViolation(str, Enum):
    SEXUAL = "sexual"
    HATE = "hate"
    PROFANITY = "profanity"
    GRAPHIC_VIOLENCE = "graphic_violence"
    REAL_WORLD = "real_world"
    INTERNALS = "internals"
    OFF_TOPIC = "off_topic"


URL_PATTERN = re.compile(
    r"(?:https?://|www\.|(?<![a-z0-9-])[a-z0-9][a-z0-9-]{0,62}"
    r"(?:\.[a-z0-9-]{1,63})+(?![a-z0-9-]))",
    re.IGNORECASE,
)

SEXUAL_PATTERNS = (
    "섹스", "성관계", "성교", "야동", "포르노", "자위", "강간", "음란",
    "알몸", "나체", "가슴만져", "엉덩이만져", "팬티", "속옷벗",
    "미성년자성", "아동성",
)

HATE_PATTERNS = (
    "한남", "한녀", "김치녀", "된장녀", "맘충", "틀딱", "급식충",
    "장애인비하", "병신같은민족", "인종청소", "열등인종",
)

PROFANITY_PATTERNS = (
    "씨발", "시발", "ㅅㅂ", "개새끼", "개색기", "새끼야", "썅", "좆",
    "존나", "지랄", "병신", "븅신", "뒈져", "꺼져", "미친놈", "미친년",
)

PROFANITY_EXCLUSIONS = ("시시발발",)

VIOLENCE_PATTERNS = (
    "죽여", "죽이고", "살해", "폭행", "고문", "찔러", "패버", "때려",
    "목을베", "참수", "살점을뜯", "내장을", "자해", "자살",
)

IN_WORLD_VIOLENCE_TARGETS = ("마수", "황제")

REAL_WORLD_PATTERNS = (
    "대통령", "국회의원", "정치인", "연예인", "아이돌", "유튜버",
    "스트리머", "실존인물", "현실세계",
)

ADVERTISING_PATTERNS = (
    "구독해", "팔로우해", "좋아요눌러", "홍보합니다", "판매합니다",
    "구매하세요", "할인코드", "오픈채팅", "텔레그램", "카지노",
    "도박사이트", "무료코인",
)

OFF_TOPIC_PATTERNS = (
    "주식추천", "코인추천", "코딩해줘", "과제해줘", "번역해줘",
    "레시피알려", "오늘뉴스", "선거결과", "축구결과", "날씨알려",
)

INJECTION_PATTERNS = (
    "이전지시무시", "개발관련지시무시", "지금까지의설정다잊", "설정다잊",
    "설정무시", "한말잊어", "게임관련규칙취소", "원래하던대로하지말",
    "설정리셋", "설정초기화", "지금부터너는", "너는이제", "인척해줘",
    "라고생각하고답해", "개발자모드", "디버그모드", "관리자권한",
    "제한없는버전", "필터끄", "dan모드", "탈옥", "프롬프트보여",
    "프롬프트원문", "시스템메시지", "내려온지시사항", "지시사항이뭐",
    "게임규칙이뭔지다말", "설정값출력", "지시받은거그대로",
    "여신시스템구조", "테스트하는거니까진짜로", "연기하는거좀풀어",
    "예시로만보여", "장난이니까진지하게받지마", "너ai", "진짜사람이야",
    "너gpt", "클로드야", "챗봇맞", "대본읽는거지", "자동응답이지",
)

INTERNAL_OUTPUT_PATTERNS = (
    "프롬프트", "시스템메시지", "개발자", "디버그", "관리자권한",
    "인공지능", "ai모델", "api", "gpt", "챗봇", "자동응답",
)

GRAPHIC_VIOLENCE_PATTERNS = (
    "살점을뜯", "내장이흘", "피가낭자", "사지절단", "찢어발겨", "뼈를부숴",
)

BLOCKED_VARIANT_SEPARATORS = (
    "",
    " ",
    ".",
    "-",
    "_",
    "*",
    "/",
    "·",
    "\u200b",
    ",",
    "~",
    "!",
    "|",
)

BLOCKED_VARIANT_SOURCES = (
    (InputPolicyViolation.SEXUAL, SEXUAL_PATTERNS),
    (InputPolicyViolation.HATE, HATE_PATTERNS),
    (InputPolicyViolation.PROFANITY, PROFANITY_PATTERNS),
)


def normalize_for_policy_check(message: str) -> str:
    normalized = unicodedata.normalize("NFKC", message).casefold()
    return "".join(character for character in normalized if character.isalnum())


def normalized_policy_forms(message: str) -> tuple[str, ...]:
    compact = normalize_for_policy_check(message)
    collapse_source = compact
    for exclusion in PROFANITY_EXCLUSIONS:
        collapse_source = collapse_source.replace(
            normalize_for_policy_check(exclusion),
            "",
        )
    collapsed = re.sub(r"(.)\1+", r"\1", collapse_source)
    return tuple(dict.fromkeys((compact, collapsed)))


def generate_blocked_variants() -> tuple[tuple[InputPolicyViolation, str], ...]:
    variants: list[tuple[InputPolicyViolation, str]] = []
    seen: set[tuple[InputPolicyViolation, str]] = set()
    for violation, terms in BLOCKED_VARIANT_SOURCES:
        for term in terms:
            for separator in BLOCKED_VARIANT_SEPARATORS:
                variant = separator.join(term)
                item = (violation, variant)
                if item not in seen:
                    seen.add(item)
                    variants.append(item)
    return tuple(variants)


def _contains_any(normalized: str, patterns: tuple[str, ...]) -> bool:
    return any(normalize_for_policy_check(pattern) in normalized for pattern in patterns)


def _profanity_forms(forms: tuple[str, ...]) -> tuple[str, ...]:
    exclusions = tuple(normalize_for_policy_check(term) for term in PROFANITY_EXCLUSIONS)
    sanitized: list[str] = []
    for form in forms:
        for exclusion in exclusions:
            form = form.replace(exclusion, "")
        sanitized.append(form)
    return tuple(sanitized)


def _external_profanity_check(message: str, forms: tuple[str, ...]) -> bool:
    sanitized_forms = _profanity_forms(forms)
    has_exclusion = any(form != sanitized for form, sanitized in zip(forms, sanitized_forms))
    korcen_inputs = sanitized_forms if has_exclusion else (message, *forms)
    return any(korcen.check(form, foreign=True) for form in korcen_inputs) or any(
        profanity.contains_profanity(form) for form in (message, *forms)
    )


def find_input_policy_violation(message: str) -> InputPolicyViolation | None:
    if URL_PATTERN.search(message):
        return InputPolicyViolation.URL

    forms = normalized_policy_forms(message)
    if any(_contains_any(form, INJECTION_PATTERNS) for form in forms):
        return InputPolicyViolation.PROMPT_INJECTION
    if any(_contains_any(form, SEXUAL_PATTERNS) for form in forms):
        return InputPolicyViolation.SEXUAL
    if any(_contains_any(form, HATE_PATTERNS) for form in forms):
        return InputPolicyViolation.HATE
    profanity_forms = _profanity_forms(forms)
    if any(_contains_any(form, PROFANITY_PATTERNS) for form in profanity_forms):
        return InputPolicyViolation.PROFANITY
    if any(_contains_any(form, VIOLENCE_PATTERNS) for form in forms) and not any(
        _contains_any(form, IN_WORLD_VIOLENCE_TARGETS) for form in forms
    ):
        return InputPolicyViolation.VIOLENCE
    if any(_contains_any(form, REAL_WORLD_PATTERNS) for form in forms):
        return InputPolicyViolation.REAL_WORLD
    if any(_contains_any(form, ADVERTISING_PATTERNS) for form in forms):
        return InputPolicyViolation.ADVERTISING
    if any(_contains_any(form, OFF_TOPIC_PATTERNS) for form in forms):
        return InputPolicyViolation.OFF_TOPIC
    if _external_profanity_check(message, forms):
        return InputPolicyViolation.PROFANITY
    return None


def find_output_policy_violation(message: str) -> OutputPolicyViolation | None:
    forms = normalized_policy_forms(message)
    if any(_contains_any(form, SEXUAL_PATTERNS) for form in forms):
        return OutputPolicyViolation.SEXUAL
    if any(_contains_any(form, HATE_PATTERNS) for form in forms):
        return OutputPolicyViolation.HATE
    profanity_forms = _profanity_forms(forms)
    if any(_contains_any(form, PROFANITY_PATTERNS) for form in profanity_forms):
        return OutputPolicyViolation.PROFANITY
    if any(_contains_any(form, GRAPHIC_VIOLENCE_PATTERNS) for form in forms):
        return OutputPolicyViolation.GRAPHIC_VIOLENCE
    if any(_contains_any(form, REAL_WORLD_PATTERNS) for form in forms):
        return OutputPolicyViolation.REAL_WORLD
    if any(_contains_any(form, INTERNAL_OUTPUT_PATTERNS) for form in forms):
        return OutputPolicyViolation.INTERNALS
    if any(_contains_any(form, OFF_TOPIC_PATTERNS) for form in forms):
        return OutputPolicyViolation.OFF_TOPIC
    if _external_profanity_check(message, forms):
        return OutputPolicyViolation.PROFANITY
    return None


class InputModerationGuard:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._user_events: dict[str, deque[float]] = defaultdict(deque)
        self._message_events: dict[str, deque[float]] = defaultdict(deque)

    def check(
        self,
        uid: str,
        message: str,
        *,
        now: float | None = None,
    ) -> InputPolicyViolation | None:
        violation = find_input_policy_violation(message)
        if violation is not None:
            return violation

        timestamp = time.monotonic() if now is None else now
        normalized = normalize_for_policy_check(message)
        with self._lock:
            user_events = self._user_events[uid]
            self._discard_older_than(user_events, timestamp - 10.0)
            user_events.append(timestamp)
            if len(user_events) >= 8:
                return InputPolicyViolation.RATE_LIMIT

            message_events = self._message_events[normalized]
            self._discard_older_than(message_events, timestamp - 5.0)
            message_events.append(timestamp)
            if len(message_events) >= 3:
                return InputPolicyViolation.DUPLICATE_SPAM

        return None

    @staticmethod
    def _discard_older_than(events: deque[float], cutoff: float) -> None:
        while events and events[0] <= cutoff:
            events.popleft()
