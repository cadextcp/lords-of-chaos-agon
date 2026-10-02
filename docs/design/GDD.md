# Game Design Document – Lords of Chaos (Agon)

> Status: **Entwurf v0.4** · Entscheidungen D1–D8 eingearbeitet (§14) · Stand 2026-10-02
> Leitquelle: **Amiga/Atari-ST-Fassung** (Blade Software / Mythos Games, 1990)

---

## 0. Quellen und Regel-Hierarchie

| Quelle | Datei (lokal, nicht im Repo) | Rolle |
|---|---|---|
| Amiga/ST-Beiblatt (S. 1–4) | `reference/amiga-manual-enfrde.pdf` | **Höchste Priorität.** Überschreibt das Grundregelwerk. |
| Players Manual (S. 5–40) | `reference/amiga-manual-enfrde.pdf` | Grundregelwerk (identisch mit der 8-Bit-Anleitung) |
| 8-Bit-Anleitung | `reference/LordsOfChaos.pdf` | Nur Querverweis |

**Regel:** Wo das Amiga-Beiblatt etwas sagt, gilt das Beiblatt. Sonst gilt das Players Manual.

Was keine der Quellen regelt, steht in §13 („Unbekannte Werte“). Das wird von uns entworfen und kalibriert.

**Lebende Referenz:** das Amiga-Original in WinUAE. Es liegt nur lokal und kommt nie ins Repo:
- Emulator: `C:\Program Files\WinUAE\winuae64.exe`
- Kickstart 1.3 und Spiel-ADF: `Desktop\amiga\`

Wir nutzen es, um die Unbekannten aus §13 zu beobachten und zu kalibrieren. Beobachtungen werden protokolliert in `docs/design/amiga-observations.md`, je mit Datum, Szenario, Vorgehen und Messwert.

Seitenangaben `[PM n]` beziehen sich auf die Seitenzahl im Players Manual. Die PDF-Seite ist n + 4. `[AMI n]` meint Seite n des Amiga-Beiblatts.

---

## 1. Vision und Leitplanken

1. **Classic zuerst.** Phase 1 baut die Amiga-Fassung regeltreu nach: gleiche Kreaturen, Zauber, Aktionen, Siegbedingungen und Kampagnenlogik. Erweiterungen gibt es in dieser Phase nicht.
2. **Agon-nativ.** Gebaut wird in C/eZ80 für 512 KB RAM. Die Darstellung läuft über ein 40×30-Zellenraster mit eigenen Glyphen. Hardware-Grenzen bestimmen das Design, nicht umgekehrt.
3. **Präsentation à la Caves of Qud.**
   - Wenige Symbole, aber Farbe, Licht und Animation tragen die Atmosphäre.
   - Jede Zelle hat ein Glyph, eine Vordergrund- und eine Hintergrundfarbe.
   - Effekte entstehen über Farbwechsel, Palette-Cycling und kurze Glyph-Animationen, nicht über Pixelgrafik.
4. **Chaos später, aber vorbereitet.** Die Kern-Datenstrukturen sehen Materialien, Welt-Ticks und Licht schon vor. Siehe §12.

### Feature-Stufen (Tags im ganzen Dokument)

| Tag | Bedeutung | Phase |
|---|---|---|
| **[C]** | Classic: Amiga-Regelwerk | M2–M4 → v1.0 |
| **[C+]** | Komfort ohne Regeländerung (Tooltips, Log, Pfad-Vorschau) | parallel, optional |
| **[X]** | Chaos-Erweiterung, ändert Regeln oder Welt | M5+ |

---

## 2. Spielübersicht [C]

- Rundenbasiertes Fantasy-Taktikspiel. Das Original erlaubt **1–4 Spieler**, Menschen (Hotseat) oder Computer-Zauberer. `[PM 3]`
- **Fokus dieses Projekts: Einzelspieler gegen Computer-Zauberer.**
  - Ziel ist die Kampagne mit Wizard Designer.
  - Hotseat-Multiplayer ist zurückgestellt und kommt nach v1.0. Der Core muss mehrere Zauberer pro Partie aber von Anfang an unterstützen, denn die KI-Gegner sind ebenfalls Zauberer.
- Jeder Spieler führt einen **Zauberer**, beschwört Kreaturen, sammelt Gegenstände und braut Tränke.
- **Ziel jedes Szenarios:** überleben, Schätze sammeln und durch das **Portal** nach „Limbo“ entkommen, solange es offen ist. `[PM 3, 29]`
- **Einzelspieler-Kampagne:** Szenarien in fester Reihenfolge. Siegpunkte werden zu Erfahrungspunkten, die im Wizard Designer ausgegeben werden. `[PM 28–29]`

### 2.1 Rundenablauf `[PM 5]`

```
Spielrunde n:
  1. Unabhängige Kreaturen (keinem Zauberer zugehörig) ziehen
  2. Zauberer 1 zieht mit allen eigenen Einheiten
  3. Zauberer 2 ... 4
  4. Rundenende: Regeneration (AP, Stamina, Mana), Effekte (Feuer, Blob,
     Vine, Flood, tödliche Wunden, Trank-Dauer), Portal-Check, Speichern-Angebot
