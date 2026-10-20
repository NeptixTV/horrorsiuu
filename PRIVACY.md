# Datenschutz – Goofy Studio

Stand: Version 1.2.0. Diese Datei beschreibt ehrlich und vollständig, welche Daten Goofy Studio speichert und welche
Verbindungen ins Internet die App aufbaut. Sie wurde anhand des Quellcodes und mit Netzwerktests (Browser und
Desktop-App) erstellt.

## Kurz gesagt

- **Deine Musik bleibt auf deinem Computer.** Projekte, Aufnahmen, Samples, Presets, Backups und Notizen werden nur
  lokal gespeichert. Es gibt kein Konto, keine Cloud und keinen Upload deiner Projekte.
- **Kein Tracking.** Keine Analyse-Tools, keine Telemetrie, keine Werbung, keine automatischen Absturzberichte.
- **Einzige Verbindung im Normalbetrieb:** die Prüfung auf Updates und Neuigkeiten bei GitHub – abschaltbar unter
  *Einstellungen → Datenschutz & Updates*. Sind beide Schalter aus, baut die App beim Start keine Verbindung auf.

## Alle Netzwerkverbindungen

| Wann | Ziel | Was wird gesendet | Zweck |
| --- | --- | --- | --- |
| Beim Start, wenn „Updates prüfen“ oder „Neuigkeiten anzeigen“ an ist, und bei *Help → Check for updates* | `api.github.com/repos/NeptixTV/horrorsiuu/contents/feed.json`, ersatzweise `raw.githubusercontent.com/NeptixTV/horrorsiuu/goofy-feed/feed.json` | Eine normale HTTPS-Anfrage (IP-Adresse, Browser-/App-Kennung „User-Agent“, ein Zeitstempel gegen Caching). Keine Projekt-, Nutzungs- oder Gerätedaten. | Kleine öffentliche Datei mit Neuigkeiten und der neuesten Versionsnummer laden |
| Nur nach Klick auf „Download & update“ | `github.com/NeptixTV/horrorsiuu/releases/download/…` (GitHub leitet zu seinem Download-Server weiter) | Normale Download-Anfrage | Update-Datei laden; sie wird vor dem Start mit ihrem SHA-256-Fingerabdruck geprüft |
| Nur im Admin-Bereich mit GitHub-Token (nur für den Herausgeber) | `api.github.com`, `uploads.github.com` – ausschließlich für dieses Repository | Der Token des Herausgebers, Nachrichtentexte, Release-Dateien | Neuigkeiten und Updates veröffentlichen |
| Nur wenn du einen Link anklickst | Der Link öffnet sich in deinem normalen Browser | – | z. B. Release-Seite |
| Nur Web-Version | Der Webserver, von dem die Seite geladen wird (z. B. GitHub Pages) | Normale Seitenabfrage | Die App selbst laden |

GitHub (bzw. der Webhost) sieht dabei wie bei jeder Webseite deine IP-Adresse. Es gelten die Datenschutzbedingungen
von GitHub: <https://docs.github.com/site-policy/privacy-policies/github-general-privacy-statement>.

**Nicht vorhanden:** Analytics, Telemetrie, Crash-Reporter, Werbe- oder Tracking-SDKs, Schriftarten oder Skripte von
fremden Servern (Inter und JetBrains Mono sind eingebaut), Cloud-Speicher.

**Desktop-App:** Die Rechtschreibprüfung von Chromium ist abgeschaltet, weil sie im Hintergrund Wörterbücher von
einem Google-Server laden würde (im Test nachgewiesen und damit abgestellt). Electron selbst sendet keine Telemetrie.

## Was lokal gespeichert wird

| Daten | Ort |
| --- | --- |
| Projekte, Aufnahmen, importierte Samples, automatische Backups | IndexedDB des Browsers bzw. des App-Profils |
| Einstellungen, eigene Presets, gesehene Neuigkeiten, zuletzt geöffnetes Projekt | `localStorage` |
| Admin-Token (nur Herausgeber, Desktop-App) | `admin-token.bin` im App-Datenordner, verschlüsselt mit dem Schlüssel des Betriebssystems (Windows: DPAPI). Ist keine Verschlüsselung verfügbar (manche Linux-Systeme ohne Schlüsselbund), liegt er dort nur mit Dateirechten geschützt. |
| Admin-Token (nur Herausgeber, Web-Version) | `sessionStorage` – wird beim Schließen des Tabs vergessen |
| Heruntergeladene Updates | Installer: eigener temporärer Ordner; portable Version: neben der laufenden .exe |

Alles davon lässt sich löschen: Projekte und Backups unter *Einstellungen → Projekte & Backups*, Einstellungen über
„Zurücksetzen“, alles zusammen durch Löschen der Website-Daten bzw. Deinstallation und Löschen des App-Datenordners.

## Mikrofon und MIDI

Mikrofon und MIDI-Geräte werden nur nach deiner Erlaubnis und nur innerhalb der App verwendet. Aufnahmen werden
lokal gespeichert. Das Mikrofon wird nach der Aufnahme wieder geschlossen (Einstellung *Mikrofon freigeben*).

## Diagnosebericht

*Einstellungen → Diagnose → Bericht kopieren* erstellt einen Text für Fehlermeldungen. Er enthält keine Gerätenamen
und keine Dateipfade, aber das Ereignisprotokoll kann Kanal- oder Spurnamen aus deinem Projekt enthalten. Er wird nur
in die Zwischenablage kopiert und nirgendwohin gesendet. Bitte vor dem Weitergeben kurz durchlesen.

## Grenzen dieser Aussage

Diese Beschreibung gilt für den Code dieser Version. Betriebssystem, Browser, Antivirus, Proxy oder Netzwerk können
unabhängig von Goofy Studio eigene Verbindungen aufbauen; das liegt außerhalb der App. Eine Garantie für Anonymität
kann keine Software geben.
