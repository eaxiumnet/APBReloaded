@echo off
setlocal
title APB Reloaded Launcher
cd /d "%~dp0"

REM Double-click = Editor -game (real window + splash UI).
REM Args:
REM   Launch_APB.bat              auto -> editor (recommended)
REM   Launch_APB.bat editor       UnrealEditor -game frontend
REM   Launch_APB.bat game         raw Dev exe (often NO WINDOW - not recommended)
REM   Launch_APB.bat staged       packaged StagedBuilds client if cooked
REM   Launch_APB.bat fullscreen
REM   Launch_APB.bat editor 1280 720

set MODE=auto
set FS=
set W=1920
set H=1080

if /I "%~1"=="game" set MODE=game
if /I "%~1"=="editor" set MODE=editor
if /I "%~1"=="staged" set MODE=staged
if /I "%~1"=="auto" set MODE=auto
if /I "%~1"=="fullscreen" set FS=-Fullscreen
if /I "%~2"=="fullscreen" set FS=-Fullscreen
if not "%~2"=="" if /I not "%~2"=="fullscreen" set W=%~2
if not "%~3"=="" set H=%~3

echo.
echo   APB Reloaded offline launcher
echo   =============================
echo   Default: Editor -game  (splash / login / character / district)
echo.

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Launch_APB.ps1" -Mode %MODE% -Width %W% -Height %H% %FS%
set ERR=%ERRORLEVEL%
if %ERR% neq 0 (
  echo.
  echo Launch failed (code %ERR%).
  echo Prefer:  Launch_APB.bat editor
  echo Engine:  D:\UE58\UE_5.8
  echo Logs:    D:\APBReloaded\Saved\Logs\APBReloaded.log
  echo.
  pause
  exit /b %ERR%
)

echo.
echo If a window does not open within ~60s, check the log path above.
timeout /t 5 >nul
endlocal
