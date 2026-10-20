<p align="center">
  <img src="public/icons/icon-192.png" width="120" alt="Goofy Studio Logo" />
</p>

<h1 align="center">Goofy Studio</h1>
<p align="center"><b>Eine vollständige Digital Audio Workstation (DAW) im Browser</b><br/>
Step-Sequencer · Playlist · Piano Roll · Mixer · Effekte · Synthesizer · Drum Machine · Mikrofon-Aufnahme · Pitch Correction</p>

---

## Neu in Version 1.2.0

**Audio-Stabilität (der große Fix)**
- Behoben: Knistern/Stottern, plötzliche starke Verzerrung, „kaputte“ Geräusche und Kopfhörer, die in der App
  verstummen. Ursachen waren u. a. ein Delay-Rückkopplungsfilter mit Resonanzspitze (Feedback konnte sich endlos
  aufschaukeln), Nachhol-Schübe des Schedulers nach Rucklern, ungültige Werte (NaN), die die Wiedergabe anhielten,
  ein offen gebliebenes Mikrofon (Bluetooth-Kopfhörer springen dann in den schlechten Telefonmodus) und Audiodaten,
  die beim Öffnen vieler Projekte nie freigegeben wurden.
- Neue **Schutzstufe vor dem Ausgang**: entfernt ungültige Samples, Brickwall-Limiter mit Look-ahead, schaltet bei
  Runaway-Signalen stumm und baut den betroffenen Teil automatisch neu auf. Watchdog bei Geräte- und Kopfhörerwechsel.
- **Diagnose** in den Einstellungen (Zähler, Ereignisprotokoll, sicherer Audio-Reset, Kopfhörer-Test).

**DreamChord** – Akkord-Instrument (inspiriert von Pocket-Akkordsynths): 7 Akkord-Pads, Live-Joystick (Septime, None,
sus, Dur/Moll-Tausch …), Display, Knöpfe, 14 verträumte Presets (Ocean Dream, Nostalgic Memories, Midnight Beach,
Warm Sunset, Dreamy Piano, Space Ambient, Retro Synth, Lo-Fi Cassette …), Arpeggiator, Strum, Hold, eigene
Klangerzeugung mit Chorus, Delay, Reverb, Filter, ADSR und Tape-Charakter. Gespieltes wird aufgenommen und ist
danach normal im Piano Roll bearbeitbar.

**Studio**
- Piano Roll: Akkord-Stempel, Skalen-Sperre, Werkzeuge (Transponieren, Humanize, Legato, Strum, Arpeggio,
  Umkehren, Spiegeln, Velocity-Verläufe), Kopieren/Einfügen, Pfeiltasten-Bearbeitung, Noten live einspielen.
- **Automation** für Lautstärke, Pan und jeden Effekt-Regler (live und beim Export).
- **Export**: WAV 16/24/32 Bit, Samplerate, Bereich, Ausklang, Normalisieren, **Stems als ZIP**, **MIDI** (Export & Import).
- Neue Effekte **Tape Machine**, **Stereo Width** (mit Goniometer) und **Noise Gate**; eigene Presets für jeden
  Effekt, Synth und DreamChord; Spitzenpegel-Anzeige im Mixer.
- Undo-Verlauf zum Springen, Marker in der Playlist, Spuren verschieben/einfügen/duplizieren, Clips kopieren/einfügen.

**Projekte**: automatische Backups mit Wiederherstellen, Projekt-Doktor repariert beschädigte Dateien beim Öffnen,
Speicherübersicht und Aufräumen, 1.1-Projekte öffnen unverändert.

**Einstellungen**: neues Einstellungs-Center mit 9 Seiten (Audio & Geräte, Performance, Diagnose, Aufnahme & MIDI,
Design, Projekte & Backups, Allgemein, Datenschutz & Updates, Über).

**Eigene Ideen in 1.2**
- 🎲 **Ideen-Würfel** (Alt+I): würfelt Akkorde, Bass, Melodie und Beat passend zu Tonart und Tempo – Teile sperren,
  den Rest neu würfeln, Strg+Z holt die vorige Idee zurück.
- ⏺ **Capture** (Alt+C): Die App merkt sich, was du in den letzten 3 Minuten gespielt hast – auch ohne Aufnahme –
  und macht daraus auf Knopfdruck ein Pattern.
