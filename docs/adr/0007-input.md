# ADR 0007 – Eingabe: Virtual-Key-Mapping, Pfeil-Akkorde, eigene Tastenwiederholung

- **Status:** angenommen für den Emulator (2026-10-02). Die Bestätigung auf echter Hardware mit der Cherry G84-4100 steht aus (Issue #3).
- **Kontext:**
  - GDD §5.2: Die G84-4100 hat keinen Ziffernblock. Diagonalen kommen aus Pfeil-Akkorden; Pos1, Bild↑, Ende und Bild↓ sind Alternativen.
  - Das Spike-Programm `loc --keytest` loggt jedes `kbuf`-Event: ASCII, Virtual Key, Modifier, down/up und Zeit.

## Befunde (Emulator, `SET KEYBOARD 2`)

| Taste | ASCII (down) | VKey |
|---|---|---|
| ↑ ↓ ← → | 0B, 0A, 08, 15 | **96, 98, 9A, 9C** |
| Pos1, Ende, Bild↑, Bild↓ | 00 | **86, 88, 93, 95** |
| ESC | 1B | 7D |
| Buchstaben a–z | ASCII | 16 + (Buchstabe − 'a'), nach Layout (deutsch: y/z getauscht) |

1. **ASCII bei Key-up-Events ist veraltet.** Es wiederholt das ASCII der zuletzt gedrückten Taste; „↑ los“ meldete ASCII 15 (→). Bewegung wird deshalb **ausschließlich über den VKey** gemappt.
2. **`kbuf` liefert kein Auto-Repeat.** Eine gehaltene Taste ergibt genau ein Down-Event.
3. **Akkorde funktionieren:** ↑+→ im Abstand von 40 ms ergibt eine Diagonale.
4. `SET KEYBOARD 2` ist das deutsche Layout; `z` und `y` sind wie erwartet vertauscht.
5. `<` und `>` ließen sich vom Testwerkzeug nicht senden (pydirectinput kennt die ISO-Taste nicht). Das muss auf Hardware geprüft werden.

## Entscheidung

- **`src/agon/input.c`** mappt Bewegung per VKey: Pfeile und WASD werden zu `ARROW_*`, Pos1, Bild↑, Ende und Bild↓ zu Diagonalen. Aktionsbuchstaben (`c`, `g` …) werden weiter per ASCII des Down-Events gelesen; dort ist das ASCII korrekt.
- **`src/core/chord.c`** (plattformfrei, getestet):
  - Zwei Pfeile innerhalb von **80 ms** ergeben eine Diagonale.
  - Ein einzelner Pfeil löst beim Loslassen aus, oder nach Ablauf des Fensters, wenn er gehalten wird.
  - **Eigene Wiederholung per Uhr:** die erste nach **350 ms**, danach alle **200 ms**. Gehaltene Akkorde wiederholen die Diagonale; nach dem Loslassen einer Taste startet die Verzögerung neu.
- Das Spiel nutzt dasselbe Modul wie `--keytest`.

## Nachtrag M2a: Ereignis-Warteschlange zuerst leeren

- **Symptom:** Auf der großen Karte kostet ein Schritt mit Scrollen mehr Zeit. Nach einem Akkord wurde ein folgendes einzelnes → als NO gelesen, und gehaltenes ↓ lief zuerst nach Norden.
- **Ursache:** Die Hauptschleife las nur **ein** `kbuf`-Event pro Durchlauf. Während gezeichnet wurde, warteten die Loslass-Events noch in der Warteschlange. `chord_poll()` sah dadurch veraltete gehaltene Tasten und löste die Wiederholung aus.
- **Lösung:** Pro Durchlauf zuerst **alle** wartenden Events verarbeiten, erst dann `chord_poll()`, Animation und Blinken (`src/agon/main.c`). Danach lief die Testfolge auf Testland korrekt: O, NO durch die Tür, O auf den Weg, 7× S, Zugende, 7× O über die Brücke.

## Nachweis im Emulator

Die Folge `↑+←`, Bild↓, Ende, „→ halten 1,3 s“ bewegt den Zauberer NW, SO, SW und zweimal nach O. Die AP sinken dabei 40 → 34 → 28 → 22 → 18 → 14 (Diagonale 6, Gerade 4).

## Offen auf Hardware (#3)

- Schafft die G84 zwei Pfeile gleichzeitig ohne Ghosting? Stimmen die VKeys?
- Welche Codes liefern `<` bzw. `>` und der Fn-Ziffernblock?
- Fühlen sich 80 ms, 350 ms und 200 ms gut an? Alle drei sind Konstanten in `src/agon/main.c`.
