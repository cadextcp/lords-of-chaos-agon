> **Originalregeln (Spectrum-Fassung).** Dieses Dokument ist eine **unveränderte Kopie** von `docs/REGELN.md` aus dem Repo `lords-of-chaos-zx-agon` (Stand Commit `06890b5`, 2026-10-07). Der Nutzer hat die Regeln dort aus dem Z80-Code und den Datentabellen gelesen. Gepflegt wird es im anderen Repo; Änderungen dort werden hierher nachgezogen (Kopie überschreiben, Stand oben anpassen).
> Es ist die **Quelle der Wahrheit für die Originalregeln** (GDD D66, D67). Die Kapitelnummern „K“ in `docs/REGELVERGLEICH-SPECTRUM.md` und `docs/PLAN-KI.md` beziehen sich auf dieses Dokument. Unsere eigenen Regeln stehen weiter im GDD (`docs/design/GDD.md`).

---

# Regeln und Werte von Lords of Chaos (Spectrum-Original)

Alles hier stammt aus dem Z80-Code und den Datentabellen des Spectrum-Originals (Adressen = Spectrum-Speicher, Szenario 1 geladen). Das Amiga/Atari-ST-Handbuch diente nur zum Gegenprüfen. Die Kreaturenwerte und die Waffenwerte stimmen mit dessen Tabellen überein.

Konventionen:

- `RND(n)` ist eine Zufallszahl 0 … n−1 (Routine `$A734`).
- `L` ist das aktuelle Zauberlevel (vor dem Wirken).
- `⌊a/b⌋` ist die Ganzzahldivision (Routine `$94B7`).
- Die Karte ist 36×36 Felder groß und **wickelt an den Rändern um** (Torus, Überläufe bei 36).
- Sicherheit: **gesichert** = direkt im Code gelesen. **(unsicher)** = Deutung oder nur überflogen.

## 1. Entfernung

Alle Reichweiten und Radien nutzen `$B045`/`$724D`:

```
dx, dy = Abstand je Achse mit Umbruch (min(d, 36−d))
D = 2·max(dx,dy) + min(dx,dy)
```

Eine gerade Strecke von n Feldern hat also D = 2n. Eine Diagonale von n Feldern hat D = 3n.

## 2. Kreaturen

Datensatz: 12 Byte je Kreatur, Zeiger im Szenarioheader (`$D003`). Der Index ist die Kreatur-Nr. (= Zauber-ID 0–24), das Kartensymbol ist `$90 + Index`. In allen drei Szenarien sind die Tabellen identisch.

| Byte | Bedeutung |
|---:|---|
| 0 | Aktionspunkte (AP) am Boden |
| 1 | AP fliegend (0 = kann nicht fliegen) |
| 2 | Stamina (max.) |
| 3 | Konstitution (max.) |
| 4 | Combat |
| 5 | Defence |
| 6 | Magic Resistance (MR) |
| 7 | Tragekapazität (Summe der Gewichte) |
| 8 | NPC-Aggressivität in % (nur für vom Spiel gesetzte Kreaturen: `RND(100) < Wert` → aktiv, `$A5B3`) |
| 9 | Potion Consumption (Tränke wirken kürzer, je größer der Wert) |
| 10 | 2 × Siegpunkte für den Sieger über diese Kreatur |
| 11 | Flags, s. u. |

Flags (Byte 11): `01` Reittier, `02` untot, `04` kann Waffen benutzen, `08` kann Reittiere reiten, `10` Wood-Typ, `20` Water-Typ, `40` Rock-Typ, `80` hat „Use“-Option.

| Kreatur | AP | AP fl. | Stam | Kon | Comb | Def | MR | Trag | Aggr.% | Trank | SP | Flags |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| Gold Dragon | 38 | 40 | 86 | 90 | 50 | 42 | 82 | 30 | 0 | 10 | 9 | Use, Water |
| Green Dragon | 32 | 36 | 80 | 82 | 40 | 43 | 75 | 26 | 0 | 10 | 8 | Use, Wood |
| Red Dragon | 34 | 40 | 90 | 72 | 32 | 34 | 90 | 32 | 0 | 10 | 7 | Use, Rock |
| Pixie | 34 | 0 | 40 | 16 | 4 | 6 | 67 | 30 | 0 | 2 | 1 | Use, Reiter, Waffen, Wood |
| Dwarf | 26 | 0 | 57 | 25 | 6 | 6 | 40 | 35 | 0 | 2 | 1 | Use, Reiter, Waffen, Wood, Rock |
| Goblin | 30 | 0 | 45 | 32 | 9 | 9 | 46 | 42 | 0 | 2 | 1 | Use, Reiter, Waffen, Wood |
| Troll | 32 | 0 | 88 | 47 | 12 | 16 | 46 | 40 | 0 | 2 | 2 | Use, Reiter, Waffen, Wood |
| Giant | 30 | 0 | 48 | 66 | 21 | 15 | 50 | 50 | 20 | 4 | 3 | Use, Waffen |
| Centaur | 48 | 0 | 62 | 36 | 8 | 10 | 54 | 34 | 0 | 3 | 1 | Use, Waffen |
| Unicorn | 56 | 0 | 72 | 40 | 12 | 9 | 42 | 0 | 0 | 3 | 1 | Reittier, Rock |
| Pegasus | 46 | 56 | 80 | 40 | 8 | 9 | 50 | 0 | 0 | 3 | 1 | Reittier |
| Gryphon | 42 | 52 | 73 | 53 | 24 | 21 | 57 | 0 | 0 | 4 | 4 | Reittier, Rock |
| Elephant | 36 | 0 | 55 | 73 | 14 | 22 | 40 | 0 | 0 | 6 | 2 | Reittier |
| Gorilla | 34 | 0 | 56 | 38 | 15 | 14 | 43 | 26 | 20 | 4 | 1 | Use, Wood |
| Lion | 54 | 0 | 64 | 34 | 21 | 14 | 46 | 0 | 20 | 4 | 2 | Wood |
| Bear | 38 | 0 | 70 | 48 | 22 | 26 | 48 | 0 | 20 | 5 | 3 | Wood |
| Crocodile | 26 | 0 | 52 | 62 | 26 | 20 | 55 | 0 | 0 | 4 | 2 | Water |
| Giant Bat | 24 | 62 | 75 | 20 | 9 | 7 | 41 | 0 | 10 | 2 | 1 | – |
| Harpy | 28 | 52 | 60 | 28 | 22 | 14 | 60 | 16 | 0 | 4 | 3 | Use |
| Giant Spider | 44 | 0 | 74 | 52 | 41 | 24 | 55 | 0 | 99 | 7 | 6 | Rock |
| Zombie | 24 | 0 | 90 | 50 | 6 | 5 | 50 | 30 | 40 | 3 | 1 | Use, untot |
| Ghost | 36 | 36 | 92 | 30 | 8 | 18 | 60 | 0 | 10 | 6 | 2 | Use, Water, untot |
| Vampire | 34 | 40 | 85 | 60 | 18 | 17 | 65 | 30 | 99 | 5 | 4 | Use, untot |
| Spectre | 38 | 0 | 80 | 72 | 28 | 24 | 70 | 0 | 80 | 7 | 5 | Use, untot |
| Demon | 30 | 0 | 90 | 78 | 38 | 31 | 78 | 0 | 99 | 9 | 7 | Use, untot |

Anmerkungen:

- Zombie, Ghost, Vampire, Spectre und Demon sind untot. Das zeigt Flag `02`, es steht auch im Handbuch.
- **Pixies sind immer unsichtbar** (`$75BE`: Kreatur `$93` bekommt Bit 3 in Byte 4 des Stat-Blocks). Das Handbuch erwähnt es nicht.
- Beim Wechsel Boden ↔ Luft werden die AP proportional zwischen AP am Boden und AP fliegend umgerechnet (`$6E8C`/`$6EA3`).

## 3. Wizards

### 3.1 Datensatz (`$5B04`, 0x46 Byte)

| Offset | Inhalt |
|---:|---|
| +0 | Mana (Maximum) |
| +1 | Level (0 = leer/Zufallswizard) |
| +2 … +0x2E | 45 Zauber-Bytes. Oberes Nibble = Maximal-/Startlevel, unteres Nibble = aktuelles Level. |
| +0x2F … +0x3A | **12-Byte-Kreaturentemplate** des Wizards (Aufbau wie in Kap. 2): AP (+0x2F), 0 (+0x30), Stamina (+0x31), Konstitution (+0x32), Combat (+0x33), Defence (+0x34), MR (+0x35), Tragekapazität (+0x36), 0 (+0x37), Trank-Verbrauch (+0x38), 2×Siegpunkte (+0x39), Flags `$8C` (+0x3A) |
| +0x3B … +0x44 | Name, 10 Zeichen (`$FA` = leer) |
| +0x45 | Erfahrungspunkte / 4 |

Beim Spielstart wird bei jedem Zauber das obere Nibble ins untere kopiert (alle Zauber werden aufgefüllt, `$60D9`).

Das Kartensymbol eines Wizards ist `$8B + Spielernummer`: Spieler 1 = `$8C` bis Spieler 4 = `$8F`. Alle Symbole unter `$90` sind Wizards. Spielernummer 0 gehört den vom Spiel gesteuerten Kreaturen (NPCs).

### 3.2 Startwerte

| Wert | Neuer Wizard (Designer) | Zufallswizard |
|---|---:|---:|
| Mana | 80 | 80 |
| Level | 1 | 0 |
| AP | 34 | 34 |
| Stamina | 34 | 34 |
| Konstitution | 34 | 34 |
| Combat | 5 | 6 |
| Defence | 5 | 6 |
| MR | 70 | 90 |
| Tragekapazität | 36 | 36 |
| Trank-Verbrauch | 2 | 2 |
| Siegpunkte-Byte (+0x39) | 20 | 20 |
| Zauberlevel | alle 0 | je Zauber `RND(3)`, also 0–2 |
| Erfahrungspunkte | 600 (+0x45 = 150, ×4) | – |

Beim Speichern setzt der Designer +0x39 auf `4·Level + 15`. Wer einen Wizard besiegt, bekommt also mit steigendem Wizard-Level mehr Siegpunkte (Byte ÷ 2).

### 3.3 Designer: Kosten in Erfahrungspunkten (XP)

**Attribute:** Jeder Klick erhöht um 1 Punkt. Der Punkt kostet `⌊aktueller Wert / Divisor⌋` XP. Beim Verkleinern gibt es die Kosten des neuen Werts zurück. Unter den Startwert geht es nicht.

| Attribut | Start (Lvl 1) | Max | Divisor |
|---|---:|---:|---:|
| Mana | 80 | 200 | 10 |
| Action Points | 34 | 40 | 4 |
| Stamina | 34 | 90 | 8 |
| Constitution | 34 | 90 | 10 |
| Combat | 5 | 30 | 2 |
| Defence | 5 | 30 | 2 |
| Magic Resistance | 70 | 100 | 16 |

(Startwerte jeweils die des Wizards beim Betreten des Menüs. Bei höheren Wizard-Leveln sind sie die gespeicherten Werte.)

**Zauber:** Ein Zauberlevel kostet beim Erhöhen von Level `L` auf `L+1` insgesamt `Basis + Inkrement × L` XP. Das Maximum ist Level 8, nach unten geht es bis zum Startlevel. Die Werte stehen in der Tabelle in Kap. 5.

**XP-Konto:** Byte `+0x45` × 4. Es wird beim Spielstart auf 0 gesetzt (`$60C7`). Nach dem Szenario ersetzt der Punktestand von Wizard 1 das Byte (`$8E44`, `$D05C`), und der Level steigt um 1, wenn Wizard 1 mit einem gestalteten Wizard gewinnt (`$8E31`–`$8E4A`, **unsicher** in den Randbedingungen).

## 4. Rundenablauf

Pro Runde kommt jeder Spieler der Reihe nach dran. Beim Beginn des Zugs eines Spielers läuft für **jede seiner Kreaturen** `$7560` ab (gesichert):

1. **Trank:** Das untere Nibble-Feld (Bits 0–4 von Stat-Byte 3) zählt um 1 herunter. Bei 0 endet die Trankwirkung. Pixies und Trank Typ 3 setzen „unsichtbar“ (Bit 3 in Stat-Byte 4).
2. **Wunden- und Feldschaden:** `Schaden = 2 × Wunden + Feldschaden` (Kap. 8.3). Die Konstitution sinkt entsprechend (Schaden direkt, ohne Defence). Wer auf ≤ 0 fällt, stirbt.
3. **Fliegende Kreaturen:** Stamina −= `⌊übrige AP / 2⌋` (die AP, die sie im letzten Zug nicht ausgegeben haben, `$763C`–`$764D`). Dazu kommt der normale Abzug beim Bewegen (Kosten ÷ 2). Insgesamt kostet das Fliegen also pro Zug etwa `AP/2` Stamina, egal ob die AP genutzt wurden. Das Abfallen auf 0 ist möglich.
4. **AP:** AP = AP-Maximum (am Boden oder fliegend, Speed-Trank ×2). Ist die Stamina `< ⌊Stamina-Max/6⌋`, ist die Kreatur **erschöpft** und bekommt nur die Hälfte. Danach `AP = ⌊AP / ⌊KonMax/KonAkt⌋⌋`, d. h. die Hälfte bei Konstitution < 50 %, ein Drittel bei < 33 % usw. (`$7650`–`$767A`).
5. **Stamina-Erholung:** `Stamina += ⌊StaminaMax / 6⌋`, mit Speed-Trank (Typ 4 oder 6) `⌊StaminaMax / 2⌋`, gedeckelt auf das Maximum.

