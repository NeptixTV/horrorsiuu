@echo off
rem ---------------------------------------------------------------------------------------
rem  Kompiliert den HorrorHeist-Editor (Unreal Engine 5.5, Win64, Development), damit sich
rem  HorrorHeist.uproject ohne die Frage "Missing modules ... rebuild them now?" oeffnen laesst.
rem  Prueft vorher alles, woran das Oeffnen sonst scheitert: vollstaendig entpacktes Projekt
rem  (Ordner Source neben der .uproject), Engine-Version, Visual Studio mit C++, Pfadlaenge,
rem  OneDrive. Erzeugt dabei auch HorrorHeist.sln. Das komplette Protokoll landet in BuildLog.txt.
rem
rem  Aufruf: Doppelklick, oder mit eigenem Engine-Pfad:
rem          Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
rem ---------------------------------------------------------------------------------------

rem Pfade uebernehmen, solange die verzoegerte Expansion noch aus ist (sonst gingen "!" im Pfad
rem verloren). Danach werden Pfade nur noch als !VAR! benutzt: Eine Klammer im Ordnernamen, z. B.
rem "...-dxau5o (1)" nach mehrfachem Download, wuerde einen %VAR% in einem ( ... )-Block beenden.
setlocal EnableExtensions DisableDelayedExpansion
set "PROJECT_DIR=%~dp0"
set "UPROJECT=%~dp0HorrorHeist.uproject"
set "LOG=%~dp0BuildLog.txt"
set "ERRORS=%~dp0BuildErrors.txt"
set "UE_ROOT=%~1"
setlocal EnableDelayedExpansion
if exist "!ERRORS!" del "!ERRORS!"

echo.
echo  The Quiet Job - Editor bauen
echo  ============================
echo.

rem --- 1. Ist das Projekt vollstaendig entpackt? --------------------------------------------
rem Doppelklick auf eine Datei IM ZIP: Windows (bzw. 7-Zip/WinRAR) entpackt nur diese eine Datei
rem in einen Temp-Ordner - ohne den Ordner Source. Genau dann kann Unreal das Spiel nicht bauen.
set "CHECK=!PROJECT_DIR:\AppData\Local\Temp\=!"
if not "!CHECK!"=="!PROJECT_DIR!" goto from_zip
set "CHECK=!PROJECT_DIR:.zip\=!"
if not "!CHECK!"=="!PROJECT_DIR!" goto from_zip

set "MISSING="
for %%F in ("HorrorHeist.uproject" "Source\HorrorHeist.Target.cs" "Source\HorrorHeistEditor.Target.cs" "Source\HorrorHeist\HorrorHeist.Build.cs" "Source\HorrorHeist\HorrorHeist.cpp" "Config\DefaultEngine.ini" "Content\Python\init_unreal.py" "Content\Python\hh_setup\run.py" "SourceArt") do (
    if not exist "!PROJECT_DIR!%%~F" (
        echo  FEHLT: %%~F
        set "MISSING=1"
    )
)
if defined MISSING goto incomplete

rem --- 2. Projektordner: kurzer Pfad, nicht in OneDrive -------------------------------------
set "PATH_PROBLEM="
set "P=!PROJECT_DIR!"
set /a PLEN=0
:count
if defined P (
    set "P=!P:~1!"
    set /a PLEN+=1
    goto count
)
if !PLEN! GTR 60 set "PATH_PROBLEM=1"
set "CHECK=!PROJECT_DIR:OneDrive=!"
if not "!CHECK!"=="!PROJECT_DIR!" set "PATH_PROBLEM=2"
if "!PATH_PROBLEM!"=="1" (
    echo  WARNUNG: Der Projektordner hat einen sehr langen Pfad ^(!PLEN! Zeichen^):
    echo    !PROJECT_DIR!
    echo  Unreal legt darunter tief verschachtelte Ordner an. Ueber 260 Zeichen schlaegt das
    echo  Kompilieren mit "cannot open include file" oder "path too long" fehl - das passiert oft,
    echo  wenn das GitHub-ZIP direkt im Download-Ordner entpackt wird.
    echo  LOESUNG: Den Ordner "HorrorHeist" z. B. nach  C:\Projekte\HorrorHeist  verschieben und
    echo  das Skript dort erneut starten.
    echo.
    call :ask "  Trotzdem weitermachen? [J/N] "
    if /I not "!ANSWER!"=="J" exit /b 1
)
if "!PATH_PROBLEM!"=="2" (
    echo  WARNUNG: Das Projekt liegt in einem OneDrive-Ordner:
    echo    !PROJECT_DIR!
    echo  OneDrive sperrt Dateien waehrend des Kompilierens und laedt Gigabytes an Zwischendateien hoch.
    echo  LOESUNG: Den Ordner "HorrorHeist" nach  C:\Projekte\HorrorHeist  verschieben.
    echo.
    call :ask "  Trotzdem weitermachen? [J/N] "
    if /I not "!ANSWER!"=="J" exit /b 1
)

