@echo off
setlocal

set "SCRIPT_DIR=%~dp0."

call "%SCRIPT_DIR%\run.bat" Release --experimental-game-udp-sync %*
exit /b %ERRORLEVEL%