Danach, einmal pro Wizard-Zug:

- **Mana-Regeneration:** `Mana += ⌊ManaMax / 25⌋` (4 %), gedeckelt (`$76F7`).
- Der Magic-Shield-Zähler des Wizards sinkt um 1 (`$7716`).
- Pro **Runde** (nach dem letzten Spieler): verzauberte Waffen verlieren einen Zähler (`$8BC1`, Kap. 5.3), Feuer, Blob usw. breiten sich aus (`$7DBD`, Kap. 8.3).

### Siegbedingung und Rundenlimit (Szenarioparameter, Szenario 1 / 2 / 3)

| Adresse | Wert | Bedeutung (unsicher in Feinheiten) |
|---|---|---|
| `$D040` / `$D041` | 12/4, 20/5, 44/8 | Das **Portal** erscheint in Runde `D040 + RND(D041)` (`$8CFA`). |
| `$D042` | 12, 15, 10 | Es bleibt so viele Runden offen, danach endet das Spiel (`$8D9D`). |
| `$D043` | 4, 4, 2 | Anzahl Startpositionen (`RND`). |
| `$D044` | 4, 1, 1 | Anzahl möglicher Portalpositionen (`RND`). |
| `$D045` | 10, 10, 10 | Siegpunkte-Bonus beim Verlassen durch das Portal (`$8049`). |

Wer durch das Portal geht, bekommt `Schätze + D045` Siegpunkte. Schätze zählen mit Byte 0 des Objektdatensatzes (Kap. 7).

## 5. Zauber

### 5.1 Allgemeine Regeln (gesichert)

- 45 Zauber, IDs 0–44. ID 45 (`$2D`) bedeutet „kein Zauber“.
- Jeder Zauber hat ein Level `L` (0–8). **Das Level ist Vorrat und Stärke:** Wirken ist nur bei `L ≥ 1` möglich, danach sinkt `L` um 1 (`$82A8`). Level 2 sind also zwei Würfe, der erste mit Stärke 2, der zweite mit Stärke 1 (`$8279`, `$82AB`).
- **Mana-Kosten** = `Basis × (L + 1)` (`$81F3`–`$81FD`). Ist das Mana kleiner als die Kosten, bricht das Wirken ab.
- Wirken kostet **8 AP** (mindestens 8 AP nötig, `$8146`/`$82CB`).
- **Reichweite** der Zielwahl: `2L + 7` Entfernungseinheiten (Kap. 1), `$866A`. Ausnahmen: Magic Eye `3L + 10`, Teleport `2L + 30`.
- Alle Effekte nutzen das Level **vor** dem Dekrement.

### 5.2 Tabelle

Die Namen der IDs 25–44 stammen aus der Reihenfolge im Handbuch. Sie passt zu den Handlern in `TBL_82FA` (IDs 36–44) und zu den Zutaten in `$84CE` (IDs 25–31). Bei den IDs 34/35 folgt die Zuordnung Vine/Flood aus den gesetzten Tiles `$37`/`$36` (Kap. 8.3, `$7F63`).

| ID | Zauber | Mana-Basis | XP-Basis | XP je Stufe |
|---:|---|---:|---:|---:|
| 0 | Gold Dragon | 23 | 47 | 23 |
| 1 | Green Dragon | 19 | 39 | 19 |
| 2 | Red Dragon | 15 | 31 | 15 |
| 3 | Pixie | 3 | 7 | 4 |
| 4 | Dwarf | 2 | 5 | 2 |
| 5 | Goblin | 3 | 7 | 3 |
| 6 | Troll | 6 | 13 | 6 |
| 7 | Giant | 8 | 17 | 9 |
| 8 | Centaur | 5 | 10 | 6 |
| 9 | Unicorn | 4 | 10 | 5 |
| 10 | Pegasus | 5 | 12 | 7 |
| 11 | Gryphon | 9 | 19 | 10 |
| 12 | Elephant | 8 | 17 | 8 |
| 13 | Gorilla | 4 | 8 | 4 |
| 14 | Lion | 5 | 10 | 5 |
| 15 | Bear | 6 | 12 | 6 |
| 16 | Crocodile | 5 | 11 | 5 |
| 17 | Giant Bat | 2 | 5 | 2 |
| 18 | Harpy | 5 | 11 | 5 |
| 19 | Giant Spider | 11 | 22 | 11 |
| 20 | Zombie | 5 | 16 | 8 |
| 21 | Ghost | 8 | 14 | 7 |
| 22 | Vampire | 14 | 29 | 15 |
| 23 | Spectre | 14 | 29 | 14 |
| 24 | Demon | 19 | 40 | 21 |
| 25 | Strength Potion | 1 | 8 | 4 |
| 26 | Protection Potion | 1 | 6 | 4 |
| 27 | Invisibility Potion | 2 | 16 | 7 |
| 28 | Speed Potion | 1 | 8 | 4 |
| 29 | Flying Potion | 1 | 5 | 3 |
| 30 | Super Potion (im Amiga-Handbuch „Bomb“) | 3 | 16 | 8 |
| 31 | Healing Potion | 1 | 5 | 3 |
| 32 | Magic Fire | 3 | 15 | 5 |
| 33 | Gooey Blob | 2 | 12 | 5 |
| 34 | Tangle Vine | 3 | 10 | 4 |
| 35 | Flood | 3 | 10 | 4 |
| 36 | Enchant | 3 | 10 | 5 |
| 37 | Subversion | 3 | 10 | 4 |
| 38 | Curse | 2 | 8 | 3 |
| 39 | Magic Attack | 3 | 10 | 6 |
| 40 | Magic Bolt | 1 | 6 | 3 |
| 41 | Magic Lightning | 2 | 12 | 5 |
| 42 | Teleport | 3 | 16 | 2 |
| 43 | Magic Eye | 1 | 8 | 1 |
| 44 | Magic Shield | 2 | 6 | 3 |

Mana-Basis: Tabelle `$B4BE`. XP: Tabelle `$DC84` im Designer (Basis, Inkrement).

### 5.3 Wirkung

**Kreaturen beschwören (ID 3–24)** (`$8342`):

- Es erscheinen `L` Kreaturen. Jede wird auf ein zufällig gewähltes freies Nachbarfeld des Wizards gesetzt: bis zu 40 Versuche mit `RND(8)` je Kreatur.
- Findet sich kein Platz, ist das Mana verloren.
- Die neue Kreatur startet mit vollen AP, Stamina und Konstitution.
- Beschwören ist nur „am Boden“ (CAST-G) möglich.

**Drachen (ID 0–2)** (`$8320`):

- Der Wizard muss auf einem **leeren Kessel** (Tile `$67`) und einem **Drachenkraut** (Objekt `$53`) stehen. Das Kraut wird verbraucht.
- Der Kessel wird zum Drachentrank-Tile `$68 + ID`. Danach erscheinen `L` Drachen wie bei normalen Kreaturen.
- Steht der passende Drachentrank-Kessel schon da, entfällt das Kraut.

**Tränke (ID 25–31)** (`$8465`):

- Der Wizard muss auf einem leeren Kessel stehen und auf der passenden Zutat.
- Die Zutat steht in `$84CE`: Strength = Mistletoe (`$54`), Protection = Clover (`$57`), Invisibility = Crystal (`$81`), Speed = Sulph (`$56`), Flying = Fairy Wing (`$58`), Super = Ambergris (`$55`), Healing = Apple (`$7F`).
- Der Kessel wird zu Tile `$6B + (ID−25)` und enthält **L + 3 Schlucke** und die Stärke `L − 1`. (Byte 5 = `(L−1) + (L+3)·16`.)
- Mit **Fill** (4 AP) füllt man eine leere Phiole (`$72`) aus dem Kessel, sie wird zu Tile `$73 + Typ−1`. Ein Schluck wird dabei abgezogen.

**Magic Fire / Gooey Blob (ID 32/33)** (`$8442`, `$86B2`, `$7BC4`, **grob**):

- Zielwahl wie Standard. Kein Sichtkontakt nötig.
- **Zündung beim Wirken:** Das Zielfeld fängt Feuer (bzw. Blob), wenn `RND(10 − L) < Entzündbarkeit` des Terrains (Nibble 0–15 aus Terrain-Byte 3: unten = Fire, oben = Blob).
- **Schaden** pro Runde (Kap. 8.3): Feuer `25 + 2·F`, Blob `16 + 2·(B − 1)`. `F`/`B` ist das Level des jeweiligen Zaubers beim **letzten Wirken** (vor dem Dekrement, `$872E`–`$8742`) und gilt für alle Felder des Wizards. Jedes weitere Wirken senkt es und schwächt damit das vorhandene Feuer. Eigene Kreaturen sind immun.
- **Ausbreitung** (einmal pro Runde für jedes Feuer- bzw. Blob-Feld, `$7DBD`–`$7E4A`, `$7EA5`). `Lv` ist `F` beim Feuer und `B − 1` beim Blob (also 1–8 bzw. 0–7). Zuerst läuft die Ausbreitung, danach die Prüfung, ob das Feld erlischt:
  1. Für jedes der 8 Nachbarfelder: `RND(70 − 3·Lv) + 1 ≤ f` ⇒ das Nachbarfeld brennt (bzw. wird Blob). `f` ist die Entzündbarkeit des Nachbarterrains (0–15). Der Wurf gilt nur, wenn die Karte dort noch Terrain zeigt (Wert < `$50`, also kein Objekt oder Wesen auf der obersten Ebene, `$7BD7`).
  2. `S` = Summe der `f` aller 8 Nachbarn. Das Feld **brennt weiter**, wenn `RND(T) < ⌊S/4⌋ + 40` gilt. Sonst erlischt es und wird zu Tile 0 (`$D049`/`$D04A` sind nirgends gesetzt, also 0).
  3. `T` je Level: Feuer `T = [160, 127, 106, 90, 79, 70, 63, 57]` für `F = 1…8` (`$B4B6`–`$B4BD`). Blob `T = [114, 100, 88, 80, 73, 66, 61, 57]` für `B − 1 = 0…7` (`$B4AD`–`$B4B4`).
  - Ein höheres Level macht das Feld zäher: `T` sinkt, also steigt die Überlebenschance `min(1, (⌊S/4⌋ + 40) / T)`. Beispiel Feuer ohne brennbare Nachbarn (`S = 0`): Level 1 → 40/160 = 25 %, Level 8 → 40/57 ≈ 70 %. Auch die Ausbreitung `f / (70 − 3·Lv)` wächst mit dem Level.
  - Alle Formeln sind gelesen. Die Rollen von `S` und `T` (Überleben statt Erlöschen) leite ich aus dem Vergleich `RND(T) < …` ab (`$7E27`–`$7E2F`), das ist **nicht im Spiel gegengeprüft**.
  - **Objekte auf dem Boden** (kein Besitzer) werden einmal pro Runde geprüft (`$7BE1`–`$7C1A`): Liegen sie auf einem Flood-Feld (`$36`), sind sie immer weg. Liegen sie auf einem Feuerfeld (`$38`–`$41`), sind sie weg, **außer** das Objekt hat Flag Bit 3 (feuerfest).

**Tangle Vine / Flood (ID 34/35)** (`$7F09`):

- Wirkt auf ein **9×9-Quadrat** um das Ziel. Felder mit Entfernung `D ≤ L + 3` sind betroffen.
- Wahrscheinlichkeit je Feld: `RND(18 − 2L + 2·D) < Empfänglichkeit` des Terrains (Nibble 0–15 aus Terrain-Byte 2: oben = Vine, unten = Flood).
- Vine setzt Tile `$37`, Flood setzt Tile `$36` (**Schreibweise der Zuordnung laut Code**: Nibble unten → `$36`, Nibble oben → `$37`).

**Enchant (36)** (`$87F1`):

- Verwandelt Waffen im Zielfeld (am Boden oder bei einer Kreatur) in magische Waffen: Tile `+7` (`$59–$5F` → `$60–$66`).
- Dauer: `L + 3` Runden, dann wird die Waffe zurückverwandelt.

**Subversion (37)** (`$895C`):

- `Erfolg ⇔ RND(8L + 55) ≥ MR` des Ziels.
- Bei Reiter und Reittier zählt das höhere MR. Wizards (Tile < `$90`) sind immun.
- Kosten bei Fehlschlag: normal.

