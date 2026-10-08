# ADR 0006 – Kachel-Renderer: VDP-Buffer-Bitmaps, Ebenen, Dirty-Felder, Static-Cache

- **Status:** angenommen (2026-10-02), M1 Issue #1
- **Kontext:**
  - ADR 0005 legt 24×24-Kacheln in einem 9×9-Fenster mit mehreren Ebenen pro Feld fest.
  - Zu klären war, wie das auf dem Agon schnell genug wird. Ziel: Voll-Redraw unter 50 ms.

## Entscheidung

1. **Kachelbank auf SD:**
   - `tools/build_tiles.py` erzeugt `tiles.bin` (RGBA2222, 1 Byte pro Pixel) und den Header `src/core/gen/tiles.h` mit `TileId`.
   - Abgeleitete Kacheln entstehen beim Build: Besitzerfarben und Halb-Böden.
2. **Upload beim Start:**
   - Jede Kachel kommt in einen eigenen VDP-Buffer (`0x2000 + id`), danach `bitmap_from_buffer(w, h, RGBA2222)`.
   - Gezeichnet wird mit `select_bitmap(id)` plus `draw_bitmap(x, y)` in Pixelkoordinaten, also etwa 12 Byte pro Kachel.
3. **Ebenen pro Feld** werden im Core berechnet (`view.c`), nicht im Renderer. Der Renderer zeichnet nur die Liste.
4. **Dirty-Felder:**
   - `view_update()` vergleicht die Layer-Liste jedes Fensterfelds mit dem letzten Frame.
   - Nur geänderte Felder werden gezeichnet: Ein Schritt ergibt 2 Felder, das Kerzenflackern 4.
5. **Static-Cache:**
   - Boden, Halb-Böden, Dekor und Feature pro Kartenfeld werden einmal berechnet (`view_rebuild`, etwa 14 KB für 36×36).
   - Pro Frame kommen nur Animation, Objekt, Einheit und Cursor hinzu.
   - Eine Generationsnummer der Welt (`world_map_changed`) invalidiert den Cache.
   - Der Selftest prüft, dass der schnelle Pfad feldweise gleich der Referenz `view_compose()` ist.

## Messwerte (fab-agon-emulator 1.2.5, 18,432 MHz, `loc --bench`)

| Messung | ohne Cache | mit Cache |
|---|---|---|
| Layer-Berechnung, 81 Felder | 30 ms | **12 ms** |
| Voll-Redraw, 81 Felder (≈ 260 Bitmaps) | 68 ms | **48 ms** |
| Kerzen-Animation (4 Felder) | 36 ms | **16 ms** |

**Nachtrag #4 (Sprite-Cursor, Animation ohne Neuberechnung):**

| Messung | vorher | nachher |
|---|---|---|
| Kerzen-Animation | 16 ms | **6 ms** (`view_animate`: nur die als animiert markierten Felder tauschen ihren Frame) |
| Cursor blinken bzw. bewegen | Feld-Redraw | **unter 2 ms** (VDP-Sprite, keine Felder) |
| Voll-Redraw | 48 ms | 46 ms |

6. **Cursor als VDP-Sprite:** 4 Frames (grün, weiß, gelb, rot) aus den Cursor-Kacheln. Er blinkt alle 300 ms wie der blinkende Cursor des Originals. Nach dem Zeichnen von Feldern holt `vdp_refresh_sprites()` ihn wieder nach oben.
7. **Animation:** `view_update()` markiert Felder mit animierten Kacheln. `view_animate(phase)` tauscht nur dort die Frames, ohne die Ebenen neu zu berechnen; der Selftest prüft, dass das gleich der Referenz ist.

**Nachtrag M2b (25 Kreaturen mit Besitzerfarben, Karte 36×36 mit Wrap):**
- Die Kachelbank hat jetzt 236 Einträge (130 KB). Kachel-IDs sind deshalb **16 Bit**; der Static-Cache wächst auf etwa 27 KB.
- Wrap-Karten haben anfangs Modulo-Divisionen pro Feld ausgeführt, die auf dem eZ80 in Software laufen. Die Berechnung stieg dadurch auf 32 ms. Jetzt wird der Ursprung einmal pro Frame normalisiert, pro Feld genügt eine Subtraktion.
- Ergebnis auf Testland: Berechnung 22 ms, Voll-Redraw **50 ms**, Animation 4 ms.

**Auflösung:** `sysvar_time` zählt in 2-cs-Schritten (VBLANK). Jede Messung mittelt deshalb über 10 Frames.

**Offen:** Messwerte auf echter Hardware (Issue #7).

## Folgen und nächste Optimierungen (bei Bedarf)

- Ein Voll-Redraw ist nur beim Scrollen nötig (Kamera mit 2 Feldern Rand) und liegt im Ziel. Normale Züge zeichnen 2–4 Felder.
- Wenn nötig, gibt es weitere Hebel:
  1. ~~VDU-Bytes eines Frames in **einen** Puffer sammeln und mit `mos_puts` senden~~ — **umgesetzt 2026-10-05** (Plattform-Audit B4/B5). `render_fields()` öffnet einen Stapel, `draw_tile()` schreibt `VDU 23,27,&20,id;` und `VDU 23,27,3,x;y;` in einen 512-Byte-Puffer, `mos_puts` schickt ihn mit Länge (RST 18h, Delimiter gilt nur bei Länge 0). Wiederholte Kacheln sparen ihr `select_bitmap`. Nur der Kachelpfad puffert, und nur innerhalb eines Stapels — so kann nichts hinter einem halbvollen Puffer umsortiert werden.
  2. ~~Pro Feld eine Zeichenfolge als **Buffered Command** auf dem VDP ablegen (Issue #2)~~ — **verworfen.** Für wechselnde Koordinaten muss man vor jedem Aufruf Bytes im Puffer patchen (Befehl 5), was etwa so viele Bytes kostet wie das Zeichenkommando selbst. Lohnt nur für Folgen, die unverändert wiederholt werden.
  3. Unveränderte Untergrund-Ebenen beim Scrollen verschieben. **Achtung:** `VDU 23,27,3` gehorcht laut VDP-Doku *weder* dem Grafik-Viewport *noch* dem Koordinatensystem — ein Viewport-Scroll (`VDU 23,7,2,…`) müsste also mit von Hand geclippten Kachelkoordinaten kombiniert werden. Vorher in `spikes/vdptest/` messen, ob das in MODE 8 überhaupt trägt.
