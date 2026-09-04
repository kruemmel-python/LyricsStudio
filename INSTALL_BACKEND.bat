@echo off
setlocal
cd /d "%~dp0"
echo.
echo Klanggeist Lyrics Studio - Whisper Backend
echo ===========================================
echo.
py -3.12 -m pip install --upgrade pip
py -3.12 -m pip install -r requirements.txt
echo.
echo Backend installiert.
pause
