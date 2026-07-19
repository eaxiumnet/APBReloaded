@echo off
cd /d "%~dp0"
title APB 2011 UI / Main Menu browser
echo Export UI first if empty: python ..\scripts\export_2011_ui.py
echo.
python view_ui.py %*
if errorlevel 1 pause
