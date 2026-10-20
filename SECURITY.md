# Sicherheitsbericht – Goofy Studio 1.2.0

Prüfdatum: 20.10.2026 · Geprüft: Quellcode, komplette Git-Historie, Build-Ausgabe (`dist/`), gepacktes Windows-Programm
(`win-unpacked`, `app.asar`), Abhängigkeiten, CI-Workflows. Diese Prüfung wurde sorgfältig gemacht, ist aber **kein
Beweis, dass die App fehlerfrei oder vollständig sicher ist**. Was nicht geprüft werden konnte, steht ausdrücklich in
Abschnitt 4 und 7.

Risikostufen: **Kritisch** – sofort ausnutzbar mit schweren Folgen · **Hoch** – schwerer Schaden möglich, braucht eine
Voraussetzung · **Mittel** – begrenzter Schaden oder schwer auszunutzen · **Niedrig** – geringe Auswirkung / Härtung.

---

## 1. Untersuchte Bereiche

| Bereich | Was genau |
| --- | --- |
| A Persönliche Daten | Alle 204 versionierten Dateien; die vollständige Historie aller 5 Remote-Branches und des PR-Refs; Commit-Autoren; Build-Ausgabe; Inhalt von `app.asar`; Metadaten aller PNG/ICO-Dateien; Logs und Diagnosebericht |
| B Code-Schwachstellen | Electron-Hauptprozess (`main.cjs`), Preload-Brücke, alle IPC-Kanäle (`ipc.cjs`), eigenes `app://`-Protokoll, Navigation, Berechtigungen, CSP, Update-Download/-Installation, Admin-Upload, Token-Speicher, Import von Projekten / Audio / MIDI / Presets / Feed, Renderer-Senken (`innerHTML`, `eval` …), temporäre Dateien, Worker/Worklets |
| C Geheimnisse | Muster für GitHub-, AWS-, Google-, OpenAI-, Slack-Tokens, private Schlüssel, Passwort-Zuweisungen, `.env`/`.pem`/`.key`-Dateien – in Dateien **und** Historie |
| D Abhängigkeiten | `npm audit`, `npm outdated`, Alter neuer Versionen, Lizenzen der ins Programm gebündelten Pakete |
| E Datenschutz / Netzwerk | Alle `fetch`-/Netzwerk-Aufrufe im Code; gemessene Verbindungen von Browser- und Desktop-App über den Proxy; Telemetrie/Crash-Reporting; lokale Speicherorte |
| F Installation / Veröffentlichung | `electron-builder.json`, Installer-Rechte, Inhalt des Pakets, Debug-Reste, Sourcemaps, Fuses, ASAR-Integrität, Signatur, Update-Kette, CI-Rechte, Lizenzhinweise |

## 2. Gefundene Schwachstellen

