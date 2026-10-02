# Testen und Debuggen

## Drei Ebenen

| Ebene | Befehl | Was | Dauer |
|---|---|---|---|
| 1. Host | `uv run tools/test.py --host` | Core-Selftest auf dem PC | unter 1 s |
| 2. Emulator headless | `uv run tools/test.py --emu` | derselbe Selftest als eZ80-Code im CLI-Emulator (kein VDP) | unter 1 s |
| 3. Emulator GUI | `uv run tools/run.py ...` | echtes Rendering und echte Eingabe | manuell bzw. skriptbar |

Ebenen 1 und 2 laufen in CI bei jedem Push und PR.

## Selftest-Konvention

- **Code:** `core_selftest()` in `src/core/selftest.c`.
- **Ausgabe:** eine Zeile pro Check (`ok` bzw. `FAIL`), am Ende `=== TEST PASS ===` oder `=== TEST FAIL ===`.
- **Exit-Code:**
  - Host: Anzahl der Fehler (begrenzt auf 1).
  - Agon: `emu_exit()` schreibt den Exit-Code auf I/O-Port 0. Der Emulator beendet sich damit. Vorher wartet `emu_exit()` darauf, dass der UART alles gesendet hat; sonst gingen die letzten Zeilen verloren.
- **Referenz-Hashes** wie `DEMO_HASH` werden bewusst angepasst, wenn sich Inhalte ändern. Sie müssen auf Host **und** Agon gleich sein.

## Screenshot-freies Debuggen in der GUI

```bash
uv run tools/run.py --dump --time 10 --keys "dddw" --screenshot
```

1. Schreibt `autoexec.txt`. Das Spiel startet nach dem Boot mit `loc --dump`.
2. Nach etwa 4 s Boot werden die Tasten per `send_keys.py` gesendet (Scancodes, funktioniert mit SDL).
3. Nach N Sekunden folgt ein Screenshot nach `build/screenshots/`, danach wird der Emulator beendet.
4. Ausgegeben wird `sdcard/staged/loc/loc.log`: Nach jedem Frame steht dort das Zellen-Grid als ASCII, wobei eigene Glyphen auf ihr ASCII-Fallback abgebildet werden.

## Bekannte Grenzen

- Der CLI-Emulator hat **keinen VDP**. VDP-Aufrufe, die auf eine Antwort warten (z. B. `vdp_mode`), dürfen im Selftest-Pfad nicht vorkommen.
- Der CLI-Emulator führt `autoexec.txt` aus. `test.py` schreibt es vor jedem Lauf neu (`cd /loc`, `loc --selftest`), `run.py` ebenso für die GUI. Beide Tools erzeugen es selbst; von Hand bearbeiten ist nicht nötig.
