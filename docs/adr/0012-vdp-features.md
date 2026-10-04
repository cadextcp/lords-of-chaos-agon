# ADR 0012: Welche VDP-Fähigkeiten das Spiel nutzt

- Status: akzeptiert für den Emulator (2026-10-04, Polish-Runde), **Hardware-Bestätigung offen**
- Kontext: Polish-Plan (Bildschirmreste, Audio, Grafik), Nutzerwunsch „mehr Agon, weniger Spectrum“
- Messprogramm: `vdptest [n]` – eigenes Agon-Programm (`spikes/vdptest/`, ~15 KB), damit es dem Spiel keinen RAM kostet. `uv run tools/run.py --vdptest [n]`; auf der SD `/loc/vdptest.bin`, Ergebnisse in `/loc/vdptest.log`

## Kontext

Bisher nutzt das Spiel vom VDP nur Bitmaps aus Buffern, ein Cursor-Sprite
und Rechteck-/Textbefehle. Die Console8-/Platform-Firmware kann mehr:
eigene Schriften, viele Sprites, Doppelpuffer, Samples, weitere
Audiokanäle, Copper/Paletten. Vor dem Einbau sollte ein kurzer Spike
klären, was davon funktioniert und was es kostet.

## Messung (GUI-Emulator 1.2.5, Console8-Firmware)

| Test | Ergebnis |
|---|---|
| A1 drei Noten direkt hintereinander auf Kanal 0 | `1 0 0` (im eigenständigen Programm `0 0 0`, dort klingt noch der MOS-Startton): **der VDP verwirft Noten auf einem belegten Kanal** |
| A2 Note nach dem Ende der vorigen | `1` |
| A3 Sample erzeugen (eZ80) + hochladen, 8000 Byte | 290 cs, fast nur die Synthese |
| A3b nur hochladen, 8000 Byte in 500-Byte-Blöcken | 12 cs |
| A4 Sample auf Kanal 1 spielen | `1` (angenommen) |
| A5 Kanäle 4 und 5 mit `enable_channel` | `1 1` |
| F1 Systemfont in Buffer kopieren und als Font wählen | funktioniert |
| F2 eigene 8×16-Schrift aus Buffer (`VDU 23,0,&95`) | wird korrekt gezeichnet, Zeilenraster 16 px |
| P1 `VDU 19` in MODE 8 (Rot → Grün) | **keine Wirkung**, Pixel bleibt rot |
| S1 8 Sprites, 150 Bewegungen mit Refresh | 52 cs = **3 ms pro Bild** |
| D1 MODE 136 (8 + 128) | 320×240, 64 Farben, Bestätigung kommt |
| D2 40 Vollbilder zeichnen + `vdp_swap` | 156 cs = 39 ms pro Bild; Kachel-Bitmaps überleben den Moduswechsel |

Nebenbefund **RAM**: Als Teil von `loc.bin` sprengten zwei 4-KB- bzw.
8-KB-Puffer den eZ80-RAM (`USERRAM overflowed by 5022 bytes`). Ohne sie
blieben nur ~6 KB Stack, und der eZ80-Selftest schlug fehl. Das Spiel
belegt ~275 KB Code (mit `-O2`) und ~121 KB statische Daten von 448 KB.
**Große Daten nur gestreamt** durch kleine Puffer; vor neuen Features
RAM zurückgewinnen (gemeinsamer Textpuffer für Hilfe/Lexikon/Zauber,
`-Oz` für kalten Code). Der Spike ist deshalb ein eigenes Programm.

## Entscheidung

1. **Audio:** Effekte und Musik werden vom Programm **pro Note getaktet**
   (Sequenzer mit `getsysvar_time()`), nie mehrere Noten auf einmal
   geschickt. Effekte nutzen **8-Bit-Samples**, die auf dem PC erzeugt und
   von der SD gestreamt werden; Musik bekommt gesampelte Instrumente.
   Zusätzliche Kanäle (4, 5 …) sind erlaubt, damit Effekte die Musik nicht
   abschneiden.
2. **Schrift:** Eigene Fonts aus Buffern (Zierschrift für Überschriften,
   Textschrift mit Umlauten). Fallback: Systemfont.
3. **Sprites:** Bewegte Einheiten, Projektile und Schadenszahlen werden
   Sprites. 8 Sprites kosten ~3 ms pro Bild.
4. **Doppelpuffer:** **nicht für das Spielbild.** Der Renderer zeichnet nur
   geänderte Felder; mit zwei Puffern müsste jede Änderung doppelt oder das
   ganze Bild (~39 ms) neu gezeichnet werden. Bildschirmwechsel bekommen
   stattdessen einfache Wipes im einfachen Puffer.
5. **Palette/Copper:** verworfen. In MODE 8 hat `VDU 19` keine Wirkung;
   Copper-Effekte setzen Palettenmodi (≤ 16 Farben) voraus, und ein
   Moduswechsel kommt für das Spiel nicht in Frage.

## Offen

- **Hardware:** VDP-Version des Geräts (QUIRK H2). `loc --vdptest` auf der
  SD-Karte laufen lassen und `loc.log` vergleichen. Fällt eine Funktion
  aus, greift ihr Fallback.
- ADSR-Sustain-Bereich (0–255 oder Prozent) ist nur per Gehör prüfbar.