**Curse (38)** (`$89D4`):

- `Erfolg ⇔ RND(8L + 55) + 10 ≥ MR`. Dann sind **7 Wunden** gesetzt (Maximum). Gegen Wizards möglich.

**Magic Attack (39)** (`$8A13`):

- Mit `n = 8L + 55`. Alle Kreaturen vom selben Typ wie das Ziel mit Entfernung `D < 2L + 1` zum Ziel werden einzeln geprüft.
- `RND(n) ≥ MR` ⇒ die Kreatur stirbt (Schaden 255). Trifft auch eigene Kreaturen.

**Magic Bolt (40)** (`$885F`):

- Angriffswert `A = 4L + 25`. Schaden `RND(min(255, 2(A+1))) − Defence_eff` des Ziels. Kein Schaden bei ≤ 0.
- Trifft auch Untote.

**Magic Lightning (41)** (`$88AB`):

- Angriffswert `A = 4L + 30`. Wirkt auf das Zielfeld und die 8 Nachbarfelder.
- Kreaturen: Schaden wie Bolt. Terrain: wird zerstört, wenn `RND(2A) ≥ Zähigkeit` (Kap. 8.4).
- Das Zielfeld darf kein massives Terrain sein.

**Teleport (42)** (`$8AE1`):

- Wahl des Ziels bis Entfernung `2L + 30`.
- Wird `D ≥ 2L`, liegt der Landepunkt zufällig daneben: `n = ⌊(D − 2L)/2⌋ + 1`, jede Achse verschiebt sich um `RND(n) − ⌊n/2⌋` (mit Umbruch 36).
- Das Ziel muss frei und begehbar sein. Sonst scheitert der Zauber (Meldung).
- Danach hat der Wizard **0 AP**.
- Teleport mit **CAST-A** (in die Luft) geht nur, wenn der Wizard unter einem **Flying-Trank** (Typ 5) steht (`$8AE2`–`$8AEF`: `Stat-Byte 3 & $E0 = $A0`). Mit CAST-G gibt es keine solche Bedingung. Ob „angebunden“ eine Rolle spielt, habe ich nicht gefunden.

**Magic Eye (43)** (`$8A8A`):

- Sicht von einem Zielfeld bis Entfernung `3L + 10` für den aktuellen Zug. Enthüllt auch Unsichtbare.

**Magic Shield (44)** (`$8BEC`):

- Zähler des Wizards `= L + 1`. Er sinkt mit jedem Wizard-Zug um 1.
- Solange er > 0 ist: **Defence-Bonus `4·(L+1) + 12`** (gedeckelt auf 255, `$9466`).

## 6. Kampf

### 6.1 Effektive Werte (`$9342`)

```
Combat_basis = Template.Combat
             + 20           (Trank Typ 1 oder 6)
             + Waffe        (Item.Combat, wenn Kreatur Waffen nutzen darf (Flag 04),
                             Item Bit0 gesetzt und Item "in Benutzung")
Defence_basis = Template.Defence
              + 25          (Trank Typ 2 oder 6)
              + Schild/Item (größter Item.Defence aller getragenen Items mit Bit0,
                             Kreatur mit Flag 04; wird NICHT addiert)
              + 4·(L+1)+12  (Magic Shield, Wizard)

Combat_eff  = max(1, ⌊Combat_basis  / ⌊KonMax / KonAkt⌋⌋)
Defence_eff = max(1, ⌊Defence_basis / ⌊KonMax / KonAkt⌋⌋)
```

`⌊KonMax / KonAkt⌋` ist 1 bei Konstitution über 50 %, 2 bei 34–50 %, 3 bei 26–33 % usw.

### 6.2 Nahkampf (`$84EC`, `$85BD`)

- Der Angreifer braucht **8 AP und 8 Stamina** und zahlt beides (Fehlermeldungen: „NOT ENOUGH ACTION POINTS“ / „…STAMINA“).
- `Schaden = RND(min(255, 2·(Combat_eff + 1))) − Defence_eff`. Bei ≤ 0 ist es ein Fehlschlag, Kreaturen verlieren nichts.
- **Gegenschlag:** Der Verteidiger darf, wenn er noch ≥ 4 AP und ≥ 4 Stamina hat, zurückschlagen, zahlt 4 AP + 4 Stamina und rechnet mit denselben Formeln in umgekehrter Rolle.
- **Untote** nehmen nur Schaden von Untoten, magischen Waffen (Tile `$60–$66`, außer Bogen `$63`) und Zaubern (`$85EB`, `$85F1`).

### 6.3 Schaden und Wunden (`$7755`)

- `Konstitution −= Schaden`. Bei ≤ 0 stirbt die Kreatur.
- **Wunde**, wenn `Schaden > ⌊KonMax / 4⌋` (mehr als 25 %) und Wunden < 7: Wunden + 1.
- Jede Wunde kostet **2 Konstitution pro Zug** (Kap. 4). Nur der Heiltrank (Typ 7) heilt Wunden.

### 6.4 Gelände angreifen und Wurfwaffen

- Gegen **unpassierbares Terrain** (Kosten `FF` im Terraineintrag): `Erfolg ⇔ RND(2·Combat_eff) ≥ Zähigkeit` (`$68A9`). Kosten **6 AP + 6 Stamina**. Der Versuch wird nur gestartet, wenn `1,5 · Combat_eff ≥ Zähigkeit` (`$9C18`).
- **Wurf** (THROW, `$7A28`): kostet **8 AP**. Der Angriffswert ist der „thrown combat“-Wert des Objekts (Kap. 7). Schaden wie Nahkampf: `RND(min(255, 2(Wurfwert+1))) − Defence_eff`.
- Wurfweite: `min(36, ⌊2·Combat_eff / Gewicht⌋ + 5)` Entfernungseinheiten (`$7AD4`).
- **FIRE** (Drachenfeuer und Bogen, Handler `$7B31`/`$7B38`) hat eigene Parameter in der Tabelle `$B60E` (8 Byte je Eintrag, geladen von `$B5F7`). Der Bogen hat im Objektdatensatz keinen Wert (Byte 4 = 0), die Werte kommen von hier:

  | Schütze | Reichweite (Einheiten) | Angriffswert | trifft Untote |
  |---|---:|---:|---|
  | Drache (Tile `$90–$92`) | 12 | 35 | ja (Feuer) |
  | Bogen (`$5C`) | 16 | 15 | nein |
  | Magischer Bogen (`$63`) | 22 | 30 | ja |

  Schaden wie beim Wurf: `RND(min(255, 2·(Wert+1))) − Defence_eff`. Kosten **8 AP**. Der Drache entzündet zusätzlich das Zielfeld, wenn `RND(20) < Entzündbarkeit` (Fire-Nibble, `$7BAD`–`$7BC8`). Der Bogen entzündet nichts.
- **Keine Bombe im Spectrum:** Das Spiel kennt kein Bomb-Potion. Im Spectrum-Text gibt es das Wort „BOMB“ nicht, dafür „SUPER“. Der sechste Trank (Ambergris) ist der **Super-Trank** (Kap. 8.2). Die Bombenexplosion aus dem Amiga-Handbuch fehlt also, im Engine- und im Szenariocode (alle drei Szenarien geprüft) gibt es dafür keinen Code.

### 6.5 Tod, Beute, Siegpunkte (`$77A4`–`$7871`, `$8037`)

- Eine tote Kreatur wird aus der Karte genommen. Alles, was sie trug (Objekte mit ihrem Index als Besitzer), fällt auf das Feld. Ein Reiter eines getöteten Reittiers bleibt auf dem Feld stehen.
- **Siegpunkte** gibt es nur, wenn der Töter einem Wizard gehört (Besitzer ≠ NPC) und das Opfer nicht demselben Wizard gehört. Gutgeschrieben wird `Byte 10` des Opfers (= 2 × SP der Kreaturentabelle), bei einem Kill durch eine **beschworene Kreatur** nur die Hälfte (`⌊Byte 10 / 2⌋`, `$77F5`–`$77FE`). Der Stand ist auf 255 gedeckelt. Opfer ist ein Wizard: `Byte +0x39` seines Datensatzes.
- Beim Verlassen durch das Portal (`$8037`–`$8056`): `Score += Schätze + D045`, jeweils auf 255 gedeckelt.
- **Wizard-Tod:** Der Zähler der toten Spieler steigt. Sind alle Spieler tot, endet das Szenario. Im Spiel mit gestaltetem Wizard endet es sofort, wenn Spieler 1 (`$8C`) stirbt (`$8C4E`–`$8C5F`).

### 6.6 Reiter und Reittier (`$66CD`, `$CA2B`, `$718E`, `$71D3`)

Aufsitzen und Auswahl:

- **Voraussetzungen** für „Ride“ (`$6B8B`–`$6BC3`): Die ausgewählte Kreatur kann reiten (Flag `08`), sie ist nicht im Flugmodus, sie hat mindestens 10 AP, das Wesen auf demselben Feld ist ein Reittier (Flag `01`) und trägt noch nichts „in Benutzung“ (also keinen Reiter). Die Kosten sind 10 AP, außer beim erneuten Aufsitzen nach „Select Rider“.
- **Reiter und Reittier** teilen sich ein Feld. Der Reiter bekommt im Positionssatz den Index des Reittiers als Besitzer-Byte (Byte 4). Das Reittier wird zur ausgewählten Kreatur (`$71D0`).
- **Select Rider** macht den Reiter wieder zu einer eigenständigen Kreatur auf dem Feld (Byte 4 = Höhenmodus `$FF`/`$FE`, `$71E3`–`$71EA`). Er handelt mit **eigenen** AP, Stamina und Werten (auch mit seinen Waffen). Laut Handbuch darf er so angreifen und zaubern, während er auf dem Reittier steht. Für andere Kreaturen auf einem Freundesfeld gilt das nicht.

Bewegung:

- Bewegt man das Reittier, kostet das **nur dessen** AP und Stamina. Reiter und alles, was er trägt, ziehen mit (`$B561`, `$B56A`–`$B58C`). Die AP des Reiters bleiben unberührt. Ein Wizard kann also reiten und danach als Reiter mit vollen AP zaubern.

Angriffe und Zauber (gelesen):

- Die Zielsuche auf einem Feld (`$66CD`) **überspringt** jedes Wesen, dessen Byte 4 kleiner als `$FE` ist, also Reiter. Je Feld bleiben höchstens zwei Wesen, eines am Boden (`$FF`) und eines in der Luft (`$FE`).
- Nahkampf (`$CA2B`), Wurf (`$7A7D`) und alle Einzelziel-Zauber (`$888F`: Bolt, Lightning je Feld, Curse, Subversion, Magic-Attack-Zielwahl) treffen das Wesen **auf der Höhe des Angreifers** bzw. des Zaubers (CAST-A = Luft, CAST-G = Boden). Bei Reiter/Reittier ist das **das Reittier**.
- Der Reiter wird dadurch nicht einzeln getroffen. Er nimmt weder Schaden noch Curse durch einen Einzelzauber.
- Das Reittier kämpft und verteidigt mit **seinen eigenen** Werten. Waffen und Schilde des Reiters zählen nicht, weil Reittiere kein Flag `04` haben (`$93D4`, `$9422`).
- **Subversion** (`$8979`–`$8998`): Es zählt das höhere MR von Reittier und Reiter. Ist der Reiter ein Wizard (Symbol < `$90`), scheitert der Zauber. Bei Erfolg wechseln beide den Besitzer (`$89BD`, auch `$897A`–`$89B1`).
- **Magic Attack** durchsucht alle Wesen vom selben Typ im Radius und trifft deshalb auch Reiter einzeln (`$8A2B`–`$8A7E`).

Tod:

- Fällt das Reittier, bleibt der Reiter auf dem Feld und ist wieder ein normales Wesen (`$7829`–`$785B`). Was das Reittier trug, fällt zu Boden.
- Der Reiter kann nur durch Magic Attack, Wundschaden zu Rundenbeginn oder nach „Select Rider“ durch normale Angriffe fallen. Das Reittier bleibt dann unverändert.

## 7. Objekte

Datensatz: 6 Byte je Objekt, Zeiger im Szenarioheader (`$D005`), Tile `$50 + Index`.

| Byte | Bedeutung |
|---:|---|
| 0 | Schatzwert (Siegpunkte beim Verlassen durch das Portal, `$8027`) |
| 1 | Gewicht |
| 2 | Combat-Bonus (bei Nahrung: Konstitution, Stamina = ×4) |
| 3 | Defence-Bonus (bei Nahrung: Mana) |
| 4 | Wurfwert (thrown combat) |
| 5 | Flags: Bit 0 = Combat-/Defence-Bonus zählt (Waffe/Schild), Bit 1 = Fernwaffe (nur Bogen), Bit 2 = essbar (`$74DE`), Bit 3 = feuerfest (überlebt Feuer am Boden; gesetzt bei Schätzen, Kessel, Schlüsseln und allen magischen Waffen) |

