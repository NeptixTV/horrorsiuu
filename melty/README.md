# Veröffentlichung auf Melty

Diese Anleitung beschreibt, wie das Hello-Neighbor/Minecraft-Mashup auf [Melty](https://melty.gg)
landet. Melty ist aus der Cloud-Umgebung nicht erreichbar (ebenso die Minecraft- und
Prism-Launcher-Server), deshalb laufen Build, Test und Upload auf deinem eigenen PC.

## Wie Melty funktioniert

- Veröffentlicht wird nicht über die Website, sondern über Melty's **MCP-Server**
  (`https://melty.gg/api/mcp`). Den Zugang mit deinem persönlichen Token bekommst du in
  Melty auf der *Create*-Seite über den Button für den **Publish-Prompt**. Der Token
  erlaubt, in deinem Namen zu veröffentlichen: er gehört **nie** ins Repo, in Logs oder
  Screenshots. Widerrufen kannst du ihn in *Melty Studio → Coding agents*.
- Melty listet nur **Mods und Mashups bestehender Spiele**, die sich mit **einem Klick
  auf Play** starten lassen. Ein eigenständiges Spiel oder ein Browser-Spiel (auch eine
  Unreal-Engine-Web-Version) wird **nicht** angenommen.
- Spieleinhalte (Modelle, Texturen, Sounds, Level) werden nie hochgeladen. Das Mashup liest
  sie zur Laufzeit aus der installierten Kopie des Spielers.

## Aufbau dieses Mashups für Melty

| Rolle im Recipe | Spiel | Was es beiträgt |
| --- | --- | --- |
| `primary` (Host) | Minecraft: Java Edition (`minecraft-java`) | Der Spieler spielt hier. Melty installiert hinein und startet es. |
| `companion` | Hello Neighbor (`custom-…`-Slug aus `search_games`) | Echte Inhalte (Nachbar, Haus, Sounds …), gelesen aus dem Ordner `{game:<slug>}` des Spielers. |

Hello Neighbor ist noch nicht in Melty's Katalog und kann deshalb **nicht Host** sein.
Wenn der Spieler direkt *in* Hello Neighbor spielen soll, geht das auf Melty derzeit nicht.

Das Release-Paket enthält:

1. **Prism Launcher (portable)** mit einer Instanz, deren `mmc-pack.json` den Loader nennt
   (Fabric, NeoForge, Forge oder Quilt). Prism installiert ihn beim ersten Start;
   `recipe.setup` startet Prism einmal, damit der Spieler sich mit seinem Minecraft-Konto
   anmeldet.
2. Die **Mod-Datei (.jar)** im Ordner `{minecraft-instance}/mods` dieser Instanz.
3. Fremde Bibliotheken nur, wenn ihre Lizenz die Weitergabe erlaubt, mit Credits.

Vorbild ist das Mashup **SkyCraft** (Skyrim + Minecraft), abrufbar mit `mashup_info skycraft`.
Müssen beide Spiele gleichzeitig laufen, kommt `recipe.together` dazu.

Wichtige Ordner-Token im Recipe: `{minecraft-instance}/mods`, `/config`, `/resourcepacks`,
`/saves`, `{game:<slug>}` für Hello Neighbor, `{localappdata}`, `{port}`, `{address}`.

## Ablauf auf deinem PC

Am einfachsten: In Claude Code (Desktop-App oder `claude` im Terminal) im Ordner dieses
Repos den aktuellen **Publish-Prompt aus Melty** einfügen und dahinter den Text aus
[`publish-prompt.md`](publish-prompt.md) anhängen. Claude erledigt dann diese Schritte:

1. **Verbinden** und `list_my_mods` aufrufen; vorhandenen Entwurf wiederverwenden.
2. **Spiele prüfen** mit `search_games` und `game_info` (Hello-Neighbor-Slug, Loader,
   Risiken, Install-Pfade) sowie `search_mashups` (gibt es schon etwas Ähnliches?).
3. **Bauen und testen**: Mod bauen, Prism-Instanz zusammenstellen, im echten Spiel testen.
4. **Paket prüfen**: `inspect_package` → `validate_recipe` → `one_click_check`, bis alles
   grün ist.
5. **Entwurf anlegen**: `create_mod` mit Titel, Tagline, Beschreibung, Spielen,
   Credits/Lizenz, Remix-Erlaubnis und `githubRepo: NeptixTV/horrorsiuu`. Die `modId`
   notieren.
6. **Hochladen**: `start_upload` (Größe + SHA-256) → PUT → `finish_upload`, dann
   `submit_release`.
7. **Screenshot** aus dem laufenden Spiel: `add_screenshot` → PUT → `finish_screenshot`.
8. **Veröffentlichen** erst nach deinem OK: `publish`, danach `mod_status` prüfen. Live ist
   es nach Melty's kurzer Sicherheitsprüfung.

Ist das geprüfte Recipe fertig, kann es als `melty.json` ins Repo-Wurzelverzeichnis.
Dann liest Melty die Einrichtung direkt aus GitHub und jedes neue Release veröffentlicht
sich selbst. Diese Datei erst anlegen, wenn `validate_recipe` sie bestätigt hat.

## Mehrspieler (falls gewünscht)

Ein Mehrspieler-Mashup wird gleich als solches eingetragen (`recipe.multiplayer`):

- `maxPlayers`: echte Höchstzahl pro Spiel.
- `connect.address`: Die Mod schreibt beim Hosten eine Zeile wie `Hosting at <adresse>`
  in eine Logdatei, Melty liest sie dort aus.
- `connect.joinArgs` / `joinEnv` / `joinFile`: So bekommt ein Mitspieler die Adresse beim
  Start. Umgebungsvariablen müssen mod-eigene sein (z. B. `HNCRAFT_JOIN`).
- Keine Portweiterleitung verlangen; Relay/Lobby nutzen. Vor dem Eintragen mit zwei
  laufenden Kopien testen.

## Was noch fehlt

- [ ] Spielkonzept und Mod-Grundgerüst (eigener Thread im Projekt)
- [ ] Spielbarer Build der Mod und eine Prism-Instanz mit Loader
- [ ] Recipe, geprüft mit `validate_recipe` und `one_click_check`
- [ ] Titel, Tagline, Beschreibung, Credits, Lizenz und Remix-Erlaubnis
- [ ] Echter Screenshot oder Clip aus dem laufenden Spiel