| # | Stufe (bei Fund) | Befund |
| --- | --- | --- |
| S1 | **Hoch** | `update:install` startete **jede beliebige .exe**, deren Pfad die Oberfläche übergab → Code-Ausführung, falls die Oberfläche kompromittiert ist. |
| S2 | **Hoch** | `gh:upload` konnte **beliebige lokale Dateien** (z. B. Dokumente, Schlüssel) in ein öffentliches GitHub-Release hochladen → Datenabfluss. |
| S3 | **Hoch** | Keine Navigationssperre und keine Absenderprüfung: Hätte eine fremde Seite das Hauptfenster übernommen (Link, Weiterleitung), hätte sie die komplette Desktop-Schnittstelle bekommen (Token benutzen, Downloads, Programme starten). |
| S4 | Mittel | `gh:request` erlaubte mit dem Admin-Token beliebige GitHub-API-Pfade und -Methoden (auch fremde Repos, Konto-Daten, `DELETE`). |
| S5 | Mittel | Update-Download: beliebige github.com-URL, keine Prüfsumme, Dateiname durfte z. B. `version.dll` sein (DLL-Hijacking neben der portablen .exe), vorhersagbarer Pfad im gemeinsamen Temp-Ordner. |
| S6 | Mittel | Web-Version ohne Content-Security-Policy; Feed-Links ungeprüft (eine `javascript:`-URL im Feed wäre in der Web-Version per `window.open` gelaufen). |
| S7 | Mittel | Admin-Token der Web-Version dauerhaft und unverschlüsselt im `localStorage`. |
| S8 | Mittel | Präparierte MIDI-Datei (riesige Zeitabstände, zehntausende Spuren) konnte die App einfrieren und ein riesiges Autosave erzeugen (DoS). |
| S9 | Mittel | CI baute bei **jedem** Push mit Schreibrechten aufs Repository; ein kompromittiertes npm-Paket hätte sie beim `npm ci` nutzen können. |
| S10 | Niedrig | `app://`-Protokoll prüfte den Pfad per `startsWith` ohne Trennzeichen (`dist-andere/…` wäre durchgegangen). |
| S11 | Niedrig | Mikrofon-/MIDI-Rechte ohne Prüfung, welche Seite fragt. |
| S12 | Niedrig | `app:info` gab den vollständigen Programmpfad an die Oberfläche (enthält unter Windows den Benutzernamen). |
| S13 | Niedrig (Datenschutz) | Versteckte Verbindung der Desktop-App zu Google (`redirector.gvt1.com`, Rechtschreib-Wörterbücher) – im Test gemessen. |
| S14 | Niedrig | Electron-Fuses nicht gesetzt (`ELECTRON_RUN_AS_NODE`, `NODE_OPTIONS`, `--inspect` wirksam; keine ASAR-Integritätsprüfung). |
| S15 | Niedrig | Debug-Objekt `window.goofy` auch in Release-Builds. |
| S16 | Info | PNG-Dateien enthielten Erstellungszeitstempel; Build-Artefakt `tsconfig.tsbuildinfo` war versioniert; Diagnose-Hinweis versprach „keine persönlichen Daten“, obwohl Kanalnamen vorkommen können. |

Zusätzlich (Stabilität, keine Sicherheitslücke): AudioWorklet-Effekte (Autotune, Voice, Gate) wurden bei jedem
Projektwechsel nie freigegeben – gemessen 5 Worklets + rund 30 weitere Audio-Knoten pro 10 Öffnungen.

## 3. Behobene Probleme

