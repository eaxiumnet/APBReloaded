@echo off
python "%~dp0..\model_viewer\view_models.py" --root "D:\APBReloaded\Content\Extracted\2011\Levels\_view_Financial_Waterfront" %*
if errorlevel 1 pause
