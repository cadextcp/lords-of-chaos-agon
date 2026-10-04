# Game Design Document – Lords of Chaos (Agon)

> Status: **Entwurf v0.5** · Darstellung überarbeitet (D9/D10, §11) · Entscheidungen §14 · Stand 2026-10-02
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
2. **Agon-nativ.** Gebaut wird in C/eZ80 für 512 KB RAM. Hardware-Grenzen bestimmen das Design, nicht umgekehrt.
3. **Nah dran wie Spectrum und Amiga (D9).**
   - Die Darstellung arbeitet mit **eigener 24×24-Pixelart** in einer **9×9-Sicht**.
   - Man sieht Möbel, Teppiche, Türen, Schubladen und Kreaturen im Detail, mehrere Ebenen übereinander pro Feld.
   - Von *Caves of Qud* übernehmen wir die **Effekte und Lesbarkeit**: Licht, Farbanimation, Partikel, klare Infos. Die abstrakte Glyphen-Optik übernehmen wir nicht.
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
- **Big Map:** strategische Übersicht über etwa die halbe Welt. Symbole: Zauberer, Bodenkreatur, Flieger, Objekt. Auf dem Amiga ist sie scrollbar. `[PM 13, AMI 4]` Bei uns zeigt die Big Map die **ganze Welt** im Kartenfenster: 36×36 Felder à 4×4 px (§11.1).
- **Kartengröße:** **36×36 Kacheln** in allen Spectrum-Szenarien (B3.1); für Amiga als Näherung angenommen, Stichprobe O2. Das ist die Classic-Referenz für eigene Karten; das Szenario-Format erlaubt andere Größen.
- **Wege** und **Wände** sind kachelbasierte Linien durch die Kachelmitte (B2.1, B3.3).

### 3.2 Ebenen pro Feld

| Ebene | Inhalt |
|---|---|
| Boden | Untergrund: Steinfliesen, Holzdielen, Gras, Weg, Wasser … |
| Boden-Dekor | Teppich, Pentakel, Blut bzw. Asche; begehbar, ändert nur das Aussehen (brennbar: Teppich) |
| Feature | Wand, Tür, Möbel (Bett, Tisch, Stuhl, Regal, Kommode, Schrank, Truhe, Kessel, Kerzenständer), Baum, Fels. Kann blockieren und kann Behälter sein (siehe 3.3). |
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

**Einrichtung (D9).** Das Original zeigt ein möbliertes Zauberer-Haus (Beobachtung B2.7); wir machen Häuser und Dungeons ebenso wohnlich:

| Feature | Bewegung | Sicht | Besonderheit |
|---|---|---|---|
| Teppich (Boden-Dekor) | frei | frei | brennbar ([X] Feuer v2) |
| Stuhl, Kerzenständer | frei (wie Boden + 2 AP) | frei | Kerzen sind Lichtquelle ([X]) |
| Tisch, Bett | blockiert | frei | Objekte können darauf liegen |
| Regal, Schrank | blockiert | blockiert | Behälter (`a` = öffnen bzw. durchsuchen) |
| **Kommode bzw. Schubladen** | blockiert | frei | **Behälter**: öffnen, Inhalt aufheben; kann verschlossen sein (Schlüssel) |
| Truhe | blockiert | frei | Behälter; verschlossen bzw. geöffnet `[PM 15]` |
| Kessel | frei | frei | Trankbrauen `[PM 19]` |

Möbel haben eine Zähigkeit wie Türen. Sie lassen sich zerschlagen, und Holzmöbel brennen ([X]).

**Umgesetzt in M2a:**
- Böden: Steinboden, Holzdielen, Gras, Weg, hohes Gras, Wald, Zauberwald (Magic Wood), Schattenwald, Sumpf, Wasser (animiert), Geröll.
- Features: Fels, Baum, Wand, Tür und Möbel.
- Wälder und hohes Gras sind **begehbare Böden**: Sie kosten mehr AP und blockieren die Sicht am Boden (`data/costs.csv`). Fels und Einzelbaum blockieren.
- Testkarte: `data/maps/testland.txt` (36×36, Wrap-around).

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
| Constitution | Lebenspunkte. Bei 0 tot; unter 50 % leiden Kampf und Verteidigung (**−2/−2**, Startwert M4a) sowie die AP-Auffüllung (halbe AP, wie Erschöpfung). |
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
- **Objekt in Benutzung:** Eine Kreatur kann mehrere Objekte tragen, aber nur eines ist „in use“, und zwar das zuletzt aufgehobene. DROP, THROW und Waffen-Boni beziehen sich darauf. Schilde wirken immer `[PM 9, 17]`, aber höchstens **ein** getragener Schild zählt (D21).
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

**Ergebnis des Spikes im Emulator (ADR 0007):**
- Akkorde funktionieren (80 ms).
- Bewegung läuft per Virtual Key, weil ASCII bei Key-up veraltet ist.
- Die Tastenwiederholung macht das Spiel selbst (350 ms, dann 200 ms), weil `kbuf` kein Auto-Repeat liefert.
- Pos1, Bild↑, Ende und Bild↓ sind als Diagonalen umgesetzt.

**Noch zu verifizieren auf der G84-4100** (Testprogramm loggt jedes `kbuf`-Event: ASCII, VKey, Modifier, down/up):
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
- **Gebunden (engaged) und freier Schlag (D26):** Wer neben einem Gegner steht, darf sich wegbewegen — aber der Gegner bekommt einen **freien Schlag** gegen den Flüchtenden. Der freie Schlag kostet den Gegner keine AP (normale Treffer-/Schadensberechnung; Untoten-Immunität und Boden-gegen-Flieger gelten wie im Nahkampf). Pro Wegziehen höchstens ein freier Schlag. **Diagonal-Schlupf:** Löst der Ausbruch den Kontakt ganz (kein Gegner mehr angrenzend), gibt es keinen freien Schlag. Flieger sind nie gebunden. Reiter greifen von befreundetem Feld an (D21).
  - **Umsetzung (Fix nach Hardware-Test):** Nahkampfkontakt bindet beide Seiten: an einen Gegner heranziehen, angreifen oder angegriffen werden. Die Bindung (`UF_ENGAGED`) endet mit der eigenen Phase des Gebundenen; ein Bodenkämpfer, der nur neben einem Gegner steht, ist nicht gebunden. Wer gebunden ist, bekommt beim Versuch wegzugehen die Meldung „Gebunden: Gegner daneben - nur Angriff.“ Flieger und Reiter sind nie gebunden.
- **Rückschlag (D27/D29):** Ein angegriffenes Ziel schlägt **sofort** zurück — der Rückschlag ist eine freie Abwehrreaktion, kostet **keine AP und keine Ausdauer**. **Jede Einheit hat eine Reaktion pro Runde** (D&D-Vorbild): Konter und freier Schlag beim Wegziehen (D26) teilen sich dieses eine Budget — wer seine Reaktion verbraucht hat, wird im selben Rundenverlauf unbestraft weiter angegriffen; mit der neuen Runde (Regeneration) ist sie zurück. Wer angreift, riskiert den Gegenschlag; wer attackiert wird, verliert dadurch nichts. (Vor D27 kostete der Rückschlag 6 AP + 3 Ausdauer — das saugte belagerte Einheiten leer: Wer oft angegriffen wurde, konnte selbst nie mehr zuschlagen, und Aussetzen half nicht.)
- **Kritische Treffer (D30):** Ein sehr guter Angriffswurf (unter 5 %) ist ein **kritischer Treffer**: die Schadenswürfel zählen doppelt, der feste Kampf-Bonus nicht (D&D-Vorbild) — Schwert 2w8 → 4w8, Bolt (3+Stufe)w6 → doppelt. Etwa jeder zwanzigste Angriff; gilt für Nahkampf, Rückschlag, Freien Schlag, Wurf, Bogen und Bolt/Blitz (jeder Wurf über die Trefferformel). Flächen, Bomben und Zerschlagen von Möbeln kritisieren nicht. Krits öffnen leichter tödliche Wunden (Schaden über 25 % Con).
- **Tödliche Wunde:** Ein einzelner Treffer über 25 % der Constitution verursacht sie.
  - Danach verliert die Kreatur jede Runde Constitution, bis sie stirbt oder einen Heiltrank trinkt.
  - Äpfel heilen Constitution, aber keine tödliche Wunde.
