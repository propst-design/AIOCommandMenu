@echo off
setlocal
title Publish AiO to GitHub
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\Publish-GitHub.ps1" %*
exit /b %errorlevel%