rem --- 3. Unreal Engine 5.5 finden ---------------------------------------------------------
if not defined UE_ROOT call :engine_dir 5.5 UE_ROOT
if not defined UE_ROOT if exist "C:\Program Files\Epic Games\UE_5.5\Engine\Build\BatchFiles\Build.bat" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.5"
if exist "!UE_ROOT!\Engine\Build\BatchFiles\Build.bat" goto engine_found
echo  Unreal Engine 5.5 wurde nicht gefunden.
set "OTHERS="
for %%V in (5.3 5.4 5.6 5.7 5.8) do call :report_engine %%V
echo.
echo  Das Projekt ist fuer Unreal Engine 5.5 geschrieben. Im Epic Games Launcher unter
echo  "Unreal Engine" - "Bibliothek" - "+" die Version 5.5 auswaehlen und installieren.
if defined OTHERS echo  Eine andere Version oeffnet das Projekt nur als Kopie/Umwandlung und ist nicht getestet.
echo.
echo  Liegt die Engine woanders, das Skript mit dem Engine-Ordner starten, z. B.:
echo    Build-HorrorHeist.bat "D:\Epic Games\UE_5.5"
echo.
pause
exit /b 1
:engine_found

rem --- 4. Visual Studio 2022 mit C++ ---------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_PATH="
if exist "!VSWHERE!" call :vs_dir
if defined VS_PATH goto vs_found
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
:vs_found

echo  Engine        : !UE_ROOT!
echo  Visual Studio : !VS_PATH!
echo  Projekt       : !UPROJECT!
echo  Protokoll     : !LOG!
echo.

rem --- 5. Projektdateien erzeugen und kompilieren -----------------------------------------
echo  [1/2] Visual-Studio-Projektdatei HorrorHeist.sln erzeugen ...
call "!UE_ROOT!\Engine\Build\BatchFiles\Build.bat" -projectfiles -project="!UPROJECT!" -game -rocket -progress > "!LOG!" 2>&1
if errorlevel 1 goto failed

echo  [2/2] HorrorHeistEditor kompilieren (Win64, Development) - beim ersten Mal einige Minuten ...
call "!UE_ROOT!\Engine\Build\BatchFiles\Build.bat" HorrorHeistEditor Win64 Development -project="!UPROJECT!" -waitmutex >> "!LOG!" 2>&1
if errorlevel 1 goto failed

echo.
echo  Fertig. HorrorHeist.uproject laesst sich jetzt per Doppelklick oeffnen
echo  (ohne die Frage nach dem Neu-Bauen). HorrorHeist.sln liegt daneben, falls du
echo  den Code in Visual Studio ansehen moechtest.
echo.
call :ask "  Editor jetzt starten? [J/N] "
if /I not "!ANSWER!"=="J" exit /b 0
start "" "!UE_ROOT!\Engine\Binaries\Win64\UnrealEditor.exe" "!UPROJECT!"
exit /b 0

:from_zip
echo  FEHLER: Das Projekt wurde direkt aus dem ZIP-Archiv heraus gestartet:
echo    !PROJECT_DIR!
echo.
echo  Windows legt dabei nur eine voruebergehende Kopie im Temp-Ordner an - meist nur von der
echo  angeklickten Datei, ohne den Ordner "Source" mit dem C++-Code des Spiels. Genau dann fragt
echo  Unreal beim Oeffnen von HorrorHeist.uproject nach "Missing modules ... rebuild?" und kann
echo  es nicht bauen.
goto howto

:incomplete
echo.
echo  FEHLER: Der Projektordner ist unvollstaendig:
echo    !PROJECT_DIR!
echo.
echo  Neben HorrorHeist.uproject muss der Ordner "Source" mit dem C++-Code liegen ^(dazu Config,
echo  Content und SourceArt^). Ohne "Source" kann Unreal das Modul HorrorHeist nicht bauen und
echo  meldet "The following modules are missing or built with a different engine version".
echo  Eine HorrorHeist.sln gibt es im Download nicht - dieses Skript erzeugt sie.
goto howto

