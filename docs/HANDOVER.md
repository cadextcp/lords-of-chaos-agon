# Übergabe: Stand und nächste Schritte

> Stand: 2026-10-03 · **M0–M3 vollständig**, **M4a–M4c gemergt** (#53, #54, #56) plus Review-Fixes (#57) · CI grün · 320 Selftest-Checks
> Für die nächste Person bzw. den nächsten Agenten. Zuerst `CLAUDE.md` lesen (Regeln, Befehle), dann dieses Dokument.

---

## 1. Kurzstatus

| Milestone | Status |
|---|---|
| **M0 Fundament** | ✅ Toolchain, Tests, CI, Docs |
| **M1 Grafik und Eingabe** | ✅ Software-seitig fertig (#1, #4, #5, #6). Offen: #2 (optional), #3 und #7 (brauchen echte Hardware) |
| **M2 Core-Skelett** | ✅ #13–#18: Terrains, Kreaturen, Rundenablauf, Sicht/Hidden Map, Luft-/Bodenebene, Bump/Look |
| **M3 Classic spielbar** | ✅ #27–#33 und #41: Kampf, Beschwörungen, Bolt/Lightning, Objekte/Waffen, Portal/VP, KI, Szenario 1; Review-Fixes und Regeln D21 |
| **M4 Classic komplett (v1.0)** | 🔨 3 von 10 Teilen: ✅ M4a (#43), ✅ M4b (#44), ✅ M4c (#45), Review-Fixes #57. Offen: M4d–M4j (#46–#52) |
| M5+ Chaos | geplant, siehe `docs/ROADMAP.md` |

**Was heute läuft (Emulator):**
- **Spielstart:** `loc` startet Szenario 1 „The Many Coloured Land“ (36×36, Wrap-around, Kartenformat v3 mit Portal). Die Zauberbücher kommen aus der Szenario-Datei (`data/scenarios/`). Eine ganze Partie gegen einen KI-Zauberer ist durchspielbar.
- **Rundenablauf:**
  - `Tab`/`Shift+Tab` wählt eine Einheit, `Leertaste` beendet sie, `Shift+E` (zweimal) beendet den Zug.
  - Runde 1 erlaubt nur Zaubern `[PM 7]`.
  - Hat der Mensch keine Einheiten mehr, spielt die KI bis zu 40 Runden zu Ende; danach folgt die Abrechnung.
- **Bewegung:**
  - Pfeile, Akkorde und Tastenwiederholung.
  - Fliegen mit `<` und `>`.
  - Bump öffnet Türen und Truhen, greift Gegner oder Terrain an; `x` startet den Look-Modus.
- **Kampf (D16, D21):**
  - Eine Trefferformel für alle Angriffe: 10–90 %, Verteidigung inklusive eines Schildes.
  - Rückschlag auch nach einem Fehlschlag; tödliche Wunden; Gebundenheit neben Gegnern.
  - Untote nehmen nur Schaden von Untoten, magischen Waffen und Zaubern; unter 50 % Constitution gibt es einen Malus.
  - Wer stirbt, lässt alles fallen; Kill-VP richten sich nach dem Opfer.
- **Magie:**
  - Beschwörungen und Bolt/Lightning.
  - Die 7 sonstigen Zauber: Shield, Eye, Teleport, Curse, Subversion, Magic Attack, Enchant.
  - Zeitlich begrenzte Wirkungen mit Panel-Icons.
  - Tränke: brauen im Kessel, `q` trinken, `v` abfüllen, die Bombenphiole werfen; Drachen nur mit Drachenkraut im Kessel.
- **Objekte:**
  - `g` aufheben, `d` fallen lassen, `w` wechseln, `t` werfen, `f` Bogen.
  - `e` essen, `r` Schriftrolle lesen.
  - Schlüssel und Truhen; Schätze bringen am Portal VP.
- **KI:** Unabhängige jagen sichtbare Ziele, unsichtbare Einheiten sieht sie nicht. Der KI-Zauberer lässt seine Kreaturen jagen, kämpft, beschwört und flieht durchs Portal.
- **Darstellung:** Hidden Map (unerforscht schwarz, erinnert abgedunkelt), animierte Kerzen, Wasser und Portal, Cursor als VDP-Sprite.

---

## 2. Zusammenarbeit mit dem Nutzer (wichtig)

- **Sprache:** Deutsch im Chat und in den Docs, Code und Kommentare auf Englisch.
- **Design vor Code:** Neue Phasen bzw. Features erst im GDD (`docs/design/GDD.md`, Entscheidungen in §14) vorschlagen und abstimmen, dann bauen. Bei echten Wahlmöglichkeiten kurz fragen (2–4 Optionen, mit Empfehlung).
- **Regelantworten als Recherche:** Der Nutzer beantwortet Designfragen oft mit Auszügen aus seinen Quellen (mit Fußnoten). Originalwerte daraus sind nur Anker (D7). Unser eigener Wert bleibt, z. B. Schild +4 statt +13.
- **Keine exakte Kopie des Originals (D7):** Werte und Formeln sind eigenes Design. Ausnahme: die Kreaturtabelle `[PM 34]` als Startwerte (D12).
- **Optik:** eigene Pixelart, 24×24, 3/4-Frontansicht wie auf dem Amiga (D9–D11). Möbel, Teppiche, Türen sollen sichtbar sein, nicht abstrakt.
- **Steuerung:** Tastatur im Stil von Caves of Qud (D5), Zieltastatur **Cherry G84-4100, deutsch, ohne Ziffernblock** (D6).
- **Fokus Einzelspieler** mit Kampagne gegen KI-Zauberer (D4). Hotseat und Maus kommen nach v1.0.
- **Review vor dem Merge:** Zweimal hat erst das Review echte Fehler gefunden, obwohl CI grün war: ein Hänger, falsche VP, kaputte Panel-Icons, eine wirkungslose Bombe. Größere PRs deshalb vor dem Merge gegen das GDD prüfen und im Emulator anspielen.
- **Mergen:**
  - Der Nutzer gibt das Mergen frei. Hat er es ausdrücklich verlangt, mit `gh pr merge <n> --merge` mergen bzw. mit `--auto` nach grünem CI.
  - **Gestapelte PRs immer mit Merge-Commits**, nicht Rebase oder Squash, sonst tauchen die Commits doppelt auf.
  - Das Auto-Merge-Werkzeug der App wurde vom Freigabesystem abgelehnt.
- **Belege zeigen:** Nach sichtbaren Änderungen einen Emulator-Screenshot machen (`tools/run.py ... --screenshot`) und ansehen. Der Nutzer reagiert auf Bilder.
- **PR #55 „Mega Drive als zweite Plattform“ (D23):** Auf Wunsch des Nutzers **ignorieren**. Nicht mergen, nicht darauf aufbauen.
- **Amiga-Referenz:** WinUAE ist installiert (`C:\Program Files\WinUAE\winuae64.exe`), Kickstart und ADF liegen in `Desktop\amiga\`. Beobachtungen kommen nach `docs/design/amiga-observations.md`.

---

## 3. Umgebung

- **Windows 11** mit **WSL Ubuntu** (agondev und gcc laufen darin), dazu **uv** und **gh** (eingeloggt als `cadextcp`).
- Repo: `C:\Users\cadex\projekte\lords-of-chaos-agon` → GitHub `cadextcp/lords-of-chaos-agon` (public).
- **Nur lokal (gitignored):**
  - `reference/`: Handbuch-PDFs, Screenshots, Spectrum-Kartenbogen; urheberrechtlich geschützt, **nie committen**
  - `emulator/`, `toolchain/`, `sdcard/`, `.cache/`, `build/`, `bin/`, `obj/`, `src/core/gen/`
- Frischer Clone: `uv run tools/setup.py` (lädt Emulator 1.2.5 und agondev v0.22, SHA-geprüft).
- Alte Projekte, nur als Archiv: `C:\Users\cadex\projekte\LordsOfChaos` (gescheiterter BASIC-Versuch), `AgonBasics`, `AgonPipeline`.

---

## 4. Befehle

```bash
uv run tools/test.py                       # Host + eZ80-Selftest (vor jedem Commit)
uv run tools/run.py                        # Spiel im GUI-Emulator (Szenario 1)
uv run tools/run.py --dump --time 35 --list --keys "shift+e,shift+e" --screenshot   # Rauchtest
uv run tools/run.py --bench --time 20      # Redraw-Messung -> loc.log
uv run tools/run.py --keytest              # Tastatur-Events anzeigen
uv run tools/mockup.py --sheet             # Mockup und Kachelübersicht
uv run tools/art/creature_sheet.py         # Kreaturen-Übersicht
```

- **Spiel-Optionen:**
  - `loc` startet Szenario 1, `loc --testland` die Entwicklungskarte, `loc --house` das Zauberer-Haus.
  - `--dump` schreibt `loc.log` mit ASCII-Karte und AP pro Frame.
  - Außerdem: `--bench`, `--keytest`, `--selftest`, `--free-round1` (keine Bewegungssperre in Runde 1) und `--fly` (eigene Flieger starten in der Luft).
- **`send_keys`-Syntax:**
  - Benannte Tasten nur mit `--list`. Ohne `--list` wird jedes Zeichen einzeln gesendet; aus `shift+e` würden dann s, h, i, f, t …
  - `up+right` ist ein Akkord, `hold=right=800` hält die Taste 800 ms.
- **Spieltasten:**
  - Zauber `c`, aufheben `g`, fallen lassen `d`, wechseln `w`, werfen `t`, Bogen `f`.
  - Essen `e`, lesen `r`, trinken `q`, Phiole füllen `v`.
  - Look `x`, fliegen `<` `>`, Einheit `Tab`, fertig `Leertaste`, Zugende `Shift+E`.

---

## 5. Architektur in Kürze

```
src/core/  plattformfrei (Host + eZ80):
  world.[ch]    Karte, Einheiten (stabile id), Objekte, Bewegung, AP/Stamina,
                Kill-Protokoll (world_kill_unit), Kessel-Datensätze, Laden (.map v3)
  turn.[ch]     Rundenablauf, aktive Einheit per id, Runden-Hook, KI-Callback
  combat.[ch]   Nahkampf, combat_damage (alle Schadensquellen), Terrain-Angriff
  items.[ch]    Inventar, Waffen-/Schild-Werte, Werfen, Bogen, Essen, Lesen, Truhen
  spells.[ch]   Mana, Zauberbücher (aus .scn), Beschwörung, Bolt/Lightning, 7 sonstige Zauber
  effect.[ch]   Wirkungen mit Laufzeit pro Einheit (Tick am Rundenende)
  brew.[ch]     Kessel, Zutaten, Phiolen, Bombe, Drachenkraut
  game.[ch]     Portal, VP, Kill-Gutschrift, Spielende, Magic Eye
  ai.[ch]       Jäger und Zauberer-KI
  sight.[ch]    Sichtlinie (Bresenham) und Hidden Map
  view.[ch]     9x9-Fenster: Ebenen pro Feld, Auto-Tiling, Dirty-Felder, Cache, Animation
  chord.[ch]    Pfeil-Akkorde und Tastenwiederholung
  names.[ch]    alle Anzeigetexte (deutsch, ohne Umlaute)
  selftest.c    läuft auf Host UND eZ80
  gen/          GENERIERT: tiles.h, data.[ch], creatures.h, maps.[ch], scenarios.[ch]
src/agon/  main.c (Hauptschleife, settle()), render.c (VDP, Panel), input.c,
           mapfile.c (.map und .scn von SD), keytest.c, log.c, emu.asm
host/      PC-Frontend (--selftest, --dump, --layers)
```

**Pipelines (laufen automatisch in `build.py` und `test.py`):**

| Quelle | Werkzeug | Ergebnis |
|---|---|---|
| `assets/tiles/*.png`, `assets/icons/*.png` | `build_tiles.py` | `build/tiles.bin` (268 Kacheln, 149 KB), `gen/tiles.h` |
| `data/*.csv` | `gen_data.py` | `gen/data.[ch]`, `gen/creatures.h` |
| `data/maps/*.txt` | `gen_maps.py` | `build/maps/*.map` (Format v3 mit Portal) und `gen/maps.[ch]` |
| `data/scenarios/*.txt` | `gen_scenarios.py` | `build/scenarios/*.scn` (Zauberbücher, „LOCS“ v1) und `gen/scenarios.[ch]` |

- Reihenfolge: Kacheln → Daten → Karten → Szenarien. `gen_maps` liest Enum-Werte direkt aus den C-Headern.
- **View-Hash `HOUSE_VIEW_HASH` (selftest.c):** Er ändert sich mit Kacheln, Karte oder Kompositionsregeln. Host und eZ80 müssen denselben Wert ausgeben; den Wert bewusst übernehmen.
- **Budget:** `loc.bin` hat jetzt 171 KB (nach M3: 137 KB). Ab etwa 250 KB ein ADR schreiben: Daten vom SD statt einkompiliert, Overlays (GDD §16).

---

## 6. Fallstricke

Die vollständige Liste steht in `docs/AGON-QUIRKS.md`. Die wichtigsten:

1. **Das agondev-Makefile kennt keine Header-Abhängigkeiten.** `build.py` baut deshalb immer clean.
2. **`int` ist auf dem eZ80 24 Bit.** Im Core nur `stdint`-Typen. `1u << i` ist ab `i = 24` undefiniert; dann `(uint32_t)1 << i` schreiben. **Divisionen sind langsam**; Hot-Paths ohne Division schreiben.
3. **ez80-clang-Bug:** `x == A || x == B || …` über Enums kann abstürzen. Lookup-Tabellen verwenden (T7).
4. **`/*` in C-Kommentaren**, z. B. „data/*.csv“, ergibt den Fehler „/* within comment“.
5. **Tastatur:** Bewegung nur per VKey. Die Hauptschleife muss die **Event-Queue vollständig leeren**, bevor sie `chord_poll` aufruft (K1–K5).
6. **Der CLI-Emulator führt `autoexec.txt` aus.** `test.py` und `run.py` schreiben es jeweils neu.
7. **Bash-Tool unter Windows:** Heredocs mit Sonderzeichen brechen; Skripte lieber per Datei schreiben (`.cache/*.py`). Python unter Windows schreibt CRLF; Dateien deshalb mit `write_bytes` schreiben. Für `wsl.exe` `MSYS_NO_PATHCONV=1` setzen.
8. **Der Agon-Systemfont hat keine Umlaute.** UI-Texte ohne ä/ö/ü („Tuer“).
9. **Tile-IDs sind 16 Bit** (268 Kacheln, Icons über 255). Tile-Tabellen nie als `uint8_t` anlegen; die Panel-Icons sind daran schon einmal gescheitert.
10. **Kachel-PNGs sind Quelle.** `tools/art/make_tiles.py` überschreibt sie, also nur mit `--only NAME` neu erzeugen.
11. **Unit-Indizes sind instabil.**
    - `world_remove_unit` tauscht mit der letzten Einheit. Einheiten über Aktionen hinweg per `Unit.id` und `world_find_unit` halten.
    - Tode immer über `world_kill_unit` bzw. `combat_damage`, damit die VP stimmen.
    - Wer in einer Schleife töten kann, liest Killer-Art und -Besitzer vorher aus.
    - Im Frontend nach jeder Aktion `settle()` aufrufen.
12. **Zielzauber brauchen Reichweite 6 und Sichtlinie (D17).** Tests, die durch die Haustür im Testland zielen, öffnen sie vorher (`feature[5][8] = FE_DOOR_OPEN`).
13. **Der Kessel-Zustand folgt dem Kessel-Objekt** (`brew_cauldron_at`). Volle Kessel lassen sich nicht tragen; Phiolen wirken vorerst mit Stufe 2.

---

## 7. Nächste Schritte

**Plan:** GDD §16, Entscheidung D22. Issues im Milestone „M4 Classic komplett (v1.0)“:

| Teil | Issue | Inhalt | Stand |
|---|---|---|---|
| M4a | #43 | Classic-Lücken, Szenario-Format mit Zauberbüchern | ✅ #53 |
| M4b | #44 | Wirkungen mit Laufzeit, 7 sonstige Zauber | ✅ #54 |
| M4c | #45 | Tränke und Brauen, Drachen | ✅ #56 |
| – | – | Review-Fixes M4a–c | ✅ #57 |
| **M4d** | **#46** | **Flächeneffekte: Magic Fire, Gooey Blob, Tangle Vine, Flood** | **als Nächstes** |
| **M4e** | **#47** | **Restliche Waffen, Reiten, Dächer (sichtbar, innen ausgeblendet; F7)** | **als Nächstes**, unabhängig |
| M4f | #48 | Hauptmenü, Wizard Designer, Kampagne (keine Gegenstände; F5) | offen, braucht M4a |
| M4g | #49 | Szenarien 2 und 3 | offen, braucht c, d, e |
| M4h | #50 | KI-Ausbau (Wächter, Zauber, Tränke; ≤ 2 s pro Zug) | offen, nach M4g |
| M4i | #51 | Speichern und Setup | offen, nach M4f |
| M4j | #52 | Politur (Kontextmenü, Big Map, Log, Hilfe, Sound, Umlaut-Font) | zum Schluss |

**Konkret für M4d (#46):**
1. **Mit dem Nutzer bestätigen**, bevor gebaut wird:
   - **F4 Ausbreitung:** Stärke = Zauberstufe. Am Rundenende versucht jedes Feld einmal, ein passendes Nachbarfeld zu belegen (Chance Stärke × 10 %). Neue Felder erhalten Stärke − 1, alte verlieren 1. Höchstens 48 Felder je Fläche.
   - **Welches Terrain** brennt bzw. ist anfällig für Blob, Vine und Flood?
   - **Wie viel Schaden** machen Feuer und Blob pro Runde?
   - **Ertrinken:** Wann ertrinkt man in der Flut?
2. Danach eine neue Feld-Ebene „Effekt“ (GDD §3.2), Tick am Rundenende über den Partie-RNG, eigene animierte Kacheln.
3. Abnahme: Ein Feuer breitet sich reproduzierbar aus und erlischt. Das Rundenende mit 4 aktiven Flächen bleibt unter 0,5 s im Emulator. Kills durch Flächen zählen einfach (§9).

**Workflow:** pro Issue ein Branch `m4/<x>-…`, Selftest-Checks, Emulator-Screenshot ansehen, CHANGELOG, PR mit `Closes #n`. Vor dem Merge ein Review (siehe §2).

---

## 8. Offene Punkte außerhalb von M4

- **Hardware-Test des Nutzers (#3, #7):** Er wird mit jedem Teil wichtiger; KI-Runden und Sicht kosten auf dem Emulator schon spürbar Zeit.
  - Auf die SD-Karte nach `/loc`: `bin/loc.bin`, `build/tiles.bin`, `build/maps/`, `build/scenarios/`.
  - Dann `SET KEYBOARD 2`, `cd /loc`, `loc --keytest`, `loc --bench`, `loc`.
  - Zu klären:
    - Akkorde ohne Ghosting?
    - Codes für `<`/`>`?
    - MOS- bzw. VDP-Version?
    - Lange Dateinamen auf FAT?
    - Ladezeit von `tiles.bin` (149 KB)?
    - Dauer einer KI-Runde?
- **Tags:** `v0.2.0` bis `v0.4.0` (Ende M1 bis M3) sind noch nicht gesetzt; vorgesehen nach dem Hardware-Test.
- **#2 Buffered Commands:** nur nötig, falls die Geschwindigkeit auf Hardware nicht reicht.
- **Bekannte Vereinfachungen, die später nachzuziehen sind:**
  - Phiolen wirken mit Stufe 2, weil Objekte keine Zusatzdaten tragen.
  - Enchant wirkt pro Einheit statt pro Waffe; Waffen am Boden werden nicht verzaubert.
  - Subversion verbietet alle Reittiere; die Ausnahme „nur Reittiere mit Zauberer“ kommt mit dem Reiten in M4e.
  - Höchstens 4 Kessel, 64 Bodenobjekte und 16 noch nicht abgerechnete Kills.
- **Kunst-Schulden:** Offene Türen sind schwer lesbar, die Kreaturen sind ein erster Entwurf, ein Font mit Umlauten fehlt (M4j).
- `tools/mockup.py` rendert nur das Zauberer-Haus (9×9).

---

## 9. Wo steht was

| Thema | Datei |
|---|---|
| Spieldesign, Entscheidungen D1–D22, M4-Plan | `docs/design/GDD.md` (§14, §16) |
| Amiga-Beobachtungen | `docs/design/amiga-observations.md` |
| Roadmap und Arbeitsweise | `docs/ROADMAP.md` |
| Architektur | `docs/ARCHITECTURE.md` |
| Architekturentscheidungen | `docs/adr/0001` bis `0009` (Rendering 0006, Eingabe 0007, Daten 0008, Sicht 0009) |
| Plattform-Quirks | `docs/AGON-QUIRKS.md` |
| Testen und Debuggen | `docs/TESTING.md` |
| Änderungen | `CHANGELOG.md` |