- **Untote:** siehe §4.
- **Angriff von befreundetem Feld aus** ist verboten („attack not allowed“), außer für Reiter.

### 6.1 Waffen `[PM 35]` — Schadenstabelle (D28)

**Werte je Waffe** (`data/weapons.csv`): Gewicht, Combat-/Defence-Bonus (in der Hand bzw. getragen beim Schild), Wurf-/Fernkampf-Flag und **Schadenswürfel**. Der Schaden eines Treffers ist `Würfel der Waffe + Kampf/5` (abgerundet; 0–10 Punkte). **Waffen machen den Unterschied** (D28): Vor D28 rechnete jeder Treffer nur mit dem Kampf-Wert — ein Schwert fühlte sich wie die bloße Faust an.

| Waffe | Würfel | Kampf+ | Ø mit Kampf 6 | Ø mit Kampf 50 | Bemerkung |
|---|---|---|---|---|---|
| Waffenlos / beliebiges Objekt | 1d4 | — | 3,5 | 12,5 | Faustschlag |
| Messer | 1d6 | +5 | 4,5 | 13,5 | leicht zu werfen |
| Wurfstern | 1d6 | +4 | 4,5 | 13,5 | Wurfwaffe |
| Keule | 2d6 | +7 | 8 | 17 | |
| Speer | 2d6 | +8 | 8 | 17 | |
| Bogen | 2d6 | — | 8 | 17 | Fernkampf (Reichweite 6), kein Konterrisiko |
| Schwert | 2d8 | +10 | 10 | 19 | |
| Axt | 2d10 | +9 | 12 | 21 | |
| Slayer | 2d10 | +13 | 12 | 21 | verletzt Untote |
| Magie-Slayer | 3d8 | +16 | 14,5 | 23,5 | verletzt Untote |
| Schild | 1d4 | **+13 Ver)** | — | — | **wird nie geführt** (D21/D28): +13 Verteidigung getragen (Original-Anker, D31) |

**Treffer bis zum Sieg** (Kampf 6, mittlerer Wurf): Goblin (Con 32) — waffenlos ~9, Schwert ~3, Axt ~3 Treffer; Zwerg (25) — Schwert ~2–3; Troll (47) — Schwert ~5; Gold-Drache (90) — Schwert ~9.

**Magische Waffen** (über Enchant): doppelte Werte; Ausnahme ist der Slayer, der einen eigenen Magic-Slayer-Eintrag hat. Sie verletzen Untote. **Der Schild nimmt nie die Hand** (D28): `w` überspringt Schilde — getragen verteidigt er immer (D21), in der Hand wäre er verschenkt.

**Ausrüstung prägt den Kampf (D31):** Die Boni stehen in **Original-Proportionen** (Anker: der Schild des Originals gibt +13 Verteidigung; D7 hatte sie auf +4/+8 verkleinert, was gegen Kreaturenwerte bis Kampf 50/Verteidigung 42 wirkungslos war — laut Arena-Simulation gewannen Waffenträger praktisch nie). Mit D31 hat eine waffenfähige Kreatur mit sehr guter Waffe und Schild eine reale Chance: in der Mittelfeld-Arena (ohne Drachen, 2000 Läufe) gewinnt der Riese mit Ausrüstung ~11 % der Schlachten, Magie-Slayer-Halter sind die erfolgreichste Waffe, und 11 % aller Sieger tragen ein Schild. Drachen bleiben Apex (~95 %).

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
- **Stufe = Stärke der Kreatur (D34):** Ein Wurf beschwört **eine** Kreatur auf einem freien Nachbarfeld; jede Stufe über 1 gibt **+15 % Kampf, Verteidigung und Konstitution** (gedeckelt bei Stufe 8, Stufe 8 ≈ doppelt so stark). Beschwörungen **verbrauchen sich nicht** und kosten immer das **Stufe-1-Mana**; begrenzt sind sie durch Mana und 10 AP pro Wurf. Ist kein Platz, verfällt das Mana. (Andere Zauber: Stufe = Anzahl der Ladungen.)
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
| Magic Fire | Ziel muss brennbar sein, keine Sichtlinie nötig. Breitet sich pro Runde aus oder erlischt. Zerstört Objekte. Schadet nur **feindlichen** Einheiten (auch Untoten), unabhängig von deren Defence. Neues Feuer schwächt alle eigenen bestehenden Feuer. **Startwerte M4d:** 6 Schaden pro Runde; brennt auf Gras, hohem Gras, Holz, Wäldern und Bäumen. |
| Gooey Blob | Wie Feuer, aber anderes Terrain. Weniger zerstörerisch, zäher. Schadet Untoten. **Startwerte:** 3 Schaden pro Runde; haftet auf allem Begehbaren außer Wasser; blockiert Bewegung ab Stärke 2. |
| Tangle Vine | Fläche je nach Stufe, nur auf anfälligem Terrain. Gefangene müssen sich freikämpfen oder werden verwundet. **Startwerte:** 2 Schaden pro Runde; nur auf Gras, hohem Gras und Wald; blockiert Bewegung ab Stärke 2 (Freikämpfen folgt). |
| Flood | Fläche, anderes Terrain. Löscht Feuer. Wer ins Wasser geht, kann ertrinken, außer Water Type. **Startwerte:** kein Schaden; überall Begehbares außer Wasser; Nicht-Wasserwesen ertrinkt mit 50 % Chance pro Runde (Flieger nicht); löscht Feuer auf dem Feld (Überschreiben). |
| Gooey Blob | Wie Feuer, aber anderes Terrain. Weniger zerstörerisch, zäher. Schadet Untoten. |
| Tangle Vine | Fläche je nach Stufe, nur auf anfälligem Terrain. Gefangene müssen sich freikämpfen oder werden verwundet. |
| Flood | Fläche, anderes Terrain. Löscht Feuer. Wer ins Wasser geht, kann ertrinken, außer Water Type. |
| Enchant | Waffen auf dem Zielfeld (getragen oder am Boden) werden magisch: doppelte Werte, wirkt gegen Untote. Zeitlich begrenzt. Darf auf den eigenen Zauberer zielen. |
| Subversion | Feindliche Kreatur wechselt die Seite. Chance hängt von Stufe und Magic Resistance ab. Reiter und Reittier nur gemeinsam. Nicht auf Zauberer oder Reittiere mit Zauberer. |
| Curse | Verursacht tödliche Wunden. Bessere Chance als Subversion oder Magic Attack. |
| Magic Attack | Kann mehrere Kreaturen desselben Typs im Umkreis vernichten, **auch eigene**. |
| Magic Bolt | Physischer Schaden; Defence zählt — **aber ohne Schildbonus (D32: Magie umgeht Rüstung)**. **(3 + Stufe)w6** (Krit: doppelt, D30) (Stufe 1: 4w6, Ø 14; Stufe 5: 8w6, Ø 28; Stufe 8: 11w6, Ø 38,5) — D&D-Upcasting (D29): volle Bücher schlagen härter, die Ladungen werden schwächer. Stufe 1 verwundet einen Goblin (Con 32) stark, tötet ihn aber nicht. |
| Magic Lightning | Wie Bolt (ebenfalls schildfrei, D32) mit **(5 + Stufe)w6** plus **2w6 Splash** auf die 8 Nachbarfelder. Zerstört Terrain am Boden. Zielfeld darf nicht massiv sein. |
| Teleport | Ungenau auf große Distanz. Danach 0 AP. Scheitert, wenn das Ziel massiv ist. |
| Magic Eye | Sicht von einem Zielpunkt aus (Luft oder Boden). Deckt Unsichtbare für eine Runde auf. |
| Magic Shield | Defence des Zauberers steigt, Höhe und Dauer nach Stufe. |

### 7.3 Wizard Designer `[PM 24–28, AMI 2]`

