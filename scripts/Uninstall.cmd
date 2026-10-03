@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Uninstall.ps1"
set "uninstallResult=%errorlevel%"
if not "%uninstallResult%"=="0" pause
exit /b %uninstallResult%
