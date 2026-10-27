@echo off
cd /d "%~dp0"
if not exist "%~dp0Python\python.exe" (
 echo Bundled Python runtime is missing.
 exit /b 1
)
"%~dp0Python\python.exe" "%~dp0start_server.py"
