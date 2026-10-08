# ADR 0009 – Sicht und verdeckte Karte (K11)

- **Status:** angenommen (2026-10-03, M2d), **überarbeitet 2026-10-08**: Shadowcasting seit 2026-10-05 (D39), Regeln des Originals K11 seit 2026-10-07 (D67, Schritt 0h), Karten bis 46×46 (D64). Hardware-Messung der K11-Fassung steht aus (#7).
- **Kontext:** GDD §3.4 und §11.2, Regelbericht `docs/REGELN-ORIGINAL-SPECTRUM.md` Kapitel 11 (K11.1 Reichweite, K11.2 Höhenpaare, K11.4 Deckung, K11.6 Schusslinie), Issue #16, Plattform-Audit `docs/AUDIT-PLATTFORM.md` (B1–B3).
- **Code:** `src/core/sight.[ch]`; Aufrufer `update_sight()` in `src/agon/main.c`, die KI über `sight_sees` (K10), Zauber über `spell_line_clear` (D68).

## Entscheidung

### 1. Reichweite: das Achteck des Originals (K11.1)

Ein Feld mit Abstand `(dx, dy)` liegt in Reichweite `R`, wenn **`2·|dx| < R`, `2·|dy| < R` und `D = 2·max + min < R`** (`sight_in_reach`). Damit ergibt sich ein Achteck, kein Quadrat:

| Höhe | R | gerade | diagonal |
|---|---|---|---|
| Boden | 19 (`SIGHT_R_GROUND`) | 9 Felder | 6 Felder |
| Luft | 23 (`SIGHT_R_AIR`) | 11 Felder | 7 Felder |

Die acht Nachbarfelder (`D < 4`) sind immer sichtbar, auch um eine Ecke herum.

### 2. Boden: rekursives Shadowcasting (D39)

- Acht Oktanten, jedes Feld wird einmal besucht (O(r²)). Die Steigungen sind exakte Brüche und werden über Kreuzmultiplikation verglichen, ohne Division und ohne Gleitkommazahlen (ADR 0003). Mit `col <= row <= 11` bleibt jedes Produkt im `int16_t`.
- Das Oktanten-Raster reicht bis 9 Felder (`SIGHT_GROUND`). Markiert wird nur, was im Achteck liegt (`2·row + col < R`).
- **Endpunkt-Regel:** Wände, Bäume und geschlossene Türen sind selbst sichtbar, die Felder dahinter nicht.
- **Feuer und Brei blockieren** wie Wände (K11.4). Sie stehen in der Blockier-Bitmap.
- Wrap: gerechnet wird in relativen Offsets, erst der Feldzugriff normalisiert (`world_wrap`).

### 3. Luft: über Wände hinweg, aber mit Deckung (K11.2, K11.4)

Ein Flieger sieht alles Gelände im Achteck (`mark_octagon`) ohne Schatten. **Bodenziele und Gegenstände unter Deckung sieht er nicht.** Deckung ist ein Dach (`world_has_roof`) oder ein Blätterdach: Gelände mit `blocks_sight_ground` (Wald, hohes Gras, Zauberwald, Totenwald) oder ein Baum. Einheiten auf den Nachbarfeldern sieht er auch unter einem Blätterdach, aber nie unter einem Dach.

### 4. Wer wen sieht: die Höhenpaare (K11.2, `sight_sees`)

| Beobachter → Ziel | Regel |
|---|---|
| Boden → Boden | Shadowcasting bzw. `sight_has_los`; Nachbarn immer |
| Boden → Luft | im Achteck **immer**, auch über Wände; unter einem Dach **nie** |
| Luft → Luft | im Achteck immer |
| Luft → Boden | nicht unter Dach; nicht unter Blätterdach, außer Nachbarn |

Unsichtbarkeit (`UF_INVISIBLE`, Pixie, Trank) prüft `sight_sees` nicht, das machen die Aufrufer.

### 5. Verdeckte Karte: fünf Bitfelder je Spieler

`Sight` hält je ein Bit pro Feld für:

- `explored`: wächst nur
- `visible`: Gelände jetzt in Sicht
- `vis_ground`, `vis_air`: Ziele dieser Höhe jetzt sichtbar
- `vis_obj`: Gegenstände jetzt sichtbar

Jedes Bitfeld hat 46 × 6 Byte, zusammen 1380 Byte. Ob eine Einheit oder ein Gegenstand sichtbar ist, lesen `sight_unit_visible` und `sight_object_visible` aus dem passenden Feld.

Darstellung wie bisher (`view.c`):

- Unerforschtes ist eine schwarze Kachel (`T_UNEXPLORED`).
- Erinnertes Gelände bekommt ein 50-%-Raster (`T_OVERLAY_REMEMBERED`).
- Fremde Einheiten erscheinen nur bei aktueller Sicht.
- Gegenstände und Skelette (D70) bleiben im Erinnerten stehen.

### 6. Schusslinien (K11.6) und Zauberlinien

- `sight_shot_clear(von, Höhe, nach, Höhe)` regelt die Schusslinie:
  - Boden zu Boden braucht eine freie Linie (`sight_has_los`, ein Bresenham-Strahl über die Blockier-Bitmap, Endpunkte ausgenommen).
  - Ist die Luft beteiligt, stoppt nur ein Dach: ein Schütze unter einem Dach schießt nicht nach oben, und ein überdachtes Bodenziel ist nicht erreichbar.
  - Wände würden nur im Dungeon-Szenario zählen, das es auf unseren Karten nicht gibt (FRAGEN F2/F22).
- Zauber nutzen `sight_has_spell_los`, bei dem hohes Gras nicht blockiert (D36). Mit Zielhöhe gilt `spell_line_clear` (D68): Boden zu Boden nimmt die Zauberlinie, sonst gilt die Schusslinie.
- Würfe fliegen weiter Feld für Feld (FRAGEN F22).

### 7. Magisches Auge

Das Achteck mit Reichweite `3L + 10` ignoriert Wände und Deckung. Es zeigt Bodenziele auch unter Dach und Blätterdach und enthüllt Unsichtbare für eine Runde (`sight_add_eye`).

### 8. Wann neu gerechnet wird, Zwischenspeicher

- `sight_compute` läuft nach eigenen Schritten und am Rundenende, **nie pro Frame**. Die KI rechnet keine ganze Karte, sondern fragt je Beobachter `sight_sees` (Sichtlisten, K10).
- Die Blockier-Bitmaps `blk` (Sicht) und `blk_spell` (ohne hohes Gras) hängen an `World.generation`. **`world_map_changed()` ist Pflicht** nach jedem Schreiben in das Gelände; die Flächeneffekte rufen es selbst auf. Ohne den Aufruf rechnen Sicht und View auf veraltetem Gelände.
- `sight_look` (Dachanzeige, D56) speichert die Sicht einer einzelnen Bodenfigur je Position und Generation zwischen.

## Messungen (`loc --bench`)

| Stand | Umgebung | `sight_compute` |
|---|---|---|
| 2026-10-03, Bresenham pro Zielfeld, erste Fassung | Emulator, Testland, 2 Einheiten | 792 ms |
| ebenso, ungewrappte Strahlen und Blocking-Bitmap, `-O2` | Emulator | 162 ms |
| ebenso | **Hardware** (Agon Light 2) | **298 ms** |
| 2026-10-05, Shadowcasting (D39) | **Hardware** | **12 ms** |
| 2026-10-08, K11 auf Level 1 (46×46) | GUI-Emulator 1.2.5 | 22 ms je Aufruf |

`-O2` statt agondevs `-Oz` ist im `Makefile` dokumentiert. Der Emulator unterschätzt die Hardware laut Audit um den Faktor 2 bis 5; die K11-Zahl ist deshalb nur ein Hinweis.

## Abgelöst

| Früher | Ersetzt durch |
|---|---|
| ein Bresenham-Strahl pro Zielfeld (M2d) | Shadowcasting (D39, 2026-10-05) |
| Reichweite als Chebyshev-Quadrat 9 bzw. 11 | Achteck R 19 bzw. 23 (K11.1, D67) |
| Flieger sehen alles (`mark_square`) | Achteck mit Deckung (K11.2, K11.4) |
| zwei Bitfelder mit je 36×36 Bit (162 Byte) | fünf Bitfelder bis 46×46 |

## Folgen

- \+ Die Sichtregeln entsprechen dem Original, auch für die KI. Spieler und KI fragen dieselben Funktionen.
- \+ Die Selftests prüfen Eigenschaften statt einer Referenzmenge:
  - freies Gelände ergibt genau das Achteck
  - 9 gerade und 6 diagonal am Boden, 11 und 7 in der Luft
  - eingemauert sieht man nur das eigene Feld und die Wände
  - Wand sichtbar, das Feld dahinter nicht
  - die Reichweite gilt auch über die Wrap-Naht
  - die Höhenpaare, Deckung, Auge, Feuerwand und die Schusslinien (Tests `sight:` und `0h:`)
- − Shadowcasting deckt in verwinkeltem Gelände andere Felder auf als Einzelstrahlen. Die Zielprüfung (`sight_has_los`) und die Flächensicht können sich an Ecken um ein Feld unterscheiden.
- − Die Deckungsregel macht Gegenstände unter Wald aus der Luft unsichtbar. Die Feldanzeige richtet sich aber nach dem Gelände (FRAGEN F20).
- **Offen:** `sight_compute` und die KI-Phase auf der Hardware nach K11 und auf der 46×46-Karte neu messen (#7).
