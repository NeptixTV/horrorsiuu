# SIGNAL UNKNOWN – Einrichten, Bauen und Starten unter Windows.
#
# Wird von START.bat aufgerufen. Macht alles, was sonst von Hand nötig wäre:
#   * findet die installierte Unreal Engine (Epic Games Launcher oder eigener Pfad)
#   * prüft Visual Studio 2022 mit C++ (und bietet die Installation per winget an)
#   * stellt das Projekt auf die gefundene Engine-Version um
#   * baut den C++-Code und schreibt dabei BuildLog.txt
#   * startet Spiel, Editor oder einen Koop-Test mit zwei Fenstern
#
# Optionen:  -Mode Spielen|Editor|Koop|Bauen    -EngineDir "D:\UE_5.6"
param(
	[ValidateSet('', 'Spielen', 'Editor', 'Koop', 'Bauen')]
	[string]$Mode = '',
	[string]$EngineDir = $env:SU_ENGINE_DIR
)

$ErrorActionPreference = 'Continue'
$ProjectDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Uproject = Join-Path $ProjectDir 'SignalUnknown.uproject'
$BuildLog = Join-Path $ProjectDir 'BuildLog.txt'
$WarmMarker = Join-Path $ProjectDir 'Saved\SU_ErsterStartErledigt.txt'
$MinVersion = [version]'5.1'

function Say([string]$Text, [string]$Color = 'Gray') { Write-Host $Text -ForegroundColor $Color }
function Fail([string]$Text)
{
	Write-Host ''
	Write-Host $Text -ForegroundColor Red
	Write-Host ''
	Read-Host 'Enter drücken zum Schließen'
	exit 2
}

Clear-Host
Say '==============================================' 'DarkYellow'
Say '   SIGNAL UNKNOWN  –  Einrichten und Starten' 'Yellow'
Say '==============================================' 'DarkYellow'
Say ''

if (-not (Test-Path $Uproject)) { Fail "SignalUnknown.uproject nicht gefunden in $ProjectDir. Bitte den ganzen Ordner entpacken." }

# Dateien aus dem Internet (ZIP) sind von Windows gesperrt -> entsperren
Get-ChildItem -Path $ProjectDir -Recurse -File -ErrorAction SilentlyContinue | Unblock-File -ErrorAction SilentlyContinue

# ---------------------------------------------------------------- Pfad prüfen
if ($ProjectDir.Length -gt 70) { Say "Hinweis: Der Ordnerpfad ist sehr lang ($($ProjectDir.Length) Zeichen). Unreal kann damit Probleme haben. Besser z. B. nach C:\Spiele\SignalUnknown verschieben." 'Yellow' }
if ($ProjectDir -match '[^\x00-\x7F]') { Say 'Hinweis: Der Ordnerpfad enthält Umlaute oder Sonderzeichen. Falls der Build scheitert: Ordner z. B. nach C:\Spiele\SignalUnknown verschieben.' 'Yellow' }
if ($ProjectDir -match 'OneDrive') { Say 'Hinweis: Der Ordner liegt in OneDrive. Das macht Unreal oft langsam oder kaputt. Besser nach C:\Spiele\SignalUnknown verschieben.' 'Yellow' }
try
{
	$Drive = Get-PSDrive -Name $ProjectDir.Substring(0, 1) -ErrorAction Stop
	if ($Drive.Free -lt 15GB) { Say ("Hinweis: Nur {0:N0} GB frei auf Laufwerk {1}:. Für Build und Shader werden etwa 15 GB gebraucht." -f ($Drive.Free / 1GB), $Drive.Name) 'Yellow' }
}
catch {}

