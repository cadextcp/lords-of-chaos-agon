# Plan: Endbildschirm, Hilfe/Tutorial, Lexikon, Animationen, Sound, Titelbild+Musik

## Context
Nach M4a–M4h ist das Spiel regelseitig komplett, aber die Präsentation ist dünn: das Spielende ist nur eine Meldungszeile (`main.c:1589`, danach nur Esc), es gibt keine Hilfe außer der F1-Liste (`draw_help`, `main.c:906`), kein Lexikon, kein Titelbild, Kämpfe sind stumm/statisch (nur 7 Einzelnoten in `sound.c`, `SND_MISS` nie verdrahtet, KI-Aktionen unsichtbar). Ziel: die acht Wünsche des Nutzers in **4 PRs** (Entscheidung des Nutzers), jeder mit grünem `uv run tools/test.py`, CHANGELOG-Eintrag, Feature-Branch → PR → CI → Merge.

Randbedingungen aus der Erkundung:
- `src/core` plattformfrei (nur Ereignisse/Zustände; Darstellung, Ton, Timing in `src/agon`). Neue Core-Logik bekommt Checks in `src/core/selftest.c` (Host + eZ80); darf RNG/`view_hash`/`DEMO_HASH` nicht verändern.
- `loc.bin` = 255 KB, über der 250-KB-Marke (GDD §16). **Texte, Titelbild, Lexikon-Beschreibungen, Tutorialkarte kommen von der SD (`/loc/...`)**, nicht ins rodata. Dazu ADR 0011 (Daten von SD, Streaming-Loader).
- Tile-Loader (`render.c:81-128`) kann nur ≤576-Byte-Einträge; Titelbild braucht eigenen Streaming-Loader (`vdp_adv_write_block_data` in Blöcken). VDP-Speicher ist ungemessen → Titelbild-Größe auf Hardware prüfen, danach Buffer freigeben.
- Kein Menü-Rücksprung heute: `main()` (`main.c:1125–1664`) beendet nach Spielende; Neuinitialisierung steht doppelt (`main.c:1162–1203`, `1221–1250`).

## PR 1 – Endbildschirm + äußere Spielschleife
- **Core** `src/core/game.[ch]`: `GameStats` (Runden, Kills, Schätze, VP je Besitzer) und `game_outcome()` → `OUT_WIN` (Spieler-Zauberer entkommen), `OUT_LOSE` (tot/nicht entkommen), `OUT_RUNNING`. Selftest-Checks.
- **Frontend**: neue `src/agon/screens.[ch]` mit `screen_end(outcome, stats)` (Vollbild, eigene `render_screen_clear()` über `vdp_clear_screen`, danach `view_invalidate()+frame()`); Gratulation bei Sieg (Name, VP, Runden, Schätze, Kills), „Game Over“ bei Niederlage. Enter → zurück ins Hauptmenü, Esc → beenden.
- Ende sofort auslösen: beim Portal-Eintritt des Spielers (`main.c:426–433`) und beim Tod des letzten Spieler-Zauberers, nicht erst bei Shift+E.
- Spielzustand-Init in eine Funktion `game_start(map, scn)` ziehen (entfernt die Doppelung), `main()` bekommt Schleife Menü → Spiel → Endbildschirm → Menü.
- `wizard_campaign_result` (`wizard.h`) hier anbinden (VP→XP, Level), `wizards_save()`.

## PR 2 – Hilfe, Tutorial, Lexikon
- **Hilfe/Tutorial-Texte**: `data/help/*.txt` → `tools/gen_help.py` → `build/help/*.hlp` (SD, Seiten mit Titel, 27/38 Spalten, Umlaute über `umfont`). Neuer Viewer `screen_help(page)` in `screens.c` (←/→ Seiten, Esc zurück). Aufrufbar aus Hauptmenü („Hilfe“, „Lexikon“, „Tutorial“) und im Spiel per F1 (ersetzt statische `draw_help`, die Tastenliste bleibt Seite 1).
- **Geführtes Tutorial-Szenario**: `data/maps/tutorial.txt` + `data/scenarios/tutorial.txt` (kleine 36×36 oder kleinere Karte), Schrittfolge: Bewegen → Einheit wechseln → Truhe/Schlüssel → Kampf → Zauber → Portal. Schritt-Engine im Core `src/core/tutorial.[ch]` (Liste von Zielen mit prüfbaren Bedingungen: „Einheit auf Feld X“, „Gegner getötet“, „Zauber gewirkt“, „Portal betreten“), Hinweiszeile im Meldungsbereich; Texte aus `data/help/tutorial.txt`. Selftest: Schrittbedingungen schalten in Reihenfolge weiter.
- **Lexikon**: Core `src/core/lexicon.[ch]` mit Bitmasken `seen_creature` (25 Bits) / `seen_object` (45 Bits), `lexicon_see_creature/object`, Serialisierung. Datei `lexicon.dat` (Muster `wizards.dat`, `mapfile.c:47–85`: Magic, Version, Größe, Validierung). Hooks: Kreaturen in `update_sight()` für sichtbare fremde Einheiten (`main.c:198`), Objekte bei Aufheben (`items_pick_up`) und Sichtbarwerden auf dem Feld. Anzeige: Listenbildschirm (nur entdeckte Einträge, „???“ sonst) + Detailseite mit Porträt (`draw_tile(CREATURE_TILE+owner)` öffentlich machen), Werten aus `CREATURES[]/OBJECTS[]/WEAPONS[]` und Kurztext aus `data/help/lexicon_de.txt` (SD). Selftest: Markieren, Serialisieren, Validierung.
- Erreichbar aus Hauptmenü und (Taste `i`) im Spiel.