Waffen (Szenario 1 und 2 gleich, sofern nicht anders vermerkt). Magische Waffen (`$60–$66`) haben **doppelte** Werte:

| Tile | Objekt | Gewicht | Combat | Defence | Wurf |
|---:|---|---:|---:|---:|---:|
| `$59` | Sword | 10 | 10 | 4 | 16 |
| `$5A` | Knife | 3 | 4 | 1 | 20 |
| `$5B` | Shield | 8 | 0 | 13 | 0 |
| `$5C` | Bow | 4 | – | – | – |
| `$5D` | Spear | 5 | 8 | 4 | 23 |
| `$5E` | Club (Sz. 1) | 9 | 5 | 1 | 8 |
| `$5E` | Slayer (Sz. 2) | 9 | 12 | 4 | 8 |
| `$5E` | Ninja Star (Sz. 3) | 4 | 0 | 0 | 25 |
| `$5F` | Axe | 7 | 9 | 0 | 28 |
| `$60` | Magic Sword | 10 | 20 | 8 | 32 |
| `$61` | Magic Knife | 3 | 8 | 2 | 40 |
| `$62` | Magic Shield | 8 | 0 | 26 | 0 |
| `$64` | Magic Spear | 5 | 16 | 8 | 46 |
| `$65` | Magic Club / Magic Slayer (Sz. 2: Combat 30, Defence 12) / Magic Ninja Star (Sz. 3: Wurf 50) | 9 / 9 / 4 | 10 / 30 / 0 | 2 / 12 / 0 | 16 / 16 / 50 |
| `$66` | Magic Axe | 7 | 18 | 0 | 56 |

Sonstiges:

| Tile | Objekt | Hinweis |
|---:|---|---|
| `$50`–`$52` | Schätze | Wert (Szenario 1): 16 / 12 / 8, Gewicht 6 / 3 / 3. Szenario 3: `$50` = 30. |
| `$53` | Drachenkraut | Gewicht 2 |
| `$54`–`$58` | Zutaten (Mistletoe, Ambergris, Sulph, Clover, Fairy Wing) | Gewicht 1 |
| `$67` | Leerer Kessel | Gewicht 15 |
| `$68`–`$6A` | Kessel mit Drachentrank (Gold/Green/Red) | nicht tragbar |
| `$6B`–`$71` | Kessel mit Trank, Typ 1–7 | nicht tragbar |
| `$72` | Leere Phiole | Gewicht 2 |
| `$73`–`$79` | Phiole mit Trank, Typ 1–7 | Gewicht 4 |
| `$7A`/`$7B`/`$7C` | Truhe geschlossen / offen / leer | nicht tragbar |
| `$7D`/`$7E` | Schlüssel (Tür / Truhe) | Gewicht 1 |
| `$7F` | Apfel (Zutat des Heiltranks, `$84CE`) | Essen: **+10 Konstitution**, +40 Stamina |
| `$80` | Nahrung (Name nicht gesichert) | Essen: **+20 Konstitution**, +80 Stamina, **+5 Mana** |
| `$82` | Nahrung (Name nicht gesichert) | Essen: +8 Mana (Szenario 2: +25 Konstitution, +25 Mana) |

Essen (`$74E5`): `Stamina += 4·Byte2` und `Konstitution += Byte2`, jeweils bis zum Maximum. Bei Wizards kommt `Mana += Byte3` dazu (bis zum Maximum).

`$7D` öffnet verschlossene Türen `$1F–$21` (sie werden zu `$22–$24`). `$7E` öffnet die Truhe `$7A` (`$7A` → `$7B`, dann `$7C`). Die Namen der Objekte folgen aus der Reihenfolge der Textliste und aus den Tile-Nummern im Code (Zutaten, Waffen, Kessel). Bei den übrigen sind sie nicht abgesichert.

## 8. Aktionen, Tränke, Terrain

### 8.1 AP- und Stamina-Kosten (gesichert)

| Aktion | AP | Stamina | Quelle |
|---|---:|---:|---|
| Bewegen (Boden) | Terrain-Kosten, **diagonal × 1,5** (`Kosten + ⌊Kosten/2⌋`) | Kosten ÷ 2 (mit dem Diagonalaufschlag) | `$B4EB`, `$CAEA`–`$CAF9` |
| Bewegen (Boden) auf Terrain, das zum Typ der Kreatur passt (Wood/Water/Rock) | 4, diagonal 6 | 2, diagonal 3 | `$CAB1`–`$CAC9` |
| Bewegen (fliegend) | 4, diagonal 6 | 2, diagonal 3 | `$CACE` |
| Nahkampf (Angreifer) | 8 | 8 | `$84F5`–`$850D` |
| Gegenschlag | 4 | 4 | `$8548`–`$855E` |
| Gelände angreifen | 6 | 6 | `$6873` |
| Zauber wirken | 8 | – | `$82CB` |
| Werfen | 8 | – | `$7AAE` |
| Aufheben (Pick up) | 8 | – | `$70EE` (Handler `$706A`) |
| Ablegen (Drop) | 0 | – | `$6FF1` |
| Benutztes Objekt wechseln (Change) | 4 (nur bei Wechsel) | – | `$7063` (Handler `$700D`) |
| Gegenstand benutzen (Use: Schlüssel an Tür/Truhe) | 4 | – | `$736A` |
| Essen (Eat) | 4 | – | `$74E5` |
| Trinken (Drink) | 4 | – | `$73D0` |
| Phiole füllen (Fill) | 4 | – | `$74CC` |
| Aufsitzen (Ride) | 10 (entfällt beim erneuten Aufsitzen nach „Select Rider“) | – | `$7196` |
| Abfliegen (Fly) | 6 | – | `$6E5D` |
| Landen (Land) | 0 | – | `$6E1B` |

- **Abfliegen** (`$6DC2`, `$6E01`–`$6E17`) ist nur möglich, wenn die Kreatur Flug-AP hat (Byte 1 der Vorlage ≠ 0), mindestens **6 AP** besitzt, nicht „angebunden“ ist, **kein anderes Wesen** auf dem Feld steht und das Terrain es erlaubt: nicht Tile `$37` (Tangle Vine), nicht `$42`–`$49` (Blob), nicht mit Terrain-Bit 4.
- **Landen** (`$6DC2`, `$6DE0`–`$6DF3`) ist nur möglich, wenn die Kreatur nicht „angebunden“ ist, **kein anderes Wesen** auf dem Feld steht und das Terrain nicht Bit 5 hat. Landen kostet keine AP, die AP werden aber proportional in den Bodenvorrat umgerechnet (Kap. 2).
- Aufheben prüft das Gewicht: `Summe Gewicht + neues ≤ Tragekapazität` (Meldung „TOO HEAVY“).
- Eine **angebundene** Kreatur („engaged“) kann nicht ziehen. Wann sie angebunden wird und wann sich das löst, steht in Kap. 11.7.

### 8.2 Tränke trinken (`$73D0`)

Typ aus Tile der Phiole: `Typ = Tile − $72` (1–7). Dauer in Zügen:

```
Dauer = ⌊(3·(L−1) + 10) / Trank-Verbrauch der Kreatur⌋        (L = Zauberlevel beim Brauen)
```

Die Dauer steht in Stat-Byte 3 (Bits 0–4), der Typ in Bits 5–7 (`$743C`–`$744B`).

| Typ | Trank | Wirkung |
|---:|---|---|
| 1 | Strength | Combat +20 |
| 2 | Protection | Defence +25 |
| 3 | Invisibility | unsichtbar für Gegner |
| 4 | Speed | AP ×2 am Rundenbeginn, Stamina-Erholung ×3 |
| 5 | Flying | Fliegen (hat die Kreatur keine AP für Flug, bekommt sie 2 × AP am Boden) |
| 6 | Super (Ambergris) | Strength + Protection + Speed zugleich (Combat +20, Defence +25, AP ×2, Stamina-Erholung ×3) |
| 7 | Healing | Konstitution und Stamina auf Maximum, alle Wunden geheilt, keine Dauer |

### 8.3 Terrain-Schaden pro Runde (`$75D2`–`$7620`)

| Tile | Quelle | Schaden pro Runde |
|---:|---|---|
| `$37` | Tangle Vine | 8 |
| `$38`–`$41` | Feuer (Tile `$38 + 2·Wizard`) | `25 + 2·Fire-Level` des Besitzer-Wizards. Wizard-eigenes Feuer: 0. |
| `$42`–`$49` | Gooey Blob (Tile `$40 + 2·Wizard`) | `16 + 2·(Blob-Level − 1)`. Eigene: 0. |

Der Schaden ignoriert Defence. Das Feuer-Level ist das Zauberlevel des Besitzers beim jeweils letzten Zauber (`$872E`).

### 8.4 Terraintabelle

5 Byte je Tile, Beginn `$D208` (ein Datensatz je Tile `$00–$4D`).

