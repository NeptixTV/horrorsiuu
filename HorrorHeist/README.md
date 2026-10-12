# The Quiet Job – Koop-Horror-Heist (Unreal Engine 5)

> Hollowmere, irgendwann im Oktober. Eine Crew aus bis zu vier Einbrechern plant ihre Jobs im Keller unter
> **Marlowe's Waschsalon** (der Hehler betreibt nebenan „Brannigan & Sons Plumbing“ als Tarnung). Nachts fährt
> der Van zu den Häusern der Stadt. Die Häuser sind nicht leer – und manche sind es nie gewesen.

| Die Stadt (Draufsicht) | 14 Ashgrove Lane – das erste Ziel | Ashgrove Lane |
|---|---|---|
| ![](SourceArt/Previews/Town_Map.jpg) | ![](SourceArt/Previews/Town_Vance.jpg) | ![](SourceArt/Previews/Town_Ashgrove.jpg) |

| Wohnzimmer der Vances | Schlafzimmer (Ziel: Münzsammlung) | Das Marrow-Haus (vorbereitet) |
|---|---|---|
| ![](SourceArt/Previews/Vance_Living.jpg) | ![](SourceArt/Previews/Vance_Master.jpg) | ![](SourceArt/Previews/Town_Marrow.jpg) |

*(Vorschau-Renderings aus Blender/Cycles mit einer Taschenlampe an der Kamera; im Spiel rendert Unreal mit Lumen,
Volumetric Fog, eigenem Tag/Nacht-Himmel und den platzierten Lichtern.)*

Alle Modelle, Texturen, Sounds und Musikstücke wurden für dieses Projekt **selbst erzeugt** (prozedural per
Skript) – es gibt keine fremden Assets. Lizenzhinweise: `SourceArt/Audio/LICENSES_AUDIO.md`, Schriften unter
der SIL Open Font License (`Content/Slate/Fonts/Licenses`).

---

## Schnellstart

**Voraussetzungen:** Unreal Engine **5.5**, Visual Studio 2022 (17.8+) mit „Spieleentwicklung mit C++“,
„Desktopentwicklung mit C++“ und „.NET-Desktopentwicklung“ (Windows).

1. **`Build-HorrorHeist.bat`** doppelklicken (findet UE 5.5, erzeugt `HorrorHeist.sln`, kompiliert
   **HorrorHeistEditor** Win64 Development; Protokoll in **`BuildLog.txt`**).
2. Editor öffnen. Beim **ersten Start** fragt ein Dialog nach dem Aufbau der Inhalte → **Yes**. Das Setup
   (`Content/Python/hh_setup`) importiert alles aus `SourceArt/` (~256 Texturen, 175 Sounds, ~355 statische
   Meshes, 18 Skeletal-Meshes, 6 Animationen), erzeugt Materialien, baut **fünf Maps** (siehe unten) und danach alle Data Assets
   (Kosmetik, Ausrüstung, 24 Beutestücke, Jobs). Das dauert je nach Rechner 10–20 Minuten.
   - Am Ende erscheint eine Zusammenfassung; **jede Warnung** steht vollständig in
     **`Saved/HHSetup_Report.txt`** (und im Output Log, Filter „HH Setup“).
   - Jederzeit erneut ausführbar: Menü **The Quiet Job → Build / Rebuild All Content**. Ein erneuter Lauf
     ersetzt nur die Actors, die das Skript selbst platziert hat (Tag `HH_Generated`) – und zwar nur in der
     jeweiligen Map, nie in gestreamten Sub-Levels. Von Hand Hinzugefügtes bleibt erhalten, es entstehen keine
     Duplikate. Teilschritte: **Rebuild Maps Only**, **Rebuild Data Assets Only**.
3. **Play** (Map `L_Hideout`). Multiplayer im Editor: Play-Dropdown → *Number of Players* 2–4,
   *Net Mode* **Play As Listen Server**.

> **Ehrlicher Hinweis:** Code und Setup-Skript wurden ohne installierte Unreal Engine geschrieben und gegen die
> UE-5.5-APIs und Build-Regeln geprüft – aber **nicht mit einem echten Compiler übersetzt und nicht im Editor
> gespielt**. Siehe Abschnitt *Stand & Grenzen*.