## PR 3 – Ereignis-Ring, Kampf-/Todesanimation, bessere Sounds
- **Core** neu `src/core/events.[ch]`: kleiner Ring `{type,x,y,kind,owner,a,b}` mit `EV_SWING, EV_HIT, EV_WOUND, EV_MISS, EV_DEATH, EV_SPELL, EV_SMASH`. Emit-Punkte (laut Erkundung): `combat_damage` (`combat.c:38`), `world_kill_unit` vor `world_remove_unit` (`world.c:344`, dort sind x/y/kind noch gültig), Verfehlt-Zweige in `combat_melee`/`combat_free_swing` (`combat.c:79,125`), Rückschlag, Bleed-Tod (`world.c:476`), Wurf/Bogen/Bolt, `combat_terrain`. Kein Einfluss auf RNG/Weltzustand; Selftest prüft Ereignisfolge und unveränderte Hashes.
- **Frontend** neu `src/agon/fx.[ch]`: `fx_play(events)` spielt den Ring ab – nicht blockierend gegenüber der Eingabe (Timer über `getsysvar_time()` mit Wrap-Behandlung, `kbuf` leeren, K5). Effekte: Schlag = Angreifer-Ruck + Slash-Kachel auf Zielfeld, Treffer = rotes Blitz-Overlay + Zahl, Verfehlt = graues „Wisch“-Overlay, Tod = 3–4 Frames (Aufblitzen → Kreatur verblasst → Staub/Kreuz). Neue Kacheln in `tools/art/make_tiles.py` (slash, hit, miss, death_0..3) → tiles.bin; `view_mark_dirty(vx,vy)` neu in `view.[ch]` zum sauberen Neuzeichnen einzelner Felder.
- **KI sichtbar machen**: optionaler Callback in `Turns` (`turn.c:198`) nach jedem `t->ai(...)`, Frontend leert den Ring und animiert; Fallback: einmaliges Abspielen nach `turn_end_phase`.
- **Sound** `src/agon/sound.c` ausbauen: Wellenform + ADSR-Hüllkurve (`vdp_audio_set_waveform`, `vdp_audio_volume_envelope_ADSR`, Frequenzhüllkurve) und mehrere Kanäle; neue Effekte pro Ereignis (Schwerthieb, Treffer dumpf, Verfehlt, Tod absteigend, Zauberarten, Bogen/Wurf, Tür, Truhe, Portal-Arpeggio, Rundenwechsel). `SND_MISS` verdrahten, stumme Pfade (Rückschlag, Bolt, Wurf, KI) anbinden.

## PR 4 – Titelbild + Titelmusik
- **Titelbild** 320×240 (oder 160×120 hochskaliert, falls VDP-RAM knapp), eigene Pixelart im Stil des Originals (3/4-Ansicht, Agon-64-Palette): Titel „LORDS OF CHAOS“, Zauberer, Turm/Landschaft. Erzeugt von neuem `tools/art/make_title.py` (PNG in `assets/title/`, Palette geprüft wie `build_tiles.py`), `tools/build_title.py` → `build/title.bin` (RGBA2222, SD). Streaming-Loader in `render.c` (Blöcke, danach `vdp_adv_clear_buffer`), `screen_title()` zeigt es vor dem Menü; Taste → Menü. Inhalt/Motiv des Bildes stimmt der Nutzer vorab kurz ab (kein Referenzbild im Repo).
- **Titelmusik**: Sequenzer in `src/agon/music.[ch]` (Notenliste, 2–3 Kanäle, nicht blockierend, aus den Menü-/Titelschleifen gepollt, bricht bei Tastendruck ab). Melodie als eigene Komposition im Stil des Originals (D7: nichts kopieren), Daten als Tabelle in `data/music/title.txt` → SD.
- Titelbild-/Musikgröße auf Hardware verifizieren (Quirk eintragen in `docs/AGON-QUIRKS.md`).

## Querschnitt
- `docs/AGON-QUIRKS.md`: Audio-Befehle/Kanäle, Streaming großer Bitmaps, VDP-Speicher. ADR 0011 (Daten von SD wegen 250-KB-Marke). GDD §11.4 (Sound/Musik) und neues Kapitel Hilfe/Lexikon ergänzen; HANDOVER/CHANGELOG pflegen.
- SD-Staging (`tools/setup.py`/`build.py`): `help/`, `title.bin`, `music/`, `lexicon.dat` mitnehmen.

## Verifikation
- Je PR: `uv run tools/test.py` (Host + eZ80), neue Selftest-Gruppen (Outcome/Stats, Tutorial-Schritte, Lexikon, Ereignisfolge ohne Hash-Änderung).
- GUI: `uv run tools/run.py --time 12 --screenshot` für Menü/Titel/Endbildschirm/Lexikon; Skripte per `--keys` (Sieg: Portal betreten; Niederlage: Zauberer sterben lassen).
- Animation/Sound sind im Emulator nicht prüfbar (CLI ohne VDP, Quirk E1) → Abnahme per SD-Paket auf Hardware (`loc-sd.zip` wie zuvor), Ergebnis im Handover vermerken.
- Hardware-Check `loc.bin`-Größe und VDP-Speicher nach PR 4.
