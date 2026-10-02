# ADR 0002 – Eigene Glyph-Grafik statt Bitmap-Tiles

- **Status:** angenommen (2026-10-02)
- **Kontext:**
  - Versuch 1 setzte auf das Pixel-Crawler-Bitmap-Pack. Dessen Lizenz verbietet die Weitergabe, das passt nicht zu einem öffentlichen Repo.
  - Bitmap-Tiles kosten viel Bandbreite zum VDP.
  - Vorbild für die Optik ist *Caves of Qud*: wenige Symbole, Wirkung durch Farbe, Licht und Animation.

## Entscheidung

- Die Grafik besteht aus **eigenen 8×8-Glyphen** (umdefinierte Zeichen ab Code 128) mit Vorder- und Hintergrundfarbe pro Zelle.
- Effekte entstehen über Farbwechsel, Palette-Cycling und Glyph-Animation.
- Glyphen werden selbst erstellt und sind damit lizenzsauber. Später kommen sie aus PNG-Quellen in `assets/glyphs/`, erzeugt per Tool.

## Folgen

- \+ Pro Zelle genügen wenige Bytes zum VDP (Position, Farben, Zeichen); Dirty-Cell-Rendering ist trivial.
- \+ Das Repo kann vollständig öffentlich sein.
- − Weniger Detail als die Amiga-Grafik. Ausgeglichen wird das über Farbe und Komposition (GDD §11.3).
