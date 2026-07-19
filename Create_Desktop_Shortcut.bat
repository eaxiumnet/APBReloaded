@echo off
setlocal
set "TARGET=%~dp0Launch_APB.bat"
set "DESKTOP=%USERPROFILE%\Desktop"
set "LNK=%DESKTOP%\APB Reloaded (Offline).lnk"

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ws = New-Object -ComObject WScript.Shell; $s = $ws.CreateShortcut('%LNK%'); $s.TargetPath = '%TARGET%'; $s.WorkingDirectory = '%~dp0'; $s.WindowStyle = 1; $s.Description = 'APB Reloaded offline client (splash/login/character/district)'; $s.Save(); Write-Host 'Created: %LNK%'"

echo.
echo Shortcut: %LNK%
echo Double-click it (or Launch_APB.bat) to play.
pause
endlocal