:howto
echo.
echo  LOESUNG:
echo    1. Das heruntergeladene ZIP im Explorer mit Rechtsklick - "Alle extrahieren..." KOMPLETT
echo       entpacken ^(nicht im ZIP-Fenster doppelklicken, keine einzelnen Dateien herausziehen^).
echo    2. Im entpackten Ordner liegt "HorrorHeist" - darin HorrorHeist.uproject, Source, Config,
echo       Content, SourceArt und dieses Skript. Den ganzen Ordner "HorrorHeist" z. B. nach
echo       C:\Projekte\HorrorHeist  verschieben.
echo    3. Dort Build-HorrorHeist.bat doppelklicken.
echo.
pause
exit /b 1

:failed
rem Nur die Fehlerzeilen herausfiltern (Compiler, Linker, UnrealHeaderTool, UnrealBuildTool).
findstr /I /C:": error" /C:"fatal error" /C:"error LNK" /C:"error C" /C:"error MSB" /C:"ERROR:" "!LOG!" > "!ERRORS!" 2>nul
set "HAVE_ERRORS=1"
if errorlevel 1 set "HAVE_ERRORS="
echo.
echo  FEHLER beim Kompilieren.
echo.
if defined HAVE_ERRORS goto show_errors
echo  Keine einzelnen Fehlerzeilen gefunden - bitte das komplette Protokoll weitergeben.
goto errors_shown
:show_errors
echo  Die ersten Fehlermeldungen:
echo  ----------------------------------------------------------------------------------
set /a SHOWN=0
for /f "usebackq delims=" %%L in ("%ERRORS%") do (
    set /a SHOWN+=1
    if !SHOWN! LEQ 40 echo   %%L
)
echo  ----------------------------------------------------------------------------------
echo.
echo  Alle Fehlerzeilen stehen in:  !ERRORS!
:errors_shown
echo  Komplettes Protokoll:          !LOG!
echo.
findstr /I /C:"path too long" /C:"PathTooLong" /C:"exceeds the maximum" "!LOG!" >nul && (
    echo  HINWEIS: Das Protokoll meldet einen zu langen Dateipfad. Den Ordner "HorrorHeist" nach
    echo  C:\Projekte\HorrorHeist verschieben und das Skript dort erneut starten.
    echo.
)
findstr /I /C:"Unable to find valid" /C:"No valid Visual C++" /C:"toolchain" "!LOG!" >nul && (
    echo  HINWEIS: Der C++-Compiler fehlt oder ist zu alt. Im Visual Studio Installer
    echo  "Spieleentwicklung mit C++" und "MSVC v143" nachinstallieren bzw. aktualisieren.
    echo.
)
echo  Bitte BuildErrors.txt (oder BuildLog.txt) weitergeben bzw. den Text oben kopieren,
echo  damit der Fehler gezielt behoben werden kann.
echo.
pause
exit /b 1

rem --- Unterprogramme ------------------------------------------------------------------------
:engine_dir
rem %1 = Engine-Version, %2 = Variable, die den Installationsordner bekommt (Registry des Epic Launchers).
rem Ausgabe von reg query:  "    InstalledDirectory    REG_SZ    C:\Program Files\Epic Games\UE_5.5"
for /f "tokens=1,2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%~1" /v InstalledDirectory 2^>nul') do (
    if /I "%%A"=="InstalledDirectory" set "%~2=%%C"
)
exit /b 0

:report_engine
set "FOUND="
call :engine_dir %1 FOUND
if defined FOUND (
    echo    Gefunden: Unreal Engine %~1  in  !FOUND!
    set "OTHERS=1"
)
exit /b 0

:vs_dir
for /f "usebackq delims=" %%I in (`"%VSWHERE%" -latest -products * -version 17.8 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS_PATH=%%I"
exit /b 0

:ask
rem Ja/Nein-Frage ohne choice.exe: "J"/"ja" gilt als Ja, alles andere (auch nur Enter) als Nein.
set "ANSWER="
set /p "ANSWER=%~1"
if defined ANSWER set "ANSWER=!ANSWER:~0,1!"
exit /b 0