| # | Behebung | Nachweis |
| --- | --- | --- |
| S1 | Nur Dateien, die der Hauptprozess in dieser Sitzung selbst heruntergeladen hat, dürfen gestartet werden. | Electron-Test: `install(beliebige.exe)` → `false`, `install(C:\Windows\…\calc.exe)` → `false` |
| S2 | Upload nur von echten `.exe`-Dateien (MZ-Kopf, Größenlimit), nur mit Release-Dateinamen, nur ins eigene Repo. | Electron-Test: Textdatei → abgelehnt; fremdes Repo → abgelehnt; `version.dll` → abgelehnt |
| S3 | `will-navigate`/`will-redirect` blockiert, keine `<webview>`, neue Fenster nur als https-Link im System-Browser; **jeder** IPC-Aufruf prüft, dass er vom App-eigenen Fenster und Ursprung `app://goofy` kommt. | Electron-Test: Navigation zu example.com → bleibt auf `app://goofy/index.html`; `window.open` → kein neues Fenster |
| S4 | Nur Pfade unter `/repos/NeptixTV/horrorsiuu`, keine `..`, nur GET/POST/PUT/PATCH, Größenlimit. | Electron-Test: fremdes Repo, `/user`, `../`, `DELETE` → abgelehnt; eigenes Repo → 200 |
| S5 | Nur `github.com/NeptixTV/horrorsiuu/releases/download/…`, nur `GoofyStudio-*.exe`, eigener privater Temp-Ordner, **SHA-256-Prüfung** gegen den Feed (der Admin-Upload schreibt die Prüfsumme jetzt automatisch in den Feed), beschädigte Downloads werden gelöscht. Releases aus der CI bekommen zusätzlich `SHA256SUMS.txt`. | Electron-Test: fremdes Repo / DLL → abgelehnt |
| S6 | CSP für die Web-Version (Skripte/Worker nur aus der App, Netzwerk nur App + GitHub-Feed, `object-src 'none'` …); Desktop-CSP verschärft (Oberfläche darf gar nicht mehr ins Netz, alles über den Hauptprozess); Feed-Parser lässt nur `https:`-Links, gültige Versionen und begrenzte Texte durch. | Browser-Test: Worklets, Worker, Feed funktionieren, `fetch(example.com)` blockiert, 0 CSP-Verstöße; Unit-Test mit `javascript:`/`file:`-Links |
| S7 | Web-Token nur noch im `sessionStorage` (weg beim Schließen des Tabs); alte Kopie im `localStorage` wird gelöscht. | Code |
| S8 | MIDI-Import: max. 1000 Takte, 48 Spuren, 200 000 Noten; Dateien > 16 MB abgelehnt. | Unit-Tests mit präparierten Dateien (Note bei Takt 600 000, 120 Spuren) |
| S9 | Build-Job nur Lesezugriff; nur der separate Release-Job bei `v*`-Tags darf schreiben. `ci.yml` explizit nur lesend. | Workflow-Dateien |
| S10 | Pfadprüfung mit Trennzeichen, Host-Prüfung, 400/403/404 statt Absturz. | Electron-Test: `%2e%2e%2f…` → 403, unbekannte Hosts blockiert |
| S11 | Rechte nur für `app://goofy`. | Electron-Test: Mikrofon-/MIDI-Anfrage der App weiterhin erlaubt (Fehler nur „kein Gerät“ im Container) |
| S12 | Programmpfad wird nicht mehr herausgegeben. | Electron-Test: `info()` ohne Pfad |
| S13 | Rechtschreibprüfung in der Desktop-App abgeschaltet. | Proxy-Messung: danach keine Verbindung zu Google mehr |
| S14 | Fuses gesetzt: RunAsNode aus, NODE_OPTIONS aus, `--inspect` aus, **ASAR-Integritätsprüfung an**, App nur aus ASAR, keine `file://`-Sonderrechte, Cookie-Verschlüsselung an. | Windows-Build hier erzeugt und geprüft: Integritäts-Ressource vorhanden und passt zum ASAR-Hash; Fuse-Werte ausgelesen |
| S15 | `window.goofy` nur im Entwicklungsmodus oder nach bewusstem Setzen von `localStorage['goofy-debug']`. | Code |
| S16 | PNG-Metadaten entfernt (Pixel identisch), `tsbuildinfo` aus Git entfernt, Diagnose-Hinweis ehrlich formuliert. | `compare`: 0 abweichende Pixel |
| Leck | Alle Worklet-Prozessoren beenden sich auf eine `dispose`-Nachricht; Effekte und Recorder senden sie beim Entfernen. | 40 Projektöffnungen: Audio-Knoten konstant 321 |

## 4. Noch offene Punkte

