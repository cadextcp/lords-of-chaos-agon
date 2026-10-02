# Changelog

Format nach [Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionen nach Milestones (siehe `docs/ROADMAP.md`).

## [Unreleased] – M0 Fundament

### Hinzugefügt
- **Projektgerüst:** `src/core` (plattformfrei), `src/agon` (VDP-Frontend), `host/` (PC-Frontend).
- **Tooling:**
  - `tools/setup.py` mit gepinntem fab-agon-emulator 1.2.5 und agondev v0.22 (SHA-256-geprüft)
  - `build.py`, `test.py`, `run.py`, `send_keys.py`, `screenshot.py`
- **Tests:** Core-Selftest, der identisch auf dem PC und im Headless-Emulator (eZ80) läuft; Exit-Code über I/O-Port 0.
- **Demo-Szene „Hello Glyph“:**
  - MODE 8 mit eigenen 8×8-Glyphen und Dirty-Cell-Rendering
  - Bewegung per Pfeiltasten/WASD
  - Bildschirm-Dump nach `loc.log`
- **CI:** GitHub Actions (Build, Host-Test, Emulator-Test, Artefakt `loc.bin`).
- **Docs:** GDD v0.4, Amiga-Beobachtungen, Roadmap, Architektur, ADRs 0001–0004.
- **Daten:** `data/spells.csv` (Mana-Formel), `data/costs.csv`, `data/actions.csv`.
