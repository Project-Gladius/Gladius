import pytest

@pytest.fixture(autouse=True)
def isolate_operator_token(monkeypatch):
    # Unit tests must not inherit the real operator's authentication requirement.
    monkeypatch.setenv('TEMPLE_CLIENT_TOKEN','')
