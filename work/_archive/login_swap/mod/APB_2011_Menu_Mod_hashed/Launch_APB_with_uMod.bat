@echo off
REM Launch APB Reloaded with uMod texture replacement ready.
REM Run this batch as Administrator.

set UMOD_DIR=%~dp0..\uMod\uMod
set APB_DIR=C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries

if not exist "%UMOD_DIR%\uMod.exe" (
    echo uMod.exe not found at %UMOD_DIR%
    pause
    exit /b 1
)

if not exist "%APB_DIR%\APB.exe" (
    echo APB.exe not found at %APB_DIR%
    pause
    exit /b 1
)

start "" "%UMOD_DIR%\uMod.exe"
echo uMod started. Now launch APB Reloaded from Steam.
echo.
echo Instructions:
echo 1. In uMod, go to Main -^> Add game and select:
echo    %APB_DIR%\APB.exe
echo 2. Launch APB Reloaded from Steam.
echo 3. Capture menu texture hashes, then load the renamed .dds files.
pause
