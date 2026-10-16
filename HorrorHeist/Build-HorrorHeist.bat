@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem ---------------------------------------------------------------------------------------
rem  Kompiliert den HorrorHeist-Editor (Unreal Engine 5.5, Win64, Development), damit sich
rem  HorrorHeist.uproject ohne die Frage "Missing modules ... rebuild them now?" oeffnen laesst.
rem  Prueft vorher alles, woran das Oeffnen sonst scheitert: Engine-Version, Visual Studio mit
rem  C++, Pfadlaenge des Projektordners, OneDrive. Das komplette Protokoll landet in BuildLog.txt.
rem
rem  Aufruf: Doppelklick, oder mit eigenem Engine-Pfad:
rem          Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
rem ---------------------------------------------------------------------------------------

set "PROJECT_DIR=%~dp0"
set "UPROJECT=%PROJECT_DIR%HorrorHeist.uproject"
set "LOG=%PROJECT_DIR%BuildLog.txt"
set "ERRORS=%PROJECT_DIR%BuildErrors.txt"
if exist "%ERRORS%" del "%ERRORS%"

echo.
echo  The Quiet Job - Editor bauen
echo  ============================
echo.

rem --- 1. Projektordner: kurzer Pfad, nicht in OneDrive -------------------------------------
set "PATH_PROBLEM="
set "P=%PROJECT_DIR%"
set /a PLEN=0
:count
if defined P (
    set "P=!P:~1!"
    set /a PLEN+=1
    goto count
)
if %PLEN% GTR 60 set "PATH_PROBLEM=1"
echo %PROJECT_DIR% | findstr /I "OneDrive" >nul && set "PATH_PROBLEM=2"
if "%PATH_PROBLEM%"=="1" (
    echo  WARNUNG: Der Projektordner hat einen sehr langen Pfad ^(%PLEN% Zeichen^):
    echo    %PROJECT_DIR%
    echo  Unreal legt darunter tief verschachtelte Ordner an. Ueber 260 Zeichen schlaegt das
    echo  Kompilieren mit "cannot open include file" oder "path too long" fehl - das passiert oft,
    echo  wenn das GitHub-ZIP direkt im Download-Ordner entpackt wird.
    echo  LOESUNG: Den Ordner "HorrorHeist" z. B. nach  C:\Projekte\HorrorHeist  verschieben und
    echo  das Skript dort erneut starten.
    echo.
    choice /C JN /M "  Trotzdem weitermachen"
    if errorlevel 2 exit /b 1
)
if "%PATH_PROBLEM%"=="2" (
    echo  WARNUNG: Das Projekt liegt in einem OneDrive-Ordner. OneDrive sperrt Dateien waehrend
    echo  des Kompilierens und laedt Gigabytes an Zwischendateien hoch.
    echo  LOESUNG: Den Ordner "HorrorHeist" nach  C:\Projekte\HorrorHeist  verschieben.
    echo.
    choice /C JN /M "  Trotzdem weitermachen"
    if errorlevel 2 exit /b 1
)

rem --- 2. Unreal Engine 5.5 finden ---------------------------------------------------------
set "UE_ROOT=%~1"
if not defined UE_ROOT (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.5" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE_ROOT=%%B"
)
if not defined UE_ROOT if exist "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.5"

if not exist "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" (
    echo  Unreal Engine 5.5 wurde nicht gefunden.
    set "OTHERS="
    for %%V in (5.3 5.4 5.6 5.7 5.8) do (
        for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%%V" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do (
            echo    Gefunden: Unreal Engine %%V  in  %%B
            set "OTHERS=1"
        )
    )
    echo.
    echo  Das Projekt ist fuer Unreal Engine 5.5 geschrieben. Im Epic Games Launcher unter
    echo  "Unreal Engine" - "Bibliothek" - "+" die Version 5.5 auswaehlen und installieren.
    if defined OTHERS (
        echo  Eine andere Version oeffnet das Projekt nur als Kopie/Umwandlung und ist nicht getestet.
    )
    echo.
    echo  Liegt die Engine woanders, das Skript mit dem Engine-Ordner starten, z. B.:
    echo    Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
    echo.
    pause
    exit /b 1
)

