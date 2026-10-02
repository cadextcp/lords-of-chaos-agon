# ADR 0003 – Plattformfreier Core, zwei Builds

- **Status:** angenommen (2026-10-02)
- **Kontext:** In Versuch 1 war Debugging nur im GUI-Emulator möglich, also langsam und screenshot-abhängig. Spielregeln sollen schnell und automatisch testbar sein.

## Entscheidung

- `src/core` ist reines C99 ohne VDP- und MOS-Aufrufe. Als Ausgabe beschreibt er ein Zellen-Grid mit Dirty-Bits (`screen.h`).
- `src/agon` zeichnet dieses Grid per VDP. `host/` gibt es als Text aus bzw. führt Tests aus.
- Der **Selftest** (`src/core/selftest.c`) läuft in **beiden** Builds, auf dem Agon headless im CLI-Emulator mit Exit-Code über Port 0.
- Zufall nur über einen geseedeten eigenen RNG; keine Gleitkommazahlen in Regeln. Die Ergebnisse sind damit plattformübergreifend bitgleich und über Hashes prüfbar.

## Folgen

- \+ Regeln, Kampfformeln und Balancing lassen sich auf dem Host in Sekunden simulieren.
- \+ eZ80-spezifische Fehler (Integer-Breite, Codegen) findet der Emulator-Selftest.
- − Disziplin nötig: Der Core darf nie Plattform-Header einbinden.
