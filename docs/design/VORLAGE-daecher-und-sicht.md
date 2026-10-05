# Entscheidungsvorlage: Sollen Dächer die Sicht blockieren?

- **Stand:** 2026-10-05, nach D41 (Dächer heben nur für die aktive Figur)
- **Anlass:** Playtest A4 — „mein Troll wurde im Laufen von etwas getroffen, ich weiß
  aber nicht von was … trotzdem wundert mich, dass ich ihn nicht sehe"
- **Status:** Vorschlag, noch nicht entschieden

## Der Kern

**Die Sicht kennt keine Dächer.** `sight.c` rechnet mit Boden und Feature; das Dach
ist eine reine Anzeigeebene (`roof` als Bitmap, nur in `view.c` ausgewertet). Eine
Kreatur unter einem geschlossenen Dach gilt den Regeln deshalb als **vollständig
sichtbar**, obwohl der Spieler nichts von ihr sieht.

Das hat drei Folgen, und die dritte ist neu:

1. **Freier Schlag aus dem Nichts.** Genau deine Beobachtung. Mit der A4-Änderung ist
   das entschärft — aber nur, weil der freie Schlag jetzt an `sight_visible` hängt,
   und das sagt für die Kreatur im Haus weiterhin „sichtbar". Der Schlag kommt also
   immer noch.
2. **Zielen und KI rechnen mit Wissen, das der Spieler nicht hat.** `sight_has_los`
   und `sight_has_spell_los` laufen über dieselbe Blockier-Bitmap ohne Dächer. Man
   kann in ein geschlossenes Haus zaubern, und die KI zielt heraus.
3. **Einheiten werden über das Dach gezeichnet.** Die Ebenenreihenfolge ist
   `Boden → Halbböden → DACH → Dekor → Objekt → Einheit`, und der Renderer zeichnet
   aufsteigend. Liegt das Dach, steht die Figur optisch **darauf**. Das war vor D41
   selten zu sehen, weil ein einziger eigener Fuß im Haus das ganze Dach hob; seit
   D41 ist das geschlossene Dach der Normalfall. **Belegt:** der Selftest
   `open: a figure under a closed roof is drawn on top of it` prüft die
   Ebenenreihenfolge und besteht — die Figur liegt im Ebenenstapel über dem Dach.

## Drei Wege

### Weg 1 — So lassen

Dächer bleiben reine Anzeige. Kosten: nichts. Preis: Punkt 1 bis 3 bleiben, und
Punkt 3 wird mit D41 sichtbarer als zuvor. Die Anzeige sagt dann dauerhaft etwas
anderes als die Regeln, und genau daraus entstand die Playtest-Beschwerde.

### Weg 2 — Dächer blockieren die Sicht (empfohlen)

Das Dach kommt in die Blockier-Bitmap: ein überdachtes Feld ist nur sichtbar, wenn
der Betrachter **unter demselben Dach steht** oder eine Sichtlinie durch eine
Öffnung hat. Das ist dieselbe Regel, die D41 schon für die Anzeige benutzt — sie
gilt dann auch für die Regeln.

**Was dadurch von selbst richtig wird:**

- Kein freier Schlag aus dem geschlossenen Haus (Punkt 1 erledigt sich ohne
  Sonderfall).
- Zauber und KI können nicht mehr durch Dächer zielen (Punkt 2).
- `push_unit` filtert Einheiten auf nicht sichtbaren Feldern bereits heraus — die
  Figur auf dem Dach verschwindet, ohne dass man an der Ebenenreihenfolge dreht
  (Punkt 3).
- Die Dachregel aus D41 ergibt sich dann aus der Sicht, statt eine **zweite**
  Mechanik daneben zu sein. `roof_lifted()` könnte durch `sight_visible` ersetzt
  werden.

**Was es kostet:**

- `world_sight_byte` muss das Dach mitlesen — aber nur für Betrachter, die nicht
  selbst darunter stehen. Die Blockier-Bitmap ist heute **betrachterunabhängig** und
  wird genau deshalb zwischengespeichert (Audit B1). Mit einer betrachterabhängigen
  Regel fällt dieser Cache für überdachte Felder weg. Ein gangbarer Weg: **zwei**
  Bitmaps cachen — eine mit Dächern, eine ohne — und je nach Standort des Betrachters
  die passende wählen. Das kostet 180 Byte und hält den Cache.
- **Eigene Einheiten im Haus bleiben sichtbar** (sie stehen unter demselben Dach) —
  für die Sicht des Spielers also kein Verlust.
- **Die KI braucht dieselbe Regel**, sonst zielt sie weiter heraus. Sie führt heute
  pro Zug keine eigene Sichtkarte; das ist derselbe offene Punkt wie bei A4.
- Betrifft Spielbalance: Häuser werden zu echten Verstecken. Das ist vermutlich
  gewollt, aber es ist eine Balance-Änderung, keine reine Korrektur.

### Weg 3 — Nur Einheiten verstecken

Dächer blockieren nicht die Sicht, aber **Einheiten** auf überdachten Feldern gelten
als unsichtbar, solange das Dach liegt. Gelände bleibt erinnert und bezaubbar.

Billiger als Weg 2 (eine Zeile in `push_unit` plus dieselbe Prüfung im freien Schlag),
löst Punkt 1 und 3, lässt Punkt 2 offen. Der Cache bleibt unangetastet.

## Empfehlung

**Weg 2**, wenn Häuser als Deckung zählen sollen — er macht Anzeige und Regeln zu
*einer* Sache statt zweier, die auseinanderlaufen können, und er löscht die
Sonderfälle, die sonst an drei Stellen gepflegt werden müssen.

**Weg 3**, wenn du die Balance nicht anfassen willst. Er behebt genau das, was im
Playtest aufgefallen ist, und lässt alles andere, wie es ist.

**Weg 1** scheidet damit praktisch aus: Punkt 3 ist bestätigt und seit D41 der Normalfall.

## Vor der Entscheidung zu klären

1. **Soll die KI dieselbe Beschränkung bekommen?** Sie steht schon bei A4 offen. Ohne
   sie ist jede dieser Regeln einseitig zugunsten des Spielers.
2. **Weg 2 oder Weg 3** — Balance anfassen oder nicht.