| Stufe | Punkt | Empfehlung |
| --- | --- | --- |
| Mittel | **Keine Code-Signatur.** Windows SmartScreen warnt, und Nutzer können nicht prüfen, ob die .exe wirklich von dir ist. | Code-Signing-Zertifikat (OV/EV), Azure Trusted Signing oder (für Open Source) SignPath. Danach `signAndEditExecutable` aktivieren und vor dem Update-Start die Signatur prüfen. |
| Mittel | **Update-Vertrauen hängt an deinem GitHub-Konto.** Wer Konto oder Token übernimmt, kann ein schädliches Update samt passender Prüfsumme an alle verteilen. | 2-Faktor-Anmeldung bei GitHub, Fine-grained Token nur für dieses Repo (nur „Contents: Read and write“), kurze Laufzeit, Token nur auf deinem eigenen PC. Langfristig: signierte Updates (siehe oben). |
| Mittel | **Electron 44.7.0** – es gibt bereits 44.8/44.9 mit Chromium-Korrekturen; diese sind aber erst wenige Tage alt (Lieferketten-Vorsicht: nichts unter 2 Wochen ungeprüft einspielen). | Kurz vor der Veröffentlichung auf den dann aktuellen 44.x-Patch aktualisieren und neu testen. |
| Niedrig | `npm audit`: 8 × moderat (electron-builder-Kette → `@electron/get` → `global-agent` → `roarr` → `sprintf-js`) und 3 × hoch (`braces`/`micromatch` über `vite-plugin-singlefile`). **Nur Build-Werkzeuge**, nichts davon steckt im ausgelieferten Programm; die Angriffe bräuchten von außen gesteuerte Muster/Formatstrings im Build. | `npm audit fix` würde mehrere 0–5 Tage alte Pakete einspielen (u. a. `pkijs` vom Prüftag) – bewusst zurückgestellt. Ab ca. 31.10.2026 aktualisieren und Build erneut prüfen. |
| Niedrig | Unter Linux ohne Schlüsselbund liegt der Admin-Token nur dateigeschützt (0600) statt verschlüsselt. Windows nutzt DPAPI. | Admin-Funktionen nur unter Windows/macOS nutzen. |
| Niedrig | Die CSP erlaubt `style-src 'unsafe-inline'` (React-Inline-Styles). Keine Skript-Ausführung möglich, nur CSS. | Akzeptabel. |
| Niedrig | Das Admin-Fenster („187“) kann jeder öffnen. Schreiben geht nur mit deinem Token – die Sicherheit hängt am Token, nicht am Code. | Akzeptabel; nie einen Token in der App anderer Leute eingeben. |
| Niedrig | Dein GitHub-Name **NeptixTV** und der Repo-Name stehen in der App und in den Paketdaten (nötig für Updates). | Wenn du anonymer veröffentlichen willst: eigene GitHub-Organisation für die App anlegen und Feed/Repo dorthin umziehen. |
| Info | CI-Aktionen sind über Versions-Tags (`@v4`) statt Commit-SHAs eingebunden. | Für maximale Lieferketten-Sicherheit auf SHAs festnageln. |
| Info | Das Projekt hat **keine eigene Lizenzdatei** – rechtlich heißt das „alle Rechte vorbehalten“. | Bewusst eine Lizenz wählen (z. B. MIT, GPL oder proprietär) und `LICENSE` anlegen. Für eine öffentlich erreichbare Web-Version in Deutschland ggf. Impressum/Datenschutzerklärung prüfen (keine Rechtsberatung). |

**Keine bekannten kritischen oder hohen Sicherheitsprobleme sind offen.**

## 5. Persönliche Informationen in Release-Dateien

**Gefunden: keine.** Geprüft wurden alle versionierten Dateien, die komplette Git-Historie aller Branches, die
Commit-Metadaten (alle Commits: `Claude <noreply@anthropic.com>`, keine private E-Mail), `dist/`, das gepackte
`app.asar` und die Bild-Metadaten. Keine E-Mail-Adressen, keine Windows-Benutzerordner, keine absoluten Pfade, keine
internen Server, keine Sourcemaps, keine Debug-Logs. Sichtbar und gewollt: der GitHub-Name `NeptixTV` (Update-Quelle).
Einziger Metadaten-Rest waren Zeitstempel in PNG-Dateien – entfernt. **Nicht lokal geprüft:** die fertige NSIS-Setup.exe
und die portable .exe aus der Windows-CI (sie enthalten dasselbe `app.asar`, der Installer-Rahmen kommt von
electron-builder).

## 6. Abhängigkeiten mit Sicherheitsrisiken

