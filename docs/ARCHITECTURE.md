# Architektur

## Schichten

```
            +---------------------------------------------+
            |  src/core  (C99, plattformfrei)             |
            |  Spielzustand, Regeln, RNG, Daten-Tabellen, |
            |  Render-Beschreibung (screen.h), Selftest   |
            +----------------------+----------------------+
                                   |  nur über Cell-Grid + Events
              +--------------------+--------------------+
              |                                         |
   +----------v-----------+                 +-----------v----------+
   | src/agon (agondev)   |                 | host/ (gcc)          |
   | VDP-Renderer (MODE 8)|                 | Terminal-Dump,       |
   | Tastatur (kbuf)      |                 | Selftest-Runner      |
   | MOS-Dateien, Log     |                 |                      |
   | emu_exit (Port 0)    |                 |                      |
   +----------------------+                 +----------------------+
        bin/loc.bin                             build/host/loc_host
```

**Regel:** `src/core` darf keine Header aus `agon/` oder `host/` einbinden und keine VDP- oder MOS-Funktionen aufrufen. Alles, was der Core zeigen will, landet im **Cell-Grid** (`screen.h`): Glyph, Vordergrund- und Hintergrundfarbe pro Zelle, plus Dirty-Bits. Das Frontend zeichnet nur, was sich geändert hat.

## Konventionen für den Core

- **`int` ist auf dem eZ80 24 Bit breit.** Im Core werden nur explizite Typen verwendet (`uint8_t`, `int16_t`, `uint32_t`).
- **Determinismus:**
  - Zufall kommt ausschließlich aus `rng.h` (xorshift32, geseedet).
  - Kein `rand()`, keine Gleitkommazahlen in Spielregeln.
- **Keine dynamische Speicherverwaltung** im Spielbetrieb. Statische Pools haben feste Obergrenzen.
- **Daten** liegen in `data/*.csv` und werden später per Generator zu const-Tabellen in C.

## Warum zwei Builds?

Der **Selftest** (`src/core/selftest.c`) wird in beide Builds kompiliert:

- **Host:** schnell, mit gcc-Warnungen als Fehler (`-Werror`).
- **Agon:** derselbe Test als echter eZ80-Code im Headless-Emulator (`loc --selftest`). Er findet Probleme mit Integer-Breite, Codegen und libc, die der Host nie sehen würde.

Beispiel: Der Szenen-Hash (`DEMO_HASH`) muss auf beiden Plattformen bitgleich sein.

## Toolchain

| Teil | Version | Wo |
|---|---|---|
| agondev (clang/LLVM für ez80, GNU as/ld) | v0.22 | `toolchain/agondev`; unter Windows über WSL ausgeführt |
| fab-agon-emulator (GUI + CLI) | 1.2.5 | `emulator/1.2.5` |
| MOS | Console8 2.3.3 (`--firmware console8`) | gleich für GUI und CLI |

Versionen werden in `tools/agon_env.py` und `tools/setup.py` gepinnt (siehe ADR 0001).
