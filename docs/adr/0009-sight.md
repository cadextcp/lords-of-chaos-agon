# ADR 0009 – Sichtlinie und Hidden Map (M2d)

- **Status:** angenommen (2026-10-03). Bestätigung auf echter Hardware ausstehend (#7): Die Emulator-Zeiten sind ein Proxy.
- **Kontext:** GDD §3.4 (Sichtweite 9/11, blockierendes Gelände, Endpunkte exklusiv), §11.2 (Unerforscht schwarz, Erinnert mit Raster-Overlay, Hidden Movement `[AMI 4]`), Issue #16.

## Entscheidung

1. **Algorithmus: Bresenham-Strahlen pro Zielfeld** (GDD D14).
   - Distanz: **Chebyshev** (8 Richtungen). Reichweite 9 am Boden, 11 in der Luft (`UF_FLYING`).
   - Blockiert wird nur Terrain **zwischen** den Endpunkten; Wände und Bäume sind selbst sichtbar. Diagonalen sind permissiv (keine Ecken-Regel) – eigenes Design (D7).
   - Sichtquelle: jede Einheit des Spielers; unsichtbar Flag (`UF_INVISIBLE`) versteht sich nur vor **Gegnern**, die eigene Einheit sieht weiterhin.
2. **Hidden Map als Bitfeld pro Spieler** (`src/core/sight.[ch]`): `explored` (akkumuliert) und `visible` (pro Berechnung), je 36×36 Bit = 162 Byte. Neuberechnung nur nach eigenen Schritten und am Rundenende, nie pro Frame.
3. **Darstellung** (`view.c`): unerforscht = eine schwarze Kachel (`T_UNEXPLORED`, neue Pixelart), erinnert = `T_OVERLAY_REMEMBERED` (50-%-Raster) über den statischen Ebenen, Einheiten nur bei aktueller Sicht (Eigene immer – ihre Position ist per Definition sichtbar). Objekte bleiben im Erinnerten stehen (sie bewegen sich nicht; Veralten kommt mit Brandschaden in M3+).

## Messung auf dem eZ80 (CLI-Emulator, `loc --bench`, Testland, 2 eigene Einheiten)

| Stufe | `sight_compute` | Anmerkung |
|---|---|---|
| 1. Erste Fassung (`world_wrap` + `world_blocks_sight` pro Ray-Schritt) | **792 ms** | Modulo = Software-Division (AGON-QUIRKS T2) |
| 2. Unwrapped Rays + bedingte Normalisierung, `world_blocks_sight_at` | 270 ms | |
| 3. Blocking-Bitmap (1 Bit/Feld, byteweiser Aufbau via `world_sight_byte`), int8-Bresenham, already-visible-Übersprung | 228 ms → **162 ms** | mit `-O2` (statt agondevs `-Oz`) |
| — `-O2` Nebenwirkung | — | View-Compose 28 → 18 ms, Voll-Redraw 52 → 44 ms; Binary 51 → 70 KB (Budget 448 KB, ADR 0008) |

`-O2` ist als Override im `Makefile` dokumentiert.

## Folgen

- **162 ms pro Neuberechnung** (Emulator-Proxy) sind pro eigenem Schritt spürbar, aber spielbar. Reicht für M2d; die Neuberechnung fällt nur auf Eingaben, nicht pro Frame.
- Falls M3 (KI) oder das Hardware-Erlebnis es verlangt, sind die nächsten Stufen: Perimeter-Strahlen mit Markierung im Vorbeigehen (~5× weniger Strahlen) oder Shadowcasting. Die Selftests vergleichen den schnellen Pfad gegen die Referenzkomposition, sodass ein Umbau abgesichert ist.
- Die schwarze Kachel verschiebt alle Tile-IDs hinter „tree“; `HOUSE_VIEW_HASH` bewusst auf `0xA470BF45` aktualisiert.
- Dächer/überdachte Felder und die Luftregeln (verdecktes Terrain, Objekte unsichtbar) folgen mit M2e.