- 📝 **Song-Notizen mit Teleprompter** (Alt+N): Lyrics im Projekt; beim Aufnehmen groß und mitlaufend, springt zu
  `[Chorus]`, wenn der Playhead einen Marker „Chorus“ erreicht.
- „Neu in 1.2“-Tour beim ersten Start nach dem Update.

## Neu in Version 1.1.0

- **Admin-Panel** (irgendwo in der App `187` tippen): Nachrichten an alle Nutzer schicken und Updates veröffentlichen.
  Beim nächsten Start sehen alle ein Popup – bei Updates mit „Download & update“ / „Not now“ und deinem Text, was neu ist.
- **Automatische Updates** ab 1.1.0: Die App lädt das neue Update selbst herunter und installiert es
  (Installer-Version) bzw. startet die neue portable .exe. *Die alte 1.0.0 hat noch keinen Update-Code – wer sie
  hat, muss 1.1.0 einmal selbst herunterladen.*
- **Rechtsklick auf einen Sound in der Playlist → Settings…**: Lautstärke exakt in dB, Pan, Speed, Reverse,
  Fade-in/out, Normalize und eigene Effektkette nur für diesen Clip – inkl. Ein-Klick-**Sound FX**
  (Unterwasser, Bass Boosted, Roboter, Chipmunk, Monster, Alien, Telefon, Radio, Megaphon, 8D-Audio, Höhle,
  Stadion, Lo-Fi, Vaporwave, Nightcore …) und Autotune-Varianten. Alles lässt sich **live beim Abspielen** ändern.
- **Jede Reihe hat ihren eigenen Mixer-Kanal** (Zahnrad oder Rechtsklick auf den Reihennamen → Settings): Lautstärke,
  Pan, Solo/Mute, Effekte. Reihen mit Sounds bekommen ihren Mixer-Kanal automatisch.
- **Sauberes Schneiden**: Das Schere-Werkzeug schneidet *genau* an der Mausposition (gestrichelte Vorschaulinie),
  Verschieben und Auf-/Zuziehen folgen exakt der Maus – nichts springt an die Kante eines anderen Clips (Modus *Free*,
  Standard). Wer im Takt bleiben will, stellt *Grid* ein (Alt = frei). Jeder Schnitt bekommt Mini-Fades, damit nichts knackt.
- **Song Splitter** (File-Menü oder *My Library*): Song reinziehen → **Vocals**, **Instrumental (ohne Stimme)**,
  **Beat/Drums** und **Melodie** trennen, anhören und als eigene Spuren in die Playlist legen.
- **My Library + Sample-Editor**: eigene Sounds importieren, umbenennen, löschen, bearbeiten (Bereich auswählen,
  Trim, Cut, Fade, Normalize, Reverse, Pitch) und Effekte vorhören & fest einrechnen.
- **Mikrofon-Live-FX**: Stimmverzerrer, Autotune-Varianten und alle Effekte live auf dem Mikrofon.
- **6 neue Effekte**: Voice Changer, Phaser, Flanger, Tremolo, 8D Auto-Pan, Limiter. Goofy Tune kann jetzt zusätzlich transponieren.
- **50 neue Sounds**: Trap-/House-Drums, Percussion, Piano, Gitarre, Glocke, Orgel, Chor, Sirene, Air Horn,
  Game-Sounds und lustige Sounds (Quack, Furz, Slide Whistle, Ba-Dum-Tss, Sad Trombone, Muh …).

## Admin-Panel: Nachrichten & Updates verschicken

Die App liest beim Start die Datei `feed.json` aus dem Branch **`goofy-feed`** dieses (öffentlichen) Repos.
Schreiben darf nur, wer deinen GitHub-Schlüssel hat – der Code `187` öffnet nur das Fenster.

**Einmalig: Schlüssel (Token) anlegen**
1. GitHub → *Settings → Developer settings → Personal access tokens → Fine-grained tokens → Generate new token*
   (oder im Admin-Panel unter 🔑 Access auf „Create token on GitHub“ klicken).
2. *Repository access*: „Only select repositories“ → `NeptixTV/horrorsiuu`.
3. *Permissions → Repository permissions → Contents*: **Read and write**.
4. Token kopieren, in Goofy Studio `187` tippen → 🔑 Access → einfügen → *Save & test*.
   Der Token wird verschlüsselt nur auf deinem PC gespeichert und ist nicht Teil der App, die du weitergibst.

