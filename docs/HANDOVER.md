# Übergabe: Stand und nächste Schritte

> Stand: 2026-10-03 · **M2 und M3 vollständig** (#13–#33) · CI grün
> Für die nächste Person bzw. den nächsten Agenten. Zuerst `CLAUDE.md` lesen (Regeln, Befehle), dann dieses Dokument.

---

## 1. Kurzstatus

| Milestone | Status |
|---|---|
| **M0 Fundament** | ✅ Toolchain, Tests, CI, Docs |
| **M1 Grafik und Eingabe** | ✅ Software-seitig fertig: #1 Renderer, #4 Sprite und Animation, #5 Datenladen, #6 Panel. Offen: #2 (optional), #3 und #7 (brauchen echte Hardware) |
| **M2 Core-Skelett** | ✅ #13–#18 (Terrains, Kreaturen, Rundenablauf, Sicht/Hidden Map, Luft-/Bodenebene, Bump/Look) |
| **M3 Classic spielbar** | ✅ #27–#33 (Kampf D16, Beschwörungen, Bolt/Lightning D17, Objekte/Waffen D18, Portal/VP D19, KI D20, Szenario 1) |
| M3–M5 | geplant, siehe `docs/ROADMAP.md` |

**Was heute läuft:**
- Im Emulator lädt `loc` die 36×36-Karte „Testland“ (Wrap-around) mit eigener 24×24-Pixelart in 3/4-Ansicht.
- Rundenablauf (M2c): `Tab`/`Shift+Tab` wählt eigene Einheiten mit AP, `Leertaste` beendet eine Einheit, `Shift+E` (zweimal) beendet den Zug. Runde 1 erlaubt kein Bewegen `[PM 7]` (für Skripte: `--free-round1`).
- Unabhängige Kreaturen streifen zu Rundenbeginn deterministisch umher, der KI-Zauberer passt (echte KI in M3).
- Bewegung mit Pfeilen, Akkorden, Pos1/Ende/Bild, Tastenwiederholung; AP/Stamina aus `data/costs.csv`.
- Rundenende: AP, 25 % Stamina, 4 % Mana; Erschöpfung (Stamina < 25 %) halbiert die AP `[PM 12]`.
- Info-Panel mit 6 Balken, Status-Icons und „Am Boden“-Liste, folgt der aktiven Einheit; Meldungszeile „Runde n – Zauberer-1: <Einheit>“.
- Bump/Look (M2f): Gegen geschlossene Türen laufen öffnet sie (6 AP, nur mit Händen); Gegner/Terrain-Bump meldet „Kampf in M3“; `x` = Look-Modus (weißer Cursor, Panel zeigt Feld/Einheit, `Esc` beendet).
- Fliegen (M2e): Boden- und Luft-Ebene pro Feld, `<`/`>` Aufsteigen/Landen (4 AP), Flug 4/6 AP über allem, kein Landen auf Wasser; Flieger 3 px höher mit Schatten, blauer Cursor in der Luft; Rundenende füllt das Ebenen-Budget (D15). Demo: `--fly` (ISO-Taste im Emulator nicht sendbar).
- Hidden Map (M2d): Unerforscht schwarz, Erinnert abgedunkelt (Raster), Gegner nur bei Sichtlinie (Bresenham, Chebyshev 9/11, GDD D14). Sicht-Neuberechnung pro eigenem Schritt ~162 ms (Emulator, ADR 0009).
- Kerzen und Wasser sind animiert, der Cursor ist ein blinkender VDP-Sprite.

---

## 2. Zusammenarbeit mit dem Nutzer (wichtig)

- **Sprache:** Deutsch im Chat und in den Docs, Code und Kommentare auf Englisch.
- **Design vor Code:** Neue Phasen bzw. Features erst im GDD (`docs/design/GDD.md`, Entscheidungstabelle §14) vorschlagen und abstimmen, dann bauen. Bei echten Wahlmöglichkeiten kurz fragen (2–4 Optionen, mit Empfehlung).
- **Keine exakte Kopie des Originals (D7):** Werte und Formeln sind eigenes Design. Messungen am Original dienen nur als Anker. Ausnahme: die Kreaturtabelle `[PM 34]` als Startwerte (D12).
- **Optik:** eigene Pixelart, 24×24, 3/4-Frontansicht wie auf dem Amiga (D9–D11). Der Nutzer will Möbel, Teppiche, Türen und Schubladen sehen, also nah dran wie Spectrum bzw. Amiga, nicht abstrakt.
- **Steuerung:** Tastatur im Stil von Caves of Qud (D5), Zieltastatur **Cherry G84-4100, deutsch, ohne Ziffernblock** (D6, GDD §5.2).
- **Fokus Einzelspieler** mit Kampagne gegen KI-Zauberer (D4). Hotseat und Maus kommen nach v1.0.
- **Merge-Freigabe:** PRs werden nach grünem CI gemergt. GitHub-Auto-Merge ist im Repo aktiv, CI-Check `build-and-test` ist Pflicht. Nach dem PR also `set_auto_merge` aktivieren (rebase).
- **Belege zeigen:** Nach sichtbaren Änderungen einen Emulator-Screenshot schicken (`tools/run.py ... --screenshot`). Der Nutzer reagiert auf Bilder.
- **Amiga-Referenz:** WinUAE ist installiert (`C:\Program Files\WinUAE\winuae64.exe`), Kickstart und ADF liegen in `Desktop\amiga\`. Der Nutzer schickt bei Bedarf Screenshots; Beobachtungen kommen nach `docs/design/amiga-observations.md`.

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
uv run tools/run.py                        # Spiel im GUI-Emulator (Testland)
uv run tools/run.py --dump --time 15 --free-round1 --list --keys "right,up+right,hold=down=900" --screenshot
uv run tools/run.py --bench --time 20      # Redraw-Messung -> loc.log
uv run tools/run.py --keytest              # Tastatur-Events anzeigen
uv run tools/mockup.py --sheet             # Mockup und Kachelübersicht
uv run tools/art/creature_sheet.py         # Kreaturen-Übersicht
```

- Spiel-Optionen: `loc` (Testland), `loc --house` (Zauberer-Haus), `--dump` (`loc.log` mit ASCII-Karte und AP pro Frame), `--bench`, `--keytest`, `--selftest`, `--free-round1` (Runde-1-Bewegungssperre aus, für Skripte), `--fly` (eigene Flieger starten in der Luft).
- `send_keys`-Syntax: `up+right` ist ein Akkord, `hold=right=800` hält die Taste 800 ms, `shift+e` ist Shift+E.

---

## 5. Architektur in Kürze

```
src/core/  plattformfrei (Host + eZ80):
  world.[ch]    Karte (Boden/Dekor/Feature), Einheiten, Objekte, Bewegung, AP/Stamina, Laden (.map)
  turn.[ch]     Rundenablauf: Runde, Phase, aktive Einheit, Umherstreifen (RNG)
  sight.[ch]    Sichtlinie (Bresenham) und Hidden Map (Bitfelder pro Spieler)
  view.[ch]     9x9-Fenster: Ebenen pro Feld (Tile-IDs), Wand-Auto-Tiling, Halb-Böden,
                Dirty-Felder, Static-Cache, Animation (view_animate), Kamera mit Wrap
  chord.[ch]    Pfeil-Akkorde und Tastenwiederholung
  names.[ch]    alle Anzeigetexte (deutsch, ohne Umlaute)
  spells.[ch]   Mana-Formel
  selftest.c    läuft auf Host UND eZ80 (Headless-Emulator, Exit über Port 0)
  gen/          GENERIERT: tiles.h, data.[ch], creatures.h, maps.[ch]
src/agon/  render.c (VDP-Bitmaps, Panel, Sprite-Cursor), input.c (VKey-Mapping),
           mapfile.c (SD), keytest.c, log.c, main.c (Hauptschleife), emu.asm
host/      PC-Frontend (--selftest, --dump, --layers)
```

**Pipelines (laufen automatisch in `build.py` und `test.py`):**

| Quelle | Werkzeug | Ergebnis |
|---|---|---|
| `assets/tiles/*.png`, `assets/icons/*.png` | `build_tiles.py` | `build/tiles.bin` (RGBA2222), `gen/tiles.h` (Besitzer-Varianten, Halb-Böden) |
| `data/*.csv` | `gen_data.py` | `gen/data.[ch]`, `gen/creatures.h` |
| `data/maps/*.txt` | `gen_maps.py` | `build/maps/*.map` (Format v2, SD) und `gen/maps.[ch]` (für Tests) |

- Reihenfolge: Kacheln → Daten → Karten.
- `gen_maps` liest Enum-Werte direkt aus den C-Headern.
- **View-Hash `HOUSE_VIEW_HASH` (selftest.c):** Er ändert sich, sobald Kacheln, Karte oder Kompositionsregeln sich ändern. Host und eZ80 müssen denselben Wert ausgeben; den Wert dann bewusst übernehmen.

---

## 6. Fallstricke

Die vollständige Liste steht in `docs/AGON-QUIRKS.md`. Die wichtigsten:

1. **Das agondev-Makefile kennt keine Header-Abhängigkeiten.** `build.py` baut deshalb immer clean (`--incremental` nur bewusst).
2. **`int` ist auf dem eZ80 24 Bit.** Im Core nur `stdint`-Typen verwenden. **Divisionen und Modulo sind langsam** (Software); Hot-Paths ohne Division schreiben (siehe `view.c` Wrap).
3. **ez80-clang-Bug:** `x == A || x == B || …` über Enums kann abstürzen („unable to legalize i14“). Lookup-Tabellen verwenden (T7).
4. **`/*` in C-Kommentaren**, z. B. „data/*.csv“, ergibt den Fehler „/* within comment“. In generierten Headern `<name>` schreiben.
5. **Tastatur:** ASCII bei Key-up ist veraltet, Bewegung deshalb nur per VKey. `kbuf` liefert kein Auto-Repeat. Die Hauptschleife muss die **Event-Queue vollständig leeren**, bevor sie `chord_poll` aufruft (K1–K5).
6. **Der CLI-Emulator führt `autoexec.txt` aus.** `test.py` und `run.py` schreiben es jeweils neu.
7. **Bash-Tool unter Windows:** Heredocs mit Sonderzeichen oder Anführungszeichen brechen; Skripte lieber per Datei schreiben (`.cache/*.py`). Für `wsl.exe` direkt `MSYS_NO_PATHCONV=1` setzen.
8. **Der Agon-Systemfont hat keine Umlaute.** UI-Texte ohne ä/ö/ü („Tuer“).
9. **Tile-IDs sind 16 Bit** (236 Kacheln). Für Kreaturen `CREATURE_TILE[kind] + owner` verwenden.
10. **Kachel-PNGs sind jetzt Quelle.** `tools/art/make_tiles.py` überschreibt sie, also nur mit `--only NAME` neu erzeugen.

---

## 7. Nächste Schritte: M4 „Classic komplett“ (v0.4.0 laut ROADMAP)

**M3 ist vollständig (#27–#33): eine Partie gegen die KI ist durchspielbar** (Beschwören, Kämpfen, Schätze sammeln, Portal, VP-Abrechnung). Wie bei M3 zuerst eine M4-Aufteilung im GDD vorschlagen und mit dem Nutzer abstimmen, dann Issues anlegen.

**Workflow:** pro Issue ein Branch `m4/<x>-…`, Selftest-Checks, Emulator-Screenshot, CHANGELOG, PR mit `Closes #n` und Auto-Merge.

**M4-Bausteine laut ROADMAP/GDD (Vorschlagsgrundlage):**
- **Tränke und Brauen** (GDD §7.2): Kessel + Zutat + Trankzauber, DRINK/FILL, Wirkdauern
- **Flächeneffekte:** Magic Fire, Gooey Blob, Tangle Vine, Flood (Ausbreitung pro Runde)
- **Weitere Zauber:** Enchant, Subversion, Curse, Magic Attack, Teleport, Magic Eye, Magic Shield
- **Waffen fertig:** Munition/pfeile?, Wurf-Bugs, Slayer-Regeln, magische Waffen vs Untote
- **Reiten** (GDD §4.2), Angriffe aus der Luft, Dächer (Datenfeld)
- **Wizard Designer + Kampagne** (XP/VP-Übertrag, Szenarien 2–3)
- **Politur:** Kontextmenü Enter, Big Map m, Log l, Hilfe F1, Sound

**Danach M5 Chaos.**

---

## 8. Offene Punkte außerhalb von M2

- **Hardware-Test des Nutzers (#3, #7):**
  - Auf die SD-Karte nach `/loc`: `bin/loc.bin`, `build/tiles.bin`, `build/maps/`.
  - Dann `SET KEYBOARD 2`, `cd /loc`, `loc --keytest`, `loc --bench`, `loc`.
  - Zu klären: Akkorde ohne Ghosting? Codes für `<`/`>` und Fn-Ziffernblock? MOS- bzw. VDP-Version? Lange Dateinamen auf FAT? Lesbarkeit und Ladezeit von `tiles.bin` (130 KB)?
- **Tag `v0.2.0`** (Ende M1) ist noch nicht gesetzt; vorgesehen nach dem Hardware-Test.
- **#2 Buffered Commands:** nur nötig, falls die Geschwindigkeit auf Hardware nicht reicht (Emulator: Voll-Redraw 50 ms).
- **Kunst-Schulden:**
  - Offene Türen sind schwer lesbar.
  - Kreaturen sind ein erster Entwurf aus Templates; der Nutzer findet sie „nice“, Verfeinerung auf Wunsch.
  - Ein eigener Font mit Umlauten fehlt.
- `tools/mockup.py` rendert nur das Zauberer-Haus (9×9). Für Testland-Ausschnitte bei Bedarf erweitern.

---

## 9. Wo steht was

| Thema | Datei |
|---|---|
| Spieldesign und Entscheidungen D1–D13 | `docs/design/GDD.md` (§14) |
| Amiga-Beobachtungen | `docs/design/amiga-observations.md` |
| Roadmap und Arbeitsweise | `docs/ROADMAP.md` |
| Architektur | `docs/ARCHITECTURE.md` |
| Architekturentscheidungen | `docs/adr/0001` bis `0008` (Rendering-Messwerte in 0006, Eingabe in 0007, Daten in 0008) |
| Plattform-Quirks | `docs/AGON-QUIRKS.md` |
| Testen und Debuggen | `docs/TESTING.md` |
| Änderungen | `CHANGELOG.md` |
