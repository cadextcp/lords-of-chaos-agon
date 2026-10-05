# Agon-Besonderheiten (Quirks)

Gesammeltes Plattformwissen. Teile stammen aus dem ersten Versuch (BBC BASIC, `LordsOfChaos/DEBUG.md`), sind hier auf C bzw. agondev übertragen und als verifiziert (✅) oder offen (❓) markiert.

## VDP und Grafik

| # | Quirk | Status |
|---|---|---|
| V1 | **MODE 2 ist 640×480 monochrom**, nicht 320×200×64 (Legacy-Doku). Für das Spiel gilt **MODE 8: 320×240, 64 Farben, 40×30 Textzellen à 8×8**. | ✅ (Versuch 1, Console8-VDP) |
| V2 | Die ersten 16 Farben der 64er-Palette entsprechen den klassischen 16 Farben (0 Schwarz … 7 Weiß, 8–15 hell). | ✅ (M0-Screenshot: Grün 2 bzw. 10 dunkel bzw. hell) |
| V3 | `VDU 23,c,b0..b7` definiert Zeichen `c` (8×8) um. agondev: `vdp_redefine_character()`. Wir nutzen Codes ab 128 für eigene Glyphen. | ✅ (M0) |
| V4 | Wird in die letzte Zelle (39,29) geschrieben, scrollt der Bildschirm. Der Renderer überspringt diese Zelle. | ✅ |
| V5 | Bitmap-API: `VDU 23,27,2` **erzeugt** eine einfarbige Bitmap, gezeichnet wird mit `23,27,3`. Alpha ist binär. | ✅ (Versuch 1) |
| V7 | **RGBA2222-Bitmaps** (Format 1): Bit 0–1 R, 2–3 G, 4–5 B, 6–7 A. Alpha 0 heißt transparent beim Zeichnen; Ebenen funktionieren damit. | ✅ (M1, `tools/build_tiles.py`) |
| V8 | `vdp_set_pixel_coordinates()`: Ursprung oben links, y nach unten. Gilt für `draw_bitmap` und Rechtecke. | ✅ (M1) |
| V9 | Kachel-Draw = `select_bitmap(buffer)` (5 Byte) + `draw_bitmap(x, y)` (7 Byte). 81 Felder mit etwa 3,2 Ebenen ≈ 38 ms Übertragung (ADR 0006). | ✅ (M1) |
| V10 | **Sprites aus Buffer-Bitmaps:** `vdp_adv_add_sprite_bitmap(bufferId)` (16-Bit-ID) fügt Frames hinzu. Danach `activate_sprites(n)`; jede Änderung braucht `vdp_refresh_sprites()`. Nach dem Zeichnen von Bitmaps unter dem Sprite ebenfalls refreshen. | ✅ (M1 #4) |
| V11 | 8 Sprites mit je 2 Frames, 150 Bewegungen inkl. `vdp_refresh_sprites()`: 3 ms pro Bild — Sprite-Animation ist praktisch gratis. | ✅ (vdptest S1) |
| V6 | Viele kleine VDU-Aufrufe sind langsam. Deshalb sammelt der Renderer Bytes in einen Puffer und gibt sie mit `mos_puts()` aus. Buffered Commands (`VDU 23,0,&A0`) prüft Spike M1. | ❓ (M1) |

## Audio (M5c)

| # | Quirk | Status |
|---|---|---|
| A1 | **Kanäle 0–3 aktiv, weitere per `vdp_audio_enable_channel(n)`** (4, 5 getestet). Noten via `VDU 23,0,&85,channel,0,volume,frequency;duration;` (`vdp_audio_play_note`). **Der VDP queued nicht:** Eine Note auf einem belegten Kanal wird verworfen (Antwort `audioSuccess` = 0). Folgen deshalb selbst takten. Die Antwort steht nach `vdp_pflag_audio` in `getsysvar_audioSuccess()`. | ✅ (vdptest A1/A2/A5, 2026-10-04) |
| A2 | Wellenformen pro Kanal: `vdp_audio_set_waveform` mit 0=Square, 1=Triangle, 2=Sawtooth, 3=Sine, 4=Noise, 5=VIC-Noise (Konstanten in vdp.h). Hüllkurven: `vdp_audio_volume_envelope_ADSR(ch, attack-ms, decay-ms, sustain-%, release-ms)`; danach wieder `disable`, sonst wirkt sie für die nächste Note weiter. | ✅ (API) |
| A3 | Audio-Befehle enthalten 0x00-Bytes (Frequenz/Dauer u16): nie über `printf` senden, immer die agondev-Wrapper (die MOS-puts mit Längenangabe nutzen). | ✅ (wie alle VDU-23-Befehle) |
| A4 | Der CLI-Emulator hat kein Audio (wie E1 kein VDP): Klang nur im GUI-Emulator/auf Hardware prüfbar. | ✅ |
| A5 | Musik auf eigenen Kanälen neben Effekten. Jede Note einzeln senden, wenn die vorige endet; Takt über `getsysvar_time()` in **Zentisekunden** (Notenlängen in ms also durch 10 teilen; bis 2026-10-04 lief die Titelmusik deshalb 10× zu langsam). Vorzeichenbehaftet vergleichen wegen Überlauf. | ✅ |
| A6 | **Samples:** 8-Bit-signed-PCM (16 kHz) in einen Buffer schreiben, `vdp_adv_consolidate`, `vdp_audio_create_sample_from_buffer(ch, id, 0)`, `vdp_audio_set_sample(ch, id)`, dann `play_note`. Upload 8000 Byte ≈ 12 cs. Erzeugen auf dem eZ80 ist zu langsam (290 cs für 8000 Byte) — Samples auf dem PC bauen und von der SD streamen. | ✅ (vdptest A3/A4) |
| A7 | **Notenstart mit Verzögerung:** Der VDP beginnt eine Note bis zu ~150 ms nach dem Senden (Audio-Puffer, GUI-Emulator). Wer eine Folgenote zur berechneten Endzeit schickt, wird teils abgewiesen. Abhilfe: Kanäle abwechseln (Musik) oder vor jeder Note `vdp_audio_reset_channel` (Effekte) – ein Reset gibt den Kanal sofort frei, auch mitten im Sample. Samples halten die Notenlänge ein; stimmbare Samples (Format 8) gehen mindestens bis 4× Grundton. | ✅ (vdptest A6–A11, Musik-Messung <1 % Ablehnung) |
| A8 | **Nach dem Booten sind nur die Kanäle 0–2 aktiv** (nicht 0–3). Befehle mit Parametern (z. B. ADSR) an einen inaktiven Kanal werden nicht verarbeitet – ihre Bytes erscheinen **als Text auf dem Bildschirm**. Vor jeder Nutzung von Kanal ≥ 3 `vdp_audio_enable_channel(n)`. Die alte Titelmusik nutzte Kanal 3 ungeschaltet (Zeichenreste, meist vom Titelbild verdeckt). | ✅ (vdptest 6) |
| A9 | **agondev-Konstante falsch:** `VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE` ist 8 – das ist aber das Bit „Abtastrate folgt“. Der VDP erwartet dann zwei weitere Bytes, verschluckt den nächsten Befehl und gibt den Rest als Text aus. **Stimmbar ist Bit 4 = 16.** | ✅ (vdptest 6) |

## VDP-Speicher und Streaming (M5d)

| # | Quirk | Status |
|---|---|---|
| S1 | **Gestreamte Bitmaps:** wiederholte `vdp_adv_write_block_data(bufferId, n, data)`-Aufrufe *hängen je einen Block an*. So lässt sich ein 320×240-RGBA2222-Bild (76 800 Byte) in 576-Byte-Häppchen durch einen kleinen Staging-Puffer laden. **Danach `vdp_adv_consolidate(bufferId)`** – ohne das bleibt die Bitmap aus mehreren Blöcken unsichtbar (der Titel war bis 2026-10-04 deshalb schwarz). Dann `select_bitmap` + `bitmap_from_buffer(320,240,1)` + `draw_bitmap(0,0)`. | ✅ (GUI-Emulator, Titelbild) |
| S2 | VDP-RAM-Budget: Kachelbank (348 Kacheln ≤576 B, ~185 KB, + Reit-Tiere, Puffer ab 0x2000) + Titel (Puffer 0x4000, 75 KB) laufen im Emulator zusammen; **auf Hardware nachzumessen** (Kacheln + Bitmaps + Titel). Kein `delete_bitmap` in der API — das Titel-Bitmap bleibt für den Programmlauf belegt. | ❓ (Hardware offen) |
| S3 | **Eigene Schriften:** Font-Bitmap (1 Byte pro Zeile bei 8 px Breite, 256 Zeichen hintereinander) in einen Buffer, `vdp_font_create(id, w, h, ascent, 0)`, `vdp_font_select(id, 0)`; zurück mit `vdp_font_select(0xFFFF, 0)`. `vdp_font_copy(id)` legt den Systemfont als Vorlage ab. 8×16 getestet. Mit `vdp_write_at_graphics_cursor()` (VDU 5) zeichnet die Schrift transparent am Grafikcursor (Schatten, Text über Bildern); danach `vdp_write_at_text_cursor()` und Systemfont wählen. | ✅ (vdptest F1/F2, Überschriften) |
| S4 | **Kein Paletten-Trick in MODE 8:** `VDU 19` ändert dort nichts (64-Farben-Modi sind nicht palettiert). Copper nur in Modi mit ≤16 Farben. | ✅ (vdptest P1) |
| S5 | **Doppelpuffer** MODE 136 = 8 + 128 läuft (320×240, 64 Farben), `vdp_swap()` wartet auf VSYNC; Buffer-Bitmaps überleben den Moduswechsel. Ein Vollbild + Swap ≈ 39 ms. Für das Spielbild ungeeignet (ADR 0012). | ✅ (vdptest D1/D2) |
| S6 | **eZ80-RAM ist knapp:** zusätzliche 12 KB statische Puffer sprengten `USERRAM` (5 KB zu viel); mit ~10 KB mehr Code blieben nur ~6 KB Stack und der eZ80-Selftest scheiterte. Große Daten nur durch kleine Puffer streamen; Werkzeuge als eigene Programme (`spikes/`). **Nachtrag 2026-10-05:** Heap und Stack teilen sich nur das Stück zwischen Ende von `.bss` und `0xB0000` (damals ~9,5 KB). Ein paar KB mehr Code ließen den Emulator-Selftest in CI an „water animates like reference“ scheitern — die Kamera liest dort das Ende von `scache`, das ganz oben im `.bss` liegt, während es lokal noch durchging. Abhilfe: das Menü borgt die `World` des Selftests statt eine eigene zu halten (−6 KB). **Nachtrag 2026-10-06:** der statische Sicht-Cache (`scache`) speichert nur noch `StaticField` (≤ 8 Ebenen, 20 B statt 34 B je Feld): −18 KB, danach ~30 KB Reserve (die Übergangs-Kacheln hatten sie auf ~11 KB gedrückt, der Selftest scheiterte). Bei neuem Fehlverhalten, das nur von der Codegröße abhängt: zuerst `bin/loc.map` auf `.bss`-Ende prüfen. | ✅ |

## Emulator

| # | Quirk | Status |
|---|---|---|
| E1 | `agon-cli-emulator` hat keinen VDP („Tom's Fake VDP“). Textausgabe geht nach stdout, Grafik wird ignoriert. | ✅ |
| E2 | **Der CLI-Emulator 1.2.5 führt `autoexec.txt` aus.** Die alte AgonPipeline-Notiz (ältere Version) behauptete das Gegenteil. `test.py` schreibt deshalb ein eigenes `autoexec.txt` mit `loc --selftest`. Achtung: Ein liegengebliebenes `autoexec.txt` aus `run.py` würde sonst im Headless-Lauf das Spiel starten. | ✅ (M0) |
| E7 | Die CLI-Ausgabe enthält rohe VDU-Bytes (Fake VDP). Tools dekodieren tolerant (`utf-8`, `errors=replace`) und filtern sie vor der Ausgabe auf Windows-Konsolen (cp1252). | ✅ (M0) |
| E3 | Ein Schreibzugriff auf **I/O-Port 0** beendet den Emulator mit diesem Exit-Code. Vorher muss der UART leer sein (LSR bit 6, Port `$C5`), sonst fehlt Ausgabe. | ✅ (M0, `src/agon/emu.asm`) |
| E4 | Die GUI startet standardmäßig mit „platform“-MOS 3.x, die CLI immer mit Console8 MOS 2.3.3. Deshalb pinnen wir die GUI auf `--firmware console8`. | ✅ |
| E5 | Normale SendKeys erreichen das SDL-Fenster nicht. Nur Scancodes per SendInput funktionieren (`pydirectinput`). | ✅ (AgonPipeline) |
| E6 | Das Linux-CLI-Binary 1.2.5 („debian13“) läuft auch unter Ubuntu 24.04 (WSL, CI). | ✅ |

## Toolchain (agondev)

| # | Quirk | Status |
|---|---|---|
| T1 | agondev gibt es nur für Linux und macOS. Unter Windows läuft es in WSL; der Pfad wird als `/mnt/c/...` übergeben. In Git-Bash ist `MSYS_NO_PATHCONV=1` nötig, wenn man `wsl.exe` direkt aufruft. | ✅ |
| T2 | Das agondev-Makefile kompiliert **alles unter `src/`**. Host-Code liegt daher in `host/` außerhalb von `src/`. | ✅ |
| T3 | `int` ist 24 Bit, `long` 32 Bit. Im Core nur `stdint`-Typen verwenden. Auch Literale: `1u << i` ist nur 24 Bit breit und ab `i = 24` undefiniert, selbst wenn das Ziel `uint32_t` ist. Dann `(uint32_t)1 << i` schreiben. Der Host-Test findet so etwas nicht. | ✅ |
| T5 | **Das agondev-Makefile verfolgt keine Header-Abhängigkeiten.** Ein geänderter Header (z. B. das generierte `gen/tiles.h`) lässt alte `.o` stehen. `tools/build.py` baut deshalb standardmäßig clean (`--incremental` zum Überspringen). | ✅ (M1) |
| T6 | `getsysvar_time()` zählt Zentisekunden in 2er-Schritten (VBLANK, 50 Hz). Für Benchmarks über mehrere Frames mitteln. | ✅ (M1) |
| T7 | **Compiler-Bug (agondev v0.22):** Eine Kette `x == A \|\| x == B \|\| …` über Enum-Werte kann zu einem Bit-Test mit ungewöhnlicher Breite (`i14`) optimiert werden. Das Backend bricht dann mit „unable to legalize instruction“ ab. Abhilfe: Lookup-Tabelle (`FEATURE_SIGHT` in `world.c`). | ✅ (M2a) |
| T8 | **Verdacht, nicht isoliert:** Ein Zeiger, der per `?:` zwischen zwei Array-Elementen gewählt und danach beschrieben wird (`uint8_t *slot = cond ? &a[y][x] : &b[y][x]; *slot = …`), ließ `view.c` (`build_overlay`) auf dem eZ80 etwas anderes berechnen als auf dem Host (Selftest „water animates like reference" scheiterte nur im Emulator). Mit einem schlichten `if/else` verschwand der Fehler. Bis zur Klärung: keine bedingten Zeiger auf Array-Elemente. | ⚠️ (C3) |
| T4 | Assembler-Funktionen: Das erste Argument liegt bei `(iy+3)` nach `ld iy,0 / add iy,sp`, weil die Rücksprungadresse 3 Byte groß ist. Symbole werden mit `_` exportiert. | ✅ (M0) |

## Tastatur (kbuf, `agon/keyboard.h`)

| # | Quirk | Status |
|---|---|---|
| K1 | **ASCII in Key-up-Events ist veraltet**: Es wiederholt das ASCII der zuletzt gedrückten Taste. Tasten, deren Loslassen zählt (Bewegung), nur per **VKey** auswerten. | ✅ (Emulator, ADR 0007) |
| K2 | **Kein Auto-Repeat über `kbuf`**: Eine gehaltene Taste liefert genau ein Down-Event. Die Wiederholung macht das Spiel selbst (`chord.c`). | ✅ (Emulator) |
| K3 | VKeys: ↑ 96, ↓ 98, ← 9A, → 9C, Pos1 86, Ende 88, Bild↑ 93, Bild↓ 95, ESC 7D, a–z = 16 + Index (nach Layout). | ✅ (Emulator) |
| K5 | Die Hauptschleife muss die `kbuf`-Warteschlange **vollständig leeren**, bevor sie Tastenwiederholung oder Zeitlogik auswertet. Sonst wirken langsame Frames (Scrollen) wie gehaltene Tasten. | ✅ (M2a) |
| K6 | **Warteschleifen dürfen kein Loslassen verschlucken.** Wer während einer Animation die Warteschlange leert (K5), muss Key-up-Ereignisse aufheben und später an `chord_key(..., false, ...)` geben (`fx_take_release`). Sonst bleibt ein kurz getippter Pfeil „gehalten“ und die Einheit läuft von selbst bis an die Wand (Fund 2026-10-04 mit den gleitenden Schritten). | ✅ |
| K4 | `SET KEYBOARD 2` ist das deutsche Layout (y/z vertauscht). `tools/run.py` setzt es standardmäßig (`--keyboard`). | ✅ |

## Hardware (Zielgerät)

| # | Thema | Status |
|---|---|---|
| H1 | Tastatur **Cherry G84-4100**, deutsches Layout, ohne Ziffernblock: Pfeil-Akkorde, Sondertasten und Auto-Repeat prüft Spike M1 (GDD §5.2). | ❓ (M1) |
| H3 | Lange Dateinamen (`maps/many_coloured_land.map`, `scenarios/ragarils_domain.scn`) funktionieren auch auf der echten FAT-SD-Karte (2026-10-03: Upload und Umbenennen auf die Karte, `loc --bench` lädt die Karte von dort). | ✅ (Hardware) |
| H2 | **Der echte Agon Light 2 läuft auf Platform MOS 3 („Arthur") und VDP 2.16.0** (Firmware-Update 2026-10-05 per USB, siehe unten). Der Emulator-Pin läuft dagegen mit Console8 MOS 2.3.3 — die beiden Zweige bleiben also bewusst auseinander, und was auf dem Emulator läuft, ist für MOS 3 nicht automatisch belegt (MOS 3 hat eigene Befehle: `Do`, `Obey`, `IfThere`, `SetMacro`, `PrintF`, Systemvariablen über `show`). **Die Versionsnummer lässt sich über USB nicht auslesen:** MOS 3 hat keinen `version`-Befehl, `credits` nennt nur FabGL/FatFS/umm_malloc, und das Boot-Banner geht auf den Bildschirm, bevor `autoexec.txt` den Konsolenmodus einschaltet. Nur am Monitor ablesbar. | ✅ (Hardware) |
| H4 | **`loc --selftest` läuft auf dem echten eZ80 durch** (2026-10-03, Stand `037521f`): `=== TEST PASS ===`. Auf der Hardware erscheint nur das Gesamtergebnis (der Selftest läuft nicht ausführlich). `emu_exit` (`out (0), a`) ist auf dem echten Agon harmlos; danach steht der MOS-Prompt wieder da. | ✅ (Hardware) |
| H5 | ~~**Das Spiel lässt sich über die USB-Konsole nicht beenden.**~~ **Behoben 2026-10-05:** `input_poll()` (src/agon/input.c) setzt `VK_ESC`, wenn ein Zeichen ohne VKey mit ASCII 27 ankommt — damit wirkt Esc von der USB-Konsole wie von der Tastatur, und ein Lauf lässt sich komplett vom PC fahren (auch beenden, was erst `loc.log` schreibt). Ursprünglicher Befund: Dort getippte Zeichen kommen als Tastendrücke ohne VKey an; `VK_ESC` (7D) gibt es nur von der echten Tastatur am Agon. `Esc` im Spiel, dann schreibt `log_close()` auch `loc.log`; bis dahin ist die Datei leer. Zum Auslesen von `loc.log` also am Gerät Esc drücken, danach `TYPE /loc/loc.log` über USB. | ✅ (Hardware) |

**Firmware-Update über USB** (2026-10-05, MOS 3.0.2 + VDP 2.16.0): Die beiden Dateien liegen
auf der Karte (`/loc/mos302.bin`, `/loc/vdp2160.bin`), geflasht wird mit `/mos/flash.bin` v1.9 am
MOS-Prompt:

```
flash vdp /loc/vdp2160.bin mos /loc/mos302.bin
```

Das Werkzeug prüft erst die CRC beider Dateien und fragt dann `Flash firmware (y/n)?`. Über die
USB-Konsole (`scripts/agonctl.py` im Lumagon-Repo) reicht ein `y` ohne CR. Zu beachten:

- **Der VDP startet mitten im Vorgang neu** und verliert dabei den Konsolenmodus. Nach dem
  VDP-Teil (`checksum ok!`, `Rebooting in 3...`) kommt über USB erst wieder etwas an, wenn MOS
  geflasht ist, neu gestartet hat und `autoexec.txt` den Konsolenmodus erneut einschaltet. Der
  MOS-Teil selbst ist über USB also nicht zu sehen — dass der Prompt samt Konsolenmodus
  zurückkommt, ist der Beleg, dass er gelaufen ist.
- **DTR und RTS müssen aus bleiben**, sonst setzt der USB-Seriell-Wandler den ESP32 zurück
  (`agonctl.py` und `agonmon.py` machen das schon). Während des Flashens nichts tippen.
- Der VDP-Teil dauert nur etwa 17 s (1.077.840 Bytes, 520 kbit/s) — die Dateien vorher per
  `agonload.py` hochzuladen ist der langsame Schritt.

**Messwerte nach dem Plattform-Audit** (2026-10-05, Stand `6506f98` + Bench-Fix, Szenario 1,
9×9-Fenster). Selftest auf dem Gerät: `=== TEST PASS ===`.

| Messung | vor dem Audit | nach dem Audit | Faktor |
|---|---|---|---|
| Cursor blinken | 0 ms | 0 ms | — |
| Fenster komponieren (81 Felder) | 90 ms | **38 ms** | 2,4× |
| **Sichtberechnung** | **298 ms** | **12 ms** | **25×** |
| 20 Flächen-Ticks | 40 ms | 40 ms | — |
| **KI-Phase** | **160 ms** | **20 ms** | **8×** |
| Voller Redraw (81 Felder) | 152 ms je Frame | **100 ms je Frame** | 1,5× |
| Nur Kerzenanimation | 6 ms | 6 ms | — |

**Einordnung:** Ein eigener Schritt kostete vorher Sicht (298) + Komposition (90) + Redraw,
also grob 0,4–0,5 s. Jetzt sind es 12 + 38 ms plus zwei bis vier neu gezeichnete Felder —
unter 60 ms, praktisch sofort. Die Sicht ist von der teuersten Einzelposition zur
billigsten geworden (Shadowcasting, D39); die KI-Phase war zu rund 7/8 der immer wieder neu
gebaute Blockier-Bitmap (Audit B1). Der volle Redraw bleibt der dickste Posten und fällt
nur beim Kameraschwenk an — dort liegt der nächste Hebel (B8, Viewport-Scroll).

**Messwerte `loc --bench` auf dem echten Agon Light 2** (2026-10-03, Stand `037521f`, Szenario 1, 9×9-Fenster):

| Messung | Zeit |
|---|---|
| Karte laden (`many_coloured_land`) | 100 ms |
| Cursor blinken | 0 ms |
| Fenster komponieren (81 Felder) | 90 ms |
| Sichtberechnung | 298 ms |
| 20 Flächen-Ticks, 4 Flächen Stufe 4 | 40 ms gesamt (etwa 2 ms je Tick, Ziel M4d: Rundenende < 500 ms) |
| KI-Phase | 160 ms |
| Voller Redraw (81 Felder) | 152 ms je Frame |
| Nur Kerzenanimation | 6 ms je Frame |

Einordnung: Die Sichtberechnung ist der größte Posten; ein Zug mit Sicht und KI-Phase braucht etwa 0,5 bis 0,6 s. Die Flächen im Bench sind kurzlebig (sie sterben nach wenigen Runden), die 40 ms sind also kein Worst Case. Bei normaler Bewegung wird nur neu gezeichnet, was sich geändert hat (Dirty-Tracking).