```

- In **Runde 1 ist keine Bewegung erlaubt**, nur Zaubern. `[PM 7]`
- **Hidden Movement:** Gegnerzüge sind unsichtbar. **Amiga:** Im Einzelspieler sieht man feindliche Bewegungen, solange eine eigene Einheit Sichtlinie hat. `[AMI 4]`
- **Timer (Amiga, optional):** Zeitlimit pro Zug in Stufe 1–8 oder aus. Ein Balken läuft ab, danach endet der Zug automatisch. `[AMI 3]`

### 2.2 Spiel-Setup-Panel (Amiga) `[AMI 3]`

| Einstellung | Werte | Hinweis |
|---|---|---|
| Spieleranzahl | 1–4 | Manche Szenarien sind nur für 1 Spieler (z. B. Szenario 3). v1.0 ist fest auf **1 Mensch und n KI-Zauberer** ausgelegt. |
| Zufalls-Zauberer-Stufe | Zahl | Stärke zufällig generierter Zauberer; bei 1 Spieler fix |
| Spiellänge | 1–5 | Zeigt die Rundenspanne an, in der das Spiel enden kann; bei 1 Spieler fix. Das Verhältnis zu den Portal-Zeiten wird in WinUAE beobachtet (§13). |
| Timer | 1–8 / x | x = kein Timer. Für Einzelspieler kaum relevant, daher niedrige Priorität. |

### 2.3 Rahmenmenüs [C]

- **Hauptmenü:**
  - Zauberer entwerfen, laden, speichern, löschen (4 Plätze)
  - Szenario laden
  - Spielstand laden
  - Spiel starten
- **Speichern:** am Ende jeder Spielrunde.
  - Im Einzelspieler höchstens **5-mal** laden, als Anti-Cheat. `[PM 27]`
  - Amiga: 2 Spielstände und 20 Zauberer pro Disk. Auf dem Agon gibt es keine Disk-Grenze; die 5-Ladungen-Regel bleibt bestehen.

---

## 3. Welt und Karte [C]

### 3.1 Karte

- Eine rechteckige **Wrap-around-Karte**: Wer am Rand weiterläuft, kommt auf der anderen Seite heraus. `[PM 6]`
- Das Kartenfenster zeigt einen Ausschnitt und scrollt mit dem Cursor.
- **Big Map:** strategische Übersicht über etwa die halbe Welt. Symbole: Zauberer, Bodenkreatur, Flieger, Objekt. Auf dem Amiga ist sie scrollbar. `[PM 13, AMI 4]` Bei uns passt die **ganze Welt** auf einen Bildschirm: 36 Spalten bei 1 Zelle pro Kachel.
- **Kartengröße:** **36×36 Kacheln** in allen Spectrum-Szenarien (B3.1); für Amiga als Näherung angenommen, Stichprobe O2. Das ist die Classic-Referenz für eigene Karten; das Szenario-Format erlaubt andere Größen.
- **Wege** und **Wände** sind kachelbasierte Linien durch die Kachelmitte (B2.1, B3.3).

### 3.2 Ebenen pro Feld

| Ebene | Inhalt |
|---|---|
| Terrain | Bodentyp bzw. Hindernis (siehe 3.3) |
| Dach | Gebäude haben Dächer. Sie blockieren Sicht und Landung zwischen Luft und Boden. `[PM 10, 16]` |
| Boden-Einheit | höchstens 1 Kreatur, plus Reiter falls beritten |
| Luft-Einheit | höchstens 1 fliegende Kreatur |
| Objekte | Stapel von Gegenständen am Boden (Anzeige bis 6) `[PM 6]` |
| Flächeneffekt | Feuer, Gooey Blob, Tangle Vine oder Flood |

### 3.3 Terrain (aus dem Manual ableitbar)

- **Wand:** unzerstörbar, blockiert Sicht.
- **Tür:** offen, geschlossen oder verschlossen. Lässt sich einschlagen.
- **Truhe:** geschlossen oder verschlossen.
- **Fels.**
- **Bäume, hohes Gras, Magic Wood, Shadow Wood:** blockieren die Sicht am Boden. Für Flieger sind sie „verdecktes“ Terrain.
- **Wasser und Sumpf.**
- **Pentakel** im Zauberer-Haus.
- **Kessel** (Cauldron) als Objekt.

Weitere Typen ergeben sich aus den Szenarien → §13.

**Regeln dazu:**
- **Kreatur-Terrain-Affinität:** Kreaturen sind *Wood-*, *Water-* oder *Rock-Type*. Das wirkt sich vermutlich auf Bewegungskosten bzw. Passierbarkeit aus, die genaue Wirkung ist offen → §13.
- **Terrain angreifen:** Wer gegen unpassierbares Terrain läuft, kann es zerstören. Jedes Terrain hat eine **Zähigkeit**; Wände sind unzerstörbar. `[PM 18]`

### 3.4 Sicht (Line of Sight) `[PM 16–17]`

- **Sichtweite:** am Boden 9 Felder, in der Luft 11 Felder.
- **Boden → Boden:** Dazwischenliegendes Terrain blockiert je nach Typ, vor allem Wände, hohes Gras und Bäume.
- **Luft → Boden:** Terrain blockiert normalerweise nicht.
  - In verdecktem Terrain (Magic Wood, Shadow Wood, hohes Gras) sind Kreaturen nur sichtbar, wenn der Flieger direkt daneben ist.
  - Objekte sind dort gar nicht sichtbar.
- **Dächer:** Keine Sicht zwischen Luft und überdachtem Boden.
- **Unsichtbare Kreaturen** sind für alle Gegner unsichtbar. Magic Eye deckt sie für eine Runde auf.
- **Hidden Map (Amiga):** Jeder Spieler hat eine eigene erkundete Karte.
  - Sie wird beim Erkunden ergänzt.
  - Sie kann **veralten**: Ein Gebiet, das man zuletzt intakt gesehen hat, kann inzwischen abgebrannt sein. `[AMI 4]`

---

## 4. Kreaturen [C]

### 4.1 Attribute (Creature Table, `[PM 34]`)

| Attribut | Bedeutung |
|---|---|
| Action Points (Ground / Flying) | AP pro Runde am Boden bzw. im Flug. 0 bei Flying heißt: kann nicht fliegen |
| Stamina | Ausdauer. Bewegung und Kampf verbrauchen sie, Teil-Regeneration pro Runde. Unter einer Schwelle ist die Kreatur **erschöpft** und bekommt nur halbe AP. `[PM 12]` |
| Constitution | Lebenspunkte. Bei 0 tot; unter 50 % leiden AP, Kampf und Verteidigung. |
| Combat / Defence | Angriff und Verteidigung, modifiziert durch Waffen und Tränke |
| Magic Resistance | Widerstand gegen Subversion, Curse und Magic Attack |
| Carry Limit | Zusatzgewicht, das getragen werden kann |
| Potion Consumption | Wie schnell Tränke „verbraucht“ sind. Groß heißt kurze Wirkung. |
| Victory Points | Punkte für das Töten dieser Kreatur |
| Flags | Mount, Ride Mounts, Undead, Use Weapons, Use Options (Türen und Truhen bedienen), Wood Type, Water Type, Rock Type |

**Datenhaltung:** Die Zahlenwerte für alle 25 Kreaturen werden in M2 aus `[PM 34]` nach `data/creatures.csv` übertragen. Das GDD definiert nur das Schema.

### 4.2 Die 25 Kreaturen

| Gruppe | Kreaturen |
|---|---|
| Drachen (nur per Drachentrank) | Gold Dragon, Green Dragon, Red Dragon |
| Humanoide mit Händen | Pixie, Dwarf, Goblin, Troll, Giant, Centaur |
| Reittiere | Unicorn, Pegasus, Gryphon, Elephant |
| Tiere | Gorilla, Lion, Bear, Crocodile, Giant Bat, Harpy, Giant Spider |
| Untote | Zombie, Ghost, Vampire, Spectre, Demon |

**Spezialregeln:**
- **Fliegen:** `[PM 10]`
  - Nur unter freiem Himmel.
  - Gleiche AP-Kosten unabhängig vom Terrain; kann Unpassierbares überfliegen.
  - Kann nur von Fliegern, Wurfwaffen, Bögen und Zaubern angegriffen werden.
  - Landen geht nur auf geeignetem, freiem Feld.
- **Reiten:** Unicorn, Pegasus, Gryphon und Elephant können Zauberer, Pixies, Dwarves, Goblins und Trolls tragen. `[PM 10]`
  - Nach RIDE wird das Reittier zur gewählten Einheit. RIDER wählt den Reiter.
  - Ein Reiter kann alles außer PICK UP.
- **Untote** können nur durch Untote, magische Waffen und Zauber verletzt werden. `[PM 18]`
- **Zauberer** sind eine eigene Kreaturklasse mit Hand, Mana und Zauberliste. Die Werte kommen aus dem Wizard Designer.

---

## 5. Aktionen [C]

Fast jede Aktion kostet **AP**. Ein Menüpunkt erscheint nur, wenn die Einheit die Aktion kann und genug AP hat. `[PM 13]`

| Modus | Aktionen | Quelle |
|---|---|---|
| Cursor | SELECT-A, SELECT-G, NEXT, INFORM, BIG MAP, END TURN, CANCEL | `[PM 12–13]` |
| Gewählte Einheit | Bewegen (ins Nachbarfeld, 8 Richtungen), END MOVE, CENTRE, BIG MAP | `[PM 8, 13]` |
| Reiten und Fliegen | RIDE, RIDER, FLY, LAND | `[PM 10, 14]` |
| Magie | CAST-A, CAST-G | `[PM 14]` |
| Inventar | PICK UP, DROP, CHANGE (Objekt in Benutzung), EAT, DRINK, FILL, READ | `[PM 14–16]` |
| Welt | USE: Tür öffnen, schließen oder aufschließen (mit Schlüssel), Truhe öffnen oder aufschließen, Spezialobjekte. **Amiga:** Ziel höchstens 1 Feld entfernt. | `[PM 15, AMI 2]` |
| Fernkampf | THROW-A/G (Reichweite nach Stärke und Gewicht), FIRE-A/G (Bogen, Flammenatem) | `[PM 15–16]` |

**Weitere Regeln:**
- **Objekt in Benutzung:** Eine Kreatur kann mehrere Objekte tragen, aber nur eines ist „in use“, und zwar das zuletzt aufgehobene. DROP, THROW und Waffen-Boni beziehen sich darauf. Schilde wirken immer. `[PM 9, 17]`
- **Bewegung auf befreundete Einheit:** Danach ist nur END MOVE möglich. Die Einheit wird dann auf das letzte freie Feld zurückgesetzt. `[PM 13]`

### 5.1 Bedienung auf dem Agon: Tastatur, angelehnt an Caves of Qud

**Entscheidung:** Wir bilden **nicht** die Original-Bedienung nach. Die Amiga-Fassung arbeitet mit Maus, Icon-Leiste und Cursor-/Select-Modi. `[AMI 2–3]` Stattdessen steuern wir per Tastatur im Stil eines Roguelikes wie Caves of Qud.

Die **Regeln** bleiben Classic. Nur der Weg, eine Aktion auszulösen, ändert sich. Maussteuerung ist nicht Teil von v1.0.

**Grundprinzipien:**
1. **Aktive Einheit statt Cursor-Modus.** Im eigenen Zug ist immer genau eine Einheit aktiv, wie die Spielfigur in Qud.
   - Bewegungstasten bewegen sie direkt.
   - `Tab` und `Shift+Tab` wechseln zur nächsten bzw. vorigen eigenen Einheit mit verbleibenden AP. `Leertaste` beendet die Einheit und springt weiter. Das ersetzt SELECT-A/G, NEXT und END MOVE.
   - Die Kamera folgt der aktiven Einheit.
2. **Bump-Interaktion.** Läuft man in einen Gegner, ist das ein Angriff (wie im Original). Läuft man gegen eine geschlossene Tür und hat Hände, öffnet man sie. Gegen unpassierbares Terrain ist es ein Angriff auf das Terrain (§3.3). Es gelten dieselben AP-Kosten wie bei der expliziten Aktion.
3. **Ein Verb, eine Taste.** Die Tasten sind mnemonisch.
   - Listen (Zauber, Inventar, Objekte am Boden) werden mit `a`–`z` gewählt.
4. **Kontextmenü als Sicherheitsnetz.** `Enter` zeigt alle Aktionen, die die aktive Einheit gerade ausführen kann, mit Taste und AP-Kosten. Neue Spieler müssen sich keine Tasten merken.
5. **Look statt Inform.** Ein freier Untersuchungscursor zeigt Terrain, Einheiten (Werte wie das Info-Panel) und Objekte. So bekommt man Infos im Qud-Stil, ohne die Aktion zu wechseln.
6. **Zielmodus**, einheitlich für Zaubern, Werfen und Schießen:
   - Bewegungstasten bewegen den Zielcursor. `Tab` springt zum nächsten sichtbaren Ziel. `<` und `>` wechseln zwischen Luft- und Bodenziel.
   - `Enter` oder ein erneuter Druck der Aktionstaste bestätigt. `Esc` bricht ohne Kosten ab.
   - Farbcode wie auf dem Amiga: **gelb** Boden, **blau** Luft, **rot** außer Reichweite. `[AMI 3]`

### 5.2 Zieltastatur: Cherry G84-4100 (Kompakt, deutsches ISO-Layout, bestätigt)

Die Referenz-Hardware ist ein Agon Light mit **Cherry G84-4100**. Daraus folgen diese Randbedingungen:

| Eigenschaft G84-4100 | Konsequenz |
|---|---|
| **Kein Ziffernblock.** Ein eingebetteter Block ist nur über `Fn` erreichbar. | Bewegung darf **nicht** vom Numpad abhängen. Fn-Numpad ist höchstens Alternative (Spike prüft, welche Codes ankommen). |
| Pfeiltasten als umgekehrtes T unten rechts, `Einfg`/`Entf` links daneben | Die rechte Hand liegt auf den Pfeilen und bewegt. |
| QWERTZ, `<`/`>` als eigene Taste links neben `Y` | `<`/`>` bequem mit der linken Hand. Kein `y`/`z` in der Belegung (Layout-Verwechslungsgefahr). |
| `?`, `_`, `/` brauchen `Shift` bzw. `AltGr` | Keine häufigen Aktionen auf Sonderzeichen. Hilfe liegt auf `F1`. |
| F1–F12 vorhanden | Seltene Meta-Funktionen auf F-Tasten |

**Ergonomie-Prinzip „rechte Hand bewegt, linke Hand handelt“:**
- Die rechte Hand bleibt auf den Pfeilen.
- Alle **häufigen** Aktionen liegen in der linken Tastaturhälfte (`Q W E R T / A S D F G / < Y X C V B`, `Tab`, `Shift`, `Leertaste`, `1–5`, `Esc`, `F1–F4`).
- Seltene Funktionen (Inventar, Karte, Log) dürfen rechts liegen.

**Diagonalen ohne Ziffernblock.** Lords of Chaos bewegt in 8 Richtungen. Diagonalen sind taktisch wichtig.
1. **Pfeil-Akkord (primär):** Zwei benachbarte Pfeile kurz nacheinander gedrückt ergeben die Diagonale. Beispiel: `↑` + `→` innerhalb von ~80 ms ist Nordost.
   - Technisch: Auswertung der Key-down/up-Events aus `kbuf`.
   - Ein einzelner Pfeil löst erst nach dem Akkord-Fenster bzw. beim Loslassen aus.
   - Das Fenster ist einstellbar.
2. **Alternative:** `Pos1` ist NW, `Bild↑` NO, `Ende` SW, `Bild↓` SO. Das ist das klassische Numpad-Prinzip ohne NumLock.
3. **Alternative (falls die Codes beim Agon ankommen):** eingebetteter Fn-Ziffernblock 1–9.
4. **[C+] Travel:** Ziel im Look-Modus wählen, die Einheit läuft den Pfad mit AP-Vorschau. Das reduziert Einzelschritte drastisch.

**Tastenbelegung (Entwurf v2, G84-optimiert, Mnemonik englisch wie die Original-Menüs):**

| Taste | Hand | Aktion | Original-Entsprechung |
|---|---|---|---|
| `←` `↑` `→` `↓`, Akkorde für Diagonalen | R | Bewegen in 8 Richtungen, Bump (Angriff, Tür öffnen) | Bewegen, Angriff |
| `Pos1` `Bild↑` `Ende` `Bild↓` | R | Diagonal NW / NO / SW / SO (Alternative) | – |
| `Leertaste` | L | Einheit fertig, weiter zur nächsten (Rest-AP bleiben für Rückschläge) | END MOVE + NEXT |
| `Tab` / `Shift+Tab` | L | Nächste / vorige eigene Einheit | NEXT, SELECT |
| `Enter` | R | Kontextmenü aller möglichen Aktionen; im Zielmodus: bestätigen | Icon-Menü |
| `c` | L | **C**ast: Zauber wirken (Liste, dann Zielmodus) | CAST-A/G |
| `1`–`5` | L | Schnellzauber-Slots [C+] (Qud-Hotbar-Prinzip) | – |
| `f` | L | **F**ire: Fernwaffe bzw. Flammenatem | FIRE-A/G |
| `t` | L | **T**hrow: Objekt in Benutzung werfen | THROW-A/G |
| `g` | L | **G**et: Aufheben (Liste) | PICK UP |
| `d` | L | **D**rop: Objekt in Benutzung fallen lassen | DROP |
| `w` | L | **W**ield: Objekt in Benutzung wechseln | CHANGE |
| `e` | L | **E**at | EAT |
| `q` | L | **Q**uaff: Trinken (Phiole oder Kessel) | DRINK |
| `v` | L | **V**ial: Phiole am Kessel füllen | FILL |
| `r` | L | **R**ead: Schriftrolle lesen | READ |
| `a` + Richtung | L+R | **A**pply: Tür oder Truhe öffnen, schließen, aufschließen, Spezialobjekt (Ziel höchstens 1 Feld) | USE |
| `b` | L | **B**oard: Reittier besteigen bzw. absteigen. Der Reiter erscheint in der `Tab`-Reihenfolge. | RIDE / RIDER |
| `<` / `>` (`Shift+<`) | L | Aufsteigen (fliegen) / Landen. Im Zielmodus: Luft- bzw. Bodenebene. | FLY / LAND, CAST-A/G |
| `x` | L | E**x**amine: Look-Modus | INFORM |
| `i` | R | Inventar (Objekt wählen, dann Verb) | – |
| `m` | R | Strategische Karte | BIG MAP |
| `l` | R | Nachrichten-**L**og | – |
| `Shift+E` | L | Zug beenden (mit Bestätigung, wie im Original) | END TURN |
| `Esc` | L | Abbrechen bzw. Spielmenü (Speichern, Laden, Optionen, Beenden) | CANCEL |
| `F1` | L | Hilfe mit Tastenübersicht | – |

**Zielmodus mit der G84:**
- Pfeile und Akkorde bewegen den Zielcursor.
- `Tab` springt zum nächsten sichtbaren Ziel.
- `<` und `>` wechseln die Ebene.
- `Enter`, `Leertaste` oder die Aktionstaste erneut (`c`/`f`/`t`) bestätigen. `Esc` bricht ab.

Alles liegt in einer zentralen Tabelle `src/agon/keymap.c`, damit die Belegung später einstellbar ist.

**Zu verifizieren im M1-Eingabe-Spike** (Testprogramm loggt jedes `kbuf`-Event: ASCII, VKey, Modifier, down/up):
- Gibt die G84 zwei Pfeile gleichzeitig ohne Ghosting aus, und funktioniert der Akkord zuverlässig?
- Welche Codes liefern `Pos1`/`Ende`/`Bild↑`/`Bild↓`, die `<>`-Taste und der Fn-Ziffernblock am Agon?
- Gibt es Konflikte mit dem MOS-Tastaturlayout (`SET KEYBOARD`, deutsch)?
- Auto-Repeat: Gehalten heißt nur ein Schritt pro Wiederholintervall, damit nicht versehentlich AP verbraucht werden.

**[C+] Komfort:**
- Travel bzw. Pfad-Vorschau mit AP-Kosten.
- Treffer- und Erfolgschance im Look- und Zielmodus.
- Letzte Aktion wiederholen.

### 5.3 Aktionskosten und Ausdauer (eigenes Design)

**Entscheidung D7:** Wir kopieren keine exakten Originalwerte. Die Kosten sind eigene, sinnvolle Startwerte, die per Host-Simulation balanciert werden. Messungen am Original dienen nur als Anker:
- Schritt auf Boden kostet 4 bzw. 6 AP (B4).
- Etwa 4 Zauber pro Runde `[PM 7]`.
- Kreaturen haben 24–62 AP `[PM 34]`.

Die Daten stehen in `data/costs.csv` (Terrain) und `data/actions.csv` (Aktionen).

**Bewegung pro Feld (orthogonal / diagonal, AP):**

| Untergrund | AP | Besonderheit |
|---|---|---|
| Weg | 3 / 5 | schnellster Untergrund; Wege lohnen sich |
| Boden, Gras, offene Tür | 4 / 6 | Referenz (gemessen) |
| Hohes Gras | 6 / 9 | blockiert Sicht am Boden |
| Wald | 8 / 12 | blockiert Sicht |
| Magic Wood, Shadow Wood | 10 / 15 | blockiert Sicht; verdeckt für Flieger |
| Geröll | 8 / 12 | |
| Sumpf | 10 / 15 | |
| Wasser | 12 / 18 | Ertrinken-Probe für Nicht-Wasserwesen |
| Wand, Fels, geschlossene Tür | – | unpassierbar; Hineinlaufen ist ein Angriff auf das Terrain |
| Fliegen | 4 / 6 | immer, unabhängig vom Boden `[PM 10]` |

- **Diagonale** kostet das Orthogonale × 1,5, aufgerundet (Gollop-Regel, B4.2).
- **Stamina pro Schritt** ist die Hälfte der AP, aufgerundet. Auf Boden also 2 bzw. 3, passend zu B4.
- **Terrain-Affinität:** Wood-, Water- bzw. Rock-Typen `[PM 34]` zahlen im passenden Terrain nur die Boden-Kosten. Water-Typen ertrinken nicht.

**Aktionen:**

| Aktion | AP | Stamina |
|---|---|---|
| Zauber wirken | 10 | – |
| Nahkampf (auch Terrain angreifen) | 10 | 4 |
| Rückschlag (automatisch im Gegnerzug) | 6 | 3 |
| Fernwaffe bzw. Flammenatem | 12 | 2 |
| Werfen | 10 | 3 |
| Aufheben (pro Objekt) | 6 | 1 |
| Fallen lassen / Wechseln | 2 / 4 | – |
| Essen / Trinken / Füllen / Lesen | 6 / 4 / 6 / 8 | – |
| Tür öffnen bzw. schließen / Aufschließen / Truhe öffnen | 6 / 8 / 8 | 1 |
| Aufsitzen / Absitzen | 6 / 4 | 1 |
| Aufsteigen / Landen | 4 / 4 | 2 / 1 |

**Was das in einer Runde bedeutet (Zauberer mit Annahme 40 AP):**
- 4 Zauber, oder
- 10 Schritte auf Boden bzw. 13 auf Wegen, oder
- Tür öffnen plus 8 Schritte, oder
- 4 Nahkampfangriffe.

Ein langsamer Zombie (24 AP) schafft 2 Angriffe; ein Löwe (54 AP) läuft 13 Felder.

**Zustände:**
- **Erschöpfung:** Stamina unter 25 % des Maximums. Dann gibt es in der nächsten Runde nur die halben AP `[PM 12]`.
- **Stamina-Regeneration:** 25 % des Maximums pro Runde (×3 mit Speed Potion `[PM 20]`).
- **Constitution unter 50 %:** AP, Combat und Defence × ¾. Das Manual sagt nur „affected“; der Faktor ist unser Design.
- **Rest-AP** verfallen am Rundenende, ermöglichen aber vorher Rückschläge im Gegnerzug. Wer AP spart, verteidigt sich besser `[PM 18]`.
- **Teleport** setzt die AP auf 0 `[PM 23]`. **Speed Potion** verdoppelt sie `[PM 20]`.

---

## 6. Kampf [C] `[PM 17–18]`

- **Nahkampf:** Man bewegt sich ins Feld des Gegners. Combat des Angreifers wird gegen Defence des Verteidigers gerechnet, mit Zufallsanteil.
- **Gebunden (engaged):** Wer neben einem Gegner steht, kann sich in diesem Zug nicht mehr wegbewegen, außer alle angrenzenden Gegner sind tot. Im nächsten Zug ist Bewegung wieder möglich.
- **Rückschlag:** Ein angegriffenes Ziel schlägt automatisch zurück, wenn es noch AP **und** Stamina hat.
- **Tödliche Wunde:** Ein einzelner Treffer über 25 % der Constitution verursacht sie.
  - Danach verliert die Kreatur jede Runde Constitution, bis sie stirbt oder einen Heiltrank trinkt.
  - Äpfel heilen Constitution, aber keine tödliche Wunde.
- **Untote:** siehe §4.
- **Angriff von befreundetem Feld aus** ist verboten („attack not allowed“), außer für Reiter.

### 6.1 Waffen `[PM 35]`

**Werte je Waffe:** Gewicht, Combat, Defence, Thrown Combat, Ranged Combat. Sie werden in M2 nach `data/weapons.csv` übernommen.

**Die Waffen:** Sword, Knife, Shield, Bow, Spear, Club, Axe, Ninja Star, Slayer, Magic Slayer.

**Magische Waffen** (über Enchant): doppelte Werte; Ausnahme ist der Slayer, der einen eigenen Magic-Slayer-Eintrag hat. Sie verletzen Untote.

---

## 7. Zauberer und Magie [C]

### 7.1 Mana und Zauberstufen `[PM 19]`

- **Mana-Regeneration:** 4 % des Startwerts pro Runde. Magische Pilze, magische Äpfel u. a. stellen Mana wieder her.
- **Zauberstufe:** Jeder Zauber hat eine Stufe von 1 bis 8.
  - Jedes Wirken senkt die Stufe um 1. Bei 0 ist der Zauber für dieses Spiel verbraucht.
  - Entworfene Zauberer bekommen im nächsten Szenario alle Stufen zurück.
- **Kosten:** steigen **linear** mit der Stufe: `Mana = Basis + Stufe × Schritt` (B3.5). Die Werte für alle 45 Zauber stehen in `data/spells.csv` (Spectrum-Tabelle; Amiga-Stichprobe O4). Beispiele: Giant Bat 5 + 2 pro Stufe, Magic Bolt 6 + 3, Gold Dragon 47 + 23.
- **Abbrechen ohne Manaverlust:** Zielcursor auf den eigenen Zauberer setzen. Ausnahme: Enchant.

### 7.2 Die 45 Zauber

**Beschwörungen (25):** eine pro Kreatur aus §4.2. `[PM 19]`
- **Anzahl** der Kreaturen = Stufe des Zaubers. Sie erscheinen auf freien Nachbarfeldern; ist kein Platz, verfällt das Mana.
- Nur mit **CAST-G**, nicht aus der Luft.
- **Drachen** brauchen einen Kessel mit **Drachenkraut** unter dem Zauberer. Trank und Drache entstehen gleichzeitig. `[PM 21]`

**Trankzauber (7), Amiga-Fassung:** `[PM 19–20, AMI 3]`

| Trank | Zutat | Wirkung |
|---|---|---|
| Strength | Mistletoe | +Combat |
| Protection | Clover | +Defence |
| Invisibility | Crystal | unsichtbar für Gegner |
| Speed | Sulph | AP ×2, Stamina-Regeneration ×3 |
| Flying | Fairywing | kann fliegen |
| Healing | Apple | heilt Constitution und Stamina sowie tödliche Wunden (sofort) |
| **Bomb (nur Amiga)** | **Nitro** | Geworfene Phiolen explodieren |

**Amiga-Änderungen bei Tränken:**
- **Kein Super Potion.**
- **Mehrere Tränke wirken gleichzeitig.** In der 8-Bit-Fassung hob ein neuer Trank den alten auf.

**Brauen:**
1. Leeren Kessel auf ein Feld stellen und die Zutat auf dasselbe Feld legen.
2. Den Zauberer auf dieses Feld stellen.
3. Den Trankzauber mit CAST-G wirken.

**Ergebnis und Benutzung:**
- Es entstehen **Stufe + 3 Schlucke**.
- Mit DRINK trinkt man direkt vom Kessel. FILL füllt eine leere Phiole, die man danach trinkt oder wirft.
- **Wirkdauer:** hängt von der Trankstufe und der Potion Consumption der Kreatur ab.

**Sonstige Zauber (13):** `[PM 21–24]`

| Zauber | Kurzregel |
|---|---|
| Magic Fire | Ziel muss brennbar sein, keine Sichtlinie nötig. Breitet sich pro Runde aus oder erlischt. Zerstört Objekte. Schadet nur **feindlichen** Einheiten (auch Untoten), unabhängig von deren Defence. Neues Feuer schwächt alle eigenen bestehenden Feuer. |
| Gooey Blob | Wie Feuer, aber anderes Terrain. Weniger zerstörerisch, zäher. Schadet Untoten. |
| Tangle Vine | Fläche je nach Stufe, nur auf anfälligem Terrain. Gefangene müssen sich freikämpfen oder werden verwundet. |
| Flood | Fläche, anderes Terrain. Löscht Feuer. Wer ins Wasser geht, kann ertrinken, außer Water Type. |
| Enchant | Waffen auf dem Zielfeld (getragen oder am Boden) werden magisch: doppelte Werte, wirkt gegen Untote. Zeitlich begrenzt. Darf auf den eigenen Zauberer zielen. |
| Subversion | Feindliche Kreatur wechselt die Seite. Chance hängt von Stufe und Magic Resistance ab. Reiter und Reittier nur gemeinsam. Nicht auf Zauberer oder Reittiere mit Zauberer. |
| Curse | Verursacht tödliche Wunden. Bessere Chance als Subversion oder Magic Attack. |
| Magic Attack | Kann mehrere Kreaturen desselben Typs im Umkreis vernichten, **auch eigene**. |
| Magic Bolt | Physischer Schaden; Defence zählt. |
| Magic Lightning | Wie Bolt, zusätzlich auf die 8 Nachbarfelder. Zerstört Terrain am Boden. Zielfeld darf nicht massiv sein. |
| Teleport | Ungenau auf große Distanz. Danach 0 AP. Scheitert, wenn das Ziel massiv ist. |
| Magic Eye | Sicht von einem Zielpunkt aus (Luft oder Boden). Deckt Unsichtbare für eine Runde auf. |
| Magic Shield | Defence des Zauberers steigt, Höhe und Dauer nach Stufe. |

### 7.3 Wizard Designer `[PM 24–28, AMI 2]`

- **Name** (Buchstaben und Leerzeichen).
- **Charakter:** Attribute mit XP erhöhen.
  - Jedes Attribut hat Kosten pro Punkt und eine Obergrenze.
  - Unter den Startwert kann man nicht senken.
- **Zauber:** Stufen mit XP erhöhen, bis maximal 8.
- **Zufalls-Zauberer:** erhalten zufällige Zauber. Ihre Stärke hängt von der Setup-Einstellung „Zufalls-Zauberer-Stufe“ ab.
- **Werte:** Startwerte, Kosten und Obergrenzen der Zauberer-Attribute stehen nicht im Manual → §13.

---

## 8. Objekte [C]

| Kategorie | Beispiele (Manual) | Regeln |
|---|---|---|
| Waffen und Schild | siehe §6.1 | Hände nötig (Use Weapons) |
| Nahrung | Apfel, Pilz, magischer Apfel, magischer Pilz | EAT: Constitution bzw. Mana |
| Zutaten | Mistletoe, Clover, Crystal, Sulph, Fairywing, Apple, Nitro, Dragon Herb | Brauen (§7.2) |
| Behälter | Kessel (leer oder voll), Phiole (leer oder voll) | FILL, DRINK, THROW |
| Schlüssel | Door Key, Chest Key | Verschwinden nach Gebrauch |
| Schriftrollen | mit Hinweisen | READ |
| Spezialobjekte | szenariospezifisch | USE |
| Schätze | siehe §9 | Siegpunkte nur beim Durchschreiten des Portals |

- Jedes Objekt hat ein **Gewicht**; es zählt gegen das Carry Limit.
- **Feuer** zerstört viele Objekte.

---

## 9. Sieg, Punkte, Kampagne [C]

**Spielende:** wenn alle Zauberer entweder durchs Portal gegangen oder tot sind. `[PM 29]`

**Siegpunkte (VP) gibt es für:**
- Töten von Kreaturen und Zauberern
- Schätze, die man beim Durchschreiten des Portals trägt
- Entkommen

Wer nicht entkommt oder stirbt, bekommt **0 VP**.

**Amiga-Regeln für Kills:** `[AMI 4]`
- Doppelte VP, wenn der **Zauberer selbst** tötet, aber nicht mit Fernwaffen.
- Kills durch Feuer, Blob, Flood und Tangle Vine zählen, aber nicht doppelt.

**Kampagne:**
- **Umrechnung VP zu XP:** **1:1** auf dem Amiga (8-Bit: 4:1).
- **Stufe:** Ein erfolgreiches Szenario hebt den Zauberer um 1 Stufe.
- **Startbedingung:** Szenario n setzt einen Zauberer der Stufe n voraus.
- **Wiederholen:** Ältere Szenarien dürfen erneut gespielt werden, um mehr VP zu sammeln.

### 9.1 Szenarien (Amiga: neu gestaltet, „basierend auf den 8-Bit-Versionen“) `[AMI 4, PM 30–31]`

| # | Name | Spieler | Portal erscheint / bleibt (8-Bit) | Schätze (Amiga-VP) |
|---|---|---|---|---|
| 1 | The Many Coloured Land | 1–4 | Runde 12–15 / 12 Runden | Rune Stone 6, Wand 8, Emerald 10, Ruby 20, Diamond 30, Gold 40 |
| 2 | Slayer's Dungeon | 1–4 | 20–24 / 15 | Emerald 10, Ruby 20, Diamond 30, Gold 40, Slayer 60 |
| 3 | Ragaril's Domain | **nur 1** | 44–51 / 10 | Emerald 10, Ruby 20, Diamond 30, Gold 40, Ragaril's Jewel 50 |

- Auf dem Amiga bestimmt die „Spiellänge“ (1–5) die Rundenspanne für das Spielende. Wie sie mit den Portal-Zeiten zusammenspielt, ist unbekannt und wird in WinUAE beobachtet (§13).
- **Expansion Kit One** (Islands of Iris, Tombs of the Undead) folgt als spätere Inhaltserweiterung nach v1.0.
- **Karten (entschieden):** **eigene, neu entworfene Karten** im Geist der Originale.
  - Übernommen werden Thema, Szenario-Beschreibung, Gegnerauswahl, Schätze, Rätselart und Portal-Timing.
  - Originalkarten werden **nicht** nachgebaut.
  - Format: Szenario-Dateien in `data/scenarios/` mit Karte, Platzierungen und Parametern, editierbar als Text.

---

## 10. Computer-Gegner (KI) [C]

- **Computer-Zauberer** (z. B. „Torquemada“) haben alle Möglichkeiten eines Spielers. `[PM 8]`
- **Unabhängige Kreaturen** ziehen zu Beginn jeder Runde.
- Das Original-Verhalten ist nicht dokumentiert. Wir entwerfen **Verhaltensprofile:**

| Profil | Verhalten |
|---|---|
| Wächter | bleibt im Gebiet, greift Eindringlinge an |
| Jäger | sucht das nächste sichtbare Ziel |
| Zauberer-KI | Mana-Haushalt, beschwört, sammelt, flieht zum Portal |

- **KI respektiert Hidden Movement:** Sie kennt nur, was ihre Einheiten sehen. Sie schummelt nicht.
- **Rechenbudget:** höchstens etwa 2 Sekunden pro KI-Zug auf dem Agon (Messung ab M3).

---

## 11. Präsentation auf dem Agon [C]

### 11.1 Bildschirmaufteilung (40×30 Zellen, je 8×8 Pixel)

**Entschieden (D8):** **MODE 8** (320×240, 64 Farben), **1 Zeichen = 1 Feld (8×8 Pixel)**, Kartenfenster **27×24 Felder**.

- **Begründung:** Der volle Sichtradius (19×19 am Boden, 23×23 in der Luft) passt komplett hinein. Das ist schnellstes Rendering (1 Zeichen pro Feld, Dirty-Cells) und entspricht der Roguelike- bzw. Qud-Ästhetik.
- **Abgrenzung:** Das 20×20-Raster aus dem alten GDD (Versuch 1) und die 7×7 großen Kacheln des Amiga werden **nicht** übernommen.
- **Welt gegen Fenster:** Die Welt ist 36×36 und wickelt sich um. Das Fenster zentriert auf die aktive Einheit bzw. den Cursor und scrollt mit Wrap-around.
- **Spike M1 prüft nur noch:** Lesbarkeit der 8×8-Glyphen am echten Monitor, Redraw-Zeit und die genaue Panel-Breite. Am Layout selbst ändert er nichts.


```
 0                         26 27                    39
 +---------------------------+------------------------+ 0
 | KARTENFENSTER 27x24       | INFO-PANEL 13x24       |
 |  (Glyphen, scrollt)       |  Name, Icons (Undead,  |
 |                           |  Fly, Mount, Wound,    |
 |                           |  Invisible), Balken:   |
 |                           |  AP/STA/CON/COM/DEF/   |
 |                           |  MANA,                 |
 |                           |  Objekte im Feld,      |
 |                           |  Kontextmenü / Listen  |
 +---------------------------+------------------------+ 24
 | NACHRICHTEN-LOG (3 Zeilen, `l` = ganzer Log)       |
 | Runde · aktive Einheit · AP · Mana · Tastenhinweis |
 +----------------------------------------------------+ 30
