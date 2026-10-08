# ADR 0013: Aseprite-Dateien als Quelle der Kacheln, PNGs bleiben eingecheckt

- Status: akzeptiert (2026-10-08, Nutzerwunsch)
- Kontext: Der Nutzer will die Grafiken mit Aseprite und dem MCP-Server [diivi/aseprite-mcp](https://github.com/diivi/aseprite-mcp) bearbeiten.
- Werkzeuge: `tools/art/aseprite.py` (import, export, check, list), `tools/setup.py` (Schritt `aseprite-mcp`), `.mcp.json`

## Kontext

Bisher waren die PNGs in `assets/tiles/` (24×24) und `assets/icons/` (8×8) die Quelle. `tools/build_tiles.py`, `tools/mockup.py` und die CI lesen sie. Aseprite ist eine Bezahlsoftware, und die CI hat kein Aseprite.

## Entscheidung

1. **Jede Kachelfamilie ist eine Sprite-Datei** `assets/aseprite/<familie>.aseprite`. Die Familien ergeben sich aus dem Namen: Präfixe `edge`, `floor`, `wall`, `fence`, `door`, `gate`, `window`, `decor`, `obj`, `fx`, `area`, `cursor` und `portal`, dazu `creatures` (alle Kreaturen mit ihren `_f1`/`_f2`-Bildern), `icons` und `misc`. 16 Dateien, zusammen rund 60 KB.
2. **Jede Kachel ist ein Slice**, benannt nach der PNG. Die Slice-Daten `tiles` oder `icons` geben den Zielordner an. Die Kacheln liegen in einem Raster aus 8 Spalten; das Aseprite-Raster entspricht der Kachelgröße. Die Palette ist `assets/palette/agon64.gpl`, der Farbmodus RGB: Im indizierten Modus würde Schwarz (Konturfarbe) mit Transparenz kollidieren.
3. **Die PNGs bleiben eingecheckt** und sind das, was der Build liest. Build und CI brauchen also kein Aseprite.
4. **Arbeitsablauf:** In Aseprite (von Hand oder über das MCP) bearbeiten, dann `uv run tools/art/aseprite.py export`. Der Export schreibt nur PNGs, deren Pixel sich geändert haben, damit ein erneuter Export die Dateien nicht byteweise neu schreibt. `check` exportiert in einen Temp-Ordner und vergleicht jede PNG pixelgenau; transparente Pixel gelten unabhängig von ihrer Farbe als gleich.
5. **Neue Kacheln aus Skripten** (z. B. `tools/art/make_remains.py`) kommen mit `import --only FAMILIE` in die Sprite-Datei. Achtung: `import` baut die ganze Familie aus den PNGs neu. Wurde im Sprite etwas noch nicht exportiert, zuerst `export` laufen lassen.
6. **MCP-Server:** `tools/setup.py` klont ihn nach `toolchain/aseprite-mcp/` (gitignored), gepinnt auf Commit `90d1696a`, führt `uv sync` aus und schreibt `ASEPRITE_PATH` in seine `.env`. Ohne Aseprite wird der Schritt übersprungen, `--no-aseprite` schaltet ihn ab. `.mcp.json` meldet den Server für Claude Code an; beim ersten Start fragt Claude Code nach der Freigabe.

## Folgen

- Ohne Aseprite lässt sich weiter alles bauen. Grafiken bearbeitet man dann direkt in den PNGs, muss danach aber `import --only FAMILIE` laufen lassen (sobald Aseprite verfügbar ist), damit die Sprite-Datei nicht veraltet.
- Die CI kann nicht prüfen, ob PNG und Sprite zusammenpassen. Vor jedem Commit mit Grafikänderungen deshalb `uv run tools/art/aseprite.py check`.
- `tools/art/make_tiles.py` bleibt als historisches Skript; die PNGs und Sprites sind die Quelle.
- Getestet mit Aseprite 1.3.2 (Windows): Import, Export und Check aller 321 Bilder sind pixelgenau.
