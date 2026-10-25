@echo off
rem SIGNAL UNKNOWN - einrichten, bauen und starten (Doppelklick genuegt)
title SIGNAL UNKNOWN
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Tools\Windows\SignalUnknown.ps1" %*
if %errorlevel% equ 1 pause