```

- **Info-Panel** wie im Original: 5 Status-Icons `[PM 11–12]`, dazu **6 Balken** wie auf dem Amiga (Beobachtung B2.4).
  - Die Balken sind AP, Stamina, Constitution, Combat, Defence und Mana.
  - Jeder Balken hat ein Icon-Glyph: Stiefel, Blitz, Herz, Schwert, Schild, Stern, in den Amiga-Farben grün, gelb, rot, grau, blau, pink.
  - Mana erscheint nur bei Zauberern.
  - Das Panel zeigt immer die aktive Einheit, im Look-Modus die untersuchte.
- **Balken** werden als Glyph-Blöcke mit Farbverlauf gezeichnet. Weil die 40×30-Auflösung wenig Platz lässt, liegen sie senkrecht nebeneinander wie auf dem Amiga oder waagerecht mit Zahlenwert; das entscheidet der Layout-Spike.
- **Textzeile:** benennt das Element unter dem Look- bzw. Ziel-Cursor, wie die Amiga-Zeile unter der Karte (B2.3).
- **Listen und Menüs** erscheinen im Panel mit Buchstaben-Kürzeln `a)`–`z)` im Qud-Stil.
- **Statuszeile:** zeigt kontextabhängig die wichtigsten Tasten, z. B. `c Zaubern · g Aufheben · E Zugende`.

### 11.2 Visuelle Sprache

- **Ein Glyph pro Kreaturtyp.** Die Farbe zeigt den Besitzer (4 Zauberer-Farben plus Neutral). Der Hintergrund zeigt den Zustand: Flieger mit Himmelstönung, Unsichtbare gedimmt (nur für den Besitzer sichtbar).
- **Terrain:** Glyph plus Farbe. Verdecktes Terrain wird dunkler dargestellt.
- **Erkundet, aber nicht in Sicht:** entsättigt bzw. grau. Das ist die Hidden Map.
- **Aktive Einheit:** blinkender bzw. pulsierender Hintergrund. Grün heißt am Boden, blau heißt in der Luft.
- **Ziel- und Look-Cursor:** Rahmen-Glyph. Gelb für ein Bodenziel, blau für ein Luftziel, rot außer Reichweite (Amiga-Farbcode); weiß im Look-Modus.
- **[C]-Animationen** (minimal):
  - Treffer-Blitz
  - Zauber-Projektil als Glyph-Spur
  - Feuer-Flackern per Palette-Cycling
  - Wasser-Schimmern
  - Portal-Pulsieren

### 11.3 Kachel-Komposition: mehrere Elemente, eine Zelle

Das Amiga-Original zeigt **7×7 große Kacheln** und überlagert pro Kachel mehrere Elemente, z. B. Boden, Tür und Weg. (Beobachtung B1, `amiga-observations.md`) Wir zeigen etwa 27×24 Zellen mit je **einem Glyph plus Vorder- und Hintergrundfarbe**.

**Bewusste Abweichung:** Der größere Ausschnitt zeigt den vollen Sichtradius (9 Felder am Boden, 11 in der Luft) ohne Scrollen. Das ist Qud-typisch und taktisch besser lesbar.

[C+] Optional nach v1.0: ein Zoom-Modus mit 16×16-Glyphen (etwa 13×12 Felder) für mehr Detail. Er ist nicht Teil der Basis (D8).

**Komposition pro Zelle.** Die drei Kanäle werden getrennt belegt:

| Kanal | Quelle (höchste Priorität zuerst) |
|---|---|
| **Glyph** | Cursor-Rahmen → Luft-Einheit → Boden-Einheit → Flächeneffekt (Feuer, Blob, Vine, Flood) → oberstes Objekt → Feature (Tür, Truhe, Kessel, Möbel, Baum, Wand) → Boden-Detail (Gras, Weg) |
| **Vordergrundfarbe** | Farbe des gewählten Glyph-Elements: Besitzerfarbe bei Einheiten, Materialfarbe bei Objekten und Terrain |
| **Hintergrundfarbe** | Untergrund: z. B. blauer Steinboden innen, dunkelgrün Wiese, braun Weg, dunkelblau Wasser. Moduliert durch Zustand: Sicht, Erinnerung, Licht, aktive Einheit, Brand-Glühen. |

So bleiben drei Informationen gleichzeitig lesbar: **wer** steht dort (Glyph), **wem** gehört es (Farbe) und **worauf** steht es (Hintergrund).

**Verdeckte Information:**
- Wenn mehr als ein Element um den Glyph-Kanal konkurriert, zeigt ein Marker-Pixel bzw. eine Glyph-Variante „hier liegt mehr“, z. B. Objekte unter einer Einheit.
- Details liefert der Look-Modus (`x`).
- [C+] Optional: Glyph-Wechsel im Takt (Einheit ↔ Objekt), Qud-ähnlich.

**Wände belegen ganze Kacheln** (geklärt, Beobachtung B2.1). Im Original verläuft die Wandlinie durch die Kachelmitte und verbindet sich mit den Nachbarwänden.

Wir übernehmen das 1:1 mit **Auto-Tiling-Glyphen**: zentrierte Linien, Ecken, T-Stücke und Kreuze im Stil der Box-Drawing-Zeichen, insgesamt 16 Varianten nach den 4 Nachbarn. **Türen** sitzen als eigener Glyph auf der Wandkachel in der Linie.

Das Kartenmodell bleibt rein kachelbasiert; es gibt keine Kanten-Daten.

### 11.4 Sound [C]

- Einfache Effekte über den Agon-Soundkanal: Schritt, Treffer, Zauber, Portal.
- Musik ist optional [X].

---

## 12. Chaos-Erweiterungen [X] (nach v1.0)

Ziel ist eine lebendigere, chaotischere Welt nach dem Vorbild von Caves of Qud und Tales of Maj'Eyal. Die Classic-Regeln bleiben als **„Classic-Modus“** spielbar.

1. **Welt-Tick:** eigene Phase zwischen den Runden für Umweltsimulation. Kreaturen-Ticks bleiben getrennt.
2. **Materialien:** jedes Terrain und Objekt erhält eine Materialklasse.
   - Fleisch, Holz, Stein, Metall, Pflanze, Wasser.
   - Daraus folgen Entflammbarkeit, Zähigkeit und Leitfähigkeit.
   - Idee aus dem alten GDD, z. B. Blitz springt über Metall.
3. **Feuer v2:** Ausbreitung nach Material und Wind.
   - Rauch blockiert die Sicht; verbranntes Gras wird zu Asche.
   - Spontane Brände durch Drachenatem oder Blitz.
   - Feuer + Flood → Dampf.
4. **Herden und Ökologie:** neutrale Tiere ziehen in kleinen Herden umher (Boids-light).
   - Sie grasen, fliehen vor Feuer und Zauberern und können in Panik stampeden.
   - Raubtiere jagen sie.
   - Herden lassen sich subvertieren oder als Ablenkung nutzen.
5. **Licht und Sicht:**
   - Tag/Nacht bzw. dunkle Dungeons.
   - Lichtquellen (Feuer, Zauber, Fackeln) färben die Zellen ein.
   - Sichtfeld mit weichem Abfall statt hartem Schnitt.
6. **Effekte:**
   - Partikel-Glyphen (Funken, Splitter, Blut, Magie)
   - Bildschirm-Flash, Erschütterung (Versatz)
   - Animierte Kreatur-Glyphen in 2 Phasen
7. **Inspiration Tales of Maj'Eyal:**
   - Lesbare taktische Infos (Tooltips mit Trefferchance)
   - Nachvollziehbares Kampflog
   - Optional: Talent-Bäume für Zauberer
8. **Prüfen, nicht versprechen:** 2×2-Kreaturen (Drachen, Riesen) aus dem alten GDD. Sie würden Kollision, Sicht und Darstellung stark verkomplizieren; Entscheidung erst nach M4.

Jede [X]-Idee wird erst als eigener Milestone mit Mini-Design ausgearbeitet.

---

## 13. Unbekannte Werte (im Manual nicht spezifiziert)

Spalte „WinUAE“: Was sich im Amiga-Original direkt beobachten lässt (●), nur grob abschätzen lässt (◐) oder von uns entworfen werden muss (○). ✅ heißt bereits geklärt.

| Thema | WinUAE | Vorgehen |
|---|---|---|
| Kartengröße pro Szenario | ✅ | **36×36** (Spectrum, B3.1); Amiga-Stichprobe O2 |
| AP-Kosten pro Aktion und Terrain | ✅ | **Eigenes Design** (§5.3, D7), Anker: Boden 4 bzw. 6 (B4) → `data/costs.csv`, `data/actions.csv` |
| Mana-Kosten pro Zauber und Stufe | ✅ | Linear, alle 45 in `data/spells.csv` (B3.5); Amiga-Stichprobe und Bomb Potion in O4 |
| Zauberer-Attribute: Startwerte, XP-Kosten, Obergrenzen | ● | Im Wizard Designer angezeigt |
| Spiellänge (1–5) gegen Portal-Zeiten | ● | Rundenspannen im Setup-Panel ablesen, Portal-Erscheinen protokollieren |
| Terrain-Typen und ihre Glyph-Entsprechung | ● | Szenarien erkunden, Terrain-Katalog anlegen |
| Stamina-Verbrauch, Regeneration, Erschöpfungsschwelle | ✅ | **Eigenes Design** (§5.3): Schritt = AP/2, Regeneration 25 %, Erschöpfung unter 25 % |
| Wirkung von Wood/Water/Rock Type | ◐ | Mit passenden Kreaturen testen |
| Wurfreichweite (Stärke, Gewicht) | ◐ | Maximale Reichweite am Zielcursor (rot) ablesen |
| Kampfformel (Combat gegen Defence plus Zufall) | ○ | Entwurf in M3, Host-Simulation (10.000 Kämpfe), grob gegen beobachtete Ergebnisse prüfen |
| Feuer, Blob, Vine, Flood: Ausbreitungsregeln | ◐ | Verhalten beobachten, deterministisch über den Seed-RNG nachbauen |
| Trank-Dauer (Stufe × Potion Consumption) | ◐ | Wirkdauer in Runden zählen |
| Teleport-Ungenauigkeit, Subversion- und Curse-Chancen | ○ | Formeln entwerfen |
| KI-Verhalten der Computer-Zauberer | ◐ | Beobachten (im Einzelspieler sind Gegnerzüge in Sicht sichtbar, `[AMI 4]`) |

**Kalibrierung:**
- Alle Formeln stehen zentral in `src/core/rules.c` und `data/*.csv`.
- Der Host-Build simuliert Kämpfe und Partien automatisch, damit Balancing ohne Emulator möglich ist.
- Beobachtungen aus WinUAE kommen nach `docs/design/amiga-observations.md`. Eine erste Beobachtungs-Session wird vor M2 empfohlen, weil Kartengröße, AP-Kosten und Terrain-Katalog das Core-Design beeinflussen.

---

## 14. Entscheidungen (Review 1, 2026-10-02)

| # | Frage | Entscheidung |
|---|---|---|
| D1 | Referenz-Original | WinUAE ist installiert, Kickstart 1.3 und ADF liegen lokal. Das Original dient als Kalibrierquelle (§0, §13). |
| D2 | Szenario-Karten | **Eigene, neue Karten** im Geist der Originale (§9.1) |
| D3 | Spiellänge gegen Portal | Unbekannt; wird in WinUAE beobachtet (§13) |
| D4 | Spielmodus-Fokus | **Einzelspieler** (Kampagne gegen KI-Zauberer). Hotseat kommt nach v1.0. |
| D5 | Steuerung | **Tastatur, inspiriert von Caves of Qud**, nicht die Original-Bedienung (§5.1). Maus ist nicht Teil von v1.0. |
| D6 | Zieltastatur | **Cherry G84-4100, deutsches ISO-Layout (QWERTZ, bestätigt)**. Kein Ziffernblock. Belegung „rechte Hand Pfeile, linke Hand Aktionen“, Diagonalen per Pfeil-Akkord (§5.2). |
| D7 | Originalwerte | **Keine exakte Kopie.** Aktionskosten, Formeln und Balancing sind eigenes Design mit sinnvollen Startwerten. Messungen am Amiga dienen nur als Anker bzw. Plausibilitätscheck (§5.3). |
| D8 | Bildausschnitt | **MODE 8, 8×8-Glyphen, Kartenfenster 27×24 Felder** (§11.1). Kein 20×20 (altes GDD), kein 7×7 (Amiga). 16×16-Zoom ist optional nach v1.0. |

**Noch offen:**
- Endgültige Tastenbelegung (Prüfung im M2-Prototyp).
- Kartengröße auf dem Amiga (Spectrum: 36×36, Stichprobe O2).
- Die Frage, ob 2×2-Kreaturen in die Chaos-Phase kommen (nach M4).

---

## 15. Umsetzungsreihenfolge (Abbildung auf die Roadmap)

| Milestone | GDD-Abschnitte |
|---|---|
| M1 Spikes | §11 (Layout, Glyphen, Palette-Cycling), Eingabe-Spike mit der G84-4100 (§5.2: Akkorde, Sondertasten, Auto-Repeat). Parallel: WinUAE-Beobachtungs-Session 1 (Kartengröße, AP-Kosten, Terrain-Katalog). |
| M2 Core-Skelett | §3 (Karte, Ebenen, Sicht, Hidden Map), §4 (Daten), §5.1 (aktive Einheit, Bewegung, Bump, `Tab`, Look-Modus), Rundenablauf §2.1 |
| M3 Classic spielbar | §6 Kampf, §7 Beschwörungen und Bolt/Lightning, §8 Basis-Objekte, §9 Portal und VP, §10 einfache KI, eigenes Szenario 1 |
| M4 Classic komplett (v1.0) | Alle 45 Zauber und Tränke, Flächeneffekte, Wizard Designer, Kampagne, eigene Szenarien 2 und 3, Setup-Panel, Speichern |
| nach v1.0 | Hotseat-Multiplayer, Timer, Maus, Expansion-Kit-Inhalte |
| M5+ | §12 Chaos |
