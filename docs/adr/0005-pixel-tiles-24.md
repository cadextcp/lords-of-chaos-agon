# ADR 0005 – Eigene 24×24-Pixel-Kacheln als VDP-Bitmaps, Sicht 9×9

- **Status:** angenommen (2026-10-02). Ersetzt ADR 0002 (Glyphen) und ADR 0004 (27×24 Felder); GDD D9/D10.
- **Kontext:**
  - Die M0-Demo mit 8×8-Glyphen sah „nicht aus wie die Referenzen“ und war „zu weit weg“.
  - Spectrum (24×24-Kacheln) und Amiga (7×7 Sicht, mehrere Ebenen pro Kachel) zeigen eingerichtete Räume: Möbel, Teppiche, Türen, Schubladen.
  - Frei lizenzierte Packs bieten nicht alles zugleich (24×24, viele Möbel, frei weitergebbar), deshalb gibt es eigene Grafik.

## Entscheidung

- **MODE 8** (320×240, 64 Farben).
- **24×24-Pixel-Kacheln**, Kartenfenster **9×9** (216×216 px); rechts ein Panel mit 104 px, unten 3 Textzeilen (GDD §11.1).
- **Ebenen pro Feld** (Boden, Dekor, Feature, Objekt, Einheiten, Effekt, Sicht-Overlay) als **VDP-Bitmaps** mit transparenten Pixeln. Der Cursor ist ein Hardware-Sprite (GDD §11.3).
- **Eigene Pixelart** in der Agon-64-Farben-Palette. Pipeline: PNG → `tools/build_tiles.py` → `tiles.bin` (RGBA2222) plus ID-Header; `tools/mockup.py` liefert Vorschau-Bilder (GDD §11.3a).
- Der Core liefert pro Feld Kachel-IDs statt Glyphen.

## Folgen

- \+ Optik nah an den Originalen; Möbel, Teppiche, Türen und Schubladen sind darstellbar; mehrere Ebenen pro Feld wie auf dem Amiga.
- \+ Bandbreite bleibt beherrschbar: etwa 11 Byte pro Kachel-Draw, rund 3 KB für ein volles Fenster, etwa 30 ms (Abschätzung, M1 misst).
- − Grafikaufwand: Jede Kachel muss gezeichnet werden. Das wird über Pipeline und Mockups früh sichtbar gemacht.
- − Kleinere Sicht (±4 Felder) als der Sichtradius. Wie im Original gleicht die Big Map das aus.
- − Palette-Cycling entfällt in MODE 8 (feste 64er-Palette); es wird durch Frame-Animation ersetzt.
- Der M0-Glyph-Renderer (`src/agon/render.c`, `src/core/glyphs.*`, `screen.*`) dient nur noch als Pipeline-Nachweis und wird in M1 ersetzt.
