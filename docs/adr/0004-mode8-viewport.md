# ADR 0004 – MODE 8, 1 Zeichen = 1 Feld, Kartenfenster 27×24

- **Status:** **ersetzt durch ADR 0005** (2026-10-02, Review 2: „zu weit weg“). Ursprünglich D8 im GDD.
- **Kontext:**
  - Das Amiga-Original zeigt 7×7 große Kacheln. Das alte GDD (Versuch 1) plante 20×20.
  - Die Sichtweite beträgt 9 Felder am Boden und 11 in der Luft; es müssen also mindestens 19×19 bzw. 23×23 Felder sichtbar sein.
  - Die Welt ist 36×36 groß (Spectrum-Karten).

## Entscheidung

- **MODE 8** (320×240, 64 Farben), **ein 8×8-Zeichen pro Feld**.
- **Kartenfenster 27×24 Felder**, rechts ein Info-Panel mit 13 Spalten, unten Log und Statuszeile (GDD §11.1).
- Das Fenster folgt der aktiven Einheit und scrollt mit Wrap-around.

## Verworfen

- **16×16 in 640×480 (MODE 0):** detaillierter, aber nur 16 Farben und 4× Bandbreite.
- **16×16 in MODE 8 (etwa 13×12 Felder):** kleiner als der Sichtradius.
- Beide bleiben als optionaler Zoom nach v1.0 denkbar.

## Folgen

- Der volle Sichtradius ist ohne Scrollen sichtbar; das Rendering ist am schnellsten.
- Spike M1 prüft nur noch Lesbarkeit und Redraw-Zeit, nicht das Layout.