# ---------------------------------------------------------------- Unreal Engine finden
function Find-Engines
{
	$found = @{}
	$add = {
		param($ver, $dir)
		if (-not $dir) { return }
		$exe = Join-Path $dir 'Engine\Binaries\Win64\UnrealEditor.exe'
		$bat = Join-Path $dir 'Engine\Build\BatchFiles\Build.bat'
		if ((Test-Path $exe) -and (Test-Path $bat))
		{
			try { $v = [version]($ver -replace '^UE_', '') } catch { return }
			$found[$dir.TrimEnd('\')] = [pscustomobject]@{ Version = $v; Dir = $dir.TrimEnd('\') }
		}
	}
	# 1. Registry (Epic Games Launcher)
	Get-ChildItem 'HKLM:\SOFTWARE\EpicGames\Unreal Engine' -ErrorAction SilentlyContinue | ForEach-Object {
		$dir = (Get-ItemProperty $_.PSPath -ErrorAction SilentlyContinue).InstalledDirectory
		& $add $_.PSChildName $dir
	}
	# 2. Launcher-Liste
	$dat = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
	if (Test-Path $dat)
	{
		try
		{
			(Get-Content $dat -Raw | ConvertFrom-Json).InstallationList | Where-Object { $_.AppName -like 'UE_*' } | ForEach-Object { & $add $_.AppName $_.InstallLocation }
		}
		catch {}
	}
	# 3. Standardordner
	foreach ($root in @("$env:ProgramFiles\Epic Games", 'C:\Epic Games', 'D:\Epic Games', 'E:\Epic Games'))
	{
		Get-ChildItem $root -Directory -Filter 'UE_*' -ErrorAction SilentlyContinue | ForEach-Object { & $add $_.Name $_.FullName }
	}
	return $found.Values | Sort-Object Version -Descending
}

$Engine = $null
if ($EngineDir)
{
	$ver = Get-Content (Join-Path $EngineDir 'Engine\Build\Build.version') -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json -ErrorAction SilentlyContinue
	if ($ver) { $Engine = [pscustomobject]@{ Version = [version]("{0}.{1}" -f $ver.MajorVersion, $ver.MinorVersion); Dir = $EngineDir.TrimEnd('\') } }
	else { Fail "Unter $EngineDir wurde keine Unreal Engine gefunden." }
}
else
{
	$Engine = Find-Engines | Where-Object { $_.Version -ge $MinVersion } | Select-Object -First 1
}

if (-not $Engine)
{
	Say 'Es wurde keine Unreal Engine 5 gefunden.' 'Red'
	Say ''
	Say 'So geht es:'
	Say '  1. Epic Games Launcher installieren (kostenlos, Epic-Konto nötig).'
	Say '  2. Im Launcher links "Unreal Engine" -> oben "Bibliothek" -> "+" -> neueste 5.x installieren.'
	Say '  3. Danach START.bat noch einmal ausführen.'
	Say ''
	if (Get-Command winget -ErrorAction SilentlyContinue)
	{
		$a = Read-Host 'Soll ich den Epic Games Launcher jetzt installieren? (j/n)'
		if ($a -match '^[jJyY]') { winget install --id EpicGames.EpicGamesLauncher -e --accept-source-agreements --accept-package-agreements }
	}
	Fail 'Ohne Unreal Engine geht es leider nicht weiter.'
}
Say ("Unreal Engine {0} gefunden: {1}" -f $Engine.Version, $Engine.Dir) 'Green'
if ($Engine.Version -lt [version]'5.3') { Say 'Hinweis: Empfohlen ist Unreal Engine 5.3 oder neuer.' 'Yellow' }

# ---------------------------------------------------------------- Visual Studio (C++-Compiler) prüfen
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = $null
if (Test-Path $vswhere)
{
	$vs = & $vswhere -latest -products * -version '[17.0,' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
}
if (-not $vs)
{
	Say ''
	Say 'Visual Studio 2022 mit C++ fehlt. Unreal braucht es, um den Spielcode zu übersetzen.' 'Red'
	Say '(Visual Studio Community ist kostenlos. Download etwa 10 GB, Installation 20–40 Minuten.)'
	if (Get-Command winget -ErrorAction SilentlyContinue)
	{
		$a = Read-Host 'Soll ich Visual Studio 2022 Community mit allem Nötigen jetzt installieren? (j/n)'
		if ($a -match '^[jJyY]')
		{
			Say 'Installation läuft. Windows fragt gleich nach Administratorrechten – bitte bestätigen.' 'Yellow'
			winget install --id Microsoft.VisualStudio.2022.Community -e --accept-source-agreements --accept-package-agreements --override "--passive --wait --add Microsoft.VisualStudio.Workload.NativeGame --add Microsoft.VisualStudio.Workload.NativeDesktop --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add Microsoft.VisualStudio.Component.Windows11SDK.22621 --includeRecommended"
			if (Test-Path $vswhere)
			{
				$vs = & $vswhere -latest -products * -version '[17.0,' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
			}
		}
	}
	if (-not $vs)
	{
		Fail ("Bitte Visual Studio 2022 Community installieren (https://visualstudio.microsoft.com/de/)`n" +
			'und dabei "Spieleentwicklung mit C++" sowie "Desktopentwicklung mit C++" anhaken. Danach START.bat erneut starten.')
	}
}
Say "Visual Studio gefunden: $vs" 'Green'

# ---------------------------------------------------------------- Projekt auf die Engine-Version umstellen
$json = [IO.File]::ReadAllText($Uproject)
$assoc = '{0}.{1}' -f $Engine.Version.Major, $Engine.Version.Minor
$newJson = [regex]::Replace($json, '"EngineAssociation"\s*:\s*"[^"]*"', ('"EngineAssociation": "{0}"' -f $assoc))
if ($newJson -ne $json -and -not $EngineDir)
{
	[IO.File]::WriteAllText($Uproject, $newJson, (New-Object System.Text.UTF8Encoding($false)))
	Say "Projekt auf Unreal Engine $assoc umgestellt." 'Green'
}

# ---------------------------------------------------------------- Menü
if (-not $Mode)
{
	Say ''
	Say 'Was möchtest du tun?' 'Yellow'
	Say '  [1] Spielen'
	Say '  [2] Editor öffnen'
	Say '  [3] Koop-Test (zwei Spielfenster auf diesem PC)'
	Say '  [4] Nur bauen'
	Say '  [Q] Beenden'
	switch -Regex (Read-Host 'Auswahl')
	{
		'^1' { $Mode = 'Spielen' }
		'^2' { $Mode = 'Editor' }
		'^3' { $Mode = 'Koop' }
		'^4' { $Mode = 'Bauen' }
		default { exit 0 }
	}
}

# ---------------------------------------------------------------- Bauen
$BuildBat = Join-Path $Engine.Dir 'Engine\Build\BatchFiles\Build.bat'
$EditorExe = Join-Path $Engine.Dir 'Engine\Binaries\Win64\UnrealEditor.exe'
Say ''
Say 'Baue den Spielcode ... (beim ersten Mal 5–15 Minuten, danach Sekunden)' 'Yellow'
$output = & $BuildBat SignalUnknownEditor Win64 Development "-Project=$Uproject" -WaitMutex 2>&1 | ForEach-Object { $line = "$_"; Write-Host $line; $line }
$code = $LASTEXITCODE
[IO.File]::WriteAllLines($BuildLog, [string[]]$output, (New-Object System.Text.UTF8Encoding($false)))
if ($code -ne 0)
{
	Say ''
	Say 'Der Build ist fehlgeschlagen. Die wichtigsten Zeilen:' 'Red'
	$output | Where-Object { $_ -match 'error|Fehler' } | Select-Object -First 25 | ForEach-Object { Say "  $_" 'Red' }
	Say ''
	Say "Das vollständige Protokoll steht in: $BuildLog" 'Yellow'
	Say 'Schick mir diese Datei BuildLog.txt – dann behebe ich die Fehler.' 'Yellow'
	Start-Process explorer.exe "/select,`"$BuildLog`""
	Fail 'Abgebrochen.'
}
Say 'Build erfolgreich.' 'Green'

# ---------------------------------------------------------------- Starten
$firstStart = -not (Test-Path $WarmMarker)
function Mark-FirstStart
{
	New-Item -ItemType Directory -Force -Path (Split-Path $WarmMarker) | Out-Null
	Set-Content -Path $WarmMarker -Value (Get-Date).ToString('s')
}

switch ($Mode)
{
	'Bauen' { Say 'Fertig.'; Start-Sleep -Seconds 2; exit 0 }
	'Editor'
	{
		Say 'Der Editor startet. Zum Spielen oben in der Mitte auf den grünen Play-Knopf klicken.' 'Green'
		if ($firstStart) { Say 'Beim allerersten Öffnen werden die Shader kompiliert (10–40 Minuten, unten rechts ist der Fortschritt zu sehen). Bitte warten.' 'Yellow'; Mark-FirstStart }
		Start-Process -FilePath $EditorExe -ArgumentList "`"$Uproject`""
	}
	'Spielen'
	{
		if ($firstStart)
		{
			Say 'Erster Start: Ich öffne einmal den Editor, weil er beim Kompilieren der Shader den Fortschritt anzeigt.' 'Yellow'
			Say 'Das dauert beim ersten Mal 10–40 Minuten. Danach oben auf den grünen Play-Knopf klicken.' 'Yellow'
			Say 'Ab dem nächsten Mal startet [1] Spielen direkt das Spiel.' 'Yellow'
			Mark-FirstStart
			Start-Process -FilePath $EditorExe -ArgumentList "`"$Uproject`""
		}
		else
		{
			Say 'Das Spiel startet ...' 'Green'
			Start-Process -FilePath $EditorExe -ArgumentList "`"$Uproject`" -game -windowed -ResX=1600 -ResY=900"
		}
	}
	'Koop'
	{
		if ($firstStart) { Say 'Tipp: Vorher einmal [1] Spielen oder [2] Editor ausführen, damit die Shader fertig sind.' 'Yellow' }
		Say 'Starte Fenster 1 (Host) ...' 'Green'
		Start-Process -FilePath $EditorExe -ArgumentList "`"$Uproject`" -game -SUHost -windowed -ResX=1280 -ResY=720 -WinX=20 -WinY=40"
		Say 'Warte 40 Sekunden, bis der Host bereit ist ...'
		Start-Sleep -Seconds 40
		Say 'Starte Fenster 2 (Client, verbindet zu 127.0.0.1) ...' 'Green'
		Start-Process -FilePath $EditorExe -ArgumentList "`"$Uproject`" 127.0.0.1 -game -windowed -ResX=1280 -ResY=720 -WinX=660 -WinY=80"
		Say 'Verbindet Fenster 2 nicht: dort im Hauptmenü "Koop beitreten" -> "localhost" -> "Verbinden".'
	}
}
Say ''
Say 'Viel Spaß – und bleib auf den Hauptstraßen.' 'DarkYellow'
Start-Sleep -Seconds 4
