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

## Nachtrag 2026-10-05: Shadowcasting (D39, Plattform-Audit)

Die Hardware hat die oben vorgesehene nächste Stufe eingefordert: auf dem
echten Agon Light 2 kostete `sight_compute` **298 ms für zwei Einheiten**
(`docs/AGON-QUIRKS.md`), also knapp doppelt so viel wie der Emulator-Proxy,
und das bei jedem eigenen Schritt.

**Umgestellt auf rekursives Shadowcasting** über acht Oktanten
(`src/core/sight.c`). Statt pro Zielfeld einen Bresenham-Strahl zu werfen
(O(r³) je Einheit), wird jedes Feld einmal besucht (O(r²)).

- **Steigungen bleiben exakte Brüche** und werden durch Kreuzmultiplikation
  verglichen — keine Division, keine Gleitkommazahlen (ADR 0003). Mit
  `col <= row <= 11` bleibt jedes Produkt weit im `int16_t`.
- **Chebyshev-Reichweite fällt aus der Zerlegung heraus:** erreichbare
  Felder erfüllen `max(|dx|, |dy|) <= radius`, die acht Oktanten decken
  genau das Quadrat ab.
- **Wrap** wie bisher: gerechnet wird in relativen Offsets, erst der
  Feldzugriff normalisiert (`world_wrap`).
- **Flieger** sehen unverändert über alles hinweg (`mark_square`), das
  Magische Auge benutzt jetzt dieselbe Funktion.
- `path_clear` bleibt — die Einzellinien-Abfragen (`sight_has_los`,
  `sight_has_spell_los`) sind mit einem Strahl richtig und billig.

### Warum die alte Referenz nicht mehr taugt

Shadowcasting deckt in verwinkeltem Gelände andere Felder auf als
Einzelstrahlen, kann also nicht gegen die alte Fassung auf Gleichheit
geprüft werden. Die Zusicherungen im Selftest sind deshalb auf
**Eigenschaften** umgestellt, die beide Algorithmen erfüllen müssen — und
zwei neue greifen genau den typischen Shadowcasting-Fehler ab:

| Zusicherung | fängt ab |
|---|---|
| freies Gelände ⇒ **exakt** (2·9+1)² = 361 sichtbare Felder | Lücken zwischen zwei Oktanten, Überreichweite |
| alle vier Diagonalecken des Quadrats sichtbar | fehlender Oktant |
| eingemauert ⇒ **exakt** 9 Felder | Schatten wirkt nicht |
| Wand sichtbar, Feld dahinter nicht | Endpunkt-Regel (GDD 3.4) |
| Reichweite 9 am Boden, auch über die Wrap-Naht | Reichweitengrenze |

### Blocking-Bitmap jetzt zwischengespeichert (Audit B1/B2)

Unabhängig vom Algorithmus baute **jede** LOS-Abfrage die Blockier-Bitmap
der ganzen Karte neu (1440 Durchläufe), und die KI fragt eine pro Kandidat
in einer Schleife. Beide Bitmaps (`blk` und das Zauber-Pendant ohne hohes
Gras, D36) hängen jetzt an `World.generation`, wie der View-Cache.

Das macht `world_map_changed()` zur **Pflicht**: ein Geländeschreibzugriff
ohne diesen Aufruf lässt Sicht und View auf veraltetem Gelände rechnen.
Der Spielcode hielt die Invariante bereits überall ein; der Selftest
schrieb `feature[][]` direkt und wurde nachgezogen — dort hatte ein
Tür-Test dadurch still seine Aussage verloren.

**Offen:** Messwerte auf echter Hardware. Erwartung: Sicht deutlich unter
100 ms, KI-Phase (160 ms) überwiegend durch B1 erledigt.
