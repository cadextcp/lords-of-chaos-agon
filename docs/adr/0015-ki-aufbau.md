# ADR 0015: Aufbau der Computer-Gegner

- Status: akzeptiert (2026-10-08). Hält fest, wie die KI seit D67 (Phasen KI 1 und KI 2, PRs #161/#162) gebaut ist.
- Kontext: GDD §10, Regelbericht `docs/REGELN-ORIGINAL-SPECTRUM.md` Kapitel 10 (K10.2 Profile, K10.3 Entscheidungsschleife, K10.5 Zauberwahl, K10.6 Gegenstände, K10.7/K10.8 Routen und Pläne), Plan `docs/PLAN-KI.md`, Architektur-Review 2026-10-08 (Lücke).
- Code: `src/core/ai.h` (öffentlich), `ai_priv.h` (intern), `ai.c`, `ai_creature.c`, `ai_items.c`, `ai_nav.c`, `ai_wizard.c`; Werkzeuge `host/duel.c`, `host/arena.c`.

## Kontext

Die KI soll wie im Original spielen (D67: „alles wie im Original“), nur sehen, was ihre Einheiten sehen (verdeckte Bewegung), auf dem eZ80 schnell genug sein und sich auf dem Host in großer Zahl durchspielen lassen. Der erste Wurf (M3f bis D62) war eine einzige Datei mit Jäger- und Wächterregeln; mit K10 kamen Entscheidungsschleife, Routen, Gegenstände und Zauberwahl dazu.

## Entscheidung

### 1. Module

| Datei | Aufgabe |
|---|---|
| `ai.c` | Wildtiere und Herden (D35–D37), Jäger und Wächter der Unabhängigen, Pläne für neue Kreaturen (`ai_plan_new`) |
| `ai_creature.c` | die Entscheidungsschleife je Kreatur (K10.3), Sichtliste (`ai_build_view`) |
| `ai_items.c` | aufheben, ausrüsten, essen, trinken, Beute zum Zauberer werfen (K10.6) |
| `ai_nav.c` | Schritte, Wege um Wände und Türen, Ring der letzten Felder |
| `ai_wizard.c` | Szenario-Profil laden (K10.2), Zauberwahl (K10.5), die Phase eines KI-Zauberers |

Nur `ai.h` ist öffentlich; `ai_priv.h` teilen sich die fünf Dateien. Alles liegt im plattformfreien Core (ADR 0003).

### 2. Ablauf einer Phase

- `turn.c` ruft je KI-Besitzer `t->ai` (= `ai_wizard_phase`) und danach `t->on_ai` (die Darstellung in `main.c`, M5c). Die Unabhängigen spielen am Rundenende in `turn_independents`.
- `ai_wizard_phase` lässt **zuerst den Zauberer** handeln (mit Buch und Profil), dann **jede Kreatur einzeln**, in der Reihenfolge eines vorher genommenen **Id-Schnappschusses**. Tode ordnen die Einheitenliste um; über die Ids handelt keine Einheit doppelt und keine wird übersprungen.
- Danach schreibt `game_credit_kills` die Abschüsse gut.

### 3. Die Entscheidungsschleife (K10.3)

Je Kreatur höchstens **50 Durchgänge** (`AI_PASSES`), jeder mit einer Handlung. Die Reihenfolge des Originals:

1. schlafen und Auslöser
2. Sofort-Handlungen (Kessel, Gegenstand unter sich, Trank, Essen, Waffenwechsel)
3. Fernangriff
4. Flucht
5. Nahkampfziel
6. Gegenstandsziel
7. Route oder Jagd
8. Schritt

Die Schleife endet, wenn keine AP mehr da sind, nichts mehr anliegt oder die Kreatur stirbt.

### 4. Wissen: nur die eigene Sicht

Jede Kreatur baut sich eine **Sichtliste** (`AiView`): bis zu 20 sichtbare Gegner und 24 sichtbare Gegenstände, die nächsten zuerst. Ob sie etwas sieht, entscheidet `sight_sees` je Beobachter (ADR 0009); eine Sichtkarte der ganzen Seite rechnet die KI nicht. Damit gilt die verdeckte Bewegung auch für den Computer.

### 5. Bewegung

- Erst **gierige Schritte** nach der Regel des Originals: die acht Nachbarn nach Abstand zum Ziel sortiert, bei Gleichstand in der Reihenfolge W, SW, S, SO, O, NO, N, NW.
- Nur wenn diese blockiert sind, eine **Breitensuche** in einem Fenster von 15 × 15 Feldern (`PATH_R` 7) zum erreichbaren Feld, das dem Ziel am nächsten liegt. Das findet auch die Tür aus einem Haus. Die Puffer liegen auf dem Stack (1,3 KB), nur solange gesucht wird.
- Ein **Ring der letzten 8 Felder** je Einheit (`visited`, im Spielstand) verhindert Hin-und-her-Laufen.

### 6. Pläne, Routen, Profile

- Routen, Pläne für Einheiten der Karte und Auslöser kommen aus der Szenariodatei (ADR 0014, K10.7/K10.8). Eine neue Kreatur würfelt nach ihrer Aggressivität, ob sie **Leibwache** wird, sonst nimmt sie eine passende Route.
- Der KI-Zauberer hat ein **Profil** (Werte, Siegpunktwert, Zauberprioritäten) aus dem Szenario. Eine Priorität halbiert sich, wenn er den Zauber wirkt (K10.5). Die Prioritäten stehen nicht im Spielstand; nach dem Laden gelten wieder die des Szenarios (FRAGEN F29).
- Der Zauberer zielt mit Bolt und Blitz auf die Höhe seines Ziels (D68).

### 7. Zufall und Prüfung

- Zufall nur über die Rundenzufallszahlen (`turns.rng`); die KI ist bei gleichem Startwert bitgleich auf Host und eZ80.
- **Selftests** prüfen Einzelregeln (Tests `ki1:`, `ki2:` und die älteren `m4h:`/`m4k:`).
- **Host-Simulationen:**
  - `host/duel.c` spielt Zauberer gegen Zauberer mit Originalwerten. Ergebnis: Der KI-Gegner schlägt den Standard-Zauberer meist.
  - `host/arena.c` lässt alle Kreaturen gegeneinander antreten.
  - Beide laufen mit den echten Core-Regeln.

### 8. Laufzeit

| Messung | Ergebnis |
|---|---|
| Budget einer KI-Phase (M4h) | ≤ 2000 ms |
| Hardware, 2026-10-05 (vor D67 KI) | 160 → 20 ms nach dem Plattform-Audit |
| GUI-Emulator, 2026-10-08, `loc --bench` (Level 1, ein Zauberer mit Goblin-Zauber) | 1000 ms |
| Hardware nach D67 KI | laut Nutzer „fein“ (2026-10-08), ohne Zahl |

Teuer sind die Sichtliste je Durchgang und die Breitensuche bei Hindernissen. Wird es zu langsam, sind die nächsten Schritte: die Sichtliste nur nach eigenen Bewegungen neu bauen und die Breitensuche je Ziel und Runde zwischenspeichern. Vorher messen.

## Folgen

- \+ Regeln je Datei auffindbar, die Schleife folgt K10.3 Schritt für Schritt.
- \+ Fairness: Die KI weiß nur, was sie sieht.
- \+ Balance lässt sich auf dem Host in Sekunden über Hunderte Partien prüfen.
- − Die Abweichungen vom Original stehen verstreut in `docs/FRAGEN.md`: Flieger landen zum Nahkampf (F23), neu entworfene Routen (F27), keine schlafenden Wächter (F28). Wer die KI ändert, muss sie dort nachlesen.
- **Offen:** eine Hardware-Zahl für die KI-Phase nach D67 und auf der 46×46-Karte.
