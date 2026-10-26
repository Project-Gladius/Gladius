@echo off
cd /d "%~dp0"
set "GLADIUS_EXTERNAL_DATA_DIR=%~dp0..\..\Content\ExternalData"
python -m uvicorn app.main:app --host 127.0.0.1 --port 8000
