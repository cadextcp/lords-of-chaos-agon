# ADR 0011: Spieldaten von der SD-Karte statt im Binary

- Status: akzeptiert (2026-10-03, M5)
- Kontext: `docs/PLAN-M5.md`, GDD §16 (250-KB-Marke)

## Kontext

`loc.bin` ist über die 250-KB-Marke des GDD §16 gewachsen (M4-Ende: 255 KB,
mit M5 wächst der Code weiter: Endbildschirm, Hilfe-Viewer, Lexikon, Titel).
Hilfe-/Lexikon-Texte, Tutorial-Hinweise und das Titelbild sind reine Daten
und würden das Binary weiter aufblähen — und sie ändern sich öfter als der
Code (Formulierungen, Rechtschreibung, Motiv).

## Entscheidung

Neue **DatenDateien kommen auf die SD-Karte** (`/loc/...`), nicht ins
rodata:

| Daten | Quelle | Datei auf SD | Loader |
|---|---|---|---|
| Hilfeseiten | `data/help/*.txt` | `help/*.hlp` | `screens.c` (`help_parse`) |
| Titelbild | `assets/title/` | `title.bin` | `render.c` (Streaming, M5c) |
| Titelmusik | `data/music/*.txt` | `music/*.bin` | `music.c` (M5c) |
| Lexikon | — | `lexicon.dat` | `mapfile.c` (Muster `wizards.dat`) |

`.hlp`-Format: `"LOCH"`, Version, Seitenzahl (u16 LE); pro Seite Titel und
Zeilen als Länge+Bytes, Umlaute als umfont-Zeichencodes (0x84/0x94/0x81/0xE1).
`tools/gen_help.py` kompiliert und validiert (38 Spalten, 26 Zeilen,
Lexikon-Seitenzahl = Kreaturen+Objekte). `tools/build.py` erzeugt,
`agon_env.stage_game()` kopiert alles nach `/loc`.

## Konsequenzen

- Der Code bleibt klein; Textänderungen brauchen keinen Binary-Neubau auf
  der Hardware (nur Datei kopieren).
- Fehlende Dateien müssen unschädlich sein: Hilfe fällt auf die eingebaute
  Tastenliste zurück, Lexikon/Titel degradieren gracefully.
- Kleine Persistenz-Dateien (`wizards.dat`, `save.dat`, `lexicon.dat`)
  bleiben im Arbeitsverzeichnis `/loc` — Muster: Magic + Version + Payload,
  bei allem Unpassenden bleibt der alte Zustand.
