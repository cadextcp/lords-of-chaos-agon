# ADR 0014: Dateiformate und ihre Versionierung

- Status: akzeptiert (2026-10-08). Hält fest, wie die Formate seit M1 gebaut sind, und legt die Regel für Versionswechsel fest.
- Kontext: Architektur-Review 2026-10-08 (Lücken: Spielstand, Szenario-Format v3). Bisher stand das Muster nur als Satz in ADR 0011.
- Code: `src/core/save.c`, `world.c` (`world_load_bin`), `spells.c` (`spellbook_load`), `ai_wizard.c` (`ai_scenario_load`), `lexicon.c`; `src/agon/mapfile.c`, `render.c`, `sound.c`, `music.c`, `screens.c`; Erzeuger in `tools/gen_*.py` und `tools/build_tiles.py`.

## Kontext

Das Spiel liest und schreibt ein Dutzend eigener Binärdateien. Sie folgen alle demselben Muster, aber das Muster war nirgends festgehalten. Die Versionsnummern sind zuletzt schnell gestiegen (Spielstand v8 bis v12 in drei Tagen), ohne dass klar war, wann ein Wechsel nötig ist und was mit alten Dateien geschieht.

## Entscheidung

### 1. Ein Muster für alle Dateien

**Magic (4 Byte `LOC?`) + Versionsbyte + Nutzdaten.** Der Lader prüft alles, bevor er etwas übernimmt. Passt etwas nicht, bleibt der alte Zustand, und das Spiel fällt auf einen sinnvollen Ersatz zurück (keine Hilfe, Standard-Zauberer, Spielstart ohne Laden). Kein Lader darf mit halb übernommenen Daten weiterlaufen.

| Datei (SD `/loc`) | Magic | Version | Erzeuger | Lader | Bei Fehler |
|---|---|---|---|---|---|
| `maps/*.map` | `LOCM` | 5 (v2 lädt noch) | `tools/gen_maps.py`, `gen_variants.py` | `world_load_bin` | das Programm beendet sich (`main.c`) |
| `scenarios/*.scn` | `LOCS` | 3 (v1, v2 laden noch) | `tools/gen_scenarios.py` | `spellbook_load`, `ai_scenario_load` | ohne KI-Profil und Bücher |
| `tiles.bin` | `LOCT` | 1 | `tools/build_tiles.py` | `render.c` | Abbruch mit Meldung, auch bei falscher Kachelzahl |
| `help/*.hlp` | `LOCH` | 1 | `tools/gen_help.py` | `screens.c` | eingebaute Tastenliste |
| `music/*.bin` | `LOCM` | 2 | `tools/gen_music.py` | `music.c` | still |
| `sfx.bin` | `LOCX` | 1 | `tools/gen_sfx.py` | `sound.c` | ohne Samples |
| `title.bin`, `win.bin`, `lose.bin` | `LOCB` | 1 | `tools/build_title.py` | `render.c` | Menü ohne Bild, Text-Überschrift |
| `save.dat` | `LOCSG` | 12 | Spiel | `save_deserialize` | „kein Spielstand“ |
| `wizards.dat` | `LOCW` | 1 + `sizeof(Wizard)` | Spiel | `wizards_load` | Standard-Zauberer |
| `lexicon.dat` | `LOCL` | 1 | Spiel | `lexicon.c` | leeres Lexikon |
| `settings.dat` | `LOCP` | – (6 Byte, Bits) | Spiel | `sound_settings_load` | Standardwerte |

**Doppeltes Magic:** Karten und Musik haben beide `LOCM`. Das ist harmlos, weil die Lader nur ihre eigenen Ordner lesen und die Versionen sich unterscheiden (5 gegen 2). Neue Formate bekommen trotzdem ein eigenes Magic.

### 2. Zwei Arten von Formaten

