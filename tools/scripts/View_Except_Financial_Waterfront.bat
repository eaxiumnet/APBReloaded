@echo off
title APB Levels - except Financial and Waterfront
python "%~dp0..\model_viewer\view_models.py" --root "D:\APBReloaded\Content\Extracted\2011\Levels\_view_Except_Financial_Waterfront" %*
if errorlevel 1 pause