**Nachricht senden:** 📢 Send news → Titel + Text → *Send to everyone*. Erscheint bei allen beim nächsten Start.

**Update veröffentlichen:**
1. In `package.json` die Version erhöhen (z. B. `1.2.0`), Änderungen pushen.
2. GitHub baut die neuen .exe-Dateien (Actions → „Build Windows .exe“ → *Artifacts*) – herunterladen und entpacken.
3. In der App `187` → ⬆️ Publish update → Version eintragen, „What's new“ schreiben,
   `Setup.exe` und/oder `portable.exe` reinziehen → *Publish update*.
   Die App legt dafür automatisch ein GitHub-Release an und lädt die Dateien hoch.
4. Alle mit Version 1.1.0 oder neuer bekommen beim nächsten Start das Update-Popup.

## Windows-Programm (.exe)

Goofy Studio gibt es als Windows-Programm – mit eigenem Chromium (Electron), daher funktionieren Audio,
AudioWorklets und Mikrofon genau wie getestet.

- **`GoofyStudio-<version>-portable.exe`** – Doppelklick und los, keine Installation nötig.
- **`GoofyStudio-<version>-Setup.exe`** – Installer mit Desktop- und Startmenü-Verknüpfung (empfohlen für automatische Updates).

Beide werden automatisch vom Workflow **„Build Windows .exe“** gebaut (GitHub → *Actions* → letzter Lauf →
*Artifacts* → `GoofyStudio-Windows`). Wird ein Tag wie `v1.0.0` gepusht, landen sie zusätzlich als Release auf GitHub.

Selbst bauen:

```bash
npm install
npm run dist:win      # → release/GoofyStudio-<version>-portable.exe (+ Setup.exe unter Windows)
npm run desktop       # Desktop-Version direkt starten (Windows, macOS, Linux)
```

> Hinweis: Die .exe ist nicht code-signiert. Windows SmartScreen zeigt beim ersten Start evtl.
> „Der Computer wurde durch Windows geschützt“ → *Weitere Informationen* → *Trotzdem ausführen*.
> Unter Linux/macOS gebaut entsteht nur die portable .exe; der Installer braucht Windows (oder Wine).

Projekte, Einstellungen und Aufnahmen speichert die Desktop-App im Benutzerprofil
(`%APPDATA%\Goofy Studio`).

## Web-Version (Entwicklung)