- **Formate des Builds** (`.map`, `.scn`, `tiles.bin`, `.hlp`, Musik, Samples): Werkzeug und Spiel entstehen im selben Build, und die SD-Karte wird neu bespielt. Ein Lader darf ältere Versionen weiter annehmen, wenn das billig ist (Karte v2, Szenario v1 und v2). Pflicht ist das nicht.
- **Formate des Spielers** (`save.dat`, `wizards.dat`, `lexicon.dat`, `settings.dat`): Sie überleben ein Update auf der SD-Karte. Für sie gilt:
  - Ein **Spielstand ist eine 1:1-Kopie der Strukturen** `World`, `Turns`, `Game`, Zauberbücher, Erkundung und Flächen (`save_serialize`). Jede Änderung an einer dieser Strukturen **erhöht `SAVE_VERSION`**, und die Begründung kommt in den Kommentar daneben. Alte Spielstände werden **abgelehnt, nicht umgewandelt**. Zusätzlich prüft der Lader die Gesamtlänge.
  - `wizards.dat` trägt `sizeof(Wizard)` im Kopf und `wizard_valid` prüft jeden Platz. Ein geändertes `Wizard` macht die Datei also von selbst ungültig. Die Spieler verlieren dann ihre Zauberer; deshalb **nur mit Ankündigung im CHANGELOG** und wenn möglich mit `WIZ_FILE_VERSION` + Umwandlung.
  - `settings.dat` erweitert sich **nur über bisher freie Bits** (D72: Bits 3 und 4), damit alte Dateien gültig bleiben.

### 3. Der Spielstand im Einzelnen

- Größe auf dem Host: `SaveGame` 11 496 Byte (davon `World` 10 282), Puffer `SAVE_BUF_SIZE` 12 288 Byte. Reserve rund 800 Byte. **Wächst eine Struktur, zuerst die Reserve prüfen**; der Selftest `m4i` scheitert, wenn `save_serialize` nicht mehr in den Puffer passt.
- Schreiben über `save.new` und Umbenennen (`mapfile.c`), damit ein Abbruch nie einen halben Spielstand hinterlässt.
- Ladegrenze (F8 des GDD): `loads_left` liegt im Spielstand.
- **Nicht im Spielstand:** Zauberprioritäten der KI (sie kommen nach dem Laden frisch aus dem Szenario, FRAGEN F29), Sichtwarnungen und gehörte Geräusche der Oberfläche (D69, D73).

### 4. Szenario-Format v3 (`.scn`)

Aufbau (Quelle: Kopf von `tools/gen_scenarios.py`):

1. `"LOCS"`, Version 3, Anzahl Bücher; je Buch Besitzer und Paare (Zauber, Stufe)
2. Profile der KI-Zauberer: Besitzer, Name (10 Byte), Mana, AP, Ausdauer, Con, Com, Def, MR, Tragkraft, Siegpunktwert, dann Paare (Zauber, Priorität)
3. Routen: Anzahl, Anzahl für Beschworene, je Route Flags und Wegpunkte
4. Pläne für Einheiten der Karte: Einheit, Route, Schritt, Flags
5. Auslöser: x, y, Id
6. **v3:** AP-Faktor in Prozent (D71), Standard 100

`spellbook_load` liest nur die Bücher und überspringt den Rest, `ai_scenario_load` liest alles danach. Die Karte wird vorher geladen; Pläne verweisen auf ihre Einheitenliste. Zauber-Ids kommen aus dem erzeugten `gen/data.h`, ein Szenario passt also nur zu dem Build, der es erzeugt hat.

### 5. Bytes in C-Arrays für die Tests

Karten und Szenarien landen zusätzlich als C-Arrays in `src/core/gen/` (`MAPBIN_*`, `SCN_*`). So prüft der Selftest auf Host und eZ80 die echten Lader mit echten Daten, auch mit absichtlich kaputten.

## Folgen

- \+ Jede Datei ist robust gegen Fremdes, Altes und Kaputtes, und das Verhalten bei Fehlern ist je Datei bekannt.
- \+ Kein Umwandlungscode für Spielstände; der Core bleibt klein.
- − Jedes Update mit Strukturänderung macht laufende Spielstände wertlos. Das ist für eine Entwicklungsfassung tragbar, für eine Veröffentlichung nicht: Spätestens vor v1.0 braucht es eine Umwandlung oder eine eingefrorene Struktur.
- − **Offen, nicht geprüft:** Ob ein Spielstand vom eZ80 auf dem Host lesbar ist. `save.c` behauptet das, aber `Unit` enthält `uint16_t`, das auf dem Host auf 2 Byte ausgerichtet wird und auf dem eZ80 nicht. Die Längen dürften sich unterscheiden. Bisher braucht das niemand.
