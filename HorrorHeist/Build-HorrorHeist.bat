@echo off
setlocal EnableExtensions
rem ---------------------------------------------------------------------------------------
rem  Erzeugt die Visual-Studio-2022-Projektdateien und kompiliert den HorrorHeist-Editor
rem  (Unreal Engine 5.5, Win64, Development). Das komplette Protokoll landet in BuildLog.txt.
rem
rem  Aufruf: Doppelklick, oder mit eigenem Engine-Pfad:
rem          Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
rem ---------------------------------------------------------------------------------------

set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%HorrorHeist.uproject"
set "LOG=%PROJECT_DIR%BuildLog.txt"

set "UE_ROOT=%~1"
if not defined UE_ROOT (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.5" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
)
if not defined UE_ROOT set "UE_ROOT=C:\Program Files\Epic Games\UE_5.5"

if not exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
    echo.
    echo Unreal Engine 5.5 wurde nicht gefunden unter:
    echo   "%UE_ROOT%"
    echo.
    echo Starte das Skript mit dem Engine-Ordner als Parameter, z. B.:
    echo   Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
    echo.
    pause
    exit /b 1
)

echo Engine : %UE_ROOT%
echo Projekt: %UPROJECT%
echo Log    : %LOG%
echo.

echo [1/2] Visual-Studio-Projektdateien erzeugen ...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%UPROJECT%" -game -rocket -progress > "%LOG%" 2>&1
if errorlevel 1 goto failed

echo [2/2] HorrorHeistEditor kompilieren (Win64, Development) - das dauert beim ersten Mal einige Minuten ...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" HorrorHeistEditor Win64 Development -project="%UPROJECT%" -waitmutex >> "%LOG%" 2>&1
if errorlevel 1 goto failed

echo.
echo Fertig! Jetzt HorrorHeist.uproject per Doppelklick oeffnen.
echo.
pause
exit /b 0

:failed
echo.
echo FEHLER beim Kompilieren. Das vollstaendige Protokoll steht in:
echo   %LOG%
echo Bitte diese Datei weitergeben, damit der Fehler gezielt behoben werden kann.
echo.
pause
exit /b 1
