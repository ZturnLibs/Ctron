@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0ctc.ps1" %*
exit /b %ERRORLEVEL%
