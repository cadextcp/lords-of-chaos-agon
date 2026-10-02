# Roadmap

Jeder Milestone endet mit etwas **Spielbarem bzw. Prüfbarem** auf dem Agon. Inhaltlich ist die Reihenfolge durch [design/GDD.md §15](design/GDD.md) festgelegt.

| Milestone | Inhalt | Exit-Kriterium | Tag |
|---|---|---|---|
| **M0 Fundament** | Repo, Setup/Build/Test/Run-Tools, CI, Demo „Hello Glyph“, Docs, ADRs | `tools/test.py` grün lokal und in CI; GUI zeigt eigene Glyphen | `v0.1.0` |
| **M1 Tech-Spikes** | (a) Vollbild-Redraw 27×24 messen (Ziel unter 100 ms); (b) Buffered VDP Commands; (c) **Eingabe-Spike mit Cherry G84-4100**: Pfeil-Akkorde, Pos1/Ende/Bild, `<>`-Taste, Auto-Repeat; (d) Palette-Cycling; (e) Daten vom SD laden; (f) Layout-Mockup 27×24 + Panel | ADR „Rendering“ und ADR „Eingabe“ mit Messwerten | `v0.2.0` |
| **M2 Core-Skelett** | Karte 36×36 mit Wrap-around, Ebenen, Sicht und Hidden Map, Kreatur- und Kostendaten aus CSV, aktive Einheit, Bewegung, Bump, `Tab`, Look-Modus, Rundenablauf | Ein Zauberer und Kreaturen bewegen sich auf einer Testkarte, auf Agon und Host | `v0.3.0` |
| **M3 Classic spielbar** | Kampf, Beschwörungen, Bolt/Lightning, Basis-Objekte, Portal und VP, einfache KI, eigenes Szenario 1 | Eine Partie gegen einen KI-Zauberer lässt sich komplett durchspielen | `v0.4.0` |
| **M4 Classic komplett** | Alle 45 Zauber und Tränke, Flächeneffekte, Wizard Designer, Kampagne, Szenarien 2 und 3, Setup-Panel, Speichern | Funktionsumfang ≈ Amiga-Version (Einzelspieler) | `v1.0.0` |
| nach v1.0 | Hotseat, Timer, Maus, Expansion-Kit-Inhalte | – | – |
| **M5+ Chaos** | Welt-Tick, Feuer v2, Herden, Licht, Effekte (je ein Milestone) | je Feature | – |

## Arbeitsweise

- **`main` ist immer grün.** Jede Änderung läuft über einen Feature-Branch und einen PR, der erst gemergt wird, wenn CI grün ist.
- **GitHub Milestones = Phasen**, **Issues** pro Feature mit Akzeptanzkriterien.
- **Spikes sind timeboxed** und haben ein Abbruchkriterium. Ihr Ergebnis ist ein ADR in `docs/adr/`, kein Produktionscode.
- **Definition of Done:**
  1. Core-Logik hat einen Selftest bzw. Host-Test.
  2. Sie läuft im Emulator, belegt durch einen Dump oder Screenshot im PR.
  3. Docs und `CHANGELOG.md` sind aktualisiert.
- **Budget im Blick:** CI meldet die Größe von `loc.bin`. RAM-Budget: 512 KB minus MOS.