- **Name** (Buchstaben und Leerzeichen).
- **Charakter:** Ein frischer Zauberer startet auf der **Mindestverteilung** — Kampf 5, Abwehr 5, Magiewiderstand 70, Konstitution 25, Ausdauer 34, Mana 90, AP 34 — und verteilt **600 XP** auf Attribute, Mana, AP und Zauber (Original-Anker 2026-10-04). **Punktekosten:** Kampf 2, Abwehr 2, Magiewiderstand 4, Konstitution 2, Ausdauer 4, **Mana 9**, **AP 8** pro Punkt; Pfeile links/rechts senken/erhöhen (volle Rückzahlung).
- **Zauber kaufen (F6-Anker):** Beschwörungen sind im Designer kaufbar (`z` schaltet um) — Stufe 1 kostet den Grundpreis, jede weitere +50 % davon, Maximum Stufe 8: Zwerg/Fledermaus 4, Kobold(Goblin)/Pixie 8, Einhorn/Löwe/Gorilla/Krokodil 10, Harpyie/Pegasus/Bär/Zentaur 12, Zombie/Troll 14, Riese/Elefant 20, Greif/Geist 22, Riesenspinne 28, Vampir 50*, Rotdrache 38, Gründrache 50, Golddrache 62, Gespenst 44, Dämon 58 (*Vampir/Pixie fehlen im Anker, interpoliert). Nicht-Beschwörungen sind nicht kaufbar — sie kommen über das Startbuch; künftig auch über Schriftrollen-Funde (Rarität nach XP-Preis, D33).
  - Jedes Attribut hat Kosten pro Punkt und eine Obergrenze.
  - Unter den Startwert kann man nicht senken.
- **Zauber:** Stufen mit XP erhöhen, bis maximal 10 (Anker: Das Original-Startbuch trägt Stufen bis 10, Nutzerablesung 2026-10-04).
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

### 11.1 Bildschirmaufteilung (D9: 24×24-Kacheln, Sicht 9×9)

**Entschieden (D9, ersetzt D8):** **MODE 8** (320×240, 64 Farben). Die Karte besteht aus **24×24-Pixel-Kacheln** aus **eigener Pixelart** (D10), das Kartenfenster ist **9×9 Felder** (216×216 px) groß.

Das entspricht dem Maßstab der Spectrum-Fassung (24×24, B3) und liegt nah am Amiga (7×7). Die abstrakte 8×8-Glyphen-Optik von M0 entfällt.

```
x: 0                       216                 319
   +------------------------+--------------------+ y=0
   | KARTENFENSTER          | [24x24] Name       |
   | 9 x 9 Felder           | Status-Icons       |
   | je 24x24 px            |                    |
   | (216 x 216 px)         | ▮  ▮  ▮  ▮  ▮  ▮   |
   |                        | ▮  ▮  ▮  ▮  ▮  ▮   |
   | Overlays: Zauberliste, | AP ST CO CB DF MA  |
   | Inventar, Big Map,     | (6 Balken, Amiga)  |
   | Menüs                  | Objekte im Feld    |
   +------------------------+--------------------+ y=216
   | Meldungszeile / Name unter Cursor (3 Zeilen)|
   +---------------------------------------------+ y=240
```

- **Info-Panel (104 px):**
  - Oben das Bild der Einheit (ihre 24×24-Kachel) mit Namen, wie der Vorschaukasten des Amiga (B2.5).
  - Darunter Status-Icons (Undead, Fly, Mount, Wound, Invisible) `[PM 11]`.
  - Dann **6 senkrechte Balken** wie auf dem Amiga (B2.4): AP, Stamina, Constitution, Combat, Defence, Mana; Mana nur bei Zauberern. Jeder Balken hat ein Icon und die Amiga-Farbe.
  - Am Ende die Objekte im Feld.
