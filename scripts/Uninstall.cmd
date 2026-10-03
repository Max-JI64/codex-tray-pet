@echo off
powershell.exe -NoProfile -File "%~dp0Uninstall.ps1"
if errorlevel 1 pause
