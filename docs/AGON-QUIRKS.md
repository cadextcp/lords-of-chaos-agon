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
| V6 | Viele kleine VDU-Aufrufe sind langsam. Deshalb sammelt der Renderer Bytes in einen Puffer und gibt sie mit `mos_puts()` aus. Buffered Commands (`VDU 23,0,&A0`) prüft Spike M1. | ❓ (M1) |

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
| T3 | `int` ist 24 Bit, `long` 32 Bit. Im Core nur `stdint`-Typen verwenden. | ✅ |
| T4 | Assembler-Funktionen: Das erste Argument liegt bei `(iy+3)` nach `ld iy,0 / add iy,sp`, weil die Rücksprungadresse 3 Byte groß ist. Symbole werden mit `_` exportiert. | ✅ (M0) |

## Hardware (Zielgerät)

| # | Thema | Status |
|---|---|---|
| H1 | Tastatur **Cherry G84-4100**, deutsches Layout, ohne Ziffernblock: Pfeil-Akkorde, Sondertasten und Auto-Repeat prüft Spike M1 (GDD §5.2). | ❓ (M1) |
| H2 | MOS- bzw. VDP-Version auf dem echten Agon Light: noch festzustellen und mit dem Emulator-Pin abgleichen. | ❓ |
