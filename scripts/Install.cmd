@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install.ps1"
set "installResult=%errorlevel%"
if not "%installResult%"=="0" pause
exit /b %installResult%