---

## Die Maps

| Map | Inhalt |
|---|---|
| `L_Hideout` | Persistente Hub-Map: das Versteck im Keller (Jobboard, Werkbank/Loadout, Spiegel/Kosmetik, Hehler-Shop, Überwachungs-Pult, **neuer Planungstisch** mit dem Grundriss des Vance-Hauses, Spinde, Van-Stellplatz), Uhr/Himmel, Licht. Streamt `L_Town`. |
| `L_Town` | Die Stadt Hollowmere, als **always-loaded Streaming-Level** in jeder persistenten Map: Main Street mit Waschsalon (über dem Versteck), Brannigan & Sons, Diner, Kleiderladen, Eisenwarenladen, Museum (mit dem Gemälde „The Lantern Bearer“ hinter einem beleuchteten Schaufenster), öffentlicher Parkplatz; Ashgrove Lane, Marrow Street, Church Road mit St.-Agnes-Kirche und Friedhof, Founders' Park mit Pavillon; 9 Nachbarhäuser, die drei Job-Häuser, Laternen, Autos, Bäume, Zäune, Hecken, Hydranten, Briefkästen, Mülltonnen, Schilder, Pfützen/Laub-Decals, unsichtbare Begrenzungswände und Wald am Stadtrand. |
| `L_Job_Vance` | **Erster Job, voll spielbar.** Heist-GameMode, Nacht-Uhr, Van am Bordstein vor 14 Ashgrove Lane, Crew-Startpunkte, 25 Beutestücke, durchsuchbare Behälter, Ersatzschlüssel, Spuk (Director, bewegliche Gegenstände, Geräte, Erscheinung). |
| `L_Job_Marrow` | Vorbereitet (Van, Starts, Hausvolumen, Spuk-Director; noch keine Beute). Job auf dem Board als „wird ausgekundschaftet“ gesperrt. |
| `L_Job_Rectory` | Vorbereitet wie Marrow. |

