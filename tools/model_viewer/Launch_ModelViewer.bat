@echo off
cd /d "%~dp0"
python view_models.py %*
if errorlevel 1 pause
