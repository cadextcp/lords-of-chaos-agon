# ADR 0003 – Plattformfreier Core, zwei Builds

- **Status:** angenommen (2026-10-02)
- **Kontext:** In Versuch 1 war Debugging nur im GUI-Emulator möglich, also langsam und screenshot-abhängig. Spielregeln sollen schnell und automatisch testbar sein.

## Entscheidung

- `src/core` ist reines C99 ohne VDP- und MOS-Aufrufe. Als Ausgabe beschreibt er pro sichtbarem Feld einen Stapel Kachel-IDs mit Dirty-Bits (`view.h`, ADR 0005/0006; bis M0 war es ein Zellen-Grid `screen.h`) und meldet Kampf, Zauber und Tode über einen Ereignis-Ring (`events.h`, M5c).
- `src/agon` zeichnet die Kacheln per VDP und spielt die Ereignisse ab. `host/` gibt sie als Text aus, führt Tests und Simulationen aus (`host/duel.c`, `host/arena.c`).
- Der **Selftest** (`tests/selftest.c`) läuft in **beiden** Builds; auf dem Agon als eigenes Programm `loctest.bin` headless im CLI-Emulator mit Exit-Code über Port 0 (QUIRK S6).
- Zufall nur über einen geseedeten eigenen RNG; keine Gleitkommazahlen in Regeln. Die Ergebnisse sind damit plattformübergreifend bitgleich und über Hashes prüfbar.

## Folgen

- \+ Regeln, Kampfformeln und Balancing lassen sich auf dem Host in Sekunden simulieren.
- \+ eZ80-spezifische Fehler (Integer-Breite, Codegen) findet der Emulator-Selftest.
- − Disziplin nötig: Der Core darf nie Plattform-Header einbinden.
