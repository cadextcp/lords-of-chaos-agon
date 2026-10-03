# ADR 0010 – Mega Drive als zweite Plattform (SGDK)

- **Status:** vorgeschlagen (2026-10-03). Wird nach dem Core-Spike (Schritt 1) mit Messwerten angenommen oder verworfen.
- **Kontext:**
  - Der Verein hat alle Retro-Konsolen. Das Spiel soll nicht am Agon hängen bleiben (GDD D23).
  - Der Core ist bereits plattformfrei (ADR 0003): C99, `stdint`-Typen, kein Float, Zufall nur über `rng.h`, keine Zeiger-Casts, keine dynamische Speicherverwaltung.
  - Verglichen wurden Mega Drive, Game Boy Advance, SNES, Dreamcast und die 8-Bit-Konsolen.

## Entscheidung

**Sega Mega Drive mit [SGDK](https://github.com/Stephane-D/SGDK)** (GCC für m68k, C).

| Kriterium | Mega Drive |
|---|---|
| RAM | 64 KB, reicht nach Schätzung (siehe unten) |
| Toolchain | SGDK: ausgereift, C, Windows-Binaries und Docker-Image fürs CI |
| Grafik | Kacheln 8×8: eine 24×24-Kachel sind 3×3 Zellen, Sprites bis 32×32 |
| Mehrspieler | 2 Pads, Team Player bzw. EA 4-Way für 4 (SGDK unterstützt beide); **Link-Kabel** über Controller-Port 2 im seriellen Modus (bis 4800 bps, wie Segas *Zero Tolerance*); optional **MegaWiFi** (API in SGDK) |
| Hardware im Verein | Konsolen vorhanden; ROMs laufen über Mega EverDrive |

Verworfen bzw. zurückgestellt:
- **GBA:** einfachster Port und Link-Kabel mit eigenem Bildschirm pro Spieler, aber Handheld statt Fernseher. Bleibt der natürliche Drittkandidat.
- **SNES:** Maus wäre attraktiv. C auf dem 65816 ist aber langsam, und die Toolchains sind unreifer.
- **Dreamcast:** zu viel Aufwand für den Nutzen.
- **NES, PC Engine, Master System:** 2–8 KB RAM, zu knapp.

## Architektur

```
              src/core (C99, plattformfrei) + neu: play.[ch]
                 |                 |                   |
          src/agon (agondev)   host/ (gcc)      md/ (SGDK, m68k-gcc)
          Tastatur → PlayCmd   Tests, Dumps     Pad → PlayCmd
          VDP-Bitmaps                           Plane B / Sprites / Window
```

1. **Ordner `md/` auf oberster Ebene.** Alles unter `src/` kompiliert agondev (CLAUDE.md). Das MD-Makefile bindet `src/core/*.c` und `md/*.c` ein.
2. **Spielablauf in den Core (`src/core/play.[ch]`).** Heute stecken Modi, Aktionsausführung und Meldungen in `src/agon/main.c` (~900 Zeilen).
   - Das Modul nimmt abstrakte Befehle (`PlayCmd`: Bewegen, nächste Einheit, Fertig, Zugende, Bestätigen, Abbrechen, Verben) und liefert UI-Modus, Cursor, Meldungen (Text plus Farb-ID) und Neuzeichnen-Flags.
   - Die Logik wird umgezogen, nicht neu geschrieben. Das Agon-Frontend übersetzt danach nur noch Tasten. Selftests spielen Befehlsfolgen ab.
   - Dieselben Befehle sind später die Nachrichten für Hotseat-Übergabe, Link-Kabel und Online.
3. **Daten im ROM statt auf SD.** Karten kommen aus dem schon generierten `src/core/gen/maps.c` (dieselben Bytes wie die `.map`-Dateien, ADR 0008). Kacheln liefert ein generiertes C-Array von `tools/build_tiles_md.py`. Speichern (M4i) geht später ins Batterie-SRAM.
4. **Darstellung** (GDD §11.5):
   - **Plane B:** Terrain-Komposit, von der CPU pro Dirty-Feld in 9 VRAM-Zellen gesetzt und per DMA übertragen. Das Fenster belegt 81 × 9 × 32 Byte ≈ 23 KB VRAM.
   - **Sprites:** Einheiten und Cursor.
   - **Plane A:** Overlays.
   - **Window:** Panel und Meldungen.
   - Das Komposit ersetzt die transparenten Bitmap-Ebenen des Agon (ADR 0006). `view.c` mit `FieldLayers` und Dirty-Bits bleibt die gemeinsame Quelle.
5. **Eingabe:** GDD §5.4, Pflicht ist das 3-Button-Pad. Die Wiederholung übernimmt die Zeiten aus ADR 0007.

## Schritt 1: Core-Spike (timeboxed, Ergebnis = Nachtrag in diesem ADR)

- **Toolchain:** SGDK-Release in `tools/setup.py` pinnen und `tools/build.py --md` anlegen.
- **Selftest-ROM:** `src/core/selftest.c` plus minimales `md/main.c`, das das Ergebnis anzeigt bzw. loggt.
  - **Emulator:** BlastEm. Prüfen, ob KDebug- bzw. Log-Ausgabe headless geht; sonst Bildschirm plus Screenshot.
  - `DEMO_HASH` und `HOUSE_VIEW_HASH` müssen bitgleich zu Host und eZ80 sein.
- **Big-Endian und Alignment:** Ein ungerader 16- oder 32-Bit-Zugriff löst auf dem 68000 einen Address Error aus. Der Selftest muss ohne Absturz durchlaufen.
- **libc:** Gibt es `snprintf` in SGDK? Falls nicht, braucht `play.c` einen eigenen kleinen Formatierer (betrifft Schritt 2).
- **Messen:**

| Größe | Schätzung | Grenze (Abbruch bzw. Umplanung) |
|---|---|---|
| Core-Zustand im RAM (`World`, Sight pro Spieler, `Turns`, `Game`, View-Cache) | ~10 KB (Karte 3 × 36 × 36 = 3,9 KB plus Pools) | > 48 KB |
| `view_update`, 81 Felder | – | > 100 ms |
| `sight_compute` (eZ80: 162 ms, ADR 0009) | ähnlich | > 500 ms |
| ein KI-Zug | – | > 3 s |

## Offene Fragen (werden in den jeweiligen Schritten entschieden)

1. **NTSC-Layout:** Bei 224 Zeilen bleibt eine Meldungszeile. Reicht das, oder wird die Karte auf NTSC 9×8? Das würde `VIEW_H` im Core parametrisieren. Entscheidung per Mockup.
2. **Besitzerfarben bei 4 × 15 Farben.** Möglichkeiten:
   - **(a)** Kreaturen mit wenigen gemeinsamen Farben plus 2 Schlüsselfarben × 5 Besitzer in einer Sprite-Palette
   - **(b)** volle Kreaturen-Palette und Besitzer als kleine Markierung in der UI-Palette
   - **(c)** Einheiten ins Komposit von Plane B; dann gilt die Paletten-Regel pro Zelle noch strenger

   Der Bericht von `build_tiles_md.py` entscheidet.
3. **Sprite-Grenzen:** 20 Sprites bzw. 320 Pixel pro Zeile. Eine Feldreihe kann 9 Boden- und 9 Luft-Einheiten plus Cursor zeigen, das sind 19 Sprites und 456 px. Bei mehr als 13 Einheiten in einer Reihe flackern bzw. fehlen Pixel. Das ist selten. Vorschlag: akzeptieren, im `MD-QUIRKS.md` dokumentieren.
4. **Hardware-Scrolling:** Feldweises Scrollen wie auf dem Agon ist der Start. Weiches Scrollen über die Scroll-Register ist eine spätere Politur.

## Folgen

- Der Core muss auf **drei** Compilern sauber bleiben: gcc auf dem Host, clang/agondev für den eZ80 und m68k-gcc. Der Selftest läuft auf allen dreien.
- Der Umbau zu `play.[ch]` kostet auf dem Agon einen eigenen PR **vor M4f**, weil Hauptmenü und Designer `main.c` sonst weiter vergrößern. Er lohnt sich aber auch ohne Mega Drive.
- Die Pixelart bleibt Agon-zuerst. Die Paletten-Regel des Mega Drive kann einzelne Kacheln zu Anpassungen zwingen. Das Tool meldet sie, statt still Farben zu verbiegen.
- CI bekommt einen dritten Build (ROM-Größe im Bericht, wie `loc.bin`).