rem --- 3. Visual Studio 2022 mit C++ ---------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -version 17.8 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%I"
)
if not defined VS_PATH (
    echo  Visual Studio 2022 ^(Version 17.8 oder neuer^) mit C++ wurde nicht gefunden.
    echo.
    echo  Ohne C++-Compiler kann Unreal das Spiel nicht bauen - genau dann fragt der Editor beim
    echo  Oeffnen nach "Missing modules ... rebuild?" und scheitert danach.
    echo.
    echo  So geht's: "Visual Studio 2022 Community" installieren ^(kostenlos^), im Installer anhaken:
    echo    - Spieleentwicklung mit C++   ^(dort rechts auch "Unreal Engine-Installationsprogramm"^)
    echo    - Desktopentwicklung mit C++
    echo    - .NET-Desktopentwicklung
    echo  und unter "Einzelne Komponenten": MSVC v143 und ein Windows 10/11 SDK.
    echo  Danach dieses Skript erneut starten.
    echo.
    pause
    exit /b 1
)

echo  Engine        : %UE_ROOT%
echo  Visual Studio : %VS_PATH%
echo  Projekt       : %UPROJECT%
echo  Protokoll     : %LOG%
echo.

rem --- 4. Projektdateien erzeugen und kompilieren -----------------------------------------
echo  [1/2] Visual-Studio-Projektdateien erzeugen ...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="%UPROJECT%" -game -rocket -progress > "%LOG%" 2>&1
if errorlevel 1 goto failed

echo  [2/2] HorrorHeistEditor kompilieren (Win64, Development) - beim ersten Mal einige Minuten ...
call "%UE_ROOT%\Engine\Build\BatchFiles\Build.bat" HorrorHeistEditor Win64 Development -project="%UPROJECT%" -waitmutex >> "%LOG%" 2>&1
if errorlevel 1 goto failed

echo.
echo  Fertig! HorrorHeist.uproject laesst sich jetzt per Doppelklick oeffnen
echo  (ohne die Frage nach dem Neu-Bauen).
echo.
choice /C JN /M "  Editor jetzt starten"
if errorlevel 2 exit /b 0
start "" "%UE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe" "%UPROJECT%"
exit /b 0

:failed
rem Nur die Fehlerzeilen herausfiltern (Compiler, Linker, UnrealHeaderTool, UnrealBuildTool).
findstr /I /C:": error" /C:"fatal error" /C:"error LNK" /C:"error C" /C:"error MSB" /C:"ERROR:" "%LOG%" > "%ERRORS%" 2>nul
echo.
echo  FEHLER beim Kompilieren.
echo.
for %%F in ("%ERRORS%") do if %%~zF GTR 0 (
    echo  Die ersten Fehlermeldungen:
    echo  ----------------------------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content -LiteralPath $env:ERRORS -TotalCount 40"
    echo  ----------------------------------------------------------------------------------
    echo.
    echo  Alle Fehlerzeilen stehen in:  %ERRORS%
) else (
    echo  Keine einzelnen Fehlerzeilen gefunden - bitte das komplette Protokoll weitergeben.
)
echo  Komplettes Protokoll:          %LOG%
echo.
findstr /I /C:"path too long" /C:"PathTooLong" /C:"exceeds the maximum" "%LOG%" >nul && (
    echo  HINWEIS: Das Protokoll meldet einen zu langen Dateipfad. Den Ordner "HorrorHeist" nach
    echo  C:\Projekte\HorrorHeist verschieben und das Skript dort erneut starten.
    echo.
)
findstr /I /C:"Unable to find valid" /C:"No valid Visual C++" /C:"toolchain" "%LOG%" >nul && (
    echo  HINWEIS: Der C++-Compiler fehlt oder ist zu alt. Im Visual Studio Installer
    echo  "Spieleentwicklung mit C++" und "MSVC v143" nachinstallieren bzw. aktualisieren.
    echo.
)
echo  Bitte BuildErrors.txt (oder BuildLog.txt) weitergeben bzw. den Text oben kopieren,
echo  damit der Fehler gezielt behoben werden kann.
echo.
pause
exit /b 1
