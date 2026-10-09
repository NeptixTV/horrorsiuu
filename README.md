<p align="center">
  <img src="public/icons/icon-192.png" width="120" alt="Goofy Studio Logo" />
</p>

<h1 align="center">Goofy Studio</h1>
<p align="center"><b>Eine vollständige Digital Audio Workstation (DAW) im Browser</b><br/>
Step-Sequencer · Playlist · Piano Roll · Mixer · Effekte · Synthesizer · Drum Machine · Mikrofon-Aufnahme · Pitch Correction</p>

---

## Windows-Programm (.exe)

Goofy Studio gibt es als Windows-Programm – mit eigenem Chromium (Electron), daher funktionieren Audio,
AudioWorklets und Mikrofon genau wie getestet.

- **`GoofyStudio-1.0.0-portable.exe`** – Doppelklick und los, keine Installation nötig.
- **`GoofyStudio-1.0.0-Setup.exe`** – Installer mit Desktop- und Startmenü-Verknüpfung.

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

## Architektur

```
electron/             Desktop-Hülle: main.cjs (Fenster, app://-Protokoll, Mikrofon-Rechte), afterPack.cjs (Icon in die .exe)
build/                App-Icons (icon.ico / icon.png)
src/
├── audio/            Audio-Engine (unabhängig von React)
│   ├── engine.ts       AudioContext, Master-Analyser, Aufnahme-Steuerung
│   ├── transport.ts    Look-Ahead-Scheduler (Web-Worker-Timer), Play/Pause/Stop/Loop
│   ├── sequencer.ts    reine Event-Berechnung aus Patterns & Playlist (getestet)
│   ├── mixgraph.ts     Mixer-Strips, Insert-Ketten, Sends, Routing, Kanäle → wird live UND offline genutzt
│   ├── input.ts        Mikrofon, Monitoring, Aufnahme
│   ├── render.ts       Offline-Mixdown + WAV-Encoder
│   ├── library.ts      synthetisierte Sample-Bibliothek
│   ├── effects/        EQ, Compressor, Reverb, Delay, Filter, Distortion, Chorus, Pitch + Beschreibungen/Presets
│   ├── instruments/    Synth, Drum-Voices, Sampler
│   └── worklets/       AudioWorklet-Prozessoren (Recorder, Pitch Correction)
├── state/            Datenmodell, Store mit Undo/Redo (zustand + immer), Aktionen, Routing-Logik
├── project/          IndexedDB-Speicher, Projekt-Lebenszyklus, Import/Export
├── midi/             Musiktheorie (Skalen, Notennamen, Frequenzen)
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

- Die **Formant**-Regelung von Goofy Tune ist eine klangliche Annäherung (Spektral-Tilt), keine echte
  Formant-Verschiebung. Tonhöhenerkennung und -korrektur selbst sind echt (in Tests: 452 Hz → 440 Hz).
- Browser liefern keine echte CPU-Auslastung; die Anzeige zeigt die Auslastung der UI-Frames, aktive Stimmen und Speicher.
- Die Buffer-Size bestimmt der Browser; einstellbar ist der Latenzmodus (Interactive / Balanced / Playback).
- Projekte werden im Browser (IndexedDB) gespeichert. Zum Sichern oder Weitergeben: *File → Export project file*.
