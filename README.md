# Lords of Chaos – Agon

Inoffizielles Fan-Remake von Julian Gollops *Lords of Chaos* (1990) für den **Agon Light** (eZ80, MOS, VDP). Es ist in C und eZ80-Assembler geschrieben, mit eigener 24×24-Pixelart nah an der Spectrum- und Amiga-Fassung und Effekten im Geist von *Caves of Qud*.

*Unofficial fan remake of Lords of Chaos for the Agon Light, written in C/eZ80.*

> **Status:** M2 (Core-Skelett) zur Hälfte fertig. Die 36×36-Testkarte mit Terrains und allen 25 Kreaturen in eigener 24×24-Pixelart ist begehbar. Stand und nächste Schritte: [docs/HANDOVER.md](docs/HANDOVER.md)
> Mockup: [docs/design/mockups/wizard-house.png](docs/design/mockups/wizard-house.png)
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
| `uv run tools/run.py --dump --time 8 --free-round1 --keys ddw --screenshot` | Skriptbare GUI-Session: Tasten senden, Screenshot, Bildschirm-Dump aus `loc.log` (`--free-round1` hebt die Runde-1-Sperre auf) |
| `uv run tools/send_keys.py` / `tools/screenshot.py` | Tastatur-Injektion und Screenshot des Emulatorfensters |
| `uv run tools/run.py --bench --time 14` | Redraw-Zeiten messen (Ergebnis in `loc.log`) |
| `uv run tools/mockup.py --sheet` | Mockup des Spielbildschirms und Kachelübersicht |

## Projektstruktur

```
src/core/   plattformfreier Spielkern (C99, keine VDP-Aufrufe) – läuft auf Agon UND PC
src/agon/   Agon-Frontend: VDP-Rendering, Tastatur, MOS-Dateien, Emulator-Steuerung
host/       PC-Frontend für Tests und schnelles Debugging
data/       Spieldaten: CSV (Zauber, Kosten …) und Karten (data/maps/*.txt)
assets/     eigene Pixelart: Kacheln, Icons, Agon-Palette
tools/      Python-Werkzeuge (uv)
docs/       Roadmap, Architektur, ADRs, Game Design
```

Einen Überblick über die Architektur gibt [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), die Teststrategie steht in [docs/TESTING.md](docs/TESTING.md).

## Rechtliches

- *Lords of Chaos* © Julian Gollop / Mythos Games. Dieses Projekt ist ein nicht-kommerzielles Fan-Remake ohne Verbindung zu den Rechteinhabern.
- Das Repo enthält **keine** Originaldateien: keine Handbücher, Disk-Images, Original-Grafiken oder Karten. Solches Referenzmaterial liegt nur lokal in `reference/` (gitignored).
- Grafik (Pixelart), Karten und Code sind eigene Arbeit. Der Code steht unter der [MIT-Lizenz](LICENSE).
