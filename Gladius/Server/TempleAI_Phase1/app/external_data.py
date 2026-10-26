"""Shared editable data in development and staged Windows builds."""
import os
from pathlib import Path

def external_data_dir() -> Path:
    configured = os.getenv("GLADIUS_EXTERNAL_DATA_DIR", "").strip()
    if configured:
        return Path(configured).expanduser().resolve()
    return Path(__file__).resolve().parents[3] / "Content" / "ExternalData"