Voraussetzung: [Node.js](https://nodejs.org) 20 oder neuer.

```bash
npm install
npm run dev        # startet die App auf http://localhost:5173
```

Weitere Befehle:

| Befehl | Zweck |
| --- | --- |
| `npm run build` | Typecheck + Produktions-Build nach `dist/` |
| `npm run build:single` | **Eine einzige HTML-Datei** (`dist-single/index.html`), die man ohne Server öffnen/teilen kann |
| `npm run preview` | Den Build lokal testen |
| `npm test` | Unit-Tests (Sequencer, Undo/Redo, Routing, Musiktheorie) |

Die Web-Version ist außerdem als PWA installierbar (Chrome/Edge → „Installieren“).

**Online stellen (GitHub Pages):** In den Repo-Einstellungen *Settings → Pages → Source: GitHub Actions* wählen und
den Workflow „Deploy to GitHub Pages“ unter *Actions* starten.

Empfohlene Browser: aktuelles Chrome oder Edge (beste Web-Audio-Unterstützung, Ausgabegeräte-Auswahl).
Firefox und Safari funktionieren ebenfalls; die Auswahl des Audio-Ausgangs ist dort ggf. nicht verfügbar.

## Features

**Start & Oberfläche**
- Animierter Splash-Screen mit Logo (schwebt nach oben, Glow-Effekte, Ladefortschritt), danach Fade-/Slide-in der Oberfläche
- Einrichtungsassistent beim ersten Start: Audio-Eingang, Audio-Ausgang, Sample-Rate, Buffer-Size, Theme
- Dunkles Studio-Design mit Themes: **Dark, Dark Blue, Purple, Neon, Custom** – Akzentfarbe und alle Oberflächenfarben frei wählbar
- Verschiebbare Panels (Browser-Breite, Dock-Höhe), schwebende Plugin-Fenster, Kontextmenüs überall

**Toolbar / Transport**
- Play/Pause, Stop, Record, Pattern-/Song-Modus, BPM (ziehen, scrollen oder tippen), Taktart, Metronom
- Positionsanzeige (Takt:Beat:Step oder Zeit), Master-Volume, Master-Peakmeter, Mini-Oszilloskop
- Performance-Anzeige (UI-Last, aktive Stimmen, Speicher), Projektname & Speicherstatus, Undo/Redo, Speichern/Öffnen

**Channel Rack / Step Sequencer**
- Beliebig viele Kanäle (Synth, Drum, Sampler) mit Mute, Solo, Volume, Pan, Kanalfarbe, Mixer-Routing
- Klickbare, animierte Steps (malen per Ziehen, Rechtsklick löscht, Mausrad = Velocity), 8–64 Steps, Swing
- Lauflicht beim Abspielen, Mini-Piano-Roll-Vorschau, Fill/Shift/Clear, Patterns anlegen/klonen

**Playlist / Arrangement**
- Spuren mit Pattern-Clips und Audio-Clips (Waveform), Zeichnen, Verschieben (auch spurübergreifend),
  Länge ändern, links trimmen, Schneiden, Duplizieren (Shift-Drag), Marquee-Auswahl
- Snap-to-Grid (Alt umgeht Snap), Zoom (Strg + Mausrad), horizontales/vertikales Scrollen, Follow-Playhead
- Loop-Bereich (Shift/Rechts-Ziehen im Lineal), Klick ins Lineal setzt die Position
- Drag & Drop von Patterns, Samples und Audiodateien vom Desktop

**Piano Roll**
- Keyboard links (klickbar), Notengitter über 9 Oktaven, Noten setzen/verschieben/verlängern/löschen
- Velocity-Lane, Snap (bis 1/32 und Triolen), Quantisieren, Zoom horizontal/vertikal
- Tonart-/Skalen-Hervorhebung, Ghost-Notes anderer Kanäle, Playback-Cursor, Vorhören beim Bearbeiten

**Audio-Engine (Web Audio API + AudioWorklet)**
- Sample-genauer Look-Ahead-Scheduler (Timer in einem Web Worker), Pattern- und Song-Modus, Tempo-Änderungen live
- Mixer mit Insert-Ketten, Sends, freiem Routing (Feedback-Schleifen werden verhindert), Solo-Logik inkl. Busse
- Mikrofon: Geräteauswahl, Input-Gain, Input-Meter, Monitoring durch einen Mixer-Insert, Aufnahme per
  AudioWorklet mit Live-Waveform und Latenzkompensation – das Ergebnis landet als Audio-Clip in der Playlist
- **WAV-Export** (Song oder Pattern) per OfflineAudioContext

**Goofy Tune – Pitch Correction**
- Echtzeit-Tonhöhenerkennung (YIN) + Pitch-Shifting im AudioWorklet
- Key, Scale (12 Skalen), Correction-Amount, Retune Speed, Humanize, Formant, Wet/Dry, Bypass
- Live-Tuner (Note, Cent-Abweichung) und Verlaufsgrafik Eingang vs. korrigiert
- Fertige Vocal-Chains: *Hard Tune Rap, Natural Pop Vocal, Lo-Fi Radio Voice, Robot Monster*

**Effekte** – jeder mit eigener Oberfläche und Live-Visualisierung
- **EQ** (5 Bänder + Gain, ziehbare Kurve, Live-Spektrum) · **Compressor** (Threshold, Ratio, Attack, Release,
  Knee, Makeup, Kennlinie + Gain-Reduction) · **Reverb** (Room Size, Decay, Pre-Delay, Damping, Wet/Dry)
- **Delay** (Time/Tempo-Sync, Feedback, Tone, Ping-Pong, Mix) · **Filter** (LP/HP/BP, Cutoff, Resonance, LFO)
- **Distortion** (Soft/Hard/Fold/Bitcrush, Drive, Tone, Mix) · **Chorus** (Rate, Depth, Delay, Spread, Mix)
- 20 Effekt-Presets

**Instrumente & Sounds**
- **Goofy Synth**: 2 Oszillatoren (Sine/Saw/Square/Triangle, Oktave, Halbton, Detune), Unison, Filter
  (LP/HP/BP, Cutoff, Resonance, Envelope), ADSR, LFO (Pitch/Filter/Amp), Glide, 11 Presets
- **Goofy Drum (DM-7)**: Kick, Snare, Clap, Hi-Hat, Open Hat (mit Choke), Tom, Perc, Rim, Cowbell, Shaker, Crash –
  jeweils mit Tune, Decay, Tone, Snap; eigene Drum-Machine-Ansicht mit Pads und Lauflicht
- **Goofy Sampler**: spielt jedes Sample chromatisch (Root, Tune, Attack, Release, Reverse)
- Sample-Bibliothek mit 22 Sounds (Drums, 808, Reese, Pads, Plucks, Vocal-„Ooh/Aah“, Riser, Impact, Loops …).
  **Alle Sounds werden beim Start synthetisiert** – keine fremden Samples, keine Lizenzprobleme.
  Eigene Audiodateien lassen sich in „My Samples“ importieren.

**Visualisierung**: Spektrum-Analyzer, Spektrogramm, Oszilloskop, Goniometer (Stereobild), VU-Meter, Peak-Meter
pro Mixer-Kanal, Waveforms, Playback-Cursor.

**Projekte**: Neu, Speichern, Speichern unter, Öffnen, Autosave (alle 45 s), Undo/Redo (150 Schritte),
Projektdatei exportieren/importieren (`.goofy.json` inkl. Aufnahmen). Gespeichert wird lokal in IndexedDB.

**Eingabe**: Computer-Tastatur als Klavier (Z–M / Q–U), Web-MIDI-Keyboards werden automatisch erkannt.

## Tastenkürzel

| Taste | Aktion |
| --- | --- |
| Leertaste | Play / Pause |
| R | Aufnahme |
| L | Pattern-/Song-Modus |
| Strg/Cmd + S / Strg + Shift + S | Speichern / Speichern unter |
| Strg/Cmd + Z / Strg + Shift + Z (oder Strg + Y) | Undo / Redo |
| Strg/Cmd + A | Alles auswählen (Playlist oder Piano Roll) |
| Strg/Cmd + D | Auswahl duplizieren |
| Entf / Backspace | Auswahl löschen |
| M / S | Ausgewählten Kanal (bzw. Mixer-Insert) muten / solo |
| F6 / F7 / F9 / F10 / F11 / F12 | Channel Rack / Piano Roll / Mixer / Drum Machine / Aufnahme & Tune / Analyzer |
| Strg + Mausrad · Alt + Ziehen · Shift + Ziehen | Zoom · Snap umgehen · Kopieren |
| Alt + C / Alt + I / Alt + N | Capture / Ideen-Würfel / Song-Notizen |
| Strg + C / X / V (Playlist, Piano Roll) | Kopieren / Ausschneiden / Einfügen |
| Pfeiltasten (Piano Roll) | Auswahl transponieren / verschieben |

## Architektur

```
electron/             Desktop-Hülle: main.cjs (Fenster, app://-Protokoll, Mikrofon-Rechte), preload.cjs + ipc.cjs
                      (Admin-Token, GitHub-Upload, Update-Download/-Installation), afterPack.cjs (Icon in die .exe)
build/                App-Icons (icon.ico / icon.png)
src/
├── audio/            Audio-Engine (unabhängig von React)
│   ├── engine.ts       AudioContext, Master-Analyser, Aufnahme-Steuerung
│   ├── transport.ts    Look-Ahead-Scheduler (Web-Worker-Timer), Play/Pause/Stop/Loop
│   ├── sequencer.ts    reine Event-Berechnung aus Patterns & Playlist (getestet)
│   ├── mixgraph.ts     Mixer-Strips, Insert-Ketten, Sends, Routing, Kanäle → wird live UND offline genutzt
│   ├── input.ts        Mikrofon, Monitoring, Aufnahme
│   ├── render.ts       Offline-Mixdown, Stems, WAV-Encoder (16/24/32 Bit)
│   ├── automation.ts   Automation von Mixer- und Effektparametern (live + offline)
│   ├── safety.ts / diagnostics.ts  Wertebegrenzung, Fehler- und Ereignisprotokoll
│   ├── library.ts      synthetisierte Sample-Bibliothek
│   ├── effects/        EQ, Compressor, Reverb, Delay, Filter, Distortion, Chorus, Pitch, Voice Changer, Phaser,
│   │                   Flanger, Tremolo, 8D, Limiter + Presets und Sound-FX-Ketten (fxPresets.ts)
│   ├── stems/          Song-Trennung (Mitten-Extraktion + HPSS im Web Worker)
│   ├── clipPlayback.ts Abspielen eines Audio-Clips (Fades, Speed, Reverse, Clip-Effekte)
│   ├── instruments/    Synth, Drum-Voices, Sampler, DreamChord-Synth
│   └── worklets/       AudioWorklet-Prozessoren (Recorder, Pitch Correction, Ausgangsschutz, Noise Gate)
├── state/            Datenmodell, Store mit Undo/Redo (zustand + immer), Aktionen, Routing-Logik
├── admin/            News-/Update-Feed (lesen für alle, schreiben per GitHub-API fürs Admin-Panel)
├── project/          IndexedDB-Speicher, Projekt-Lebenszyklus, Import/Export
├── midi/             Musiktheorie, Akkorde, Noten-Werkzeuge, MIDI-Dateien, Capture, Live-Aufnahme, Ideen-Würfel
├── ui/               React-Komponenten: toolbar, browser, playlist, channelrack, pianoroll, mixer, effects,
│                     instruments, drums, vocal, visualizers, dialogs, setup, splash, components
└── styles/           Theme-System und globale Styles
```

Designprinzipien:
- **Projekt = unveränderlicher Datenbaum.** Jede Änderung erzeugt per immer einen neuen Baum mit struktureller
  Teilung. Daraus ergeben sich billiges Undo/Redo, gezielte Re-Renders und ein schneller Abgleich mit der Audio-Engine
  (die Engine vergleicht nur Referenzen und baut ausschließlich geänderte Teile neu).
- **Keine Audio-Daten im React-State.** Playhead, Meter und Visualisierungen laufen über eine gemeinsame
  `requestAnimationFrame`-Schleife und zeichnen direkt auf Canvas bzw. ins DOM.
- **Playlist und Piano Roll sind Canvas-basiert** (statische Ebene + Overlay), dadurch bleiben auch viele Clips flüssig.
- **Kontinuierliche Gesten** (Knopf drehen, Clip ziehen) werden zu einem einzigen Undo-Schritt zusammengefasst.
- Die DSP-Klassen sind gekapselt (`Effect`, `Instrument`), sodass sich später leicht stärkere Engines (z. B. WASM)
  einsetzen lassen.

## Ehrliche Hinweise zu Grenzen

- Die **Formant**-Regelung (Goofy Tune, Voice Changer) ist eine klangliche Annäherung (Spektral-Tilt), keine echte
  Formant-Verschiebung.
- Der **Song Splitter** arbeitet mit klassischer Signalverarbeitung, nicht mit einer KI. Er funktioniert am besten bei
  Stereo-Studiosongs mit mittig gemischter Stimme; zwischen den Spuren bleibt etwas „Übersprechen“. Mono-Songs lassen
  sich kaum trennen.
- Das Update-System nutzt GitHub: Ohne Internet gibt es keine News/Updates, die App funktioniert aber normal. Tonhöhenerkennung und -korrektur selbst sind echt (in Tests: 452 Hz → 440 Hz).
- Browser liefern keine echte CPU-Auslastung; die Anzeige zeigt die Auslastung der UI-Frames, aktive Stimmen und Speicher.
- Die Buffer-Size bestimmt letztlich der Browser bzw. das Betriebssystem; einstellbar sind Latenzmodus und eine
  Wunsch-Latenz in ms, die der Browser aber nur als Hinweis nimmt.
- Der **Ideen-Würfel** ist ein regelbasierter Generator (Musiktheorie + Zufall), keine KI.
- Die Noise Gate braucht AudioWorklets (alle aktuellen Browser und die Desktop-App haben sie); ohne sie lässt sie das
  Signal unverändert durch.
- Die Windows-.exe ist **nicht code-signiert** (dafür braucht es ein kostenpflichtiges Zertifikat); Windows
  SmartScreen kann deshalb beim ersten Start warnen.
- Projekte werden im Browser (IndexedDB) gespeichert. Zum Sichern oder Weitergeben: *File → Export project file*.