- **Im Programm gebündelt:** react, react-dom, scheduler, zustand, immer (MIT), Schriften Inter und JetBrains Mono
  (SIL OFL 1.1), Electron/Chromium (MIT bzw. BSD u. a.). Für diese meldet `npm audit` **keine** Lücken. Alle Lizenzen
  erlauben die Weitergabe; die vollständigen Texte liegen jetzt automatisch als `THIRD_PARTY_NOTICES.txt` bei
  (Electron/Chromium liefern `LICENSE.electron.txt` und `LICENSES.chromium.html` mit).
- **Nur Build-Werkzeuge:** siehe Abschnitt 4 (11 Meldungen, nicht im Programm).
- Keine verdächtigen oder unnötigen Pakete gefunden: Es gibt keine Laufzeit-`node_modules` im Paket, und die
  Entwicklungsabhängigkeiten sind wenige, bekannte Projekte.

## 7. Tatsächlich durchgeführte Tests

- TypeScript-Typprüfung (strikt) und **72 Unit-Tests** in 13 Dateien, darunter Feed-Bereinigung, präparierte MIDI-Dateien,
  Projekt-Reparatur, Kompatibilität mit einem echten 1.1-Projekt.
- Produktions-Build, Single-File-Build, **Windows-Build** (hier erzeugt) mit Prüfung von Fuses und ASAR-Integrität;
  Inhalt von `app.asar` geprüft.
- **Electron-Angriffstest** gegen jeden IPC-Kanal (16 Fälle), Navigation, `window.open`, Pfad-Traversal,
  Berechtigungen.
- Netzwerkmessung über den Proxy für Browser- und Desktop-App; CSP-Test im Browser und in Electron.
- Geheimnis- und Personendaten-Scan über Dateien und komplette Historie; Bild-Metadaten-Analyse.
- `npm audit`, `npm outdated`, Veröffentlichungsdaten neuer Versionen.
- End-to-End-Tests der Funktionen (Studio, Effekte, Presets, DreamChord, Ideen-Würfel, Notizen, Capture), 8-Minuten-
  Dauertest, 40 bzw. 100 Projektöffnungen, Engine-Neustart, Schutzstufe (Spitzen, Runaway, NaN).
- CI auf GitHub (Linux-CI und Windows-.exe-Build) für alle Pushes: erfolgreich.

**Nicht durchführbar in dieser Umgebung:** die .exe auf einem echten Windows-PC starten (Fuses/Integritätsprüfung zur
Laufzeit, SmartScreen, Installer), echte Audio-Hardware (Kopfhörer, Bluetooth, Gerätewechsel – nur simuliert), ein
echter Update-Durchlauf mit veröffentlichtem Release, macOS.

## 8. Schritte vor einer öffentlichen Veröffentlichung

1. Windows-Build aus der CI auf einem echten PC testen: Installation **ohne** Admin-Rechte, Start, Audio mit Kopfhörern,
   Mikrofon, Projekt speichern/öffnen, Update-Benachrichtigung.
2. Electron auf den dann aktuellen 44.x-Patch heben (Version mindestens 2 Wochen alt), danach Schritt 1 wiederholen.
3. Lizenz für dein Projekt festlegen (`LICENSE`), ggf. Impressum/Datenschutzerklärung für die Web-Version.
4. GitHub absichern: 2-Faktor-Anmeldung, Fine-grained Token nur für dieses Repo, Ablaufdatum setzen.
5. Code-Signatur einrichten (empfohlen; ohne sie warnt Windows).
6. Ab Ende Oktober die Build-Werkzeuge aktualisieren (`npm audit fix`), Build und Tests wiederholen.
7. Ein Release erst über einen `v1.2.0`-Tag erstellen, wenn 1–4 erledigt sind – die App wurde **nicht** veröffentlicht,
   und es wurden keine Tags oder Feed-Einträge erstellt.

**Einschätzung:** Es sind keine bekannten kritischen oder hohen Probleme offen, und alle hier möglichen Prüfungen waren
erfolgreich. Weil der Test auf einem echten Windows-PC (Schritt 1) noch fehlt, ist die App **noch nicht als
veröffentlichungsbereit freigegeben** – nach den Schritten 1–4 ist sie es aus Sicht dieser Prüfung.
