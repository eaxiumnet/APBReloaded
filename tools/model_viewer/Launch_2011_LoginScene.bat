@echo off
cd /d "%~dp0"
title 2011 APB Login / Main Menu art
echo.
echo 2011 login is Scaleform GameFlow UI + art (NO 3D UIDistrict in 2011 client).
echo Browsing hero_login_art ...
echo.
python view_ui.py --root "D:\APBReloaded\Content\Extracted\2011\LoginScene\hero_login_art"
if errorlevel 1 pause
