# Übergabe: Stand und nächste Schritte

> Stand: 2026-10-04 · **M0–M5 vollständig**, **Polish-Runde** in 5 PRs (#113 gemergt, #114–#117 gestapelt offen) · CI grün · `loc.bin` 282 KB
> Für die nächste Person bzw. den nächsten Agenten. Zuerst `CLAUDE.md` lesen (Regeln, Befehle), dann dieses Dokument.

---

## 1. Kurzstatus

| Milestone | Status |
|---|---|
| **M0 Fundament** | ✅ Toolchain, Tests, CI, Docs |
| **M1 Grafik und Eingabe** | ✅ Software-seitig fertig (#1, #4, #5, #6). Offen: #2 (optional), #3 und #7 (brauchen echte Hardware) |
| **M2 Core-Skelett** | ✅ #13–#18 |
| **M3 Classic spielbar** | ✅ #27–#33, #41 |
| **M4 Classic komplett (v1.0)** | ✅ 10 von 10 Teilen (#43–#52) |
| **M5 Präsentationsrunde** | ✅ #88–#92: Endbildschirm mit Menü-Rücksprung/Kampagne, Hilfeseiten (SD), geführtes Tutorial, Lexikon (persistent), Ereignis-Ring mit Kampf-/Todesanimation, 16 Sound-Effekte mit Wellenformen/ADSR, KI sichtbar, Titelbild (Streaming) + Titelmusik (3 Kanäle) |
| **Polish-Runde** | #113 ✅ Bildschirmreste/Titelbild-Loader · #114 VDP-Spike (ADR 0012) · #115 Audio (Samples, Musik, Jingles) · #116 Titelbild, Zierschrift, Menü-/Endbilder, Tränke · #117 Sprite-Effekte. Plan: `C:\Users\cadex\.claude\plans\schau-mal-das-spiel-soft-dongarra.md` (Nutzer-Entscheide dort) |
| M6+ Chaos | geplant, siehe `docs/ROADMAP.md` |

**Was heute läuft (Emulator, Stand Polish-Runde):**
- **Start:** Titelbild (Schlachtgetümmel) + Titelmusik (4 Stimmen, Samples), die Musik läuft im Menü weiter; Menü mit Titelbild-Hintergrund; Überschriften in eigener 8×16-Zierschrift.
- **Klang:** 16 eigene Samples (`tools/gen_sfx.py`, `/loc/sfx.bin`), Effekt-Sequenzer auf Kanal 0/4, Musik auf 1–3/5–9 (je zwei Kanäle pro Stimme), Jingles am Spielende; Setup schaltet Musik (M), Effekte (T), Gleiten (G). Vorhören: `uv run tools/audio_preview.py`.
- **Effekte:** Projektile/Zauber/Schadenszahlen als VDP-Sprites, gleitende Schritte.
- **Werkzeug:** `uv run tools/run.py --vdptest [n]` startet das separate Messprogramm `vdptest` (ADR 0012).
- **Spielende:** Endbildschirm (Sieg/Niederlage) mit Runden/Kills/Beute/VP, Kampagne verbucht XP/Level; Enter zurück ins Menü, Esc beendet.
- **Tutorial:** kleine Karte, 7 Schritte (Bewegen → Wechseln → Schlüssel → Truhe → Kampf → Zauber → Portal), Hinweiszeile unten; Runde-1-Sperre aufgehoben.
- **Lexikon (Taste `i`):** entdeckte Kreaturen/Objekte, Detailseite mit Porträt und Text; persistent in `/loc/lexicon.dat`.
- **Kampf sichtbar und hörbar:** Ereignis-Ring im Core (Schwung/Treffer/Wunde/Verfehlt/Tod/Zauber/Zerschmettern), Frontend spielt Overlay-Animationen und 16 Sounds (Wellenform + ADSR, Kanal 0); KI-Phasen werden animiert (`Turns.on_ai`).
- (M4-Bestand unverändert: Runden/AP, Kampf D16/D21/D26, Magie inkl. Tränke/Brauen, Objekte, KI, Hidden Map, Speichern, Kontextmenü/Big Map/Log.)

**Was heute läuft (Emulator) — M4-Bestand:**
- **Spielstart:** `loc` startet Szenario 1 „The Many Coloured Land“ (36×36, Wrap-around, Kartenformat v3 mit Portal). Die Zauberbücher kommen aus der Szenario-Datei (`data/scenarios/`). Eine ganze Partie gegen einen KI-Zauberer ist durchspielbar.
- **Rundenablauf:**
  - `Tab`/`Shift+Tab` wählt eine Einheit, `Leertaste` beendet sie, `Shift+E` beendet den Zug sofort; sind alle Einheiten fertig, auch `Leertaste`.
  - Runde 1 erlaubt nur Zaubern `[PM 7]`.
  - Hat der Mensch keine Einheiten mehr, spielt die KI bis zu 40 Runden zu Ende; danach folgt die Abrechnung.
- **Bewegung:**
  - Pfeile, Akkorde und Tastenwiederholung.
  - Fliegen mit `<` und `>`.
  - Bump öffnet Türen und Truhen, greift Gegner oder Terrain an; `x` startet den Look-Modus.
- **Kampf (D16, D21):**
  - Eine Trefferformel für alle Angriffe: 10–90 %, Verteidigung inklusive eines Schildes.
  - Rückschlag auch nach einem Fehlschlag; tödliche Wunden; Gebundenheit neben Gegnern.
  - Untote nehmen nur Schaden von Untoten, magischen Waffen und Zaubern; unter 50 % Constitution gibt es einen Malus.
  - Wer stirbt, lässt alles fallen; Kill-VP richten sich nach dem Opfer.
- **Magie:**
  - Beschwörungen und Bolt/Lightning.
  - Die 7 sonstigen Zauber: Shield, Eye, Teleport, Curse, Subversion, Magic Attack, Enchant.
  - Zeitlich begrenzte Wirkungen mit Panel-Icons.
  - Tränke: brauen im Kessel, `q` trinken, `v` abfüllen, die Bombenphiole werfen; Drachen nur mit Drachenkraut im Kessel.
- **Objekte:**
  - `g` aufheben, `d` fallen lassen, `w` wechseln, `t` werfen, `f` Bogen.
  - `e` essen, `r` Schriftrolle lesen.
  - Schlüssel und Truhen; Schätze bringen am Portal VP.
- **KI:** Unabhängige jagen sichtbare Ziele, unsichtbare Einheiten sieht sie nicht. Der KI-Zauberer lässt seine Kreaturen jagen, kämpft, beschwört und flieht durchs Portal.
- **Darstellung:** Hidden Map (unerforscht schwarz, erinnert abgedunkelt), animierte Kerzen, Wasser und Portal, Cursor als VDP-Sprite.

---

## 2. Zusammenarbeit mit dem Nutzer (wichtig)

- **Sprache:** Deutsch im Chat und in den Docs, Code und Kommentare auf Englisch.
- **Design vor Code:** Neue Phasen bzw. Features erst im GDD (`docs/design/GDD.md`, Entscheidungen in §14) vorschlagen und abstimmen, dann bauen. Bei echten Wahlmöglichkeiten kurz fragen (2–4 Optionen, mit Empfehlung).
- **Regelantworten als Recherche:** Der Nutzer beantwortet Designfragen oft mit Auszügen aus seinen Quellen (mit Fußnoten). Originalwerte daraus sind nur Anker (D7). Unser eigener Wert bleibt, z. B. Schild +4 statt +13.
- **Keine exakte Kopie des Originals (D7):** Werte und Formeln sind eigenes Design. Ausnahme: die Kreaturtabelle `[PM 34]` als Startwerte (D12).
- **Optik:** eigene Pixelart, 24×24, 3/4-Frontansicht wie auf dem Amiga (D9–D11). Möbel, Teppiche, Türen sollen sichtbar sein, nicht abstrakt.
- **Steuerung:** Tastatur im Stil von Caves of Qud (D5), Zieltastatur **Cherry G84-4100, deutsch, ohne Ziffernblock** (D6).
- **Fokus Einzelspieler** mit Kampagne gegen KI-Zauberer (D4). Hotseat und Maus kommen nach v1.0.
- **Review vor dem Merge:** Zweimal hat erst das Review echte Fehler gefunden, obwohl CI grün war: ein Hänger, falsche VP, kaputte Panel-Icons, eine wirkungslose Bombe. Größere PRs deshalb vor dem Merge gegen das GDD prüfen und im Emulator anspielen.
- **Mergen:**
  - Der Nutzer gibt das Mergen frei. Hat er es ausdrücklich verlangt, mit `gh pr merge <n> --merge` mergen bzw. mit `--auto` nach grünem CI.
  - **Gestapelte PRs immer mit Merge-Commits**, nicht Rebase oder Squash, sonst tauchen die Commits doppelt auf.
  - Das Auto-Merge-Werkzeug der App wurde vom Freigabesystem abgelehnt.
- **Belege zeigen:** Nach sichtbaren Änderungen einen Emulator-Screenshot machen (`tools/run.py ... --screenshot`) und ansehen. Der Nutzer reagiert auf Bilder.
- **PR #55 „Mega Drive als zweite Plattform“ (D23):** Auf Wunsch des Nutzers **ignorieren**. Nicht mergen, nicht darauf aufbauen.
- **Amiga-Referenz:** WinUAE ist installiert (`C:\Program Files\WinUAE\winuae64.exe`), Kickstart und ADF liegen in `Desktop\amiga\`. Beobachtungen kommen nach `docs/design/amiga-observations.md`.

---

## 3. Umgebung

- **Windows 11** mit **WSL Ubuntu** (agondev und gcc laufen darin), dazu **uv** und **gh** (eingeloggt als `cadextcp`).
- Repo: `C:\Users\cadex\projekte\lords-of-chaos-agon` → GitHub `cadextcp/lords-of-chaos-agon` (public).
- **Nur lokal (gitignored):**
  - `reference/`: Handbuch-PDFs, Screenshots, Spectrum-Kartenbogen; urheberrechtlich geschützt, **nie committen**
  - `emulator/`, `toolchain/`, `sdcard/`, `.cache/`, `build/`, `bin/`, `obj/`, `src/core/gen/`
- Frischer Clone: `uv run tools/setup.py` (lädt Emulator 1.2.5 und agondev v0.22, SHA-geprüft).
- Alte Projekte, nur als Archiv: `C:\Users\cadex\projekte\LordsOfChaos` (gescheiterter BASIC-Versuch), `AgonBasics`, `AgonPipeline`.

---

## 4. Befehle

```bash
uv run tools/test.py                       # Host + eZ80-Selftest (vor jedem Commit)
uv run tools/run.py                        # Spiel im GUI-Emulator (Szenario 1)
uv run tools/run.py --dump --time 35 --list --keys "shift+e" --screenshot   # Rauchtest
uv run tools/run.py --bench --time 20      # Redraw-Messung -> loc.log
uv run tools/run.py --keytest              # Tastatur-Events anzeigen
uv run tools/mockup.py --sheet             # Mockup und Kachelübersicht
uv run tools/art/creature_sheet.py         # Kreaturen-Übersicht
```

- **Spiel-Optionen:**
  - `loc` startet Szenario 1, `loc --testland` die Entwicklungskarte, `loc --house` das Zauberer-Haus.
  - `--dump` schreibt `loc.log` mit ASCII-Karte und AP pro Frame.
  - Außerdem: `--bench`, `--keytest`, `--selftest`, `--free-round1` (keine Bewegungssperre in Runde 1) und `--fly` (eigene Flieger starten in der Luft).
- **`send_keys`-Syntax:**
  - Benannte Tasten nur mit `--list`. Ohne `--list` wird jedes Zeichen einzeln gesendet; aus `shift+e` würden dann s, h, i, f, t …
  - `up+right` ist ein Akkord, `hold=right=800` hält die Taste 800 ms.
- **Spieltasten:**
  - Zauber `c`, aufheben `g`, fallen lassen `d`, wechseln `w`, werfen `t`, Bogen `f`.
  - Essen `e`, lesen `r`, trinken `q`, Phiole füllen `v`.
  - Look `x`, fliegen `<` `>`, Einheit `Tab`, fertig `Leertaste`, Zugende `Shift+E`.

---

## 5. Architektur in Kürze

```
src/core/  plattformfrei (Host + eZ80):
  world.[ch]    Karte, Einheiten (stabile id), Objekte, Bewegung, AP/Stamina,
                Kill-Protokoll (world_kill_unit), Kessel-Datensätze, Laden (.map v3)
  turn.[ch]     Rundenablauf, aktive Einheit per id, Runden-Hook, KI-Callback
  combat.[ch]   Nahkampf, combat_damage (alle Schadensquellen), Terrain-Angriff
  items.[ch]    Inventar, Waffen-/Schild-Werte, Werfen, Bogen, Essen, Lesen, Truhen
  spells.[ch]   Mana, Zauberbücher (aus .scn), Beschwörung, Bolt/Lightning, 7 sonstige Zauber
  effect.[ch]   Wirkungen mit Laufzeit pro Einheit (Tick am Rundenende)
  brew.[ch]     Kessel, Zutaten, Phiolen, Bombe, Drachenkraut
  game.[ch]     Portal, VP, Kill-Gutschrift, Spielende (outcome), Magic Eye
  events.[ch]   Darstellungs-Ereignis-Ring (Beobachtung ohne Nebenwirkung, M5c)
  tutorial.[ch] Schritt-Engine des geführten Tutorials (M5b)
  lexicon.[ch]  entdeckte Kreaturen/Objekte als Bitmasken (M5b)
  ai.[ch]       Jäger und Zauberer-KI
  sight.[ch]    Sichtlinie (Bresenham) und Hidden Map
  view.[ch]     9x9-Fenster: Ebenen pro Feld, Auto-Tiling, Dirty-Felder, Cache, Animation
  chord.[ch]    Pfeil-Akkorde und Tastenwiederholung
  names.[ch]    alle Anzeigetexte (deutsch, ohne Umlaute)
  selftest.c    läuft auf Host UND eZ80
  gen/          GENERIERT: tiles.h, data.[ch], creatures.h, maps.[ch], scenarios.[ch]
src/agon/  main.c (Hauptschleife, settle()), render.c (VDP, Panel, Titel-Streaming),
           screens.c (Endbildschirm, Hilfe-Viewer, Lexikon, Titel, M5),
           fx.c (Ereignis-Animation), music.c (Titelmusik-Sequencer),
           sound.c (16 Effekte, Wellenform+ADSR), input.c, umfont.c,
           mapfile.c (.map, .scn, wizards/lexicon/save von SD), keytest.c, log.c, emu.asm
host/      PC-Frontend (--selftest, --dump, --layers)
```

**Pipelines (laufen automatisch in `build.py` und `test.py`):**

| Quelle | Werkzeug | Ergebnis |
|---|---|---|
| `assets/tiles/*.png`, `assets/icons/*.png` | `build_tiles.py` | `build/tiles.bin` (291 Kacheln, 162 KB), `gen/tiles.h` |
| `data/*.csv` | `gen_data.py` | `gen/data.[ch]`, `gen/creatures.h` |
| `data/maps/*.txt` | `gen_maps.py` | `build/maps/*.map` (Format v4) und `gen/maps.[ch]` |
| `data/scenarios/*.txt` | `gen_scenarios.py` | `build/scenarios/*.scn` (Zauberbücher, „LOCS“ v1) und `gen/scenarios.[ch]` |
| `data/help/*.txt` | `gen_help.py` | `build/help/*.hlp` (Seiten, Umlaut-Codes; Lexikon-Seitenzahl = Kreaturen+Objekte) |
| `data/music/*.txt` | `gen_music.py` | `build/music/*.bin` (Noten, „LOCM“) |
| `assets/title/title.png` | `build_title.py` (Quelle: `tools/art/make_title.py`) | `build/title.bin` (320×240 RGBA2222, „LOCB“) |

- Reihenfolge: Kacheln → Daten → Karten → Szenarien → Hilfe → Musik → Titel. `gen_maps` liest Enum-Werte direkt aus den C-Headern.
- **View-Hash `HOUSE_VIEW_HASH` (selftest.c):** Er ändert sich mit Kacheln, Karte oder Kompositionsregeln. Host und eZ80 müssen denselben Wert ausgeben; den Wert bewusst übernehmen.
- **Budget:** `loc.bin` ist 292 KB — über der 250-KB-Marke; seit M5 kommen Texte/Titel/Musik von der SD (ADR 0011).

---

## 6. Fallstricke

Die vollständige Liste steht in `docs/AGON-QUIRKS.md`. Die wichtigsten:

1. **Das agondev-Makefile kennt keine Header-Abhängigkeiten.** `build.py` baut deshalb immer clean.
2. **`int` ist auf dem eZ80 24 Bit.** Im Core nur `stdint`-Typen. `1u << i` ist ab `i = 24` undefiniert; dann `(uint32_t)1 << i` schreiben. **Divisionen sind langsam**; Hot-Paths ohne Division schreiben.
3. **ez80-clang-Bug:** `x == A || x == B || …` über Enums kann abstürzen. Lookup-Tabellen verwenden (T7).
4. **`/*` in C-Kommentaren**, z. B. „data/*.csv“, ergibt den Fehler „/* within comment“.
5. **Tastatur:** Bewegung nur per VKey. Die Hauptschleife muss die **Event-Queue vollständig leeren**, bevor sie `chord_poll` aufruft (K1–K5).
6. **Der CLI-Emulator führt `autoexec.txt` aus.** `test.py` und `run.py` schreiben es jeweils neu.
7. **Bash-Tool unter Windows:** Heredocs mit Sonderzeichen brechen; Skripte lieber per Datei schreiben (`.cache/*.py`). Python unter Windows schreibt CRLF; Dateien deshalb mit `write_bytes` schreiben. Für `wsl.exe` `MSYS_NO_PATHCONV=1` setzen.
8. **Der Agon-Systemfont hat keine Umlaute.** UI-Texte ohne ä/ö/ü („Tuer“).
9. **Tile-IDs sind 16 Bit** (291 Kacheln, Icons über 255). Tile-Tabellen nie als `uint8_t` anlegen; die Panel-Icons sind daran schon einmal gescheitert.
10. **Kachel-PNGs sind Quelle.** `tools/art/make_tiles.py` überschreibt sie, also nur mit `--only NAME` neu erzeugen.
11. **Unit-Indizes sind instabil.**
    - `world_remove_unit` tauscht mit der letzten Einheit. Einheiten über Aktionen hinweg per `Unit.id` und `world_find_unit` halten.
    - Tode immer über `world_kill_unit` bzw. `combat_damage`, damit die VP stimmen.
    - Wer in einer Schleife töten kann, liest Killer-Art und -Besitzer vorher aus.
    - Im Frontend nach jeder Aktion `settle()` aufrufen.
12. **Zielzauber brauchen Reichweite 6 und Sichtlinie (D17).** Tests, die durch die Haustür im Testland zielen, öffnen sie vorher (`feature[5][8] = FE_DOOR_OPEN`).
13. **Der Kessel-Zustand folgt dem Kessel-Objekt** (`brew_cauldron_at`). Volle Kessel lassen sich nicht tragen; Phiolen wirken vorerst mit Stufe 2.

---

## 7. Nächste Schritte

**Polish-Runde (Stand 2026-10-04):** #114–#117 sind gestapelt (Merge-Commits, in Reihenfolge 114 → 115 → 116 → 117) und warten auf die Merge-Freigabe des Nutzers. Danach auf Hardware prüfen (SD-Paket `bin/loc-sd.zip` neu): `vdptest` (Log mit dem Emulator vergleichen, ADR 0012), Klang/Musik, Schrift, Sprites, Ladezeit (`sfx.bin` 110 KB zusätzlich), VDP-RAM. Offen aus dem Plan: Copper/Doppelpuffer verworfen (ADR 0012); KI-Bewegungen gleiten noch nicht (nur eigene Schritte).

**Fallstricke aus der Polish-Runde:** Der eZ80-RAM ist knapp (QUIRK S6) – große Puffer nur streamen, Werkzeuge als eigene Programme (`spikes/`). Audio: VDP queued nicht (A1), Kanal 3+ erst freischalten (A8), stimmbar = Flag 16 (A9) – sonst landen Befehlsbytes als Text auf dem Schirm. Python-Patches unter Windows immer mit `encoding="utf-8"` lesen.


**M5 Präsentationsrunde ist fertig** (alle vier PRs gemergt, CI grün): #88 Endbildschirm + Menü-Rücksprung + Kampagnenergebnis, #90 Hilfeseiten/Tutorial/Lexikon, #91 Ereignis-Ring/Kampf-/Todesanimation/Sound/KI-sichtbar, #92 Titelbild/Titelmusik. Plan war `docs/PLAN-M5.md` (4 PRs nach Nutzerentscheid).

**Als Nächstes: Hardware-Abnahme von M5 durch den Nutzer** (der CLI-Emulator hat weder VDP-Bild fein noch Audio — QUIRKS E1/A4):
1. **SD-Paket `bin/loc-sd.zip`** (liegt bereit; entpacken nach `/loc` auf der Karte): `loc.bin`, `tiles.bin`, `title.bin`, `maps/`, `scenarios/`, `help/`, `music/`.
2. Prüfen: Titelbild + Menü, Titelmusik (Klang!), Effekt-Klang (Kampf), Animations-Timing, `loc --selftest`, `loc --bench`, Ladezeit (tiles 162 KB + title 75 KB).
3. **VDP-RAM messen** (QUIRKS S2): Kacheln + Titel-Bitmap zusammen — im Emulator ok, Hardware offen.
4. **Titel-Motiv abstimmen:** Das Bild ist ein eigener Vorschlag (Platzhalter). Quelle: `assets/title/title.png`, Generator `tools/art/make_title.py`. Änderungswünsche gerne — anderes Motiv, anderer Schriftzug-Stil.
5. **Spielstand-Format v3** (M5c): alte v2-Spielstände werden abgelehnt („Kein Spielstand“) — einmal löschen.

Danach: M6/Chaos laut `docs/ROADMAP.md` (GDD §12), oder Politur aus §8.

**Workflow:** pro Issue ein Branch, Selftest-Checks, Emulator-Screenshot ansehen, CHANGELOG, PR. Vor dem Merge ein Review (siehe §2).

---

## 8. Offene Punkte

- **Hardware-Test des Nutzers (#3, #7):** Er wird mit jedem Teil wichtiger; KI-Runden und Sicht kosten auf dem Emulator schon spürbar Zeit.
  - Auf die SD-Karte nach `/loc`: Inhalt von `bin/loc-sd.zip` (siehe §7).
  - Dann `SET KEYBOARD 2`, `cd /loc`, `loc --keytest`, `loc --bench`, `loc`.
  - **Runde 1 ohne Bewegung:** `loc` startet mit der Original-Regel `[PM 7]` — in Runde 1 ist nur Zaubern moeglich. `Shift+E` beendet den Zug; `loc --free-round1` hebt die Sperre auf. Im Tutorial ist sie ohnehin aufgehoben.
  - **Stand 2026-10-03 (Hardware, Stand `037521f`):** Upload nach `/loc` per USB, `loc --selftest` PASS und `loc --bench` sind gelaufen (Werte in `docs/AGON-QUIRKS.md`, Ablauf in `docs/TESTING.md`); lange Dateinamen auf FAT sind geklärt. Das Spiel selbst, `--keytest` und die Eingabe am Gerät stehen noch aus. Der Lumagon-Autostart ist auf der Karte abgeschaltet (Sicherung `/autoexec.lum`).
  - Zu klären: Akkorde ohne Ghosting? Codes für `<`/`>`? MOS-/VDP-Version? Ladezeiten (tiles.bin 162 KB, title.bin 75 KB)? Dauer einer KI-Runde? VDP-RAM mit Titel (S2)? Klang?
- **Titel-Motiv:** Platzhalter-Vorschlag, wartet auf Abstimmung (siehe §7).
- **Tags:** `v0.2.0` bis `v0.4.0` sind noch nicht gesetzt; vorgesehen nach dem Hardware-Test. `v1.0.0` (M4) und ein M5-Tag sind des Nutzers Sache.
- **#2 Buffered Commands:** nur nötig, falls die Geschwindigkeit auf Hardware nicht reicht.
- **Bekannte Vereinfachungen, die später nachzuziehen sind:**
  - Phiolen wirken mit Stufe 2, weil Objekte keine Zusatzdaten tragen.
  - Enchant wirkt pro Einheit statt pro Waffe; Waffen am Boden werden nicht verzaubert.
  - Subversion verbietet alle Reittiere; die Ausnahme „nur Reittiere mit Zauberer“ kommt mit dem Reiten.
  - Höchstens 4 Kessel, 64 Bodenobjekte und 16 noch nicht abgerechnete Kills; Ereignis-Ring 16 Einträge (volle Ring verwirft Neues — Darstellung only).
  - Lexikon: geteilte Phiole-Kacheln markieren den ersten passenden Objekt-Typ.
- `tools/mockup.py` rendert nur das Zauberer-Haus (9×9).

---

## 9. Wo steht was

| Thema | Datei |
|---|---|
| Spieldesign, Entscheidungen D1–D22, M4-Plan | `docs/design/GDD.md` (§14, §16) |
| Amiga-Beobachtungen | `docs/design/amiga-observations.md` |
| Roadmap und Arbeitsweise | `docs/ROADMAP.md` |
| Architektur | `docs/ARCHITECTURE.md` |
| Architekturentscheidungen | `docs/adr/0001` bis `0011` (Rendering 0006, Eingabe 0007, Daten 0008, Sicht 0009, SD-Daten 0011) |
| Plattform-Quirks | `docs/AGON-QUIRKS.md` |
| Testen und Debuggen | `docs/TESTING.md` |
| Änderungen | `CHANGELOG.md` |