**Nicht betretbare Gebäude sagen das ehrlich:** Läden sind nachts geschlossen (gemalte Inneneinrichtung hinter
dem Glas), Nachbarhäuser haben zugezogene Vorhänge, abends erleuchtete Fenster und abgeschlossene Türen
(„Nobody lets you in…“ bzw. auf einem Job „Not tonight's job“). Es gibt keine leeren, scheinbar betretbaren
Hüllen.

### Die Häuser

* **A – 14 Ashgrove Lane (Familie Vance), spielbar:** zwei Etagen + Keller. Erdgeschoss: Diele mit Standuhr,
  Wohnzimmer mit Kamin, Esszimmer, Arbeitszimmer, Küche mit Hintertür, Hinterflur; Obergeschoss: Elternschlafzimmer,
  Gästezimmer, Kinderzimmer, Bad; Keller mit Ölheizung, Waschmaschine, Werkbank und einer alten Truhe.
  Einstiege: **Vordertür** (Dietrich, leise, oder der **Ersatzschlüssel** unter dem Gartenzwerg), **Hintertür**
  (klemmt: Brecheisen = **laut**, oder Glasschneider = leise). Von innen lassen sich die Türen aufschließen.
* **B – Das Marrow-Haus (Marrow Street), vorbereitet:** viktorianisch, seit 1974 leer, vernagelte Fenster,
  verwilderter Garten, Möbel unter Staubtüchern, Licht im Dachfenster. Erdgeschoss eingerichtet, Obergeschoss
  im Rohbau.
* **C – St.-Agnes-Pfarrhaus (Church Road), vorbereitet:** größeres Steinhaus hinter Eisenzaun und Hecke,
  **besser gesichert** (Vordertür nur mit Schlüssel, Hintertür mit langem Dietrich-Vorgang). Erdgeschoss
  eingerichtet, Obergeschoss im Rohbau.

---

## Ein Einbruch – der Ablauf

1. **Jobboard (PLAY):** Der Crew-Leader pinnt *The Vance Residence*. Nicht freigegebene Jobs zeigen
   „still being cased“ und lassen sich nicht pinnen. Jobs starten **nur nachts** (20:00–05:00); tagsüber
   erscheint **WAIT UNTIL DARK** (Crew-Leader) und spult die Uhr bis zur Dämmerung vor.
2. **Ready-Check** (Van-Schiebetür oder `R`) → Countdown → Server-Travel aller Spieler nach `L_Job_Vance`
   (die Uhrzeit wird mitgenommen).
3. **Einsteigen:** Ziele erscheinen oben links. Lärm (Brecheisen, Klappern an Türen, Rennen) erhöht die
   **Nachbarschafts-Alarmstufe** (oben Mitte); bei 100 % ruft jemand die Polizei – danach läuft ein Countdown.
4. **Beute:** **5 Inventarplätze pro Spieler, Werkzeuge eingeschlossen** (Licht, Einbruchwerkzeug, Utility,
   Gadget). Platz schaffen = Gegenstand fallen lassen (`G`). Halten-Interaktionen (Dietrich, Durchsuchen, schwere
   Beute) werden serverseitig validiert. Der Fernseher ist schwer (beide Hände, langsamer, kein Sprinten).
   Der Wert jedes Gegenstands wird beim Jobstart auf dem Server ausgewürfelt und im HUD angezeigt.
5. **Der Spuk eskaliert** – je länger die Crew im Haus ist, je mehr sie mitnimmt und je größer sie ist: erst
   Knarren und ferne Geräusche, dann erlöschende Lichter, sich öffnende Türen, verstellte Gegenstände, ein
   Bildschirm voller Rauschen, Flüstern, fallendes Glas, eine Gestalt am Ende des Flurs … und ganz am Ende die
   Jagd. Verfluchte Beute (das Medaillon im Keller) beschleunigt das massiv. Psyche sinkt im Dunkeln und allein,
   bei 0 wird man ohnmächtig, verliert die getragene Beute und wacht draußen am Van auf.
6. **Abhauen:** Beute im Van verstauen (Heckklappe). Erst wenn **alle** am Van sind, kann gefahren werden
   (Fahrertür, gedrückt halten). Ergebnis: *Success* (Mindestbeute erreicht), *Partial*, *Caught* (Polizei)
   oder *Abandoned* (Crew-Leader bricht im Pausenmenü ab).
7. **Auswertung:** Karte mit gesicherter/verlorener Beute, Anteil pro Spieler (+20 % mit dem Hauptziel),
   XP, Dauer und Spukstufe → automatische Rückkehr ins Versteck (morgens), Geld und XP sind gespeichert;
   im Versteck erscheint noch einmal eine kurze Nachbesprechung.

---

## Tag & Nacht

`AHHTimeOfDay` (eine pro persistenter Map) treibt Sonne, Mond, Himmel (Sky Atmosphere), Skylight, Nebel,
**Straßenlaternen**, **erleuchtete Fenster** und Tag/Nacht-Ambience (Vögel ↔ Grillen). Server-autoritativ:
Es repliziert nur ein Anker (Uhrzeit + Server-Zeit + Rate), alle Clients rechnen selbst. Standard: ein Tag
dauert 48 Minuten, das Spiel beginnt um 16:30; Jobs laufen mit 35 % Geschwindigkeit, damit die Nacht reicht.
Tempo einstellbar unter *Settings → Gameplay → World → Time Speed*; Uhr im HUD oben rechts.

---

## Steuerung

| Aktion | Tastatur/Maus | Gamepad |
|---|---|---|
| Bewegen / Umsehen | WASD / Maus | Linker / rechter Stick |
| Sprinten / Ducken / Springen | Shift / Strg / Leertaste | L3 / B / A |
| Interagieren (Halten bei Dietrich, Durchsuchen …) | E | X |
| Inventarplatz wählen | 1–5, Mausrad | Steuerkreuz ← / → |
| Gegenstand fallen lassen | G | LB |
| Taschenlampe | F | Steuerkreuz ↑ |
| Push-to-Talk | V | RB |
| Loadout / Jobboard / Ready | Tab / M / R | Y / View / Steuerkreuz ↓ |
| Ich- ↔ Third-Person | T | R3 |
| Menü / Pause | Esc | Menu |

Alle Tastatur-Belegungen sind unter *Settings → Controls* änderbar.

**Entwickler-Konsolenbefehle** (`~`, Host): `HHGiveCash 5000`, `HHGiveXP 10000`, `HHUnlockAll`,
`HHResetProfile`, `HHForceAmbientEvent 0..6`, `HHSetTime 21.5`, `HHTimeSpeed 10`, `HHAddAnger 30`,
`HHHauntEvent 0..11` (0 Settle, 1 DistantSound, 2 LightOut, 3 DoorOpen, 4 ObjectMoved, 5 DoorSlam,
6 DeviceOn, 7 Flicker, 8 Whisper, 9 GlassFall, 10 Apparition, 11 Hunt), `HHEndJob 1..4`
(Success/Partial/Caught/Abandoned), `hh.DebugNoise 1`.

---

## Testen im Editor (Checkliste)

1. `L_Hideout` öffnen, Play (Listen Server, 2 Spieler). Erwartung: beide Spieler stehen sichtbar im Keller;
   im Ich-Modus sieht man den eigenen Körper (Kopf ausgeblendet) und die Hand mit dem Licht; `T` zeigt den
   ganzen Charakter. *Settings → Audio*: Master/Music/SFX/Ambient hörbar wirksam, Werte bleiben nach Neustart.
2. Treppe hoch → **Stahltür „Go outside“** → Parkplatz des Waschsalons → Main Street → Ashgrove Lane.
   Vance-Haustür: „Locked – Nobody lets you in. Come back with the crew after dark.“
3. `HHTimeSpeed 30`: Dämmerung, Laternen und Fenster gehen an, Grillen ersetzen Vögel.
4. Zurück zum Jobboard: Vance pinnen → **WAIT UNTIL DARK** → beide **Ready** → Reise nach `L_Job_Vance`.
5. Im Job: Gartenzwerg durchsuchen → Schlüssel → Vordertür aufschließen; Beute aufheben (Slots füllen sich,
   Wert im HUD), Kommode oben durchsuchen (Münzsammlung), mit `HHAddAnger 60` den Spuk vorspulen,
   Beute am Van verstauen, alle zum Van, Fahrertür halten → Auswertung → Rückkehr ins Versteck.

---

## Projektaufbau

```
HorrorHeist/
├─ Source/HorrorHeist/        C++ (ein Runtime-Modul, Präfix HH)
│  ├─ Core/        Typen, GameInstance, GameData, GameModeBase (Spawn/Travel), Noise-Bus, Developer Settings
│  ├─ Data/        Item-/Kosmetik-/Ausrüstungs-/Beute-/Missions-/Charakter-Definitionen + Registry
│  ├─ Progression/ Profil-SaveGame, Geld, XP, Besitz, Loadouts, Job-Ergebnisse
│  ├─ Settings/    GameUserSettings-Erweiterung, Enhanced-Input-Subsystem (Laufzeit-Actions)
│  ├─ Audio/       Lautstärke-Routing (Sound-Mix je Kategorie), UI-Sounds, Musik, Captions
│  ├─ Online/      Sessions (Host/Suche/Beitritt/IP)
│  ├─ Lobby/       Hub-GameMode (Ready, Nacht-Prüfung, Abreise) und GameState
│  ├─ Heist/       Heist-GameMode/-GameState (Regeln, Ziele, Alarm, Auszahlung), Beute, Behälter, Van,
│  │               Hausvolumen, Begrenzung, Spuk-Director, Spuk-Requisiten/-Geräte, Erscheinung
│  ├─ Player/      Charakter, Controller, PlayerState, Inventar (5 Plätze), AnimInstance (Kopf/Hand-IK),
│  │               Kosmetik, Interaktion (Halten), Schritte
│  ├─ Interaction/ Interaktions-Interface, Stationen, Türen mit Schlössern, Schalter, Radio, Van
│  ├─ World/       Tageszeit, Praxis-Lichter, Ambience-Director, Gewitter, Emitter, Vorschau-Mannequin
│  └─ UI/          Slate-Stil + alle Menüs und das Heist-HUD (keine UMG-Assets nötig)
├─ Content/Python/ init_unreal.py + hh_setup (Import, Materialien, Maps, Data Assets, Bericht)
├─ SourceArt/      Generierte Quell-Assets (FBX, PNG/JPG, WAV) + Layout-JSONs + Katalog-JSON
└─ Tools/AssetGen/ Generatoren: audio/, textures/, blender/ (Props, Stadt, Häuser, Maps)
```

**Architektur:** kein GameManager-Monolith. Jedes System ist ein Subsystem, eine Komponente oder ein kleiner
Actor mit klarer Aufgabe; Kommunikation über Delegates. Häuser und Jobs sind **daten- und tag-getrieben**: Ein
neues Haus braucht kein C++ – nur einen Missions-Eintrag (Map, Haus-Tag, Mindestbeute, Hauptziel) und eine Map
mit getaggten Actors (`HH_HouseLight`, `HH_HouseDoor`, `HH_HauntPoint`, `HH_ApparitionPoint`, `HH_WakePoint`,
Hausvolumen, Van-Punkte). Der Server validiert alle Anfragen (Interaktionen, Haltezeiten, Inventar, Reisen).

### Inhalte neu erzeugen (optional)

Python 3.11+ mit `numpy scipy pillow bpy` (bpy = Blender als Modul, getestet mit 5.2):

```bash
cd HorrorHeist
python Tools/AssetGen/audio/gen_sfx.py && python Tools/AssetGen/audio/gen_music.py && python Tools/AssetGen/audio/gen_world_sfx.py
python Tools/AssetGen/textures/gen_textures.py && python Tools/AssetGen/textures/gen_unique.py
python Tools/AssetGen/textures/gen_world_textures.py && python Tools/AssetGen/textures/gen_world_images.py
python Tools/AssetGen/textures/gen_blueprint.py
python Tools/AssetGen/blender/catalog.py
python Tools/AssetGen/blender/items.py && python Tools/AssetGen/blender/render_loot_icons.py
python Tools/AssetGen/blender/build_world.py [--render]      # Versteck + Stadt + Häuser + Job-Maps
python Tools/AssetGen/blender/build_character.py --preview   # Körper, Animationen, Kosmetik, Icons
```

`build_world.py` ist deterministisch: ein zweiter Lauf ändert keine Datei (Meshes werden nur exportiert, wenn
sich ihr Inhalts-Hash geändert hat). Danach im Editor **The Quiet Job → Build / Rebuild All Content**.

---

## Stand & Grenzen

- **Implementiert:** alles oben Beschriebene (C++, Python-Setup, Assets, Maps-Layouts).
- **Kompiliert:** *nein* – in dieser Umgebung gibt es keine Unreal Engine. Der Code wurde statisch geprüft
  (Signaturen, Includes, Variablen-Verdeckung als Fehler, Unity-Build-Kollisionen), die Layouts automatisch gegen
  die C++-Properties und Asset-Listen abgeglichen, und die Blender-Generatoren laufen hier vollständig durch.
- **Im Spiel getestet:** *nein*. Nur die Blender-Vorschaubilder wurden geprüft. Belichtung, Lichtstärken,
  Lautstärken, Spuk-Timing und Bewegungsgefühl werden im Editor Feintuning brauchen.
- **Voice-Chat:** nutzt die VoIP-Schnittstelle des Online Subsystems (Push-to-Talk/Offenes Mikro, Lautstärke über
  die Voice-Klasse) – ohne echte Mehrspieler-Sitzung ungetestet.
- **Jobs 2 und 3** (Marrow, Pfarrhaus) haben Häuser und vorbereitete Maps, aber noch keine Beute/Spuk-Elemente;
  sie sind auf dem Board bewusst gesperrt. Whitlock hat noch kein Haus.
- **Upscaler** (DLSS/FSR/XeSS) erscheinen nur mit installiertem Plugin.
