# Lords of Chaos – Agon

Inoffizielles Fan-Remake von Julian Gollops *Lords of Chaos* (1990) für den **Agon Light** (eZ80, MOS, VDP). Es ist in C und eZ80-Assembler geschrieben, mit einer Glyph-Grafik im Stil von *Caves of Qud*.

*Unofficial fan remake of Lords of Chaos for the Agon Light, written in C/eZ80.*

> **Status:** M0 Fundament. Build, Tests, Emulator und CI laufen; das Spiel selbst ist noch eine Demo-Szene.
> Plan: [docs/ROADMAP.md](docs/ROADMAP.md) · Design: [docs/design/GDD.md](docs/design/GDD.md)

## Schnellstart

Voraussetzungen:
- Windows 10/11 mit **WSL (Ubuntu)**, darin `make` und `gcc`, oder Linux x86_64
- [uv](https://docs.astral.sh/uv/)

```bash
uv run tools/setup.py      # einmalig: Emulator + agondev laden (SHA-256-geprüft)
uv run tools/test.py       # Host-Selftest + Headless-Selftest im Emulator
uv run tools/run.py        # Spiel im GUI-Emulator starten
```

| Befehl | Zweck |
|---|---|
| `uv run tools/build.py [--host\|--all]` | Agon-Binary `bin/loc.bin` bzw. Host-Build bauen |
| `uv run tools/test.py [--host\|--emu] [-v]` | Automatische Tests (auch in CI) |
| `uv run tools/run.py --dump --time 8 --keys ddw --screenshot` | Skriptbare GUI-Session: Tasten senden, Screenshot, Bildschirm-Dump aus `loc.log` |
| `uv run tools/send_keys.py` / `tools/screenshot.py` | Tastatur-Injektion und Screenshot des Emulatorfensters |

## Projektstruktur

```
src/core/   plattformfreier Spielkern (C99, keine VDP-Aufrufe) – läuft auf Agon UND PC
src/agon/   Agon-Frontend: VDP-Rendering, Tastatur, MOS-Dateien, Emulator-Steuerung
host/       PC-Frontend für Tests und schnelles Debugging
data/       Spieldaten als CSV (Zauber, Kosten, …)
tools/      Python-Werkzeuge (uv)
docs/       Roadmap, Architektur, ADRs, Game Design
```

Einen Überblick über die Architektur gibt [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), die Teststrategie steht in [docs/TESTING.md](docs/TESTING.md).

## Rechtliches

- *Lords of Chaos* © Julian Gollop / Mythos Games. Dieses Projekt ist ein nicht-kommerzielles Fan-Remake ohne Verbindung zu den Rechteinhabern.
- Das Repo enthält **keine** Originaldateien: keine Handbücher, Disk-Images, Original-Grafiken oder Karten. Solches Referenzmaterial liegt nur lokal in `reference/` (gitignored).
- Grafik (Glyphen), Karten und Code sind eigene Arbeit. Der Code steht unter der [MIT-Lizenz](LICENSE).
