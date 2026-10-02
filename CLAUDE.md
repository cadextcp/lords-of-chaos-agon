# CLAUDE.md – Hinweise für KI-Agenten

Lords of Chaos Remake für den Agon Light (eZ80). C99 mit agondev, Python-Tools mit uv. Docs auf Deutsch, Code und Kommentare auf Englisch.

## Befehle

```bash
uv run tools/setup.py                 # einmalig (Emulator + agondev, gepinnt)
uv run tools/build.py --all           # bin/loc.bin + build/host/loc_host
uv run tools/test.py                  # MUSS grün sein vor jedem Commit
uv run tools/run.py --dump --time 8 --keys "dd" --screenshot   # GUI-Check
```

## Regeln

- **`src/core` ist plattformfrei.** Keine `agon/`-, MOS- oder VDP-Header, nur `stdint`-Typen (`int` ist auf dem eZ80 24 Bit), Zufall nur über `rng.h`, keine Gleitkommazahlen in Regeln (ADR 0003).
- **Alles unter `src/` wird von agondev kompiliert.** Host-Code gehört nach `host/`.
- **Neue Core-Logik bekommt Checks in `src/core/selftest.c`.** Sie laufen auf Host **und** eZ80.
- **Spieldesign:** `docs/design/GDD.md` ist die Quelle der Wahrheit. Entscheidungen D1–D8 stehen in §14. Keine Originalwerte kopieren (D7), außer sie sind dort ausdrücklich übernommen.
- **Niemals committen:**
  - `reference/`: Handbücher, ADF/DSK, Screenshots, Fremdkarten; urheberrechtlich geschützt
  - `emulator/`, `toolchain/`, `sdcard/`, `.cache/`
- **Plattformwissen** steht in `docs/AGON-QUIRKS.md`. Neue Erkenntnisse dort eintragen.
- **Workflow:** Feature-Branch → PR → CI grün → Merge. `CHANGELOG.md` pflegen.
