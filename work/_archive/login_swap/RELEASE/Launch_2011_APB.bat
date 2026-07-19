@echo off
REM Launch the original 2011 APB client with the classic main menu.
REM This is the only known working way to experience the 2011 APB menu.
REM
REM The 2011 client shows a harmless "Ambiguous package name" dialog at launch.
REM This launcher automatically clicks OK and waits for the main menu.

python "%~dp0Launch_2011_APB.py"
