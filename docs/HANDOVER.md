# Übergabe: Stand und nächste Schritte

> Stand: 2026-10-02 · `main` @ `f802aa7` · CI grün
> Für die nächste Person bzw. den nächsten Agenten. Zuerst `CLAUDE.md` lesen (Regeln, Befehle), dann dieses Dokument.

---

## 1. Kurzstatus

| Milestone | Status |
|---|---|
| **M0 Fundament** | ✅ Toolchain, Tests, CI, Docs |
| **M1 Grafik und Eingabe** | ✅ Software-seitig fertig: #1 Renderer, #4 Sprite und Animation, #5 Datenladen, #6 Panel. Offen: #2 (optional), #3 und #7 (brauchen echte Hardware) |
| **M2 Core-Skelett** | 🟡 Halb fertig: ✅ #13 M2a Terrains und Testland, ✅ #14 M2b Kreaturen (Daten und Pixelart). Offen: **#15 M2c Rundenablauf**, #16 M2d Sicht und Hidden Map, #17 M2e Luft- und Bodenebene, #18 M2f Bump und Look-Modus |
| M3–M5 | geplant, siehe `docs/ROADMAP.md` |

**Was heute läuft:**
- Im Emulator lädt `loc` die 36×36-Karte „Testland“ (Wrap-around) mit eigener 24×24-Pixelart in 3/4-Ansicht.
- Ein Zauberer läuft mit Pfeilen, Pfeil-Akkorden (Diagonalen), Pos1/Ende/Bild sowie Tastenwiederholung.
- AP- und Stamina-Kosten kommen aus `data/costs.csv`; `Shift+E` füllt die AP wieder auf.
- Info-Panel mit 6 Balken, Status-Icons und „Am Boden“-Liste.
- Kerzen und Wasser sind animiert, der Cursor ist ein blinkender VDP-Sprite.
- Andere Kreaturen stehen noch still, weil es noch keinen Rundenablauf gibt (M2c).

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
uv run tools/run.py --dump --time 15 --list --keys "right,up+right,hold=down=900" --screenshot
uv run tools/run.py --bench --time 20      # Redraw-Messung -> loc.log
uv run tools/run.py --keytest              # Tastatur-Events anzeigen
uv run tools/mockup.py --sheet             # Mockup und Kachelübersicht
uv run tools/art/creature_sheet.py         # Kreaturen-Übersicht
```

- Spiel-Optionen: `loc` (Testland), `loc --house` (Zauberer-Haus), `--dump` (`loc.log` mit ASCII-Karte und AP pro Frame), `--bench`, `--keytest`, `--selftest`.
- `send_keys`-Syntax: `up+right` ist ein Akkord, `hold=right=800` hält die Taste 800 ms, `shift+e` ist Shift+E.

---

## 5. Architektur in Kürze

```
src/core/  plattformfrei (Host + eZ80):
  world.[ch]    Karte (Boden/Dekor/Feature), Einheiten, Objekte, Bewegung, AP/Stamina, Laden (.map)
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

## 7. Nächste Schritte (M2c–M2f)

**Workflow:** pro Issue ein Branch `m2/<x>-…`, Selftest-Checks ergänzen, Emulator-Screenshot, CHANGELOG, dann PR mit `Closes #n` und Auto-Merge.

### M2c – Rundenablauf (#15), als Nächstes

- Neues Core-Modul `turn.[ch]`:
  - Rundenzähler und Phase (unabhängige Kreaturen, dann Zauberer 1..n)
  - aktiver Spieler und aktive Einheit
- Eingabe: `Tab`/`Shift+Tab` (eigene Einheiten mit AP > 0), `Leertaste` (Einheit fertig, nächste), `Shift+E` (Zugende mit Bestätigung, GDD §5.2). `world_new_turn()` existiert bereits (AP auffüllen, 25 % Stamina).
- **Unabhängige Kreaturen:** Platzhalter-Umherstreifen mit seedetem RNG (`rng.h`); AP, Blockade und Belegung respektieren. Die echte KI kommt in M3.
- **Gegner-Zauberer (p2):** Platzhalter-Zug, z. B. passen.
- **Runde 1:** keine Bewegung `[PM 7]`. Für Tests per Schalter abschaltbar machen, sonst blockiert sie Emulator-Läufe.
- Panel und Kamera folgen der aktiven Einheit; die Meldungszeile zeigt „Runde n – Zauberer-1“.
- Selftest: Reihenfolge, `Tab` überspringt Einheiten ohne AP bzw. fremde Einheiten, AP- und Stamina-Erholung, deterministisches Umherstreifen (Hash).

### M2d – Sichtlinie und Hidden Map (#16)

- Reichweite: Boden 9, Luft 11 (GDD §3.4). Blockade über `world_blocks_sight()` (Böden laut `costs.csv` plus hohe Features; Endpunkte exklusiv).
- Algorithmus: Bresenham-Strahlen oder Shadowcasting, deterministisch, auf dem Host getestet. **Auf dem eZ80 messen**; ggf. Sicht nur nach einem Zug bzw. Schritt neu berechnen und als Bitfeld cachen.
- Pro Spieler ein Bitfeld „erkundet“ (36×36 Bit = 162 Byte) und „aktuell sichtbar“.
- View-Ebenen: unerforscht ergibt eine schwarze Kachel (fehlt noch), erinnert das Overlay `T_OVERLAY_REMEMBERED` (existiert); gegnerische Einheiten nur bei Sicht zeigen (Hidden Movement).
- Dächer bzw. überdachte Felder: erst mit eigenem Datenfeld (später).

### M2e – Luft- und Bodenebene (#17)

- `Unit.airborne`; pro Feld eine Boden- und eine Luft-Einheit. `world_unit_at` braucht einen Ebenen-Parameter.
- Fliegen: konstante Kosten laut `costs.csv`-Zeile `air` (4/6). Basis ist `ap_fly` (bereits in Unit). Aufsteigen bzw. Landen über `actions.csv` (`take_off`, `land`); nicht auf belegtem Feld bzw. unter Dach landen.
- Tasten `<` / `>`: auf dem Emulator nicht testbar (#3). Per ASCII `<` und `>` des Down-Events auswerten; auf Hardware prüfen.
- View: Luft-Einheit über der Boden-Einheit, z. B. 2–3 px höher plus Schatten; Panel-Status-Icon „fliegt“ (`UF_FLYING`).

### M2f – Bump und Look-Modus (#18)

- Bump auf geschlossene Tür: Tür öffnen (`ACTIONS[ACT_OPEN_DOOR]`, nur mit `CF_USE`), danach `world_map_changed()` (View-Cache).
- Bump auf Gegner: Meldung „Kampf folgt in M3“.
- Look-Modus `x`: freier Cursor, Sprite-Frame weiß, Steuerung per Akkorden; Panel und Meldungszeile zeigen das untersuchte Feld bzw. die Einheit (`render_panel` auf beliebige Einheit bzw. Feld erweitern); `Esc` beendet.

**Danach M3 (Classic spielbar):** Kampf (Formel ist eigenes Design, Host-Simulation zum Balancing), Beschwörungen (`spell_mana()` existiert), Bolt/Lightning, Objekte, Portal und Siegpunkte, einfache KI, eigenes Szenario 1.

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