| Byte | Bedeutung |
|---:|---|
| 0 | Bewegungskosten (AP). `$FF` = unpassierbar. |
| 1 | Zähigkeit für den Angriff auf unpassierbares Terrain. `$FF` = unzerstörbar. |
| 2 | Empfänglichkeit: unten = Flood-Zauber, oben = Vine-Zauber (0–15) |
| 3 | Entzündbarkeit: unten = Feuer, oben = Blob (0–15) |
| 4 | Typ: Bit 0 = Wood, Bit 1 = Water, Bit 2 = Rock. Bit 3 (`$08`) = **Flug gesperrt**, fliegende Kreaturen dürfen das Feld nicht betreten (Meldung „CAN'T FLY THERE“, `$CACE`–`$CAD8`). Bit 4 (`$10`) = **kein Abheben** (drinnen, unter Dach, `$6E01`). Bit 5 (`$20`) = **keine Landung** (`$6DE9`). Die Bits 6 und 7 sind nicht entschlüsselt. |

Die Tabelle ist **je Szenario verschieden**, außer den festen Tiles `$36`–`$49`:

| Tile | Kosten | Zähigkeit | Bytes 2/3 | Typ | Bedeutung |
|---:|---:|---:|---|---|---|
| `$36` | 12 | `FF` | `20 50` | Water | überschwemmtes Feld (Flood) |
| `$37` | `FF` | 40 | `01 04` | `$10` | Tangle Vine |
| `$38`–`$41` | 16 | `FF` | `0A 00` | `$20` | Feuer |
| `$42`–`$49` | `FF` | 50 | `04 04` | `$20` | Gooey Blob |

Szenario 1 (Auszug, Tilebereiche, mit Namen aus der Textliste, Zuordnung nicht überall sicher):

| Tile | Kosten | Zähigkeit | Bytes 2/3 | Typ |
|---|---:|---:|---|---|
| `$00` | 4 | `FF` | `4E 30` | – |
| `$01` | 8 | 70 | `9A 62` | Rock |
| `$02`–`$03` | 4 | 60 | `EF BC` | – |
| `$04`–`$0F` | 4 | 65 | `EC AB` | – |
| `$10`–`$17`, `$19`–`$1E` | `FF` | `FF` | `00 01` | `$30` |
| `$18` | 4 | 80 | `DB AA` | `$30` |
| `$1F`–`$24` | `FF` | 80 | `00 05` | `$30` |
| `$25`–`$27` | 4 | 80 | `C0 78` | `$25`–`$27` |
| `$28`–`$2D` | 12 | `FF` | `4F 60` | Water |
| `$2E` | 6 | 80 | `A5 6E` | Wood |
| `$2F` | 8 | 80 | `94 5C` | Wood |
| `$30` | 6 | 40 | `67 7F` | – |
| `$31`–`$32` | 16 | 200 | `3E A3` | – |
| `$33` | 10 | `FF` | `53 61` | Rock |
| `$34` | 4 | `FF` | `00 00` | `$30` |
| `$35` | 4 | `FF` | `00 00` | – |
| `$4A`–`$4D` | `FF` | `FF` | `00 01` | `$30` |

Szenario 2 (Tilebereiche, Kosten / Zähigkeit / Bytes 2,3 / Typ):

| Tile | Kosten | Zähigkeit | Bytes 2/3 | Typ |
|---|---:|---:|---|---|
| `$00` | 4 | `FF` | `4E 31` | `$38` |
| `$01`–`$13`, `$16`–`$17` | `FF` | `FF` | `00 00` | `$08` |
| `$14`–`$15` | 4 | `FF` | `A4 FF` | `$38` |
| `$18`–`$1D` | 4 | `FF` | `00 00` | `$38` |
| `$1E` | 8 | `FF` | `20 20` | `$38` |
| `$1F`–`$24` | `FF` | 80 | `00 05` | `$38` |
| `$25`–`$27` | 4 | 80 | `C0 65` | `$38` |
| `$28` | 4 | `FF` | `00 00` | `$38` |
| `$29`–`$34` | 0 | 0 | `00 00` | `$38` |
| `$35` | 4 | `FF` | `00 00` | `$38` |

Szenario 3:

| Tile | Kosten | Zähigkeit | Bytes 2/3 | Typ |
|---|---:|---:|---|---|
| `$00` | 4 | `FF` | `4E 30` | – |
| `$01` | 4 | `FF` | `00 00` | `$30` |
| `$02`–`$03` | 4 | 60 | `EF BC` | – |
| `$04`, `$0A`, `$0D`–`$0E`, `$18` | 4 | `FF` | `00 00` | `$30` |
| `$05`–`$06`, `$10`–`$17`, `$1A`–`$24` | `FF` | `FF` | `00 00` | `$30` |
| `$07` | 8 | `FF` | `00 00` | `$30` |
| `$08`, `$2B` | 4 | `FF` | `00 00` | – |
| `$09` | `FE` | `FF` | `00 00` | `$30` |
| `$0B` | `FF` | 50 | `00 00` | `$70` |
| `$0C` | 4 | `FF` | `00 0F` | `$30` |
| `$19` | `FF` | 5 | `00 00` | `$70` |
| `$25`–`$2A` | 4 | `FF` | `00 00` | `$30` |
| `$2E` | 6 | 80 | `A5 6E` | Wood |
| `$2F` | 8 | 80 | `94 5C` | Wood |
| `$0F`, `$2C`–`$2D`, `$30`–`$33` | 0 | 0 | `00 00` | – |
| `$34` | `FF` | `FF` | `0F 00` | `$30` |
| `$35` | 4 | `FF` | `00 00` | – |
| `$36` | 12 | `FF` | `00 00` | `$30` |
| `$37` | `FF` | 40 | `01 04` | `$50` |
| `$4A`–`$4D` | `FF` | `FF` | `00 00` | `$30` |

Kosten `0` sind vermutlich ungenutzte Tiles. Kosten `FE` (Tile `$09` in Szenario 3) gelten nicht als „unpassierbar“, denn der Test nimmt nur `FF` (`$CADF`–`$CAE3`). Das Feld verlangt aber 254 AP und ist damit praktisch nicht betretbar (und es gibt keinen Angriff auf das Terrain). Die oberen Bits im Typ-Byte (`$10`/`$20`/`$30`/`$38`/`$50`/`$70`) sind nicht entschlüsselt.

## 9. Gelesene, aber nicht geklärte Punkte

Geschlossen gegenüber der ersten Fassung: Feuer-/Blob-Ausbreitung (5.3), Bogen und Drachenfeuer (6.4), Flug-Stamina (Kap. 4), Siegpunkte und Tod (6.5), Terraintabellen aller Szenarien (8.4). Der Spectrum hat **keine** „Game Length“-Option: Das Menü fragt nur nach 1–4 Spielern (`$6094`–`$60A8`). Die Parameter `$D040`–`$D043` sind feste Szenariowerte.

Noch offen:

- **Namen von Terrain und Objekten.** Die Phrasentabelle (`$D00B`, Wörter ab `$D00D`) ist nicht einfach nach Tile-Nummer indiziert. Einzelne Phrasen sind um ein Wort verschoben („HERB MISTLETOE“ statt „DRAGON HERB“ / „MISTLETOE“), und es gibt leere Einträge. Die Namen in diesem Dokument stammen deshalb aus den Tile-Nummern im Code (Zutaten, Waffen, Kessel, Feuer, Blob, Vine, Flood).
- **Szenario-Population und -Hooks** (`$FE03`, Hooks ab `$D01B`): Verteilung der Objekte und Kreaturen sowie Sonderregeln je Szenario (z. B. Slayer, Ragarils Fallen).
- **Sicht und Schusslinie:** Geklärt in Kap. 11. Offen sind nur Einzelheiten der Anzeige (Zeichnen, Karte).
- **KI-Details (Kap. 10):** Geklärt sind die Struktur, die Tabellenformate (10.8), die Nachbarwahl und das Auslösen. Die Sichtlisten und die Schusslinie sind in Kap. 11 beschrieben. Offen bleiben nur Feinheiten wie die Reihenfolge gleich entfernter Nachbarfelder.

## 10. KI der Computer-Wizards und der NPC-Kreaturen

Gelesen aus `$96EB`–`$A740` und den Szenario-Hooks. Die Zahlen und Bedingungen stammen aus dem Code. Wo ich nur die Absicht erschließe, steht (**unsicher**).

### 10.1 Wer von der KI gesteuert wird

- **Spielernummer 0 (NPCs):** Die Kreaturen, die das Szenario setzt oder die sich aus dem Szenario ergeben, handeln jede Runde vor den Spielern (`$6141`–`$6147`).
- **Gegner-Wizard als Spieler 2:** Bei **einem** Spieler wird das Spiel im Modus „gestalteter Wizard“ gestartet (`$60ED`–`$6119`): Es gibt zwei Spieler, und Spieler 2 ist der **Gegner-Wizard** des Szenarios. Er und alle von ihm beschworenen Kreaturen werden von der KI gesteuert (`$616E`–`$6174`). Der Wizard wird aus dem Szenario nach Spieler 2 kopiert und liegt im Positionssatz **Nr. 1** (`$5B60`; Nr. 0 ist Spieler 1). Bei 2–4 Spielern gibt es keinen KI-Wizard.
- **Der Gegner-Wizard bewegt sich selbst.** Er wird wie jede KI-Kreatur behandelt (`$974E`), hat beim Spielstart einen eigenen Plan mit Route (`$8D8B`–`$8D9C`) und läuft diese Route ab, solange nichts Wichtigeres ansteht (Kap. 10.3/10.7). Seine Routen sind die mit Flag Bit 6 (nur für Wizards). Das sind in Szenario 1 die Routen 0, 2, 3 und 8, in Szenario 2 die Routen 0–6 und in Szenario 3 nur Route 0, der Ring um die Mitte (Kap. 10.8). Aggressiv ist er nie (Byte 8 seiner Vorlage = 0), er kann also vor Gegnern fliehen. Nach der Portalrunde läuft auch er zum Portal.
- Die KI-Zauberroutinen fragen das Mana von Spieler 2 fest ab (`D_D0E1`). Sie sind also nur für diesen einen Wizard gebaut.
- Beim Spielstart mit gestaltetem Wizard muss dessen Level gleich der Szenario-Nummer sein, sonst wird der Start abgebrochen (`$6101`–`$610B`).

### 10.2 Gegner-Wizards (Datensatz im Szenario bei `$D082`)

| | Szenario 1 | Szenario 2 | Szenario 3 |
|---|---|---|---|
| Name | TORQUEMADA | ELBO SMOGG | RAGARIL |
| Mana | 120 | 140 | 200 |
| AP | 34 | 34 | 34 |
| Stamina | 68 | 68 | 78 |
| Konstitution | 63 | 63 | 50 |
| Combat / Defence | 9 / 10 | 9 / 10 | 13 / 18 |
| MR | 90 | 90 | 90 |
| Tragekapazität | 48 | 48 | 48 |
| Siegpunkte (Byte ÷ 2) | 12 | 12 | 12 |

Zauberlevel und KI-Priorität (Tabelle `$D3DC`, 45 Byte) je Zauber. Nur Zauber mit Level oder Priorität sind aufgeführt. `-` = beides 0. Das Level ist das **aktuelle** Level zu Spielbeginn (unteres Nibble, kein Auffüllen). Level 0 heißt: nicht wirkbar.

| ID | Zauber | Sz.1 Lvl/Prio | Sz.2 Lvl/Prio | Sz.3 Lvl/Prio |
|---:|---|---|---|---|
| 3 | Pixie | - | 2 / 80 | 2 / 140 |
| 4 | Dwarf | - | 2 / 160 | 2 / 0 |
| 5 | Goblin | 2 / 100 | 1 / 90 | 2 / 180 |
| 6 | Troll | 1 / 95 | 1 / 100 | 2 / 170 |
| 7 | Giant | - | 2 / 130 | 2 / 160 |
| 8 | Centaur | 2 / 90 | 2 / 100 | 2 / 1 |
| 11 | Gryphon | - | - | 2 / 160 |
| 12 | Elephant | 1 / 85 | - | - |
| 13 | Gorilla | - | 1 / 30 | - |
| 15 | Bear | 1 / 80 | - | - |
| 16 | Crocodile | 1 / 75 | 2 / 80 | - |
| 17 | Giant Bat | 1 / 70 | 1 / 50 | - |
| 18 | Harpy | 2 / 65 | - | - |
| 19 | Giant Spider | - | 1 / 0 | - |
| 20 | Zombie | - | 0 / 130 | 2 / 120 |
| 21 | Ghost | - | 0 / 150 | - |
| 22 | Vampire | 2 / 110 | - | 1 / 150 |
| 23 | Spectre | 1 / 60 | 1 / 190 | 2 / 160 |
| 24 | Demon | - | 1 / 200 | 1 / 140 |
| 25 | Strength P. | 2 / 180 | 2 / 160 | 3 / 200 |
| 26 | Protection P. | 2 / 170 | 3 / 140 | 3 / 130 |
| 27 | Invisibility P. | 2 / 160 | 2 / 180 | 3 / 210 |
| 28 | Speed P. | 2 / 150 | 1 / 100 | 2 / 100 |
| 29 | Flying P. | 2 / 140 | - | 3 / 70 |
| 30 | Super P. | 2 / 190 | 3 / 200 | 3 / 210 |
| 31 | Healing P. | 2 / 160 | 4 / 0 | 2 / 70 |
| 32 | Magic Fire | 5 / 120 | - | - |
| 33 | Gooey Blob | 3 / 60 | 6 / 160 | - |
| 34 | Tangle Vine | 5 / 100 | 5 / 170 | - |
| 35 | Flood | 3 / 30 | - | - |
| 39 | Magic Attack | - | - | 0 / 180 |
| 40 | Magic Bolt | 3 / 200 | 4 / 180 | 3 / 180 |
| 41 | Magic Lightning | 2 / 220 | 3 / 190 | 3 / 0 |
| 44 | Magic Shield | 3 / 60 | 5 / 200 | 4 / 240 |

Die KI wirkt nur diese Zauber (Handlertabelle bei `$D019`): Beschwörungen, Drachen, Tränke, **Fire/Blob/Vine/Flood** (nur Szenario 1 und 2), **Bolt**, **Lightning** und **Shield**. Enchant, Subversion, Curse, Magic Attack, Teleport und Magic Eye wirkt sie nie (der Handler ist ein leeres `ret`, `$A45F`). In Szenario 3 fehlt auch der Flächenzauber-Handler.

**Startposition der Spieler und des Gegner-Wizards** (`$8CFA`–`$8D5A`): Die Startplätze stehen im Szenario ab `$D072` (je x, y), ihre Anzahl in `$D043`. Die Spieler werden der Reihe nach gesetzt (Spieler 1, dann der Gegner-Wizard). Jeder bekommt einen Platz `RND($D043)`. Ist der Platz schon vergeben (Marke `$FF`), wird neu gewürfelt, sonst wird er vergeben (`$8D3C`–`$8D49`). Die Plätze werden also ohne Zurücklegen gezogen. Das Mana startet auf dem Maximum aus dem Wizard-Datensatz (`$8D5A`–`$8D68`).

| Szenario | Plätze (`$D043`) | Position | Auswahl |
|---|---:|---|---|
| 1 | 4 | (9, 9), (26, 26), (9, 26), (26, 9) | zufällig, ohne Zurücklegen |
| 2 | 4 | (9, 9), (26, 26), (9, 26), (26, 9) | zufällig, ohne Zurücklegen |
| 3 | 2 | (31, 4) für Spieler 1, (17, 17) für den Gegner-Wizard | **fest**, `$D048 = 1`: Platz = Spielernummer − 1 (`$8D2D`–`$8D37`) |

In Szenario 1 und 2 bekommt der Gegner-Wizard also einen der drei Plätze, die Spieler 1 übrig lässt. In Szenario 3 steht Ragaril immer in der Mitte (17, 17), das ist der Ring seiner Route 0.

**Wie viele Kreaturen kann der Gegner-Wizard höchstens beschwören?** Der Code kennt keine feste Obergrenze je Wizard, sondern mehrere Grenzen:
- Jeder Wurf beschwört `L` Kreaturen und senkt `L` danach um 1. Ein Zauber mit Startlevel `n` liefert also höchstens `n·(n+1)/2` Kreaturen. Nur Zauber mit Level ≥ 1 und Priorität > 0 werden gewirkt.
- Das Mana begrenzt weiter (Startwert und 4 % je Zug, Kap. 4). Ohne sichtbare Gegner bleiben immer 40 Mana als Reserve.
- **Pläne:** Jede Kreatur von Spieler 2 braucht einen Plan in der Tabelle bei `$CF76` mit **35 Einträgen** (`$A4FA`, `$A4FD`). Der Wizard belegt einen davon, es bleiben **34 gleichzeitig lebende** beschworene Kreaturen. Der Plan wird beim Tod frei (`$77C2`–`$77CB`). Ist die Tabelle voll, entsteht die Kreatur trotzdem, behandelt sich aber wie eine Kreatur ohne Plan (Kap. 10.7).
- **Allgemeine Grenzen** (alle Spieler und NPCs zusammen): 86 Stat-Blöcke (`$830C`) und 141 Positionssätze, die sich mit Gegenständen teilen (`$806F`).

Obergrenze aus den Zauberlevels der Gegner-Wizards (Kap. 10.2):

| Szenario | Obergrenze Kreaturen | Mana für alle Würfe | Startmana |
|---|---:|---:|---:|
| 1 | 18 | 217 | 120 |
| 2 | 21 | 211 | 140 |
| 3 | 26 | 331 | 200 |

Dabei gilt: In Szenario 3 wird Centaur (Level 2, Priorität 1) praktisch nie gewirkt, denn die Priorität 1 wird beim ersten Durchgang ohne Wurf auf 0 abgerundet (Kap. 10.5). Die Obergrenze liegt dann bei 23. Die Zahl 5 kommt im Original nirgends vor, die tatsächliche Zahl hängt vom Mana, der Reserve und den freien Nachbarfeldern ab.

### 10.3 Ablauf pro Kreatur (`$974E`, `$9795`–`$9841`)

Jede Kreatur der KI wird einzeln abgearbeitet (Reihenfolge der Positionssätze). Sie wiederholt die folgende Schleife höchstens **50-mal** (sonst bricht sie ab, `$982B`–`$9841`), bis sie eine Aktion beendet, die den Zug schließt.

1. **Schlafende** Kreaturen tun nichts (Bit 6 im Plan, s. 10.7). Sie wachen auf, sobald sie einen Gegner sehen (`$9771`–`$9779`).
2. Steht die Kreatur auf einem befreundeten Wesen, werden die Schritte 3 und 4 übersprungen.
3. Reihenfolge der „Sofortaktionen“ (`$97A5`–`$97BA`), je Durchgang höchstens eine, solange AP reichen:
   1. **Trank vom Kessel trinken** (`$A196`).
   2. **Zaubern** (nur Wizards, `$A382`).
   3. **Gegenstand aufheben** (`$A0F0`), wenn sie auf dem Zielgegenstand steht.
   4. **Phiole trinken** (`$A279`).
   5. **Essen** (`$A2FC`).
   6. **Werfen** (nur Nicht-Wizards, `$A5BD`).
   7. **Waffe wechseln** (`$A22B`).
   8. **Abfliegen** (`$9E3A`): Jede Kreatur mit Flug-AP, die am Boden steht, hebt in jedem Durchgang sofort ab, wenn Kap. 8.1 es erlaubt (6 AP). Hat sie weniger als 6 AP, endet ihr Zug (`$9E4F`–`$9E5B`). Fliegende bleiben normalerweise in der Luft.
4. **Fernangriff** (`$9CC5`).
5. **Flucht** (`$9E7D`). Falls nicht geflohen wird: **Nahkampfziel** (`$9DAD`). Falls keins: **Gegenstand** als Ziel (`$A021`). Falls keins: **Route/Jagd** (`$9992`).
6. **Schritt** (`$9A0D`, `$9A48`, `$9AF3`): Für das aktuelle Ziel `Z` (`D_D1B6`) wird jedem der 8 Nachbarfelder die Entfernung `D` (Kap. 1, `2·max + min` mit Umbruch) zu `Z` zugeordnet. Die Nachbarn werden **aufsteigend nach `D`** sortiert (Bubble-Sort, bei Gleichstand wird getauscht, die Reihenfolge innerhalb gleicher `D` ist also unbestimmt). Bit 7 im Sortierbyte markiert Diagonalen (Reihenfolge der Richtungen: W, SW, S, SE, E, NE, N, NW) und zählt nicht mit. Danach werden die Nachbarn in dieser Reihenfolge ausprobiert (Ergebnis von `$CA2B`, Tabelle `$9B27`):
   - **Gegner auf dem Feld:** Angriff (Kap. 6.2), wenn AP ≥ 8, Stamina ≥ 8 und die Untoten-Regel passt (`$9B61`). Sonst nächstes Feld.
   - **Freies Feld:** Schritt, wenn die Kreatur nicht „angebunden“ ist (Stat-Byte 4, Bit 4) und das Feld **nicht zu den letzten 8 besuchten Feldern** gehört (`$9AC4`, Liste `$9AA2`, gegen Hin-und-her-Laufen).
   - **Unpassierbares Terrain:** Kreaturen mit Flag `80` öffnen eine **geschlossene Tür** (Tile `$22`–`$24`) für 4 AP und eine **verschlossene Tür** (`$1F`–`$21`) mit Schlüssel `$7D` für 4 AP (`$9BD4`–`$9C13`). Alles andere bricht die Kreatur nur durch, wenn `1,5·Combat_eff ≥ Zähigkeit` gilt (Kap. 6.4).
   - **Zu wenig AP oder Stamina** für den Schritt: Zug endet.
   - Ist kein Nachbar möglich, endet der Zug.

**Landen der KI** (`$9858`, `$988C`): Nur fliegende Kreaturen. Sie landen vor dem Schritt, wenn **alle** Punkte gelten:
- Sie fliehen nicht gerade (`D_D198 = 0`).
- Sie haben ein Gegenstandsziel oder ein Nahkampfziel. Eine Kreatur ohne Ziel (Tile ≥ `$90`) bleibt oben. Wizards (Tile < `$90`) dürfen auch ohne Ziel landen.
- Das Ziel ist **angrenzend**: Das nächste sortierte Nachbarfeld ist das Zielfeld (`D_D18E`, `$9A31`–`$9A41`).
- Ist das Ziel eine Truhe (`$7A`/`$7B`), muss noch Tragekapazität frei sein (`$9879`–`$988B`).
- Landen ist erlaubt (Kap. 8.1: nicht angebunden, kein anderes Wesen, Terrain ohne Bit 5).

Außerdem landet die KI vor Aktionen, die den Boden brauchen, wenn sie in der Luft ist: **Gegenstand aufheben** (`$A116`), **Trank vom Kessel trinken** (`$A1C7`) und **Beschwören** (`$A4A6`). Kann sie nicht landen, entfällt die Aktion. Fürs Landen beim Nahkampf prüft die KI die Höhe des Gegners nicht. Ein landender Flieger kann deshalb einen fliegenden Gegner nicht mehr treffen und hebt im nächsten Durchgang wieder ab (**Beobachtung aus dem Code, nicht im Spiel gesehen**).

### 10.4 Fernangriff, Nahkampf, Flucht

Alle Vergleiche benutzen die effektiven Werte (Kap. 6.1) aus den Listen der **sichtbaren Gegner** (`D_D0E4`, je Gegner: Index, Combat_eff, Defence_eff).

- **Fernangriff** (`$9CC5`): Nur mit Fernwaffe. Angriffswert `A` und Reichweite `R` kommen aus der Tabelle in Kap. 6.4 (Drache 35/12, Bogen 15/16, magischer Bogen 30/22).
  - Ein Ziel kommt nur in Frage, wenn `2·A ≥ 1,5·Defence_eff` des Ziels.
  - Entfernung `D < R`, die AP reichen (≥ 8) und das Ziel ist im Feuerbereich (Sichtlinie `$7AE9`).
  - **Untote Ziele:** nur mit magischem Bogen, oder wenn der Schütze ein Drache ist und das Ziel am Boden auf brennbarem Gelände steht (Fire-Nibble ≠ 0, `$9D0D`–`$9D2D`).
  - Kostet 8 AP.
- **Nahkampfziel** (`$9DAD`): Ein Gegner kommt in Frage, wenn `2·Combat_eff(eigen) ≥ 1,5·Defence_eff(Gegner)` (`$9DC4`–`$9DD1`). Untote Gegner nur für Untote oder bei magischer Waffe (Tile `$60`–`$67` außer `$63`). Außerdem muss der Gegner auf derselben Höhe stehen. Gewählt wird der **nächste** (kleinste Entfernung D).
- **Flucht** (`$9E7D`–`$A020`): Ein Gegner gilt als **Bedrohung**, wenn `2·Defence_eff(eigen) < 1,5·Combat_eff(Gegner)`. Ausnahmen: Fliegende ignorieren Bedrohungen von Gegnern ohne Flug. Untote fürchten nur Untote und Gegner mit magischer Waffe. **Aggressive** Kreaturen (Bit 7 im Plan, s. 10.7) fliehen nie. Das Fluchtfeld wird so bestimmt:
  1. Jedes Nachbarfeld wird als **gefährdet** markiert, wenn es unpassierbar ist oder ein Gegner (aus der Liste) freie Schusslinie dorthin hat (`$7AF5`, `$B40F`).
  2. Gibt es ein **nicht gefährdetes** Nachbarfeld, wird das erste davon das Ziel. Ist die Kreatur dort angekommen, endet ihr Zug (`$981B`–`$9828`).
  3. Sind alle gefährdet, wird für jedes Nachbarfeld die **Summe der Entfernungen zu allen Bedrohungen** gebildet (gedeckelt auf 255), und das Feld mit der größten Summe wird das Ziel (`$9FD1`–`$A01D`).

### 10.5 Zauberwahl (`$A382`–`$A460`)

- Nur der Wizard, mit mindestens **8 AP**, und nicht auf einem Freund stehend. Sind **keine Gegner sichtbar**, zaubert er nur, wenn seine AP **mindestens die Hälfte** des AP-Maximums betragen (`$A39A`–`$A3AE`).
- **Auswahl:** Die Zauber werden nach der Priorität in `D_D3DC` abgearbeitet (hoher Wert zuerst). Pro Versuch gilt:
  1. Level ≥ 1 und Mana ≥ Kosten (Kap. 5.1).
  2. Zauberspezifische Bedingung (s. u.). Erfüllt sie sich nicht, wird der Zauber in diesem Durchgang übersprungen.
- **Nach einem Wurf wird die Priorität dieses Zaubers halbiert** (`$A4DA`: `srl` auf den Eintrag in `D_D3DC`). Das Ergebnis bleibt **dauerhaft** im Speicher: Es gibt keine Rückstellung, auch nicht zu Rundenbeginn. Aus 200 wird 100, 50, 25, 12, 6, 3, 1, 0. Zurückgesetzt wird die Tabelle erst, wenn das Szenario neu geladen wird. Das ist nach jedem Spiel nötig, denn `$C92D` setzt `D_D03F` auf 0 und der Spielstart verlangt dann ein geladenes Szenario.
  - **Nebenwirkung der Markierung:** Während eines Durchgangs wird jeder geprüfte, aber nicht gewirkte Zauber um 1 erhöht, damit er nicht erneut gewählt wird (`$A446`). Erreicht ein Durchgang das Ende ohne Wurf, löscht `$A3FB` bei **allen** Einträgen Bit 0. Dadurch wird jede **ungerade Priorität dauerhaft um 1 abgerundet** (z. B. 95 → 94, 85 → 84, 1 → 0). Nach einem Wurf bleiben die Markierungen der zuvor gescheiterten Zauber stehen, bis ein späterer Durchgang ohne Wurf endet (die Priorität dieser Zauber liegt bis dahin um 1 höher).
- **Beschwörungen** (ID 3–24, `$A460`): Mana nach dem Wurf **≥ 40** (`$D046`, in allen drei Szenarien gleich: 40), außer es sind Gegner sichtbar. Es müssen mindestens **L freie Nachbarfelder** da sein (`$A49E`).
- **Drachen und Tränke** (`$A644`): Der Wizard braucht einen **leeren Kessel** und die **Zutat** (Kap. 5.3) auf seinem Feld oder im Gepäck. Er legt sie, falls nötig, vorher ab.
- **Shield** (ID 44): nur wenn gerade kein Shield aktiv ist (`$E1C1`: Zähler `D_D055` = 0).
- **Bolt/Lightning:** Für jeden sichtbaren Gegner: `2·(4L + 25) ≥ 1,5·Defence_eff` des Gegners (`$E1D8`–`$E1E5`), das Ziel ist kein Reiter, Entfernung `D ≤ 2L + 6`. **Lightning** zusätzlich nur, wenn **keine eigene Kreatur** (Besitzer 2) näher als **4** am Ziel steht (`$E21B`).
- **Fire/Blob** (Szenario 1/2): Ziel ist ein Gegner im Radius `2L + 6`, das Zielterrain ist empfänglich (Nibble ≠ 0), und das Ziel steht nicht auf Tile `$36`, `$37`, `$3C`, `$44` (`$E274`–`$E2B6`). **Vine/Flood** zusätzlich nur, wenn **keine eigene Kreatur** innerhalb `2L + 1` vom Ziel steht (`$E2BF`).

### 10.6 Gegenstände

Tabelle `D_D3B9` (ab Tile `$50` bei `$D409`). Wert in den unteren 7 Bits = Wunsch der KI (0 = ignorieren). `*` = Bit 7 gesetzt (bei Nicht-Wizards: Wurfgegenstand, `$A5E3`–`$A5EC`, **unsicher** ob gewollt: Bit 7 sitzt bei Schätzen, Zutaten und Nahrung). Spalten: Szenario 1 / 2 / 3.

| Tile | Wert |
|---|---|
| `$50` | 127* / 127* / 127* |
| `$51` | 115* / 115* / 115* |
| `$52` | 105* / 105* / 105* |
| `$53` | 100* / 100* / 100* |
| `$54` | 50* / 50* / 50* |
| `$55` | 50* / 50* / 50* |
| `$56` | 50* / 50* / 50* |
| `$57` | 50* / 50* / 50* |
| `$58` | 50* / - / 50* |
| `$59` | 75 / 75 / 75 |
| `$5A` | 50 / 50 / 50 |
| `$5B` | 75 / 75 / 75 |
| `$5C` | 90 / 90 / 90 |
| `$5D` | 65 / 65 / 65 |
| `$5E` | 45 / 125 / 45 |
| `$5F` | 55 / 55 / 55 |
| `$60` | 100 / 100 / 100 |
| `$61` | 75 / 75 / 75 |
| `$62` | 100 / 100 / 100 |
| `$63` | 105 / 105 / 105 |
| `$64` | 90 / 90 / 90 |
| `$65` | 70 / 125 / 70 |
| `$66` | 80 / 80 / 80 |
| `$67` | 20* / 20* / 20* |
| `$6B` | 124 / 124 / 124 |
| `$6C` | 106 / 106 / 106 |
| `$6D` | 127 / 127 / 127 |
| `$6E` | 102 / 102 / 102 |
| `$6F` | 90 / - / 90 |
| `$70` | 120 / 120 / 120 |
| `$71` | 125 / 125 / 125 |
| `$72` | 10 / 10 / 10 |
| `$73` | 124 / 124 / 124 |
| `$74` | 106 / 106 / 106 |
| `$75` | 127 / 127 / 127 |
| `$76` | 102 / 102 / 102 |
| `$77` | 90 / - / 90 |
| `$78` | 120 / 120 / 120 |
| `$79` | 125 / 125 / 125 |
| `$7A` | 127 / 127 / 127 |
| `$7B` | 110 / 110 / 110 |
| `$7D` | 80 / 80 / 70 |
| `$7E` | 70 / 70 / 80 |
| `$7F` | 80* / 80* / 80* |
| `$80` | 80 / 80 / 80* |
| `$81` | 50* / 50* / 50* |
| `$82` | 90* / 90* / 90* |
| `$83` | 60* / 60* / - |
| `$84` | 3* / 13* / 5* |
| `$85` | 26 / - / - |
| `$86` | 31 / 13 / 1 |
| `$87` | 5* / 14 / 2 |
| `$88` | 20 / 16 / 3 |
| `$89` | 21 / 1 / 8* |
| `$8A` | 22 / 8 / 6 |
| `$8B` | 23 / 2 / 7 |

- **Aufheben** (`$A021`): Je sichtbarem Gegenstand zählt `⌊Wert / (D + 1)⌋` (D = Entfernung zur Kreatur). Der größte Wert gewinnt, bei Gleichstand der spätere. Nicht in Frage kommen Gegenstände, deren Feld nicht erreichbar ist, die über die Tragekapazität gehen (`Rest = Kapazität − Summe der Gewichte`), und alles, wenn die Kreatur schon **10 Gegenstände** trägt (`$A033`). Truhe `$7A` nur mit Schlüssel `$7E`. Kessel `$6B`–`$70` mit Trank nur, wenn kein Trank wirkt, der Kessel `$71` nur bei Konstitution unter Maximum.
- **Waffe wechseln** (`$A22B`): Aus den getragenen Waffen (Tile `$59`–`$66`, ohne Schilde `$5B`/`$62`) die mit dem größten Wert, kostet 4 AP. Nur Kreaturen mit Flag `04`.
- **Phiole trinken** (`$A279`): Heiltrank (`$79`) nur bei `1,5 · Konstitution < KonMax`. Andere nur, wenn der neue Wert höher ist als der der gerade wirkenden Phiole (`D_D409 + $22 + Typ`). Kostet 4 AP.
- **Essen** (`$A2FC`): Wenn `1,5 · Konstitution < KonMax` oder `Stamina ≤ StaminaMax / 4`. Gewählt wird die Nahrung mit dem größten Wirkwert, bei Wizards zählt `2 × Mana-Bonus`, falls vorhanden. Kostet 4 AP.
- **Werfen** (`$A5BD`): Nicht-Wizards der KI werfen Gegenstände mit Bit 7 **dem eigenen Wizard zu** (Positionssatz Nr. 1 = Gegner-Wizard, `$A606`–`$A610`), sobald die Entfernung kleiner als die Wurfweite (Kap. 6.4) ist. Der Gegenstand landet auf dem Feld des Wizards und richtet keinen Schaden an (Wurfwert 0). Die Kreatur trägt die Beute also zum Wizard und wirft sie ihm zu. Solange sie so einen Gegenstand trägt, läuft sie zum Wizard (`$9992`). Kostet 8 AP.
- **Truhen und Türen** (`$A1ED`): Mit Schlüssel (`$7E` für Truhe `$7A`) 4 AP. Offene Truhen (`$7B`) öffnen Kreaturen mit Flag `80`.

### 10.7 NPC-Routen, Aggression, Portal (`$9992`, `$A4FA`, `$A700`)

- Jede KI-Kreatur hat einen **Plan** (4 Byte: Positionssatz-Nr., Route, Schritt, Flags). Pläne von NPCs stehen im Szenario (`D_D015`), die von Spieler 2 entstehen beim Beschwören (`$A4FA`) im Speicher bei `$CF76`. Beim Beschwören wird die **Route** gewürfelt: `RND($D047)` (= 11 / 7 / 6, `$A516`–`$A51F`). Es kommen also nur die **ersten** `$D047` Routen in Frage, die übrigen gehören den festen NPC-Plänen. Der Wurf wird wiederholt, bis die Route zur Kreatur passt (Routenflags `$D011`): Bit 0 Flieger, Bit 1 „Use“-Flag (80), Bit 2 Tragekapazität > 0, Bit 3 Wood, Bit 4 Water, Bit 5 Rock, Bit 6 nur für Wizards erlaubt (Wizards brauchen es, Kreaturen dürfen es auch) (`$A51C`–`$A56E`).
- **Aggression (Leibwache):** Plan-Bit 7 wird gesetzt, wenn `RND(100) < Aggressivität` (Byte 8 der Kreaturentabelle, Kap. 2). Eine Kreatur **ohne** Plan gilt ebenfalls als aggressiv (`$9758`–`$9767`). Aggressive Kreaturen laufen zum **Gegner-Wizard** (Positionssatz Nr. 1, `$99A2`) und bleiben bei Entfernung `< 5` stehen (`$99B8`–`$99C0`). Sie fliehen nie. NPC-Pläne aus dem Szenario sind in allen drei Szenarien nicht aggressiv.
- **Routen:** Eine nicht aggressive Kreatur läuft ihre Wegpunkte ab (Wegpunkt erreicht → nächster, am Ende von vorn) (`$99D7`–`$9A08`).
- **Portal:** Sobald die Runde die **Portalrunde** (Kap. 4) erreicht, laufen **alle** KI-Kreaturen zum Portalfeld (`$99C7`–`$99D3`).
- **Schlaf und Auslöser:** Pläne mit Flag Bit 6 sind **schlafend**: Die Kreatur tut nichts. Sie wacht auf, sobald sie einen Gegner sieht (`$9771`–`$9777`), oder wenn ein **Auslöser** feuert. Die unteren 6 Bits des Plan-Flags sind die Auslöser-ID (0 = keine). Ein Auslöser feuert, wenn das **Terrain** auf seinem Feld verändert wird, also bei Tür öffnen, Wand durchbrechen, Blitz auf Terrain, Feuer/Blob/Vine/Flood setzen oder Truhe öffnen (`$7381`/`$7339` → `$A716` → `$A700`). Bloßes Betreten reicht nicht.

### 10.8 Szenariotabellen der KI (Routen, Wegpunkte, Pläne, Auslöser)

Aufbau (Zeiger im Szenarioheader, alle Werte 8 Bit):

| Zeiger | Tabelle | Format |
|---|---|---|
| `$D00F` | Routen | Aneinandergereihte Routen. Jede Route: ein Längenbyte (Länge inkl. dieses Bytes, Bit 7 gesetzt), danach die Wegpunkt-IDs. Das Ende der Tabelle ist ein Byte `$80`. Das Ende einer Route erkennt `$99F7` am Bit 7 des nächsten Bytes. |
| `$D011` | Routenflags | 1 Byte je Route (Bedeutung in 10.7). |
| `$D013` | Wegpunkte | 2 Byte je ID: x, y. |
| `$D015` | NPC-Pläne | 4 Byte je Eintrag: Positionssatz-Nr. der Kreatur, Route, Startschritt, Flags (Bit 7 aggressiv, Bit 6 schlafend, unten 6 Bit Auslöser-ID). Ende `$FE`. |
| `$D017` | Auslöser | 3 Byte je Eintrag: x, y, ID. Ende `$FE`. |

**Szenario 1**

| Route | Flags | Wegpunkte (ID → x,y) |
|---:|---|---|
| 0 | `$42` | 26 (17,18) → 31 (35,18) |
| 1 | `$48` | 20 (3,3) → 21 (3,32) → 22 (32,32) → 23 (32,3) |
| 2 | `$42` | 0 (9,9) → 5 (9,26) → 10 (26,9) → 15 (26,26) |
| 3 | `$40` | 13 (31,9) → 12 (26,14) → 19 (26,21) → 18 (31,26) → 6 (4,26) → 9 (9,21) → 2 (9,14) → 1 (4,9) |
| 4 | `$00` | 3 (14,9) → 2 (9,14) → 9 (9,21) → 8 (14,26) → 16 (21,26) → 19 (26,21) → 12 (26,14) → 11 (21,9) |
| 5 | `$01` | 24 (34,1) → 10 (26,9) → 26 (17,18) → 5 (9,26) → 25 (1,34) |
| 6 | `$00` | 4 (9,4) → 3 (14,9) → 11 (21,9) → 14 (26,4) → 17 (26,31) → 16 (21,26) → 8 (14,26) → 7 (9,31) |
| 7 | `$00` | 14 (26,4) → 13 (31,9) → 1 (4,9) → 4 (9,4) → 7 (9,31) → 6 (4,26) → 18 (31,26) → 17 (26,31) |
| 8 | `$46` | 24 (34,1) → 25 (1,34) |
| 9 | `$10` | 27 (14,34) → 30 (15,2) → 28 (21,34) → 29 (21,3) |
| 10 | `$01` | 27 (14,34) → 30 (15,2) → 29 (21,3) → 28 (21,34) |

Pläne: Kreatur 4 → Route 1, Schritt 0, Flags `$00`; Kreatur 5 → Route 1, Schritt 2, Flags `$00`; Kreatur 6 → Route 8, Schritt 1, Flags `$00`.

Auslöser: keine.

**Szenario 2**

| Route | Flags | Wegpunkte (ID → x,y) |
|---:|---|---|
| 0 | `$40` | 0 (9,9) → 13 (7,3) → 14 (2,3) → 16 (1,31) → 1 (9,26) → 8 (16,27) → 2 (26,26) → 17 (35,33) → 14 (2,3) → 28 (8,7) → 3 (26,9) → 4 (15,8) |
| 1 | `$40` | 0 (9,9) → 4 (15,8) → 3 (26,9) → 7 (24,22) → 2 (26,26) → 8 (16,27) → 1 (9,26) |
| 2 | `$40` | 11 (4,9) → 9 (16,18) → 8 (16,27) → 15 (9,31) → 16 (1,31) → 14 (2,3) → 28 (8,7) → 3 (26,9) → 7 (24,22) → 19 (31,26) → 17 (35,33) → 11 (4,9) |
| 3 | `$40` | 0 (9,9) → 12 (2,15) → 24 (34,15) → 22 (30,9) → 4 (15,8) |
| 4 | `$40` | 17 (35,33) → 16 (1,31) → 15 (9,31) → 8 (16,27) → 2 (26,26) → 21 (26,32) |
| 5 | `$40` | 3 (26,9) → 7 (24,22) → 21 (26,32) → 17 (35,33) → 15 (9,31) → 5 (7,19) → 12 (2,15) → 24 (34,15) → 23 (34,10) |
| 6 | `$40` | 4 (15,8) → 3 (26,9) → 28 (8,7) → 14 (2,3) → 17 (35,33) → 2 (26,26) → 8 (16,27) → 1 (9,26) → 16 (1,31) → 14 (2,3) → 13 (7,3) → 0 (9,9) |
| 7 | `$09` | 27 (11,33) → 26 (11,1) → 25 (29,1) → 18 (29,34) → 2 (26,26) → 1 (9,26) → 0 (9,9) → 3 (26,9) |
| 8 | `$09` | 22 (30,9) → 6 (26,14) → 7 (24,22) → 20 (30,21) → 17 (35,33) → 15 (9,31) → 1 (9,26) → 5 (7,19) → 0 (9,9) → 4 (15,8) → 3 (26,9) |
| 9 | `$09` | 0 (9,9) → 5 (7,19) → 1 (9,26) → 8 (16,27) → 2 (26,26) → 21 (26,32) → 2 (26,26) → 7 (24,22) → 6 (26,14) → 3 (26,9) → 22 (30,9) → 4 (15,8) |

Pläne: Kreatur 8 → Route 7, Schritt 2, Flags `$41`; Kreatur 9 → Route 7, Schritt 0, Flags `$42`; Kreatur 10 → Route 7, Schritt 0, Flags `$43`; Kreatur 11 → Route 8, Schritt 6, Flags `$44`; Kreatur 12 → Route 8, Schritt 6, Flags `$44`; Kreatur 13 → Route 8, Schritt 0, Flags `$45`; Kreatur 14 → Route 8, Schritt 0, Flags `$45`; Kreatur 15 → Route 8, Schritt 0, Flags `$45`; Kreatur 16 → Route 8, Schritt 7, Flags `$46`; Kreatur 17 → Route 8, Schritt 7, Flags `$46`; Kreatur 18 → Route 9, Schritt 8, Flags `$47`; Kreatur 19 → Route 9, Schritt 5, Flags `$48`; Kreatur 20 → Route 9, Schritt 3, Flags `$00`; Kreatur 21 → Route 9, Schritt 8, Flags `$00`.

Auslöser: (23,6) → ID 1; (23,2) → ID 2; (13,33) → ID 3; (4,27) → ID 4; (30,6) → ID 5; (4,19) → ID 6; (28,14) → ID 7; (28,30) → ID 8; (16,33) → ID 1; (16,33) → ID 2; (16,33) → ID 3.

**Szenario 3**

| Route | Flags | Wegpunkte (ID → x,y) |
|---:|---|---|
| 0 | `$46` | 0 (16,16) → 1 (16,19) → 2 (19,19) → 3 (19,16) |
| 1 | `$02` | 6 (31,9) → 7 (31,13) → 5 (24,13) → 4 (23,9) → 5 (24,13) → 7 (31,13) → 8 (31,29) |
| 2 | `$02` | 9 (10,4) → 10 (10,12) → 9 (10,4) → 11 (27,4) |
| 3 | `$02` | 13 (21,23) → 14 (21,32) → 15 (27,32) → 14 (21,32) |
| 4 | `$02` | 17 (12,25) → 16 (9,25) → 12 (9,30) → 16 (9,25) |
| 5 | `$01` | 23 (14,2) → 24 (2,18) → 25 (16,34) → 26 (33,16) |
| 6 | `$10` | 27 (19,25) → 28 (19,32) |
| 7 | `$10` | 29 (5,30) → 12 (9,30) → 18 (9,19) → 19 (12,19) → 18 (9,19) → 20 (7,17) → 21 (7,15) → 22 (12,15) → 21 (7,15) → 20 (7,17) → 18 (9,19) → 12 (9,30) |

Pläne: Kreatur 19 → Route 1, Schritt 3, Flags `$41`; Kreatur 20 → Route 1, Schritt 0, Flags `$42`; Kreatur 21 → Route 6, Schritt 1, Flags `$00`; Kreatur 22 → Route 6, Schritt 0, Flags `$00`; Kreatur 23 → Route 7, Schritt 0, Flags `$43`; Kreatur 24 → Route 7, Schritt 0, Flags `$43`; Kreatur 25 → Route 7, Schritt 0, Flags `$43`.

Auslöser: (16,12) → ID 1; (27,23) → ID 2; (30,27) → ID 2; (5,30) → ID 3.

## 11. Sicht, Schusslinie und „angebunden“

Gelesen aus `$B148`, `$B1A6`–`$B403`, `$B40F`–`$B458`, `$7AF5` und `$9C8C`. Die Regeln gelten für Spieler und KI gleich. Die KI benutzt nur zusätzlich die Sichtlisten (11.5).

### 11.1 Beobachter und Reichweite

Der **Beobachter** ist die ausgewählte Kreatur (bei der KI die gerade handelnde). Bei **Magic Eye** ist es das Zielfeld, und die Unsichtbarkeit wird aufgehoben. Die Sicht wird neu berechnet, wenn die Kreatur gewählt wird, sich bewegt, angreift, landet oder abhebt (`$B148`).

Ein Feld kommt nur in Frage, wenn beide Achsenabstände `dx`, `dy` (mit Umbruch, Kap. 1) und die Entfernung `D = 2·max + min` unter dem Limit `R` liegen:

```
2·dx < R,  2·dy < R,  D < R       R = 19 (Boden), R = 23 (fliegend)
```

Das sind am Boden höchstens 9 Felder in gerader Linie (D = 18) oder 6 Felder diagonal, in der Luft 11 Felder gerade oder 7 diagonal.

Betrachtet werden alle Positionssätze mit Byte 4 ≥ `$FE`, also Wesen und Gegenstände auf der Boden- oder Luftebene. Getragene Gegenstände und Reiter werden übersprungen. Ein sichtbarer Reiter wird aber mitgeprüft (`$B3BB`–`$B3D5`) und zählt in 11.5 als zusätzlicher Gegner.

### 11.2 Wer ist sichtbar?

Für jedes Ziel im Bereich gilt (`$B2E8`–`$B356`):

1. **Unsichtbare Wesen** (Stat-Byte 4, Bit 3: Pixies, Invisibility- und Super-Trank, Kap. 8.2): nur für den eigenen Spieler sichtbar. Für Gegner nur mit **Magic Eye** (`$B2F3`–`$B305`). Magic Eye löscht das Bit bei gesehenen Gegnern (`$B3AB`). Es wird beim nächsten Zug des Besitzers wieder gesetzt (Kap. 4, Punkt 1).
2. **Verschiedene Höhe** (Beobachter und Ziel nicht auf derselben Ebene):
   - Steht der **Beobachter am Boden** auf einem Feld mit Flag `04` („überdacht“), sieht er die andere Ebene gar nicht (`$B30E`–`$B312`).
   - Ein **Ziel am Boden** auf einem Feld mit Flag `04` ist für einen fliegenden Beobachter unsichtbar (`$B318`–`$B31C`).
3. **Beobachter am Boden:** Fliegende Ziele sind immer sichtbar. Bodenziele sind sichtbar bei `D < 4` oder wenn die **Sichtlinie** frei ist (Kap. 11.3).
4. **Fliegender Beobachter:** Fliegende Ziele sind immer sichtbar. Bodenziele sind sichtbar bei `D < 4`, sonst nur, wenn ihr Feld **nicht** das Flag `02` („Blätterdach“) hat. Gegenstände auf einem Feld mit Flag `02` sieht er nie. Es gibt keine Sichtlinie: Fliegende sehen über Wände.

Beim Zeichnen zeigt das Spiel auf einem Feld mit mehreren Objekten den **schwersten Gegenstand** (Gewicht, `$B37F`–`$B390`), danach das Wesen.

### 11.3 Sichtlinie (Boden → Boden)

Die Linie läuft von der Beobachter- zur Zielposition (Linienalgorithmus, `$B045`/`$B0D0`, Umbruch 36): Es wird in jedem Schritt entlang der Hauptachse gegangen, die Nebenachse bei Fehler-Unterlauf (Start `major/2`). Geprüft werden alle **Felder dazwischen**, **nicht** Start und Ziel (`$B42E`–`$B458`). Blockiert wird, wenn das Tile-Flag mit der Maske `$01` gesetzt ist.

### 11.4 Tile-Flags (Tabelle `D_D38E`, 1 Byte je Tile)

| Bit | Bedeutung |
|---:|---|
| `01` | blockiert die Sichtlinie am Boden |
| `02` | „Blätterdach“: versteckt Bodenziele ab `D ≥ 4` vor fliegenden Beobachtern, versteckt Gegenstände vor ihnen ganz |
| `04` | überdacht: versteckt die jeweils andere Höhenebene (11.2, Punkt 2) |
| `08` | massiv: Ziel für Lightning ausgeschlossen (`$88B8`) |
| `10` | blockiert Linien, an denen Luft beteiligt ist (11.6). Nur in Szenario 2 |

Beispiele (Tile-Bereich → Wert): Szenario 1: `$10`–`$17` = `09` (Wand), `$18` = `04`, `$19` = `00` (Fenster, Sicht frei), `$1A`–`$1E` = `09`, `$1F`–`$24` = `0D` (Türen), `$25`–`$27` = `04` (offene Tür), `$2E`–`$30` = `03` (Wald), `$34` = `04` (Portal), `$37` = `03` (Vine), `$38`–`$49` = `01` (**Feuer und Blob blockieren die Sicht**), `$4A`–`$4D` = `09`. Szenario 2: Wände `$01`–`$13`, `$16`–`$17` = `1F`, Böden `04`, `$37` = `07`, `$38`–`$49` = `05`. Szenario 3: Wände `0D`, Böden `04`, `$2E`/`$2F` = `03`, `$38`–`$49` = `01`. Vollständig in `D_D38E` ab `$D38E`.

### 11.5 Zähler und Sichtliste der KI

- `$5B40` zählt die sichtbaren **Gegner am Boden**, `$5B41` die sichtbaren **Gegner in der Luft** (`$B3D9`–`$B3E5`). Ein Gegner ist ein sichtbares Wesen, das nicht dem aktuellen Spieler gehört. Ein sichtbarer Reiter zählt zusätzlich zum Reittier.
- **Sichtliste** (`D_D0E4`, `$9C8C`): Nur wenn die KI am Zug ist, wird jeder sichtbare Gegner an die Liste angehängt. Höchstens **20 Einträge**, Doppelte werden ausgelassen. Jeder Eintrag hat 3 Byte: Positionssatz-Nr., `Combat_eff`, `Defence_eff` des Gegners (Kap. 6.1). Die Liste wird für jede KI-Kreatur neu gefüllt (`$973C`). `D_D188` ist die Anzahl.
- **Gegenstandsliste** (`D_D148`): bis zu **24** sichtbare Gegenstände (Zeiger auf den Positionssatz, `D_D189` = Anzahl, `$B362`–`$B37A`). Die KI wählt daraus (Kap. 10.6).

### 11.6 Schusslinie für Wurf, Bogen und Feuer (`$7AF5`, `$7AE9`)

Ob ein Wurf oder Schuss das Ziel erreicht, hängt von den Höhen ab. Ein Ergebnis „frei“ ist die Voraussetzung, sonst ist der Zug nicht erlaubt (KI: Ziel überspringen).

| Schütze | Ziel | Prüfung |
|---|---|---|
| Boden | Boden | Sichtlinie wie 11.3 (Maske `$01`) |
| Boden | Luft | Steht der Schütze auf einem Terrain mit Typ-Bit 4 („kein Abheben“, Kap. 8.4), ist es blockiert. Sonst Linie mit Maske `$10` |
| Luft | Boden | Hat das Zielfeld Typ-Bit 4, ist es blockiert. Sonst Linie mit Maske `$10` |
| Luft | Luft | Linie mit Maske `$10` |

Die Linie mit Maske `$10` blockiert nur Felder mit Tile-Flag `10`. Das gibt es nur in Szenario 2 (Wände, Vine, Feuer, Blob). Szenarien 1 und 3 haben kein solches Flag, dort stoppt nichts einen Schuss, an dem Luft beteiligt ist, außer überdachtem Boden (Typ-Bit 4).

### 11.7 „Angebunden“ (engaged)

- **Wann:** Beim Berechnen der Sicht (11.1) wird die Kreatur angebunden (Stat-Byte 4, Bit 4), wenn ein **sichtbarer Gegner auf derselben Höhe** mit `D < 4` steht (`$B3F5`–`$B400`). `D < 4` sind genau die 8 Nachbarfelder (gerade 2, diagonal 3).
- **Neu bewertet** wird der Status nur, wenn die Kreatur sich bewegt oder angegriffen hat (`$5B29` = 1, `$B4EB`, `$8512`) oder wenn sie schon angebunden war (`$9052`, so kann sie sich lösen, wenn der Gegner weg ist). Eine nicht angebundene Kreatur wird also nicht angebunden, nur weil ein Gegner neben sie zieht. Der Gegner selbst wird dabei angebunden, da seine eigene Bewegung die Sicht neu berechnet.
- **Unsichtbare** Kreaturen werden nicht angebunden (`$B191`).
- **Gelöst** wird es zu **Beginn des eigenen Zugs** (`$76CF`) und bei jeder Neubewertung ohne Gegner nebenan (`$B19C`).
- **Wirkung:** Eine angebundene Kreatur kann nicht ziehen (Zustand 6 in `$CB21`–`$CB28`). Angriffe auf Gegner nebenan bleiben möglich. Auch Abheben und Landen (Kap. 8.1) sind gesperrt.
