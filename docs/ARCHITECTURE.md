# Architektur

## Schichten

```
            +---------------------------------------------+
            |  src/core  (C99, plattformfrei)             |
            |  Spielzustand, Regeln, RNG, Daten-Tabellen, |
            |  KI, Sicht, Kachel-Beschreibung (view.h)    |
            +----------------------+----------------------+
                                   |  nur über Kachel-IDs + Ereignisse
              +--------------------+--------------------+
              |                                         |
   +----------v-----------+                 +-----------v----------+
   | src/agon (agondev)   |                 | host/ (gcc)          |
   | VDP-Renderer (MODE 8)|                 | Terminal-Dump,       |
   | Tastatur (kbuf)      |                 | Selftest, duel, arena|
   | MOS-Dateien, Log     |                 |                      |
   | emu_exit (Port 0)    |                 |                      |
   +----------------------+                 +----------------------+
        bin/loc.bin                             build/host/loc_host
```

**Regel:** `src/core` darf keine Header aus `agon/` oder `host/` einbinden und keine VDP- oder MOS-Funktionen aufrufen. Alles, was der Core zeigen will, beschreibt er als Daten, das Frontend zeichnet.

- **M0 (veraltet):** Zellen-Grid (`screen.h`) mit Glyph und Farben pro Zelle.
- **Ab M1 (D9, ADR 0005):** pro sichtbarem Feld eine Liste von **Kachel-IDs** (Boden, Dekor, Feature, Objekt, Einheiten, Effekt, Sicht-Overlay) plus Dirty-Bits. Das Agon-Frontend zeichnet sie als VDP-Bitmaps übereinander; der Host gibt sie als Text bzw. PNG aus.

## Konventionen für den Core

- **`int` ist auf dem eZ80 24 Bit breit.** Im Core werden nur explizite Typen verwendet (`uint8_t`, `int16_t`, `uint32_t`).
- **Determinismus:**
  - Zufall kommt ausschließlich aus `rng.h` (xorshift32, geseedet).
  - Kein `rand()`, keine Gleitkommazahlen in Spielregeln.
- **Keine dynamische Speicherverwaltung** im Spielbetrieb. Statische Pools haben feste Obergrenzen.
- **Daten (ADR 0008):**
  - Regeltabellen (`data/*.csv`) werden zu const-Tabellen kompiliert (`gen/data.c`).
  - Karten (`data/maps/*.txt`) werden zu `.map`-Dateien, die das Spiel von SD lädt (`world_load_bin`, vollständig validiert).
  - Kacheln kommen als `tiles.bin` von SD.

## Warum zwei Builds?

Der **Selftest** (`tests/selftest.c`) wird in beide Test-Builds kompiliert:

- **Host:** schnell, mit gcc-Warnungen als Fehler (`-Werror`).
- **Agon:** derselbe Test als echter eZ80-Code im Headless-Emulator, als eigenes Programm `loctest.bin` (nicht im Spiel, QUIRK S6). Er findet Probleme mit Integer-Breite, Codegen und libc, die der Host nie sehen würde.

Beispiel: Der Ansicht-Hash des Zauberer-Hauses (`HOUSE_VIEW_HASH`) muss auf beiden Plattformen bitgleich sein.

## Module des Core (Überblick)

| Bereich | Dateien | ADR |
|---|---|---|
| Welt, Einheiten, Karte laden | `world.[ch]`, `map_def.h` | 0008, 0014 |
| Runden und Phasen | `turn.[ch]` | – |
| Regeln: Kampf, Zauber, Gegenstände, Tränke, Reiten, Flächen, Effekte | `combat`, `spells`, `items`, `brew`, `ride`, `area`, `effect` | GDD D67, D68 |
| Sicht und verdeckte Karte | `sight.[ch]` | 0009 |
| Computer-Gegner | `ai.h`, `ai_priv.h`, `ai*.c` | 0015 |
| Wertung, Portal, Kampagne | `game.[ch]`, `wizard.[ch]` | – |
| Spielstand, Lexikon | `save.[ch]`, `lexicon.[ch]` | 0014 |
| Darstellung: Kachel-Komposition, Ereignisse | `view.[ch]`, `events.[ch]` | 0005, 0006 |
| Zufall | `rng.[ch]` | 0003 |


## Toolchain

| Teil | Version | Wo |
|---|---|---|
| agondev (clang/LLVM für ez80, GNU as/ld) | v0.22 | `toolchain/agondev`; unter Windows über WSL ausgeführt |
| fab-agon-emulator (GUI + CLI) | 1.2.5 | `emulator/1.2.5` |
| MOS | Console8 2.3.3 (`--firmware console8`) | gleich für GUI und CLI |

Versionen werden in `tools/agon_env.py` und `tools/setup.py` gepinnt (siehe ADR 0001).
