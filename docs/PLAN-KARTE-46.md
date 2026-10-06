# Plan: Level 1 auf 46×46, geräumigere Häuser

> Stand 2026-10-06 · Wunsch des Nutzers: „Map 10 Felder größer, die Häuser auch, damit mehr Luft im Haus ist.“
> Entscheidung D64 (abgestimmt 2026-10-06). Ersetzt die Größengrenze aus D54 („bleibt 36×36“).

## Warum es jetzt geht

D54 hat 36×36 wegen des RAM festgelegt. Seit #142 ist der Selftest ein eigenes Programm. Die Reserve für Heap und Stack liegt jetzt bei **129 KB** statt ~24 KB.

Was 46×46 kostet (820 Felder mehr):

| Posten | je Feld | zusätzlich |
|---|---|---|
| Statischer Sicht-Cache `scache` (view.c) | 20 B | ~16 KB |
| `World` ×3: Spiel, Probe-Kopie des Kontextmenüs, `SaveGame` in der Arena | ~3 B | ~8 KB |
| Speicherpuffer `SAVE_BUF_SIZE` 8 → 12 KB (der Spielstand passt sonst nicht) | – | 4 KB |
| Kartenlade-Puffer `MAPFILE_MAX` (mapfile.c) | 4 B | ~3 KB |
| Sicht-Bitmaps, Dach-Bits | < 1 B | < 1 KB |
| **Summe** | | **~32 KB → Reserve ~97 KB** |

`build.py` prüft die Reserve bei jedem Build (Grenze 16 KB).

**Laufzeit:** Sichtfenster, Panel und Zeichnen bleiben gleich (9×9). Mit der Fläche wachsen nur Schleifen über alle Felder: Laden, Rundenende der Flächeneffekte, Gesamtkarte. Das sind 1,6-mal so viele Felder. Messen mit `loc --bench` vor und nach.

## Umfang

Nur **Level 1** („The Many Coloured Land“ und seine 16 Varianten) wird größer. Ragaril's Domain, Slayer's Dungeon, Tutorial und Testland behalten ihre Größe; Karten dürfen kleiner als das Maximum sein.

## Schritte (3 PRs)

### PR 1 – Motor für 46×46 (ohne Inhaltsänderung)
- `MAP_MAX_W/H` 36 → 46 (`SIGHT_COLS` bleibt 6 Byte je Zeile).
- `SAVE_BUF_SIZE` 8 → 12 KB, Spielstand v8.
- `gen_maps.py`: Grenze 1..46.
- **Gesamtkarte (`m`):** Bisher 5 px je Feld ab Zeile 16. 46 Zeilen ergäben 246 px, mehr als die 240 des Bildschirms. Neu: Kantenlänge so wählen, dass die Karte passt (4 px bei Karten über 44 Feldern).
- Selftest: eine 46×46-Testkarte laden, Rand-Wrap, Sicht und Speichern/Laden prüfen.
- Messung `loc --bench` (Emulator), RAM-Reserve im PR nennen.

### PR 2 – Neue Häuser (erst Entwurf, dann Karte)
1. **Entwurf zur Abstimmung:** beide Häuser als Textkarte und als Bild (`map_preview.py`), bevor die Karte gebaut wird.
2. Grundsätze für die Häuser:
   - Räume mindestens 5×4 Felder innen, Möbel an den Wänden, ein freier Gang durch jeden Raum.
   - **Jede Tür hat auf beiden Seiten des Durchgangs ein freies Feld für ihr Blatt (D61)**, und keine zwei Türen teilen sich einen Blattplatz (Check wie `d61`, erweitert).
   - Dieselbe Grundausstattung in beiden Häusern (Kessel, Bett, Regale, Truhe, Schwert, Schild, Schriftrolle).
   - Das Gegnerhaus etwa gleich groß, damit die KI (D62) ihre fünf Kreaturen im Haus unterbringt; Wächter haben Platz.
3. Skizze für das eigene Haus (15×11 statt 11×9):

```
###W#####W#####
#B.SS..#B...M.#
#K...K.#.K....W
W..C...D...hT.#
#K...K.#.K....#
#......#......#
###D######D####
#Th...M#S...S.#
#X.....D......W
#......#......#
######W###D####
```

### PR 3 – Varianten und Bevölkerung
- `gen_variants.py`: `W = H = 46`; Hauskästen, Freiflächen, Portal, Wegstümpfe und die Biom-Startpunkte für die neue Karte neu setzen (heute fest für 36×36). Prüfungen wie bisher: Wege zusammenhängend, keine Sackgassen, alles erreichbar.
- Bevölkerung (`populate.c`): Truhen und Funde wachsen mit der Fläche (×1,6). Bei den Wildtieren hält `MAX_UNITS` = 32 die Grenze, denn die KI-Kreaturen (bis 5) und die Herden brauchen Platz.
- Selftests mit Level-1-Koordinaten anpassen (`d61`, `d62`, Szenario-Start, Portal).
- SD: 16 Varianten zu je ~11 KB statt ~5 KB.

## Entscheidungen (Nutzer 2026-10-06)

1. **46×46.**
2. **Vier große Räume** wie in der Skizze.
3. **Wildtiere wie heute**, `MAX_UNITS` bleibt 32.
