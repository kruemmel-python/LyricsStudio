@echo off
setlocal
cd /d "%~dp0"

rem MSYS2's CMake can appear first on PATH, but it does not provide the
rem Visual Studio generator. Prefer the native Kitware installation when
rem it is available and verify the selected executable before configuring.
set "CMAKE_EXE=cmake"
if exist "%ProgramFiles%\CMake\bin\cmake.exe" set "CMAKE_EXE=%ProgramFiles%\CMake\bin\cmake.exe"

"%CMAKE_EXE%" --help 2>nul | findstr /C:"Visual Studio 17 2022" >nul
if errorlevel 1 goto :cmake_fail

if not exist build mkdir build
"%CMAKE_EXE%" -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail
"%CMAKE_EXE%" --build build --config Release --target KlanggeistLyricsStudio
if errorlevel 1 goto :fail
"%CMAKE_EXE%" --build build --config Release --target KlanggeistVideoSmoke
if errorlevel 1 goto :fail
"%CMAKE_EXE%" --build build --config Release --target KlanggeistExportControllerSmoke
if errorlevel 1 goto :fail
"%CMAKE_EXE%" --build build --config Release --target KlanggeistLyricsRefreshSmoke
if errorlevel 1 goto :fail
"build\Release\KlanggeistLyricsRefreshSmoke.exe"
if errorlevel 1 goto :fail
echo.
echo Strikter Build erfolgreich: Hauptprogramm + Smoke-Test-Binaries, /W4 + /WX.
echo Fertig: build\Release\KlanggeistLyricsStudio.exe
exit /b 0

:cmake_fail
echo.
echo Kein natives CMake mit Visual-Studio-2022-Unterstuetzung gefunden.
echo Installiere CMake fuer Windows von https://cmake.org/download/
echo und aktiviere beim Setup "Add CMake to the system PATH".
pause
exit /b 1

:fail
echo.
echo Build fehlgeschlagen.
pause
exit /b 1