- **Umsetzung auf dem Agon (M1 #6):** Das Panel liegt im 8×8-Textraster (Spalten 27–39).
  - Porträt im Rahmen, daneben „Stufe“ und die 5 Status-Icons; nur aktive Icons werden angezeigt.
  - Zeile 5: Name. Zeile 6: AP und Mana als Zahl.
  - Balken in y 58–168, Icons darunter. Combat und Defence haben die Skala 0–50; der Mana-Balken entfällt bei Nicht-Zauberern.
  - Zeilen 23–26: „Am Boden“ mit bis zu 3 Einträgen: Objekte, begehbares Feature, Dekor, sonst Boden.
  - Alle Anzeigetexte des Cores liegen in `src/core/names.c`. Die UI ist vorerst deutsch, ohne Umlaute, weil der Agon-Systemfont keine hat; Umlaute kommen später mit einem eigenen Font.
- **Meldungszeile (3 Textzeilen):** benennt das Element unter dem Cursor (B2.3), zeigt Kampf- und Zaubermeldungen und kontextabhängige Tastenhinweise.
- **Listen und Menüs** (Zauberliste, Inventar, Aufheben, Kontextmenü `Enter`, Big Map) erscheinen als **Overlay über dem Kartenfenster**. Das Panel ist für Zaubernamen zu schmal; das Amiga zeigt die Zauberliste ebenso als eigenen Bildschirm. Auswahl mit `a`–`z` bzw. Pfeilen (§5.1).
- **Kamera:** Das Fenster scrollt, wenn die aktive Einheit bzw. der Cursor weniger als 2 Felder vom Rand entfernt ist, und bei `Tab` auf die neue Einheit. Wrap-around über die 36×36-Welt.
- **Sicht gegen Fenster:**
  - Der Sichtradius (9/11) ist größer als das Fenster (±4). Das ist wie im Original (Amiga ±3).
  - Der Rest der Welt ist über die **Big Map** (`m`) erreichbar: 36×36 Felder à 4×4 px = 144×144 px im Kartenfenster, mit Farbe je Terrain und Punkten für eigene bzw. gesichtete Einheiten.

### 11.2 Visuelle Sprache

- **Pixelart:** 24×24 Pixel je Kachel, Farben ausschließlich aus der **Agon-64-Farben-Palette** (RGB 2-2-2) plus Transparenz. Der Stil orientiert sich am Spectrum bzw. Amiga: klare Umrisse, gut lesbare Silhouetten, Möbel und Kreaturen erkennbar auf einen Blick.
- **Perspektive: 3/4-Frontansicht wie auf dem Amiga (D11).**
  - Böden, Teppiche und Wege sind flach von oben gesehen.
  - Möbel, Wände und Kreaturen zeigen **Oberseite (hell) und Front (dunkler)**: Kopfteil des Betts, Tischbeine, Schubladen-Fronten, Ziegel-Front der Wände.
  - Stehende Dinge werfen einen gerasterten Schatten.
  - Alles bleibt innerhalb seiner 24×24-Kachel; es gibt keinen Überstand in die Kachel darüber, damit Ebenen und Dirty-Rendering einfach bleiben.
- **Wände in 3/4:** Durch die Kachelmitte läuft eine helle Kappe (Oberseite). Wo südlich keine Wand anschließt, liegt darunter die Ziegel-Front. Nord-Süd-Verläufe zeigen nur die schmale Kappe. Es gibt 16 Auto-Tiling-Varianten.
- **Halb-Böden an Wänden:** Jede Seite der Wandlinie zeigt den Boden des Nachbarfelds auf dieser Seite, also innen die Fliesen und außen das Gras, wie auf dem Amiga (B2). Der Renderer zeichnet dafür zugeschnittene Boden-Kacheln; auf dem Agon geschieht das über vorab erzeugte Halb-Kacheln pro Bodentyp und Richtung.
- **Besitzerfarbe:** Jede Kreaturen-Kachel hat definierte **Schlüsselfarben** (z. B. Robe bzw. Schabracke). Das Tile-Tool erzeugt daraus Varianten für die 4 Zauberer und Neutral (Palette-Swap beim Build).
- **Hidden Map:** Unerforschtes ist schwarz. Erkundetes, aber nicht in Sicht, wird mit einem **Raster-Overlay** (50-%-Schachbrett, schwarz) abgedunkelt; das ist Retro-typisch und kostet nur eine Overlay-Kachel.
- **Cursor:** Ein **Hardware-Sprite** zeigt den Rahmen. Farbe: weiß im Look-Modus, grün bei gewählter Einheit am Boden, blau in der Luft; beim Zielen gelb (Boden), blau (Luft) bzw. rot (außer Reichweite) (Amiga-Farbcode, B2.6, `[AMI 3]`).
- **Animation [C] (Frame-Animation, nur für sichtbare Felder):**
  - Kerzen- und Feuerflackern (2–4 Frames)
  - Wasser
  - Portal-Pulsieren
  - Treffer-Blitz (Overlay)
  - Zauber-Projektil (Sprite-Flug)

  In den 64-Farben-Modi ist die Palette fest. Palette-Cycling entfällt deshalb und wird durch Frame-Animation ersetzt; Spike M1 prüft das.
- **Qud-Erbe [X]:** Licht und Schatten (Lichtquellen wie Kerzen und Feuer hellen die Umgebung auf, als Overlay-Stufen), Partikel und Bildschirm-Effekte.

### 11.3 Kachel-Komposition: Ebenen pro Feld

Wie beim Amiga werden pro Feld **mehrere Ebenen übereinander** gezeichnet (B1.2). Die Kacheln liegen als **VDP-Bitmaps** im Grafikspeicher des Agon; transparente Pixel lassen die darunterliegende Ebene durchscheinen.

| Reihenfolge | Ebene | Beispiele |
|---|---|---|
| 1 | Boden | Steinfliesen (blau, wie Amiga), Holzdielen, Gras, Weg, Wasser |
| 2 | Boden-Dekor | Teppich, Pentakel, Blutfleck, Asche |
| 3 | Feature | Wand (Auto-Tiling), Tür offen bzw. zu, Möbel, Kommode, Truhe, Kessel, Kerzenständer, Baum |
| 4 | Objekt | oberstes Objekt im Feld als kleines Icon. Liegen mehrere, zeigt ein Marker „mehr“; Details per `x` bzw. `g`. |
| 5 | Boden-Einheit | Kreatur bzw. Zauberer (Besitzerfarbe), Reiter auf Reittier |
| 6 | Luft-Einheit | Flieger, leicht nach oben versetzt, mit Schatten auf dem Boden |
| 7 | Flächeneffekt | Feuer, Gooey Blob, Tangle Vine, Flood (animiert, halbtransparent per Raster) |
| 8 | Sicht-Overlay | Raster für „erinnert, nicht in Sicht“ |
| – | Cursor | Hardware-Sprite, kein Neuzeichnen nötig |

- **Wände** belegen ganze Kacheln. Die Wandlinie läuft durch die Kachelmitte und verbindet sich per **Auto-Tiling** (16 Varianten nach den 4 Nachbarn) mit Nachbarwänden, wie im Original (B2.1). Türen sitzen in der Wandlinie.
- **Datenmodell:** pro Feld je ein Byte für Boden, Dekor und Feature, plus Zustandsbits (Tür offen, Behälter verschlossen, gesehen bzw. erinnert). Einheiten und Objekte liegen in Pools mit Positionen. Der Core liefert dem Frontend pro sichtbarem Feld eine **Liste von Kachel-IDs** statt eines Glyphen; das ersetzt das Zellen-Grid aus M0.

**Bandbreite (gemessen in M1, ADR 0006):** Ein Voll-Redraw dauert 48 ms, ein normaler Schritt zeichnet 2 Felder, das Kerzenflackern 16 ms pro Frame. Die ursprüngliche Abschätzung lautete:
- Ein Kachel-Zeichenbefehl kostet etwa 11 Byte (Bitmap wählen plus zeichnen).
- Ein volles Kartenfenster hat 81 Felder mit etwa 3,5 Ebenen, also rund 3,1 KB. Bei 1.152.000 Baud zum VDP sind das etwa **30 ms**.
- Gezeichnet werden nur geänderte Felder; beim Scrollen das ganze Fenster.
- Grafikspeicher: 24×24 in RGBA2222 sind 576 Byte pro Kachel; 400 Kacheln brauchen etwa 230 KB VDP-Speicher (M1 prüft das Budget).

### 11.3a Pixelart-Pipeline (D10: eigene Grafik)

- **Quelle:** `assets/tiles/*.png`, Kachelbögen im 24×24-Raster, gezeichnet mit der Palette `assets/palette/agon64.gpl` (64 Farben plus Transparenz). Bearbeitbar mit jedem Pixel-Editor (Aseprite, LibreSprite, GIMP).
- **Tool `tools/build_tiles.py`:**
  - prüft Größe und Palette (falsche Farben gelten als Fehler)
  - erzeugt Besitzerfarben-Varianten
  - schreibt `tiles.bin` (RGBA2222) für die SD-Karte, die ID-Tabelle `gen_tiles.h` für den Core und eine Vorschau-PNG
- **Mockups:** `tools/mockup.py` rendert aus Kacheln und einer Szenen-Beschreibung ein 320×240-Bild des Spielbildschirms nach `docs/design/mockups/`. So lässt sich die Optik beurteilen, bevor der Agon-Renderer existiert.
- Alle Grafiken sind eigene Arbeit (D10) und dürfen ins öffentliche Repo.

### 11.4 Sound [C] (Polish-Runde: Samples)

- **Samples statt Piepser (ADR 0012):** `tools/gen_sfx.py` synthetisiert eigene
  8-Bit-Samples (16 kHz, nichts aufgenommen oder kopiert): Schritt, Wisch,
  Klirren, dumpfer Treffer, Stöhnen, Donner, Zisch, Knarren, Truhendeckel,
  Funkeln, Beschwörung, Blip, Blubbern, Krachen, dazu die Instrumente Zupfsaite
  und Trommel. Datei `/loc/sfx.bin` (~110 KB), beim Start in VDP-Buffer geladen.
  WAV-Vorschauen: `build/sfx/preview/`.
- **Effekte** (`src/agon/sound.c`) sind kurze Schrittlisten (Sample oder
  Wellenform-Ton) mit Priorität auf zwei Kanälen (0, 4); `sound_poll()` spielt
  Schritt für Schritt, weil der VDP Noten auf belegten Kanälen verwirft
  (QUIRK A1). Fehlt `sfx.bin`, klingen die Wellenform-Ersatztöne.
- **Zuordnung:** Schwung/Wurf/Bogen = Wisch, Treffer = dumpfer Schlag, Krit =
  zusätzlich Metallklirren (D30), Tod = Stöhnen, Bolt = Zisch, Blitz = Donner,
  Beschwörung, Teleport, Fluch/Subversion, Tränke/Brauen = Blubbern, sonstige
  Zauber = Funkeln; Tür, Truhe, Aufheben, Essen, Trinken, Fliegen/Reiten,
  Portal, Rundenwechsel; Menü bewegen/bestätigen/zurück; rote Meldungen
  (verweigerte Aktion) = kurzer tiefer Ton.
- **Setup:** Musik (M) und Toneffekte (T) einzeln abschaltbar, gespeichert in
  `/loc/settings.dat`.

#### 11.4.1 Ereignisse und Kampfanimation [C] (M5c)

- Der Core meldet Darstellungsereignisse in einen kleinen Ring
  (`src/core/events.[ch]`): `EV_SWING, EV_HIT, EV_WOUND, EV_MISS, EV_DEATH,
  EV_SPELL, EV_SMASH` mit Position und Beteiligten. Beobachtung ohne
  Nebenwirkungen — RNG, Weltzustand und Savegames bleiben unangetastet.
- Emit-Punkte: Nahkampf/Freier Schlag (Schwung, Treffer, Verfehlt, Rückschlag),
  `combat_damage` (jede Schadensquelle: Bolt, Blitz, Wurf, Bogen, Flächen,
  Bombe), `world_kill_unit` und Blutungstod (Tod), `pay_for_spell`
  (Zauberwirkung), Terrain-Angriff und Blitz (Zerschmettern).
- Das Frontend (`src/agon/fx.c`) spielt den Ring ab: Overlay-Kacheln
  (fx_slash, fx_hit, fx_miss, fx_death_0–3) über dem Kartenfenster,
  Schadenszahl in Rot, passender Sound; kurze getimete Frames, Tasten
  während der Show werden verworfen (keine Geister-Eingaben, K5). Nach der
  Show werden die Felder über `view_mark_dirty` neu gezeichnet.
- Die KI wird sichtbar: nach jeder KI-Phase (und den unabhängigen Kreaturen)
  leert ein Callback in `turn_end_phase` den Ring und spielt ihn ab.
- **Sprite-Effekte (Polish-Runde, ADR 0012):** Neues Ereignis `EV_PROJECTILE`
  (Start, Zielversatz, Art) von Bolt, Blitz, Bogen, Wurf und Bombenphiole.
  Das Frontend lässt Projektile als VDP-Sprites pixelgenau fliegen (Bolt-Kugel,
  Blitz mit Funkenschweif, Pfeil in 8 Richtungen, rotierende Wurfwaffe);
  Zauber spielen am Ziel ihre eigene Sprite-Folge (Beschwörungswirbel,
  Teleport-Funken, Schildkuppel, Fluchschädel, Trankblasen, Funkeln);
  Schadenszahlen steigen als Ziffern-Sprites auf (Krit mit „!“). Sprites
  liegen über der Karte – nichts muss neu gezeichnet werden, nichts bleibt
  stehen. Eigene Schritte gleiten in 80 ms von Feld zu Feld (Setup: G).

### 11.5 Hilfe, Tutorial und Lexikon [C] (M5)

- **Hilfeseiten** (F1, Hauptmenü): seitenweise Texte (Steuerung, Aktionen,
  Spielziel, Runden/AP, Kampf, Magie, Objekte), ←/→ blättert. Die Texte liegen
  als `.hlp`-Dateien auf der SD-Karte (`/loc/help`, ADR 0011), nicht im
  Programm — Umlaute über die umdefinierten Font-Glyphen (M4j). Die erste
  Seite ist die Tastenliste (früher die statische F1-Überlagerung).
- **Geführtes Tutorial** (Hauptmenü): kleine Karte (16×12) mit Zauberer,
  Zwerg, Schluessel, Truhe, Goblin und Portal (oeffnet Runde 2). Eine
  Schritt-Engine im Core (`tutorial.c`) prüft Weltzustand plus zwei
  Meldungen (Tab gedrückt, Zauber gewirkt): Bewegen → Einheit wechseln →
  Schluessel aufheben → Truhe oeffnen → Goblin besiegen → Zaubern → Portal.
  Die Hinweiszeile steht in Meldungszeile 3; Texte aus `help/tutorial.hlp`.
  Die Runde-1-Bewegungssperre [PM 7] ist im Tutorial aufgehoben.
- **Lexikon** (Taste `i`, Hauptmenü): was der Spieler je gesehen hat —
  Kreaturen (26) und Objekte (40) als Bitmasken, persistent in
  `/loc/lexicon.dat`. Liste zeigt nur Entdecktes mit Namen, Rest „???“;
  Enter öffnet die Detailseite mit Porträt, Werten aus den Tabellen und
  Kurztext aus `help/lexicon.hlp`. Markiert wird beim Sehen (Sichtregel)
  und Aufheben.

### 11.6 Titelbild und Titelmusik [C] (M5d)

- **Titelbild** 320×240 (Polish-Runde): ein Schlachtgetümmel auf Schwarz im
  Geist der 8-Bit-Ladebilder, eigene Komposition (D7): großer Zauberer wirkt
  einen Blitz auf einen Dämon, davor Troll, Zentaur, Zombie und Zwerg, oben ein
  Drache und eine Fledermaus vor einem Magiewirbel, Schriftzug „LORDS OF
  CHAOS" in abgeschrägten Goldbuchstaben. Die Figuren sind die eigenen
  24×24-Kreaturen, mit Scale2x/Scale3x vergrößert (glatte Kanten, Palette
  bleibt). Quelle `assets/title/title.png`, Generator `tools/art/make_title.py`.
- **Hauptmenü:** Das Titelbild bleibt als Hintergrund; oben sichtbar (Logo,
  Drache, Wirbel), darunter die Einträge in einem gerahmten Kasten.
- **Endbildschirm:** 96×96-Bild neben den Werten – Sieg: Zauberer vor dem
  leuchtenden Portal mit Gold und Edelsteinen; Niederlage: Grab bei Nacht,
  Hut auf dem Stein, zerbrochener Stab (`tools/art/make_endpics.py`,
  `win.bin`/`lose.bin` auf der SD).
- **Überschriften-Schrift:** eigene 8×16-Zierschrift (`tools/build_font.py`,
  `/loc/fonts/head.fnt`), am Grafikcursor mit Schatten gezeichnet – Menü,
  Designer, Setup, Hilfe, Lexikon, Endbildschirm, Overlays. Fehlt die Datei,
  gilt die Systemschrift.
- **Streaming-Loader:** `show_picture()` (render.c) lädt Titel und Endbilder
  häppchenweise über den 576-Byte-Staging-Puffer in eigene VDP-Puffer
  (0x4000–0x4002) und fügt die Blöcke zusammen (QUIRKS S1). Fehlt eine
  Datei, bleibt es bei Text.
- **Musik (Polish-Runde):** eigenes Stück in a-Moll, 16 Takte in zwei
  Hälften (A: Am F C G | Am F E E, B: Dm Am F E | Dm Am E Am), vier Stimmen:
  Zupfsaite (Sample) als Melodie, Dreieck-Bass, leise Sinus-Begleitung mit
  Pausen, Trommel (Sample). Läuft in Schleife unter Titel und Menü weiter und
  endet erst beim Spielstart. Sieg- und Niederlage-Jingles auf dem
  Endbildschirm (`data/music/win.txt`, `lose.txt`). Format LOCM v2
  (`tools/gen_music.py`): Instrument, Ersatz-Wellenform, Lautstärke, ADSR,
  Schleife. Jede Stimme wechselt zwischen zwei VDP-Kanälen (1/6, 2/7, 3/8,
  5/9), damit Noten ausklingen dürfen; Zeitplan in Millisekunden aus der
  Zentisekunden-Uhr. Vorhören am PC: `uv run tools/audio_preview.py`.

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
   - Partikel (Funken, Splitter, Blut, Magie)
   - Bildschirm-Flash, Erschütterung (Versatz)
   - Animierte Kreaturen (2 Frames, Idle)
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
| Terrain-Typen und ihre Kachel-Entsprechung | ● | Szenarien erkunden, Terrain-Katalog anlegen |
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
| ~~D8~~ | ~~Bildausschnitt~~ | ~~MODE 8, 8×8-Glyphen, 27×24 Felder~~ → **ersetzt durch D9** (Review 2: „zu weit weg, sieht nicht aus wie die Referenzen“) |
| D9 | Darstellung | **24×24-Pixel-Kacheln, Kartenfenster 9×9**, MODE 8, mehrere Ebenen pro Feld als VDP-Bitmaps, Möbel, Teppiche, Türen und Schubladen sichtbar (§11). Nah an Spectrum (24×24) und Amiga (7×7). |
| D10 | Grafikquelle | **Eigene 24×24-Pixelart von Anfang an**, keine Fremd-Packs. Pipeline PNG → Agon (§11.3a). |
| D11 | Perspektive | **3/4-Frontansicht wie auf dem Amiga** für Möbel, Wände und Kreaturen; flache Böden; Halb-Böden an Wänden (§11.2). |
| D12 | Kreaturwerte | **Kreaturtabelle des Originals `[PM 34]` als Startwerte** für alle 25 Kreaturen (`data/creatures.csv`). Balancing später; Abweichungen werden in der CSV kommentiert. |
| D13 | Kreaturgrafik | **Alle 25 Kreaturen bekommen schon in M2 eigene 24×24-Pixelart** (3/4-Ansicht, Besitzerfarben). |
| D26 | Gebunden: freier Schlag beim Wegziehen (Review-Fix) | **Wegbewegen aus dem Nahkontakt ist erlaubt** (Bewegungskosten wie üblich); der angrenzende Gegner erhält **einen freien Schlag** ohne AP-Kosten (normale Trefferchance/Schaden, Untoten-Regel, Boden-gegen-Flieger). Löst der Ausbruch den Kontakt ganz (diagonal heraus, kein Gegner mehr angrenzend), entfällt der Schlag. Pro Wegziehen ein Schlag. Ersetzt die M3a-Regel „keine Bewegung im Engagement“. |
| D27 | Freier Rückschlag (Playtest 2026-10-04) | **Der Rückschlag ist eine freie Abwehrreaktion: keine AP, keine Ausdauer, immer.** Vorher kostete er 6 AP + 3 Ausdauer — wer oft angegriffen wurde (KI-Goblin: bis 3 Angriffe pro Zug), musste unfreiwillig zurückschlagen und startete ausgelaugt in den eigenen Zug; AP regenerierten scheinbar nie (Ausdauer-Absturz halbiert die Auffüllung), Aussetzen half nichts. Angriffe kosten weiter 10 AP + 4 Ausdauer (Angreifen bleibt eine Entscheidung). |
| D28 | Waffen- und Zauberschaden mit Würfeln (Playtest 2026-10-04, D&D-orientiert) | **Schaden = Würfel der Waffe + Kampf/5** (Tabelle §6.1: waffenlos 1w4 … Magie-Slayer 3w8) — vorher rechnete jeder Treffer nur mit Kampf/4, das Schwert brachte keinen spürbaren Schaden. **Zauber würfeln eigene Würfel** (§7.2): Magic Bolt **7w10**, Magic Lightning **10w8 + 3w6 Splash** — ein treffender Bolt tötet einen Goblin meist sofort; Zauber verbrauchen Stufen und müssen sich darin lohnen. **Der Schild nimmt nie die Hand** (`w` überspringt Schilde; getragen verteidigt er immer, D18/D21). Con-Werte der Kreaturen bleiben Anker (D12); die Skalierung steckt in den Würfeln. |
| D32 | Zauber ignorieren Schilde (Zauberer-Duell-Befund 2026-10-04) | **Bolt und Blitz rechnen gegen die Verteidigung ohne getragenen Schild** (`items_defence_noshield`): Magie umgeht Rüstung (D&D: Angriff gegen RK vs. Rettungswurf). Nahkampf, Bogen und Wurf treffen weiterhin auf die volle Verteidigung; Zauberschild/Protection-Trank zählen auch gegen Magie (sie sind selbst magisch). Grund: Im Arena-Duell wurden geschildete Zauberer für Bolts praktisch unhittbar (10-%-Boden), Blaster/Evoker 6-7 % vs. Nekromant 34 %. |
| D31 | Waffenbonis in Original-Proportionen (Arena-Befund 2026-10-04) | **Combat-Bonis deutlich erhöht** (Schwert +4→+10, Axt +3→+9, Slayer +6→+13, Magie-Slayer +8→+16, Messer +5, Speer +8, Keule +7, Wurfstern +4) und **Schild +4→+13 Verteidigung** (Anker: Original-Wert; bewusst übernommen statt D7-Verkleinerung). Grund: Die Arena-Simulation zeigte, dass Ausrüstung die Kämpfe nicht prägte — waffenführende Kreaturen hatten ~0 % Siege. Mit D31 gilt: sehr gute Waffe + Schild = reale Siegchance im Mittelfeld; Drachen bleiben dominant. Validiert über `host/arena.c` (2000 Läufe, Vorher/Nachher). |
| D30 | Kritische Treffer (Wunsch 2026-10-04, D&D-orientiert) | **Angriffswurf < 5 % = kritisch: Schadenswürfel doppelt, Boni einfach** — Schwert 2w8 → 4w8, Stufe-1-Bolt 4w6 → 8w6. Etwa jeder zwanzigste Angriff; Nahkampf, Rückschlag, Freier Schlag, Wurf, Bogen, Bolt/Blitz; Flächen/Bomben/Möbel nicht. Meldung „KRIT! …“, rotes Overlay mit Crash-Sound. |
| D29 | Eine Reaktion pro Runde; Zauber skalieren nach Stufe (Playtest 2026-10-04) | **Jede Einheit hat eine defensive Reaktion pro Runde** (D&D-5e-Vorbild): der freie Rückschlag (D27) und der freie Schlag beim Wegziehen (D26) teilen sich dieses Budget (`UF_REACTED`, geräumt mit der Regeneration). Mehrere Angriffe im selben Rundenverlauf laufen danach unbestraft ins Ziel. **Magic Bolt auf (3+Stufe)w6 und Lightning auf (5+Stufe)w6 + 2w6 Splash reduziert** (7w10 war zu stark): D&D-Upcasting — die Buchstufe beim Wirken bestimmt die Würfel, volle Bücher schlagen am härtesten, entladene am schwächsten. Stufe 1 = 4w6 (Ø 14): clearly über einem Schwertstreich, aber kein Sofort-Kill. |
| D22 | M4-Aufteilung (Review 3) | **Zehn Teile M4a–M4j** in der Reihenfolge von §16: erst Systeme, dann Kampagne, Szenarien, KI, Speichern, Politur. **Kampagne ohne Gegenstände** (F5), **Dächer sichtbar** und innen ausgeblendet (F7). Die Formeln F1–F4 und die Vorschläge F6, F8, F9 gelten als Startwerte. |
| D21 | Regeln aus dem M3-Review | **Rückschlag gegen Flieger:** Greift ein Flieger selbst am Boden an, schlägt der Verteidiger zurück; von unten angreifen geht weiter nicht. **Blitz ohne Freund-Feind-Erkennung:** Der Splash trifft alle 8 Nachbarfelder, auch eigene Einheiten und den Zaubernden (Gollop-Tradition); eigene Opfer bringen keine VP. **Eine Trefferformel für alle Angriffe:** Werfen, Bogen und Bolt nutzen D16 (10–90 %, Defence inklusive Schild). **Schilde stapeln nicht:** Ein getragener Schild zählt immer (D18), weitere nicht. **Beute fällt:** Wer stirbt (Kampf, Zauber, Verbluten), lässt alles Getragene auf sein Feld fallen, Flieger auf den Boden darunter; wer durchs Portal entkommt, nimmt es mit. |
| D20 | Einfache KI (M3f) | **Jäger für Unabhängige** (nächstes Ziel per Sichtstrahl, Angriff wenn angrenzend, sonst Umherstreifen); **Zauberer-KI**: Kreaturen jagen zuerst, dann Nahkampf/Beschwörung (günstigster Zauber, bis 3 Begleiter)/Weg zum Portal und Eintritt. Kein Schummeln: Ziele nur in eigener Sichtlinie. |
| D19 | Portal und VP (M3e) | **Erscheinungsrunde** deterministisch aus der Szenario-Spanne (RNG mit Partie-Saat); **Entkommen +10 VP** plus getragene Schätze (objektbezogen aus objects.csv); Kills nach Kreaturtabelle, Zauberer im Nahkampf doppelt (AMI 4), Fernkampf einfach; Spielende ohne Zauberer. |
| D18 | Objekte und Waffen (M3d) | **Startwerte aus dem Manual als Basis** (objects/weapons.csv, D7): Waffenboni nur für das Objekt in Benutzung, **Schild zählt immer beim Tragen**; Werfen fliegt bis 6 Felder und landet vor dem Hindernis; Bogen 12 AP, Reichweite 6, trifft auch Flieger; Trage-Limit aus der Kreaturtabelle; Aufheben nimmt das oberste Objekt (Liste folgt). |
| D17 | Zauber-Reichweite und Zielmodus (M3c) | **Eigene Reichweite 6 Felder** (Chebyshev) für Bolt/Lightning bis WinUAE-Messung mehr sagt (§13); Sichtlinie nötig. Bolt trifft nach dem Kampf-Modell (D16), Lightning zusätzlich auf die 8 Nachbarfelder und zerschlägt zerstörbares Terrain am Ziel; massive Zielfelder (Wände) werden abgelehnt. Ziel-Cursor: gelb Boden, blau Luft, rot außer Reichweite/Sicht; Enter/Leertaste wirkt, Esc bricht ohne Kosten ab, Zielen auf den eigenen Zauberer bricht ab. |
| D16 | Kampfformel (M3a) | **Eigenes Design:** Trefferchance `50 + 5 × (Combat − Defence)`, begrenzt auf 10–90 %. Schaden `(Combat + Zufall(0..Combat)) / 4`, mindestens 1. Rückschlag automatisch bei AP+Stamina (`return_attack`, actions.csv). Tödliche Wunde bei Einzeltreffer > 25 % Con, −1 Con pro Runde. Gebundene Bodeneinheiten können sich nicht fortbewegen. Terrain-Angriff: Feature fällt bei `Schaden + Zufall(0..3) > Zähigkeit` (features.csv); Wände unzerstörbar. Balancing später über die CSV-Werte. |
| D15 | Fliegen (M2e) | **Ein AP-Budget pro Einheit, ebenenabhängig aufgefüllt** (Rundenende: `ap_max` am Boden, `ap_fly` in der Luft). Aufsteigen (`<`) und Landen (`>`) zahlen die Aktionskosten aus `actions.csv`; Luftbewegung konstant 4/6 und ignoriert Terrain und Boden-Einheiten; Landen nicht auf Ertrinkungs-Terrain. Flieger werden 3 px höher mit Bodenschatten über der Boden-Einheit gezeichnet; Dächer folgen mit dem späteren Dach-Datenfeld. |
| D14 | Sichtalgorithmus | **Bresenham-Strahlen pro Zielfeld** (M2d): Chebyshev-Distanz (9 Boden / 11 Luft), blockierendes Gelände nur **zwischen** den Endpunkten, Diagonalen permissiv (keine Ecken-Regel). Sicht wird als Bitfeld pro Spieler cached und nur nach eigenen Schritten bzw. am Rundenende neu berechnet. Messwerte und Optimierungen: ADR 0009. |

**Noch offen:**
- Endgültige Tastenbelegung (Prüfung im M2-Prototyp).
- Kartengröße auf dem Amiga (Spectrum: 36×36, Stichprobe O2).
- Die Frage, ob 2×2-Kreaturen in die Chaos-Phase kommen (nach M4).

---

## 15. Umsetzungsreihenfolge (Abbildung auf die Roadmap)

| Milestone | GDD-Abschnitte |
|---|---|
| M1 Spikes | §11 neu: Bitmap-Kachel-Renderer (Ebenen, Bandbreite, VDP-Speicher, Frame-Animation, Hardware-Sprite-Cursor), Pixelart-Pipeline und Mockup, Eingabe-Spike mit der G84-4100 (§5.2). Parallel: WinUAE-Beobachtungs-Session 1 (Kartengröße, AP-Kosten, Terrain-Katalog). |
| M2 Core-Skelett | §3 (Karte, Ebenen, Sicht, Hidden Map), §4 (Daten), §5.1 (aktive Einheit, Bewegung, Bump, `Tab`, Look-Modus), Rundenablauf §2.1 |
| M3 Classic spielbar | §6 Kampf, §7 Beschwörungen und Bolt/Lightning, §8 Basis-Objekte, §9 Portal und VP, §10 einfache KI, eigenes Szenario 1 |
| M4 Classic komplett (v1.0) | Alle 45 Zauber und Tränke, Flächeneffekte, Wizard Designer, Kampagne, eigene Szenarien 2 und 3, Setup-Panel, Speichern. Aufteilung M4a–M4j und offene Fragen: §16 |
| nach v1.0 | Hotseat-Multiplayer, Timer, Maus, Expansion-Kit-Inhalte |
| M5+ | §12 Chaos |

---

## 16. M4-Aufteilung „Classic komplett“ (Review 3, 2026-10-03, entschieden: D22)

**Ziel:** Funktionsumfang ≈ Amiga-Version im Einzelspieler, Tag `v1.0.0` (ROADMAP). M4 ist deutlich größer als M3. Jeder Teil endet wie bisher mit Selftests, Emulator-Beleg, CHANGELOG und PR.

**Leitlinien:**
- **Mechanik vor Inhalt:** Erst die Systeme (Wirkungen, Flächen, Behälter), dann Kampagne und Szenarien, die sie nutzen.
- **KI-Minimum pro Teil:** Jede neue Mechanik kommt mit einer einfachen KI-Nutzung oder wenigstens einer KI, die sie sicher ignoriert. Der Ausbau der KI folgt in M4h.
- **Budget:** `loc.bin` hat nach M3 137 KB. Die Größe im CI-Bericht beobachten; ab etwa 250 KB folgt ein ADR (Daten vom SD statt einkompiliert, Overlays).

| Teil | Inhalt | GDD | Abnahme |
|---|---|---|---|
| **M4a Classic-Lücken und Szenario-Format** | Untote nur durch Untote, magische Waffen und Zauber verletzbar (§4.2). Malus unter 50 % Constitution (§4.1). Nahrung mit EAT (Apfel, Pilz, magische Varianten). Schlüssel und Truhen als Behälter-Features, Schriftrollen mit READ (§8). **Szenario-Datei** `data/scenarios/` mit Karte, Zauberbüchern und Parametern der KI-Zauberer; das Test-Zauberbuch entfällt (§9.1). | §4, §8, §9.1 | Szenario 1 lädt Zauberbücher aus der Datei; Untote lassen sich mit normalen Waffen nicht verletzen |
| **M4b Wirkungen und sonstige Zauber** | Zeitlich begrenzte Wirkungen pro Einheit (Art, Stärke, Restrunden), Tick am Rundenende (§2.1), Status-Icons im Panel. Zauber: Magic Shield, Magic Eye, Teleport, Curse, Subversion, Magic Attack, Enchant (magisch-Flag pro getragenem Objekt). | §7.2 | Jeder der 7 Zauber wirkt im Emulator; Wirkungen laufen nach ihrer Dauer ab |
| **M4c Tränke und Brauen** | Kessel (leer/voll, Schlucke) und Phiole als Objekte, 7 Zutaten, 7 Trankzauber (Amiga-Satz, kein Super Potion), DRINK, FILL, Werfen der Bombe. Mehrere Tränke wirken gleichzeitig. Drachen-Beschwörung mit Drachenkraut im Kessel (§7.2, `[PM 21]`). | §7.2, §8 | Brauen → Trinken → Wirkung → Ablauf; ein Drache entsteht nur mit Kraut |
| **M4d Flächeneffekte** | Neue Feld-Ebene „Effekt“ (§3.2) mit Stärke. Magic Fire, Gooey Blob, Tangle Vine, Flood: Ausbreitung am Rundenende, deterministisch über den Partie-RNG. Schaden, Objekte verbrennen, Ertrinken, Feuer löschen. Eigene animierte Kacheln. Kills zählen einfach (§9). | §3.2, §7.2 | Ein Feuer breitet sich aus und erlischt reproduzierbar; Rundenende mit 4 aktiven Flächen bleibt unter 0,5 s im Emulator |
| **M4e Kampf komplett** | Restliche Waffen (Knife, Spear, Club, Axe, Ninja Star, Slayer, Magic Slayer) mit eigenen Werten (D7). **Reiten** (RIDE, RIDER, Angriff vom befreundeten Feld). **Dächer** als Datenfeld im Kartenformat v4: blockieren Sicht und Landung (§3.2). | §3.2, §4.2, §6 | Ein Zauberer reitet ein Einhorn in den Kampf; Flieger können auf Dächern nicht landen |
| **M4f Zauberer und Kampagne** | Hauptmenü (§2.3), Wizard Designer mit Attributen und Zauberstufen (§7.3), 4 Zauberer-Plätze auf SD, VP → XP 1:1, Stufenaufstieg, Szenario-Folge 1 → 2 → 3, Wiederholen erlaubt (§9). | §2.3, §7.3, §9 | Ein entworfener Zauberer spielt Szenario 1, steigt auf und gibt XP im Designer aus |
| **M4g Szenarien 2 und 3** | „Slayer's Dungeon“ und „Ragaril's Domain“ als eigene Karten im Geist der Originale (D2), mit Schätzen, Portal-Timing und Gegnerauswahl aus §9.1. | §9.1 | Beide Szenarien sind durchspielbar |
| **M4h KI-Ausbau** | Wächter-Profil (§10). Zauberer-KI nutzt Angriffszauber, Tränke und Flächen, sammelt Schätze. Rechenbudget ≤ 2 s pro KI-Zug messen (Emulator, später Hardware). | §10 | Die KI gewinnt gelegentlich gegen einen passiven Spieler; Zugzeit gemessen und dokumentiert |
| **M4i Speichern und Setup** | Spielstand am Rundenende auf SD, höchstens 5 Ladungen im Einzelspieler (§2.3). Setup-Panel nur mit den für Einzelspieler relevanten Einstellungen (§2.2). | §2.2, §2.3 | Speichern, Neustart, Laden ergibt denselben Spielstand (Hash) |
| **M4j Politur** | Kontextmenü mit Enter, Big Map `m` (§3.1), Log `l`, Hilfe F1, Sound (§11.4), Font mit Umlauten, Kunst-Schulden (offene Türen). | §5.1, §11 | Vollständige Partie ohne Hilfe von außen spielbar |

**Abhängigkeiten:** a → b → c (Tränke nutzen die Wirkungen aus b). d braucht nur a. e ist unabhängig. f braucht a; g braucht c, d und e (Szenario-Inhalte). h nach g, i nach f, j zum Schluss.

### 16.1 Designfragen zu M4

Die Vorschläge sind als **Startwerte** übernommen (D22). Jede Frage wird vor ihrem Teil noch einmal geprüft; Balancing später über die CSV-Dateien.

| # | Frage | Teil | Entscheidung bzw. Startwert |
|---|---|---|---|
| F1 | Wirkdauer von Tränken und Zaubern | b, c | Trank: `Runden = 8 × Trankstufe / Potion Consumption`, mindestens 1 (Zauberer mit 3 → Stufe 3 hält 8 Runden, Drache mit 10 → 2). Zauber wie Magic Shield: `2 × Stufe` Runden. Eigenes Design (D7). Der Kessel merkt sich die gebraute Stufe; Phiolen wirken vorerst mit Stufe 2, bis Objekte Zusatzdaten tragen. |
| F2 | Chancen für Subversion, Curse, Magic Attack | b | Wie D16: `50 + 5 × (4 × Stufe − Magic Resistance / 4)`, begrenzt auf 10–90 %. Curse +20 Punkte (GDD: bessere Chance), Magic Attack −10. |
| F3 | Teleport-Ungenauigkeit | b | Abweichung bis `Distanz / 4` Felder (zufällig, auf freies Feld); massives Ziel lässt den Zauber scheitern; danach 0 AP. |
| F4 | Ausbreitung der Flächen | d | Stärke = Zauberstufe. Am Rundenende versucht jedes Feld einmal, ein passendes Nachbarfeld zu belegen (Chance `Stärke × 10 %`); neue Felder erhalten `Stärke − 1`, alte verlieren 1. Höchstens 48 Felder je Fläche (Leistung). |
| F5 | Was überträgt die Kampagne? | f | **Entschieden:** Attribute, Zauberstufen (voll aufgefüllt) und XP. Schätze werden beim Durchschreiten des Portals zu VP und danach zu XP. Waffen, Schilde, Tränke und Schlüssel bleiben im Szenario; der Zauberer startet unbewaffnet. |
| F6 | Startwerte, Kosten und Obergrenzen im Wizard Designer | f | **Anker 2026-10-04 (Nutzer):** Startbuch Stufen 4–10 (Buchdeckel 10); **Mindestverteilung** Kampf 5 / Abwehr 5 / Magiewiderstand 70 / Konstitution 25 / Ausdauer 34 / Mana 90 / AP 34 und **600 XP zum Verteilen**; **Kosten** Kampf 2 / Abwehr 2 / Magiewiderstand 4 / Konstitution 2 / Ausdauer 4 / Mana 9 / AP 8 pro Punkt; **Beschwörungs-Grundpreise** (Drache rot 38, grün 50, gold 62; Zwerg/Fledermaus 4; Kobold 8; Einhorn/Löwe/Gorilla/Krokodil 10; Harpyie/Pegasus/Bär/Zentaur 12; Zombie/Troll 14; Elefant 20; Greif/Geist 22; Spinne 28; Gespenst 44; Dämon 58), **jede weitere Stufe +50 % des Grundpreises, Maximum 8**. Diese Kosten sind **Erfahrung, kein Mana** (F6-Klarstellung des Nutzers). Offen: Obergrenzen der Attribute (eigene Werte bleiben). |
| D33 | Schriftrollen lehren Zauber (Nutzerregel 2026-10-04) | Schriftrollen-Funde bringen Zauberstufen ins Buch — die Rarität einer Schriftrolle richtet sich nach dem XP-Preis des Zaubers (teuer = selten). Umsetzung folgt (items_read); die Kostenbasis steht bereits über F6. |
| D34 | Beschwörungs-Stufe = Kreaturstärke (Nutzerregel 2026-10-04) | **Zaubersprüche haben eine Anzahl, Kreaturen ein Level.** Eine Beschwörung erzeugt pro Wurf genau eine Kreatur; ihre Stufe aus dem Zauberbuch gibt +15 % Kampf/Verteidigung/Konstitution je Stufe über 1 (Deckel 8). Beschwörungen sind unbegrenzt wirkbar, kosten festes Mana (Stufe-1-Preis) und 10 AP. Im Designer kostet jede weitere Stufe +50 % des Grundpreises (F6). Ersetzt die Original-Regel „Anzahl = Stufe“ `[PM 19]`. Anzeige: Spalte „Stf“ statt „Anz“. |
| D35 | Zufällige Welt, Wildtiere, Herden (Nutzerregel 2026-10-04) | **Jede Partie ist anders:** Zufallsstartwert aus der Uhr (Tests/Dump: 42). **Die Gegner starten allein:** In den Kampagnen-Karten stehen nur noch die Zauberer; der KI-Zauberer beschwört seine Kreaturen selbst. **Wildtiere** (5–8, Abstand ≥ 8 zu Zauberern, nicht in Häusern/Wasser): *friedlich* (Gorilla, Fledermaus) streifen umher und wehren sich nur gegen den, der sie angegriffen hat; *territorial* (Löwe, Bär, Krokodil, Spinne, Greif) greifen jeden an, der ihrem Heimfeld auf 3 Felder nahekommt; *Herdentiere* (Elefant, Einhorn, Pegasus) sind friedlich. **Herden:** ab Runde 4 mit 15 % je Runde (höchstens eine gleichzeitig) betreten 3–4 Herdentiere an einem Kartenrand die Karte, ziehen geradeaus hinüber (Hindernissen weichen sie seitlich aus) und verlassen sie wieder. **Beute:** 5–7 Truhen an Zufallsorten (Schätze, Waffen, Tränke, selten der Slayer), 2 Truhenschlüssel und 6–9 lose Fundstücke passend zum Boden (Wald: Pilze/Misteln/Feenflügel, Sumpf: Schwefel/Salpeter, Wiese: Äpfel/Klee/Kristall). Fest bleibt nur die Hausausstattung. Modul `populate.c`. |
| D36 | Zauber durch hohes Gras; Aufheben vom Nachbarfeld (Nutzerwunsch 2026-10-04) | Hohes Gras blockiert die Sicht, aber **nicht die Zauberlinie** (`sight_has_spell_los`); Bäume, Wände, Dächer schon. **Aufheben** erreicht das eigene Feld und die 8 Nachbarfelder (gleiche AP); bei mehreren Gegenständen fragt ein Auswahlmenü (einzeln oder alle). |
| F7 | Dächer: nur Regel oder auch sichtbar? | e | **Entschieden:** sichtbar. Von außen sieht man das Dach; steht eine eigene Einheit im Gebäude, wird das Dach über dem Gebäude ausgeblendet. |
| F8 | 5-Ladungen-Grenze beibehalten? | i | Ja, aber im Setup abschaltbar. |
| F9 | Setup-Panel und Timer in v1.0? | i | Nur die Zufalls-Zauberer-Stufe; Spiellänge folgt aus dem Szenario, Timer nach v1.0. |
