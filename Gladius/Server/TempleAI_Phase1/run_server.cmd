@echo off
setlocal
cd /d "%~dp0"

echo Starting TempleAI on this computer only...
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_server.ps1" -HostAddress 127.0.0.1 -Port 8765

set "EXIT_CODE=%ERRORLEVEL%"
if not "%EXIT_CODE%"=="0" echo TempleAI stopped with error code %EXIT_CODE%.
pause
exit /b %EXIT_CODE%
