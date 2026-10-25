# SIGNAL UNKNOWN

First-Person-Open-World-Koop-Horror (1–4 Spieler) für **Unreal Engine 5**, reines C++-Projekt.
Ihr seid Ranger auf dem Beobachtungsposten 07: Ihr nehmt Funksprüche der Zentrale an, fahrt mit
dem alten Geländewagen zu Einsatzorten, dokumentiert mit echten Fotos und rechnet am Einsatzcomputer ab.
Gleichzeitig entscheidet ein Horror-Director, ob etwas im Wald passiert – und wann besser nicht.

> **Ehrlicher Stand:** Der Unreal Editor konnte in der Entwicklungsumgebung **nicht gestartet**
> werden (keine Engine installiert). Der Code wurde **nicht mit Unreal Engine kompiliert** und
> **nicht im Spiel getestet**. Getestet ist die engine-unabhängige Logik (native Tests) und die
> Syntax gegen eine Nachbildung der UE-API (siehe [Prüfungen](#prüfungen)). Beim ersten echten
> Build ist mit einzelnen Compilerfehlern zu rechnen (z. B. API-Abweichungen je nach Engine-Version).
> Alle Grafiken sind **Platzhalter** aus Engine-Grundformen, nicht fotorealistisch.

---

## Inhalt

1. [Schnellstart (lokal testen)](#schnellstart-lokal-testen)
2. [Status der Systeme](#status-der-systeme)
3. [Der erste Einsatz „Der letzte Funkspruch“](#der-erste-einsatz-der-letzte-funkspruch)
4. [Steuerung](#steuerung)
5. [Prüfungen](#prüfungen)
6. [Architektur](#architektur)
7. [Platzhalter, Assets und Lizenzen](#platzhalter-assets-und-lizenzen)
8. [Bekannte Einschränkungen](#bekannte-einschränkungen)
9. [Nächste Schritte](#nächste-schritte)

---

## Schnellstart (lokal testen)

### Am einfachsten: `START.bat`

1. Ordner entpacken, am besten nach `C:\Spiele\SignalUnknown` (kurzer Pfad, keine Umlaute, nicht in OneDrive).
2. **Doppelklick auf `START.bat`.** Das Skript findet die installierte Unreal Engine, prüft Visual Studio
   (und bietet die Installation an), stellt das Projekt auf deine Engine-Version um, baut den Code und
   startet das Spiel. Menü: *Spielen*, *Editor öffnen*, *Koop-Test (zwei Fenster)*, *Nur bauen*.
3. Beim **allerersten** Start öffnet sich der Editor und kompiliert die Shader (10–40 Minuten, Fortschritt
   unten rechts). Danach oben auf den grünen **Play**-Knopf klicken. Ab dann startet *Spielen* direkt.
4. Schlägt der Build fehl, steht alles in **`BuildLog.txt`** im Projektordner – diese Datei schicken.

Es müssen keine Texturen oder Modelle heruntergeladen werden: die Welt wird beim Start aus
Engine-Grundformen erzeugt (Platzhaltergrafik).

### Von Hand

Voraussetzungen: Windows 10/11, **Unreal Engine 5** (eingetragen ist 5.8; mit älteren 5.x-Versionen
über Rechtsklick auf `SignalUnknown.uproject` → *Switch Unreal Engine version* umstellen),
**Visual Studio 2022** mit „Spieleentwicklung mit C++“ und dem passenden Windows-SDK.

1. Rechtsklick auf `SignalUnknown/SignalUnknown.uproject` → **Generate Visual Studio project files**.
2. `SignalUnknown.sln` öffnen, Konfiguration **Development Editor / Win64**, Projekt `SignalUnknown`
   bauen. Treten Compilerfehler auf: bitte melden oder beheben – siehe Hinweis oben.
3. `SignalUnknown.uproject` öffnen. Der Editor startet auf der leeren Engine-Map `Entry`;
   die komplette Spielwelt wird **zur Laufzeit** erzeugt (kein Map-Asset nötig).
4. **Play** (PIE, „Selected Viewport“ oder „New Editor Window“). Das Hauptmenü erscheint mit
   Blick über den Turm. „Neues Spiel (allein)“ wählen.
5. Nach etwa 20 Sekunden meldet sich das Funkgerät im Turm → hingehen, **[E]**.

### Koop mit zwei Instanzen auf einem Rechner

```bat
:: Host (überspringt das Menü und startet eine Koop-Sitzung)
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Pfad\SignalUnknown\SignalUnknown.uproject" -game -SUHost -windowed -ResX=1280 -ResY=720 -log

:: Client (verbindet direkt zum Host)
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "D:\Pfad\SignalUnknown\SignalUnknown.uproject" 127.0.0.1 -game -windowed -ResX=1280 -ResY=720 -log
```

Alternativ im Hauptmenü „Koop hosten“ bzw. „Koop beitreten“ (IP eingeben). Über das Netzwerk muss
beim Host **UDP-Port 7777** erreichbar sein. Weitere Startoptionen: `-SUSolo` (direkt allein spielen).

### Was man prüfen sollte (Checkliste)

| Prüfen | Erwartung |
|---|---|
| Welt | Turm mit Treppen, Kabine, Parkplatz, Wald, Straßen, See, Tankstelle, Forsthaus, Siedlung |
| Funkgerät | Rauschen, Piepen, [E] annehmen, Untertitel nur in Hörweite des Funkgeräts oder im Auto |
| Kamera | [C], [RMT], [LMT]: Foto erscheint in der Galerie [P], Beweise mit Qualität |
| Auto | Einsteigen [E] an einer Tür, [F] Motor, fahren, Licht [L], Radio [N], Tank sinkt |
| Tanken | Zapfpistole nehmen, Tankdeckel öffnen, einsetzen, tanken, zurückhängen, an der Kasse zahlen |
| Einsatz | alle Schritte bis zum Bericht am Einsatzcomputer, Geld auf dem Teamkonto |
| Horror | Konsole: `su.HorrorDebug 1`, `su.HorrorEvent TreeWatcher` / `RoadsideFigure` / `HunterChase` |
| Speichern | [F5] speichern, [F9] laden (nur Host), Fortsetzen im Hauptmenü |

---

## Status der Systeme

Legende:
**Code fertig** = vollständig geschrieben und syntaktisch geprüft, aber *nicht in der Engine
getestet*. **Logik getestet** = der Kern liegt in `Source/SignalUnknown/Logic/` und ist durch die
nativen Tests abgedeckt. **Teilweise** = funktioniert mit Platzhaltern oder Vereinfachungen.
**Vorbereitet** = Schnittstellen/Daten vorhanden, Funktion fehlt. **Nicht umgesetzt** = fehlt.

| System | Status | Anmerkungen |
|---|---|---|
| Prozedurale Welt (Terrain, Straßen, Wald, See, Grenzen) | Code fertig, Logik getestet | deterministisch per Seed auf allen Rechnern; Layout-Vorschau `Tests/Native/run_tests.sh --preview` |
| Aussichtsturm (begehbar, Kabine, Schuppen, Generator) | Code fertig | Platzhaltergeometrie, echte Türen, Lampen, Schalter, Schubladen |
| Funkgerät + Funksprüche | Code fertig | annehmen, wiederholen, bestätigen; Stimme ist ein prozedurales Platzhalter-Gebrabbel + Untertitel |
| Einsatzcomputer (Einsatz, Fotos/Bericht, Konto, Archiv) | Code fertig | Berichtsvorschau, Auszahlung, Bewertung |
| Wandkarte + Karte [M] | Code fertig | Karte ist eine echte Draufsicht-Aufnahme der erzeugten Welt |
| Kamera / Fotos / Beweise | Code fertig, Logik getestet | echte Bildschirmaufnahme ohne HUD, PNG-Dateien, Erkennung nach Sichtbarkeit, Größe, Distanz, Licht, Verwackeln, Fokus, Duplikate |
| Wirtschaft (Geld, Berichte, Preise) | Code fertig, Logik getestet | Teamkonto, Buchungen |
| Fahrzeug | Code fertig, Logik getestet | eigene Raycast-Fahrphysik (siehe Einschränkungen), 4 Sitze, Tank, Licht, Blinker, Wischer, Hupe, Radio, Kofferraum, Schaden, Rückspiegel |
| Tankstelle (Zapfsäule, Kasse, Werkstatt) | Code fertig | realer Ablauf, Säule gesperrt bis bezahlt, Wegfahren ohne Zahlen wird vermerkt |
| Autoradio | Teilweise | 3 prozedurale Sender, Nachrichten als Text je nach Story, seltsame Einspielungen; keine echten Musik-/Sprachaufnahmen |
| NPCs (Kassiererin, Anwohner) | Teilweise | Laufwege, Dialogzeilen, Tag/Nacht; Platzhalterfigur, keine Animationen/Sprachaufnahmen |
| Horror-Director | Code fertig, Logik getestet | Abklingzeiten, Spannung, Gewichte, Sicherheitsregeln, entscheidet oft bewusst „nichts“ |
| Kreaturen: Beobachter, Jäger | Teilweise | Zustandsautomat, Wahrnehmung, Taschenlampe vertreibt, Fahrzeugangriffe; Platzhalterkörper, prozedurale Animation, keine Navmesh-Pfadsuche |
| Skript-Ereignis „Gestalt am Fenster“ (M01) | Code fertig | echte Trigger + Kreatur-KI, mit Rückfallpfaden damit der Einsatz immer abschließbar bleibt |
| Einsatz 1 „Der letzte Funkspruch“ | Code fertig | vollständig abschließbar (siehe unten) |
| Einsatz 2 „Die Nordstraße“ | Teilweise | Ort, Beweise, Ablauf und Bericht vorhanden; ohne eigene Horror-Sequenz |
| Inventar, Gegenstände, Kofferraum | Code fertig | Batterien, Verband, Snacks, Werkzeug, Beweisstücke |
| Speichern/Laden | Code fertig | versioniert (v1), Backup-Slot, Migration, nur Host |
| HUD + Menüs | Code fertig | Canvas-UI ohne Assets: Hauptmenü, Pause, Einstellungen, Beitreten, Inventar, Terminal, Karte, Shop, Dokument, Galerie, Hilfe, Kofferraum |
| Einstellungen | Code fertig | Grafik-Preset, Auflösung, Fenstermodus, Auflösungsskalierung, FPS-Limit, VSync, FOV, Bewegungsunschärfe, Filmkorn, Audio, Untertitel, Horror-Intensität |
| Koop 1–4 Spieler | Code fertig | Listen-Server per IP; Replikation aller Spielsysteme |
| Sprachchat (Nähe) | Teilweise | Engine-VoIP (OnlineSubsystem/VOIPTalker) mit räumlicher Abschwächung, Push-to-Talk [T]; **ungetestet**, hängt von der Online-Subsystem-Konfiguration ab |
| Wetter, Tag/Nacht, Gewitter | Code fertig | Sonne, Mond, Wolken, Nebel, Regen, Blitze, Donner |
| Prozedurales Audio | Code fertig, Logik getestet | Wind, Regen, Vögel, Grillen, Motor, Schritte, Türen, Kreaturen – alles synthetisiert |
| Missionen als DataTable | Vorbereitet | `USUGameplaySettings::MissionTable` kann die eingebauten Missionen überschreiben |
| Kunst-Überschreibungen | Vorbereitet | Projekteinstellungen → *Signal Unknown Art*: eigene Bäume, Felsen, Materialien |
| Polizei-System | Vorbereitet | unbezahlte Tankvorgänge werden gezählt, noch keine Folgen |
| Funkgeräte (Walkie-Talkies) | Nicht umgesetzt | bewusst weggelassen statt ohne Funktion |
| Spieler-Entführung (Phase 9) | Nicht umgesetzt | |
| Mimic (Stimmen-Imitator), Infizierte | Nicht umgesetzt | |
| Echte 3D-Modelle, Animationen, Sprachaufnahmen | Nicht umgesetzt | alle Inhalte sind Platzhalter |
| Online-Dienste (Steam/EOS, Lobby-Suche) | Nicht umgesetzt | nur direkte IP-Verbindung |

---

## Der erste Einsatz „Der letzte Funkspruch“

1. Funkgerät meldet sich → [E] annehmen, Zentrale schildert die Lage (Kontakt zum Forsthaus abgebrochen).
2. Auftrag am Funkgerät bestätigen (Wiederholen: [Q]).
3. Einsatzort auf der Wandkarte oder mit [M] ansehen.
4. Kamera und Taschenlampe einpacken (Kabine).
5. Turm verlassen, zum Geländewagen gehen, einsteigen, Motor starten.
6. Zum Forsthaus fahren – optional an der Tankstelle Pine Ridge tanken und an der Kasse zahlen.
7. Forsthaus betreten, beschädigte Möbel fotografieren, seltsame Spuren dokumentieren.
8. Das Notizbuch des Försters finden und lesen (mit [Q] mitnehmen).
9. „Sieh dich im Haus weiter um …“ – es klopft. Wer durch das Fenster schaut, sieht etwas.
   Die Tür schlägt zu, unter dem Fenster bleiben Abdrücke (seltener Beweis).
10. Haus verlassen, zum Wagen zurück, zum Turm fahren.
11. Fotos am Einsatzcomputer auswerten, Bericht abschicken → Auszahlung und Bewertung.
12. Nach einer Pause meldet sich die Zentrale mit Einsatz 2.

Die Kreatur am Fenster kann fotografiert werden (seltener Bonus). Schaut niemand hin, endet die
Sequenz trotzdem nach einiger Zeit (Klopfen an der Scheibe, Tür knallt), damit niemand festhängt.

---

## Steuerung

| Zu Fuß | | Im Fahrzeug | |
|---|---|---|---|
| W A S D | gehen | W / S | Gas / Bremse, rückwärts |
| Shift links | rennen | A / D | lenken |
| Strg links | ducken | Leertaste | Handbremse |
| Leertaste | springen | F | Motor an/aus |
| E | benutzen / einsteigen | L / K | Licht / Fernlicht |
| Q | alternative Aktion (mitnehmen …) | H | Hupe |
| F | Taschenlampe | Q / R | Blinker links / rechts |
| C | Kamera ausrüsten | X | Warnblinker |
| RMT / LMT | Sucher / Foto | V | Scheibenwischer |
| Mausrad / G | Zoom / Blitz | N, Pfeil links/rechts, Mausrad | Radio, Sender, Lautstärke |
| Tab oder I | Inventar | E | aussteigen (nur im Stand) |
| M / J / P | Karte / Aufgaben / Fotos | | |
| T (halten) | Sprechen (Koop) | | |
| F1 / Esc | Hilfe / Pause | | |
| F5 / F9 | speichern / laden (Host) | | |

Gamepad wird für Gehen, Fahren und Menüs unterstützt (Belegung in `Core/SUInputConfig.cpp`).

---

## Prüfungen

Beide Prüfungen laufen ohne Unreal Engine (Linux/macOS/WSL, `clang++` bzw. `g++`, `python3`):

```bash
# engine-unabhängige Logik: Welt-Layout, Beweisbewertung, Wirtschaft, Fahrzeugmodell,
# Horror-Director, Aufgaben, Audio-DSP  (aktuell 8861 Prüfungen, 0 Fehler)
bash SignalUnknown/Tests/Native/run_tests.sh
bash SignalUnknown/Tests/Native/run_tests.sh --preview   # schreibt eine Kartenvorschau (PPM)

# Syntaxprüfung des Unreal-Codes gegen eine Nachbildung der UE-API
SignalUnknown/Tools/SyntaxCheck/check.sh
```

Die Syntaxprüfung besteht aus drei Stufen:

1. `gen_uht.py` – Mini-„UHT“: erzeugt die `.generated.h`-Dateien und prüft UHT-/Replikationsregeln
   (RPCs, `ReplicatedUsing`, alle replizierten Variablen in `DOREPLIFETIME`, `AddDynamic` nur mit
   `UFUNCTION`, keine `int`-Typen in `UPROPERTY` …).
2. `clang++ -fsyntax-only -Wall -Wshadow-all -Werror` gegen `Tools/SyntaxCheck/mock/*.h`
   (inkl. Prüfung der `FString::Printf`/`UE_LOG`-Argumente).
3. `lint.py` – UTF-8-BOM (MSVC!), Sonderzeichen nur in `TEXT()`, Formatstrings, fehlende Engine-Includes.

**Grenzen:** Die Nachbildung enthält nur die Teile der Engine-API, die das Projekt verwendet, so wie
sie aus UE 5.x bekannt sind. Stimmt eine dort angenommene Signatur nicht mit der echten Engine
überein, findet die Prüfung das **nicht**. Ein erfolgreicher Durchlauf ist also kein Ersatz für einen
echten Build. Aktueller Stand: 54 Dateien ohne Fehler, 0 Regelverstöße, Lint ohne Befund.

---

## Architektur

```
Source/SignalUnknown/
  Logic/        engine-unabhängige Kernlogik (Welt-Layout, Beweise, Wirtschaft, Fahrzeug, Director, DSP)
  Core/         GameMode/State/Instance, PlayerController/State, Einstellungen, Eingabe, Speichern, Event-Bus
  World/        WorldBuilder (Terrain, Straßen, Wald, Himmel), Orte in Sites/ (Turm, Tankstelle, Forsthaus …)
  Interaction/  Interaktions-Interface, Türen, Lampen, Gegenstände, Dokumente, Möbel
  Player/       Charakter, Inventar, Interaktion, Fotokamera
  Evidence/     Beweis-Komponenten, Bewertung, Foto-Speicher, Laufzeit-Beweise
  Mission/      Missions-Datenbank, Missions-Manager (Funk, Aufgaben, Berichte), Bereiche
  Base/         Funkgerät, Einsatzcomputer, Wandkarte, Wetterradio
  Vehicle/      Geländewagen
  GasStation/   Zapfsäule, Kasse
  NPC/          Kassiererin, Anwohner
  Horror/       Horror-Director, Kreaturen, Skript-Sequenzen
  Audio/        prozedurale Klangerzeugung, Umgebung, Sprachchat-Einstellungen
  UI/           HUD und alle Bildschirme
```

Grundprinzipien:

* **Server-autoritativ.** Alle Interaktionen laufen als RPC zum Server; der Server prüft Entfernung
  und Zustand. Clients zeigen nur an.
* **Welt ohne Map-Asset.** `ASUWorldBuilder` erzeugt die Welt deterministisch auf jedem Rechner
  (`ASUGameState::PostInitializeComponents`); interaktive Objekte spawnt nur der Server, sie werden
  repliziert.
* **Event-Bus** (`USUEventSubsystem`): Aufgaben reagieren auf Ereignisse wie `Area.Entered`,
  `Photo.Category`, `Fuel.Refueled`, `Horror.Scripted` – Missionen sind reine Daten.
* **Logik testbar halten.** Rechenintensive oder regelbasierte Teile liegen in `Logic/` ohne
  Engine-Abhängigkeit und werden nativ getestet.

---

## Platzhalter, Assets und Lizenzen

* Es werden **keine fremden Assets** verwendet. Geometrie besteht aus `/Engine/BasicShapes`
  (Würfel, Kugel, Zylinder …) mit einfarbigen Materialien, Klänge werden zur Laufzeit synthetisiert.
* Das mitgeschickte Adobe-Stock-Bild (Wasserzeichen) diente **nur als Stimmungsreferenz** und ist
  nicht im Repository.
* Austausch gegen echte Inhalte: *Projekteinstellungen → Signal Unknown Art* (Baum-/Fels-/Busch-Meshes,
  Materialien). Häuser, Fahrzeug, Figuren und Kreaturen sind im Code markiert (`PLACEHOLDER`) und
  sollen später durch Static/Skeletal Meshes ersetzt werden.

---

## Bekannte Einschränkungen

* **Nicht mit Unreal Engine kompiliert, nicht gespielt.** Siehe oben.
* **Fahrphysik:** eigene Raycast-Federung mit Reibungskreis auf einem Chaos-Rigidbody statt
  *Chaos Vehicles* (braucht ein geriggtes Skeletal Mesh). Die Eingabeschnittstelle ist so gebaut,
  dass das Modell später ausgetauscht werden kann. Werte (Federn, Grip) müssen im Spiel abgestimmt werden.
* **Keine Navmesh:** NPCs und Kreaturen steuern direkt mit Hindernis-Sweeps. In dichtem Wald oder
  in Gebäuden können sie hängen bleiben (sie wechseln dann das Ziel bzw. ziehen sich zurück).
* **Sprachchat** benötigt ein funktionierendes Online-Subsystem (Null-Subsystem für LAN/IP ist
  eingetragen). Ungetestet.
* **Fotos** werden pro Rechner gespeichert; Teammitglieder sehen die Bilder anderer Spieler nur als
  Metadaten (Beweise zählen trotzdem für den Bericht).
* **Leistung:** ~45.000 Bäume als HISM, Lumen, virtuelle Schatten – auf schwächeren Rechnern das
  Grafik-Preset senken.
* Platzhaltergrafik, keine Animationen, keine Sprachaufnahmen.

---

## Nächste Schritte

1. Echten Build herstellen, Compilerfehler beheben, im Editor durchspielen (Checkliste oben).
2. Fahrzeug- und Kreaturwerte im Spiel abstimmen, Performance messen.
3. Platzhalter schrittweise durch lizenzierte/eigene Modelle, Animationen und Sprachaufnahmen ersetzen.
4. Einsatz 2 ausbauen, Walkie-Talkies, Spieler-Entführung, Polizei-Folgen, weitere Kreaturen.
