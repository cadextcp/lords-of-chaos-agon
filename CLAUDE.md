# CLAUDE.md – Hinweise für KI-Agenten

Lords of Chaos Remake für den Agon Light (eZ80). C99 mit agondev, Python-Tools mit uv. Docs auf Deutsch, Code und Kommentare auf Englisch.

**Einstieg:** [docs/HANDOVER.md](docs/HANDOVER.md) enthält den aktuellen Stand, die Zusammenarbeit mit dem Nutzer, Fallstricke und die nächsten Schritte.

## Befehle

```bash
uv run tools/setup.py                 # einmalig (Emulator + agondev, gepinnt)
uv run tools/build.py --all           # bin/loc.bin + build/host/loc_host
uv run tools/test.py                  # MUSS grün sein vor jedem Commit
uv run tools/run.py --dump --time 8 --keys "dd" --screenshot   # GUI-Check
```

## Grafik- und Karten-Pipeline

- **Kacheln** sind `assets/tiles/*.png` (24×24, nur Farben aus `assets/palette/agon64.gpl`). `tools/build_tiles.py` erzeugt `build/tiles.bin` und `src/core/gen/tiles.h`.
- **Karten** sind `data/maps/*.txt`. `tools/gen_maps.py` erzeugt `build/maps/*.map` (Binärformat, Laden von SD) und `src/core/gen/maps.c` (dieselben Bytes für Tests). `tools/mockup.py` liest dieselben Textdateien (ADR 0008).
- **Regeltabellen** sind `data/*.csv`. `tools/gen_data.py` erzeugt `src/core/gen/data.[ch]`: Zauber, Bodenkosten, Aktionen.
- `src/core/gen/` ist generiert und gitignored. `tools/build.py` und `tools/test.py` erzeugen es automatisch.
- `tools/art/make_tiles.py` hat die ersten Kacheln erzeugt. Die PNGs sind jetzt die Quelle; das Skript nur mit `--only NAME` neu laufen lassen.

## Regeln

- **`src/core` ist plattformfrei.** Keine `agon/`-, MOS- oder VDP-Header, nur `stdint`-Typen (`int` ist auf dem eZ80 24 Bit), Zufall nur über `rng.h`, keine Gleitkommazahlen in Regeln (ADR 0003).
- **Alles unter `src/` wird von agondev kompiliert.** Host-Code gehört nach `host/`.
- **Neue Core-Logik bekommt Checks in `src/core/selftest.c`.** Sie laufen auf Host **und** eZ80.
- **View-Hash (`HOUSE_VIEW_HASH`)** ändert sich, wenn Karte, Kacheln oder Kompositionsregeln sich ändern. Den neuen Wert aus der Testausgabe übernehmen, aber nur bewusst.
- **Spieldesign:** `docs/design/GDD.md` ist die Quelle der Wahrheit. Entscheidungen D1–D13 stehen in §14. Keine Originalwerte kopieren (D7), außer sie sind dort ausdrücklich übernommen.
- **Niemals committen:**
  - `reference/`: Handbücher, ADF/DSK, Screenshots, Fremdkarten; urheberrechtlich geschützt
  - `emulator/`, `toolchain/`, `sdcard/`, `.cache/`
- **Plattformwissen** steht in `docs/AGON-QUIRKS.md`. Neue Erkenntnisse dort eintragen.
- **Workflow:** Feature-Branch → PR → CI grün → Merge. `CHANGELOG.md` pflegen.
