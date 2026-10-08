# ADR 0008 – Daten: Regeltabellen einkompiliert, Karten und Kacheln von SD

- **Status:** angenommen (2026-10-02), M1 Issue #5
- **Kontext:** Bisher waren Karten als Zeichen-Strings einkompiliert, Kacheln kamen schon von SD (ADR 0006). Mit Szenarien, Kreaturen und Zaubern wächst die Datenmenge. Zu entscheiden war, was in `loc.bin` gehört und was von der SD-Karte kommt.

## Entscheidung

| Daten | Größe | Wo | Warum |
|---|---|---|---|
| Regeltabellen: Zauber, Kosten, Aktionen, später Kreaturen und Waffen | < 2 KB | **einkompiliert**: `tools/gen_data.py` → `src/core/gen/data.[ch]` | winzig, immer verfügbar; Host- und eZ80-Selftest prüfen dieselben Werte; keine Dateifehler möglich |
| Karten bzw. Szenarien | ≤ 4,2 KB pro Karte (36×36) | **SD**: `/loc/maps/<name>.map` | Inhalte wachsen und ändern sich ohne Neukompilieren; die Kampagne braucht nur eine Karte im RAM |
| Kacheln | 37 KB, später etwa 230 KB | **SD**: `/loc/tiles.bin` (ADR 0006) | landen ohnehin im VDP-Speicher |

**Kartenformat `.map` v1:**
- Aufbau: `"LOCM"`, Version, Kachelanzahl, Breite, Höhe, Wrap, drei Ebenen als Enum-Werte, Einheiten, Objekte. Details in `tools/gen_maps.py`.
- `tools/gen_maps.py` liest die Enum-Werte **direkt aus den C-Headern**, damit Python und C nicht auseinanderlaufen können.
- `world_load_bin()` **prüft alles vor dem Übernehmen**: Magic, Version, Kachelanzahl (veraltete Karte gegenüber Kachelbank), Größe, Wertebereiche, Längen. Bei einem Fehler bleibt die Welt unverändert.
- Dieselben Bytes landen zusätzlich als C-Array in `gen/maps.c`. So testet der Selftest auf Host und eZ80 den echten Lader, auch mit absichtlich kaputten Daten.

## Messwerte (Emulator)

- Laden des Zauberer-Hauses (266 Byte) von SD dauert unter 20 ms (unter der Uhrauflösung). Der View-Hash ist identisch zur früheren einkompilierten Karte.
- Speicher von `loc.bin`:

| Bereich | Größe | Inhalt |
|---|---|---|
| `.text` | 27,6 KB | Code |
| `.rodata` | 5,0 KB | Tabellen, Namen, Testkarte |
| `.bss` | 30,5 KB | Welt 4,6 KB, View-Cache 14,3 KB, Kartenpuffer 4,2 KB, Selftest-Welt 4,6 KB |

  Insgesamt sind das etwa 63 KB von 448 KB Programm-RAM.

## Nachtrag 2026-10-08: heutige Zahlen

Die Entscheidung gilt weiter; die Zahlen oben sind von M1. Stand nach D64–D71:

| Was | damals | heute |
|---|---|---|
| Kartenformat | `.map` v1, bis 36×36, 4,2 KB | v5, bis 46×46 (D64), Level 1: 8,5 KB |
| Regeltabellen | < 2 KB | Zauber, Kosten, Aktionen, Kreaturen, Waffen, Objekte, Gelände, KI-Gegenstände (`gen/data.c`), weiter einkompiliert |
| Szenarien | Teil der Karte | eigene Datei `.scn` v3 (ADR 0014) |
| Kacheln | 37 KB | `tiles.bin` 298 KB |
| `loc.bin` | 63 KB RAM belegt | 277 KB Binary; RAM-Reserve für Heap und Stack 62 KB (`build.py` bricht unter 16 KB ab) |

Hilfe, Musik, Samples und Titelbild sind nach ADR 0011 ebenfalls auf die SD gewandert. Alle Formate und ihre Versionsregeln stehen in ADR 0014.

## Folgen

- Neue Szenarien entstehen als Text in `data/maps/` und werden beim Build kompiliert.
- Neue Regelwerte kommen als CSV in `data/` und werden zu Tabellen.
- Offen auf Hardware (#7): lange Dateinamen (`wizard_house.map`) auf der echten FAT-SD-Karte prüfen. Notfalls auf 8.3-Namen umstellen.
