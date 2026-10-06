# Plan: Regeln des Originals und schlauere KI

> Stand 2026-10-07 · Wunsch des Nutzers: „Mach einen Plan für die schlauere KI.“ Entscheidung am 2026-10-07: **„Alles wie im Original.“**
> Grundlage: `docs/REGELVERGLEICH-SPECTRUM.md` (Vorschläge R1–R39) und `lords-of-chaos-zx-agon/docs/REGELN.md` einschließlich K10 (KI) und K11 (Sicht, Schusslinie, „angebunden“), Stand `24b8c03`.
> Zweite Fassung vom 2026-10-07 nach den Nachträgen: Abheben und Landen (K8.1, K8.4, K10.3), Sicht und Schusslinie (K11), „angebunden“ (K11.7), Teleport-Bedingung (K5.3).
> Dieses Dokument ist ein **Plan**, es ändert noch keinen Code. Die Entscheidung steht im GDD als D67.

---

## 1. Entscheidung und Ziel

**Alle Vorschläge R1–R39 des Regelberichts werden wie im Original umgesetzt.** Das ersetzt unsere eigenen Regeln, wo sie dem Original widersprechen (§3). „Schlauer“ heißt für die KI, was das Original an ihr kann und unsere nicht:

- Kreaturen **wägen Stärken ab**: Sie greifen nur an, was sie verletzen können, und fliehen vor Stärkeren (R28).
- Sie **schießen** mit Bogen und Drachenfeuer (R29) und **nutzen Gegenstände** (R30); Beute **bringen sie ihrem Zauberer** (R31).
- Der KI-Zauberer hat **eigene Werte und große Bücher** (R33), **wirkt mit Prioritäten** (R32) und **läuft Routen** statt im Haus zu bleiben (K10.1).
- Kreaturen **schlafen, laufen Routen, werden zur Leibwache, ziehen bei Portalöffnung zum Portal** (R34, R35).

Nicht Ziel: Spectrum-Löcher nachbauen (Zauber-KI fest auf Spieler 2, leere Handler für Enchant, Subversion, Curse, Magic Attack, Teleport, Eye).

### Was bleibt, weil es das Original nicht kennt

Dies sind **Zusätze** ohne Gegenstück im Original. Sie bleiben, solange keine Originalregel dagegen steht; wer etwas davon doch streichen will, sagt es:

- Wildtiere, Herden, Revier und Erschrecken (D35–D37, D59), Biome und Habitate (D55)
- Türen mit Blatt, Fenster, Zaun und Tor, Dächer, Nachtkarten, Hidden Map, Level-1-Varianten, 46×46 (D54–D58, D61, D64, D65)
- eigene Karten (D2), Wegsuche der KI per Breitensuche (D62, die Nachbarwahl des Originals bleibt als Rückfall, siehe §4)

Dazu kommt eine Anpassung, die „wie im Original“ nicht wörtlich geht: Die **Routen, Wegpunkte, Auslöser und Geländetabellen** des Originals gehören zu seinen Spectrum-Karten. Unsere Karten sind eigene Entwürfe, also werden Routen und Auslöser **für unsere Karten neu entworfen**, mit Struktur, Dichte und Flags des Originals (K10.8) und unseren Gelände-Familien mit den Kosten des Originals.

---

## 2. Wo wir stehen

| Bereich | Heute (`ai.c`, 1142 Zeilen) | Original (K10) |
|---|---|---|
| Kreatur im Kampf | nächstes sichtbares Ziel, drei Aktionen | Stärkefilter `2·C ≥ 1,5·Def`, Flucht, bis 50 Schleifendurchgänge |
| Fernwaffen | keine | Bogen, magischer Bogen, Drachenfeuer |
| Gegenstände | Waffe/Schild aufnehmen, Schätze sammeln | Wunschtabelle, Essen, Tränke, Waffenwechsel, Beute zum Zauberer werfen |
| Zauberer | Bolt, bis 5 Kreaturen, Haus plündern, Portal; bleibt im Haus (D62) | Prioritätstabelle, Schild, Tränke, Drachen, Flächenzauber; **läuft Routen**, flieht, geht zum Portal |
| Zauberer-Werte | Zeile `wizard` (Con 30, Mana 80) | Torquemada Con 63 / Mana 120, Elbo Smogg 63 / 140, Ragaril 50 / 200 |
| Bewegung | Jagd, Wachposten, Wildtiere, Breitensuche | Routen, Schlaf, Auslöser (bei Geländeänderung), Leibwache, Portal-Sammeln |
| Messung | `host/arena.c` (alle gegen alle) | – |

Laufzeit auf der Hardware (Bench 46×46): KI-Phase 80 ms bei frischem Start.

---

## 3. Phase 0 – Regeln wie im Original

Die KI-Schwellen rechnen mit den Werten des Originals (`C_eff`, `Def_eff`, `A = 4L+25`) und mit seiner Sichtliste. Darum kommen die Regeln zuerst. Acht PRs:

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 0a | **Mana:** neue Spalte `mana` je Zauber (K5.2), `spell_mana = Basis × (L+1)`; die bisherigen `mana_base`/`mana_step` heißen `xp_base`/`xp_step` und gehören dem Designer. KI-Wahl „günstigster Zauber“ und Mana-Tests ziehen mit | R1 | S |
| 0b | **Zustände:** Wunden als Zähler 0–7 (2 Con je Zug, Curse = 7); Constitution-Faktor `⌊ConMax/ConAkt⌋` auf AP, Combat, Defence; Ausdauer: Schwelle und Erholung `Max/6`, Kosten je Aktion nach K8.1 (Nahkampf 8, Zaubern, Werfen, Schießen, Aufheben 0), Ertrinken (D48) neu auf dieser Grundlage; Flug kostet Ausdauer auch im Stand; **Abheben und Landen nach K8.1/K8.4:** Abheben nur mit Flug-AP, mindestens 6 AP, nicht angebunden, ohne anderes Wesen auf dem Feld und nicht auf Vine, Blob oder unter Dach (Gelände-Bit „kein Abheben“); Landen nicht angebunden, ohne anderes Wesen auf dem Feld, nicht auf Gelände mit Bit „keine Landung“, **kostenlos**, die AP werden **anteilig** zwischen Boden- und Flugvorrat umgerechnet (ersetzt das Budget je Ebene aus D15); Gelände-Bit „Flug gesperrt“. Spielstand v9 | R9, R10, R11, R12, R20 | M |
| 0c | **Nahkampf:** `Schaden = RND(min(255, 2·(C_eff+1))) − Def_eff`; Waffentabelle K7 mit Combat-, Defence- und Wurfwerten (magische doppelt); **Rückschlag zahlt 4 AP und 4 Ausdauer jedes Mal**; **Gebunden nach K11.7:** Eine Kreatur wird angebunden, wenn bei der Sichtberechnung ein sichtbarer Gegner auf derselben Höhe mit `D < 4` (die 8 Nachbarfelder) steht; neu bewertet nur nach eigener Bewegung oder eigenem Angriff oder wenn sie schon angebunden war, gelöst zu Beginn des eigenen Zugs; Unsichtbare werden nicht angebunden; angebunden zieht, hebt ab und landet sie nicht, Angriffe nebenan bleiben möglich; Gelände angreifen nach K6.4 (6 AP, `RND(2·C_eff) ≥ Zähigkeit`) | R5, R7, R8, R27 | M–L |
| 0d | **Zauber, Tränke, Schild:** Bolt `4L+25`, Blitz `4L+30` gegen Defence; Strength +20, Protection +25, Dauer `⌊(3(L−1)+10)/Verbrauch⌋`; Magic Shield `4(L+1)+12` für `L+1` Züge; Reichweite `2L+7` (Eye `3L+10`, Teleport `2L+30`); Enchant `L+3` Runden, Curse, Magic Attack, Teleport-Streuung nach K5.3 (**Teleport in die Luft nur unter einem Flying-Trank**, am Boden ohne Bedingung); **Subversion auf Reittiere** (Wizards immun, Reiter und Reittier mit dem höheren MR) | R6, R13, R15, R16, R18, R24 | M |
| 0e | **Flächen, Fernwaffen, Drachenfeuer:** Feuer `25+2F`, Blob `16+2(B−1)`, Vine 8, Ausbreitung und Überleben nach K5.3, Geländetabellen mit Entzündbarkeit je Gelände-Familie; Wurfweite `min(36, ⌊2·C/Gewicht⌋+5)`, Bogen 16/22, **Drachenfeuer als Aktion** (Reichweite 12, Angriff 35) | R17, R26 | L |
| 0f | **Beschwören und Designer:** `L` Kreaturen je Wurf, die Stufe wird verbraucht (ersetzt D34); Designer-Preise `Basis + Inkrement × L` für alle 45 Zauber bis Stufe 8; Attributpreise `⌊Wert/Divisor⌋`, Maxima Mana 200, AP 40, Stamina 90, Con 90, Combat 30, Defence 30, MR 100; Zufallszauberer des Originals | R2, R3, R14 | M |
| 0g | **Wertung und Sonderfälle:** Siegpunkte (Wizard-Kill `2×SP`, Beschworener `SP`, Opferwert `(4·Level+15)/2`, Schätze roh); **Portal schließt** nach 12/15/10 Runden und beendet das Spiel; **Pixies immer unsichtbar**; **Reiter handelt mit eigenen AP, Ausdauer und Werten** (Select Rider, K6.6) | R19, R22, R23, R25 | M–L |
| 0h | **Sicht wie im Original (K11):** Reichweite als Achteck `2·dx < R, 2·dy < R, D < R` mit `R` = 19 am Boden und 23 in der Luft (gerade 9 bzw. 11 Felder, diagonal 6 bzw. 7), Ziele mit `D < 4` immer sichtbar; **Flieger sehen über Wände**, Bodenziele unter Blätterdach ab `D ≥ 4` und Gegenstände dort nie; überdachte Felder verstecken die jeweils andere Höhenebene; Unsichtbare nur für den Besitzer und mit Magic Eye; **Feuer und Blob blockieren die Sicht**; Schusslinie für Wurf, Bogen und Feuer nach K11.6 (Luft beteiligt: nur überdachter Boden stoppt, in Dungeons auch Wände). **Der Algorithmus bleibt Shadowcasting (D39):** Er liefert das ganze Sichtfeld für die Hidden Map und ist auf der eZ80 25-mal schneller als eine Linie je Ziel; die Randfälle weichen minimal ab | R36, R37, R38 | M |

Ersetzt durch D67: D15 (Budget je Ebene), D16, D26, D27, D28, D29, D30, D32, D34, D40, D42, die Formeln F1, F2, F4, die Designer-Anker aus F6 (Preise) und die Reichweite aus D17/D18. Das GDD (§6, §7) wird mit Phase 0 neu geschrieben, fast alle Kampf-Selftests mit. Die Arena (`host/arena.c`) ist danach neu zu eichen.

Nicht verlangt, bleibt wie es ist: Mana-Regeneration, Portalrunden und Fluchtbonus (sind schon gleich, §3 des Berichts).

---

## 4. KI-Phasen

### Phase 1 – Fundament (1 PR, M)

Nichts davon ändert das Verhalten sichtbar.

- **Datei aufteilen:** `ai.c` wächst sonst auf über 2000 Zeilen. Vorschlag: `ai.c` (Schleife, gemeinsames), `ai_creature.c`, `ai_wizard.c`, `ai_items.c`. Alles bleibt plattformfrei unter `src/core`.
- **Sichtlisten** (K11.5): Die Gegnerliste hat höchstens **20 Einträge** (Eintragsnummer, `C_eff`, `Def_eff`, ohne Doppelte; ein sichtbarer Reiter zählt zusätzlich zum Reittier), die Gegenstandsliste bis zu **24**. Beide werden **für jede KI-Kreatur neu** gefüllt, aus der Sicht des Besitzers (0h). Dazu die **Schusslinie** nach K11.6.
- **Entscheidungsschleife je Kreatur** nach K10.3: höchstens 50 Durchgänge; Sofortaktionen in fester Reihenfolge (Kessel trinken, Zaubern, Aufheben, Phiole, Essen, Werfen, Waffe wechseln); danach Fernangriff, Flucht, Nahkampfziel, Gegenstand, Route/Jagd, Schritt.
- **Schritt:** Das Original sortiert die 8 Nachbarn nach Entfernung zum Ziel, meidet die **letzten 8 besuchten Felder** und öffnet Türen für 4 AP (Schlüssel `$7D`). Wir behalten die **Breitensuche** (D62) als Planer, weil unsere Häuser Türen und Winkel haben, und übernehmen die Besuchsliste und die Türkosten.
- **Plan je Einheit** (4 Byte wie im Original: Eintragsnummer, Route, Schritt, Flags mit Bit 7 aggressiv, Bit 6 schlafend, unten 6 Bit Auslöser-ID). Das gehört in denselben Spielstand v9 wie die Wunden.
- **Daten:** `creatures.csv` bekommt `aggr` (K2); `data/ai_items.csv` die Wunschwerte (K10.6, auf unsere Objekte abgebildet); die Szenariodatei bekommt Zauberer-Werte, Prioritäten und Routen (Phasen 3, 4).

### Phase 2 – Taktik der Kreaturen (4 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 2a | **Ziel und Flucht** (K10.4): Nahkampfziel bei `2·C_eff ≥ 1,5·Def_eff`, nächstes gewinnt; Bedrohung bei `2·Def_eff < 1,5·C_eff(Gegner)`; Fluchtfeld: erst ein **nicht gefährdetes** Nachbarfeld (nicht unpassierbar, keine freie Schusslinie eines Gegners), sonst das mit der größten Abstandssumme; Ausnahmen für Flieger, Untote und Aggressive. Eine „angebundene“ Kreatur zieht nicht (0c, K11.7); Flucht ist ihr also verwehrt, sie kämpft | R28 | M |
| 2b | **Fernangriff:** Bogen und Drachenfeuer mit Reichweite, Filter `2·A ≥ 1,5·Def_eff` und 8 AP; Untote nur mit magischem Bogen oder Drache auf brennbarem Gelände | R29 | M |
| 2c | **Gegenstände:** Aufheben nach `⌊Wert/(D+1)⌋` bis 10 Gegenstände und zur Tragkraft; Essen bei `1,5·Con < ConMax` oder Ausdauer ≤ Max/4; Heiltrank bei `1,5·Con < ConMax`; Waffenwechsel nach Wert. **Beute (Bit 7) wirft die Kreatur dem eigenen Zauberer zu** (landet auf seinem Feld, kein Schaden) und läuft zu ihm, solange sie so etwas trägt | R30, R31 | M |
| 2d | **Abheben und Landen der KI** (K10.3): Jede Kreatur mit Flug-AP, die am Boden steht, **hebt in jedem Durchgang sofort ab**, wenn die Regeln es erlauben (6 AP; mit weniger endet ihr Zug); Flieger bleiben normalerweise oben. **Landen** nur, wenn sie nicht fliehen, ein Gegenstands- oder Nahkampfziel haben (Wizards auch ohne), das Ziel **angrenzend** ist, bei Truhen noch Tragkraft frei ist und Landen erlaubt ist. Vor Aufheben, Kessel-Trank und Beschwören landet sie, wenn nötig. **Eigenheit des Originals** (im Code gelesen, nicht im Spiel gesehen): Beim Nahkampf prüft die KI die Höhe des Gegners nicht, ein gelandeter Flieger trifft einen fliegenden Gegner dann nicht und hebt im nächsten Durchgang wieder ab. Wir bauen es so nach und prüfen es im Playtest | R20 | S–M |

### Phase 3 – Der KI-Zauberer (3 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 3a | **Werte und Bücher aus der Szenariodatei:** Format `LOCS` v2 mit Name, Mana, AP, Ausdauer, Constitution, Combat, Defence, MR, Tragkraft, Siegpunkten und je Zauber Stufe und Priorität (K10.2). Gegner: Torquemada (Szenario 1), Elbo Smogg (2), Ragaril (3). **Sofortiger Effekt auf den Schwierigkeitsgrad** | R33 | S–M |
| 3b | **Zauberwahl** (K10.5): Prioritätsliste, Halbieren nach dem Wurf **dauerhaft** (200 → 100 → 50 → … → 0, keine Rückstellung bis zum Neuladen), dazu die Eigenheit, dass ein Durchgang ohne Wurf alle ungeraden Prioritäten um 1 abrundet; Bedingungen je Zauber (Bolt/Lightning mit Stärkefilter und Eigenschutz, Flächenzauber im Radius `2L+6`, Shield nur ohne aktives, Beschwören mit Mana ≥ 40 danach und freien Nachbarfeldern); „ohne sichtbare Gegner nur bei halben AP“ | R32 | L |
| 3c | **Tränke und Drachen:** leerer Kessel und Zutat, vorher ablegen, brauen, aus dem Kessel trinken, Phiole füllen (`brew.c` wird vom KI-Code genutzt) | R32 | M |
| 3d | **Der Zauberer läuft Routen** (ersetzt D62): Er wird wie jede KI-Kreatur behandelt, hat einen Plan mit Route (Flag Bit 6 „nur für Wizards“), ist nie aggressiv, flieht vor Gegnern und geht nach der Portalrunde zum Portal. Die D62-Teile „plündert sein Haus“ und „wirkt Beschwörungen zuerst“ bleiben als Sofortaktionen erhalten, soweit das Original sie kennt (Aufheben, Zaubern) | K10.1 | M |

### Phase 4 – Bewegung und Szenario (2 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 4a | **Leibwache und Portal:** `RND(100) < aggr` beim Erscheinen setzt Bit 7; Kreaturen ohne Plan gelten als aggressiv. **Aggressive laufen zu ihrem eigenen Zauberer und bleiben unter Entfernung 5**, sie fliehen nie. Ab der Portalrunde ziehen alle KI-Kreaturen **und der Zauberer** zum Portal | R21, R35 | S–M |
| 4b | **Routen, Schlaf, Auslöser:** Kartenformat v5 mit Wegpunkt-IDs, Routen (Flags Bit 0 Flieger, 1 Use, 2 Tragkraft, 3–5 Wood/Water/Rock, 6 nur Wizards), Plänen und Auslöserfeldern; `gen_maps.py`, Textkarten (`route`-, `waypoint`-, `plan`-, `trigger`-Zeilen). Schläfer wachen bei Sicht eines Gegners oder wenn ein **Auslöser feuert**: bei Geländeänderung auf seinem Feld (Tür öffnen, Wand durchbrechen, Blitz auf Gelände, Feuer/Blob/Vine/Flood, Truhe öffnen), nicht beim Betreten. **Beschworene bekommen einen Zufallsplan** (`RND` über die ersten 11/7/6 Routen, bis die Flags passen) | R34 | M–L |

Das Entwerfen der Routen für **unsere** Karten ist Kartenarbeit: Level 1 (46×46 mit zwei Häusern), Slayer's Dungeon, Ragaril's Domain. Das Original hat 11/7/6 Routen mit 2 bis 12 Wegpunkten (K10.8) als Maßstab.

### Phase 5 – Messen und Eichen (1–2 PRs, M)

- **KI-Duell im Host-Programm:** `host/arena.c` kann alle gegen alle. Neu: Szenario laden, KI-Zauberer (Spieler 2) gegen einen einfachen Test-Gegner über N Partien mit festen Startwerten; Ausgabe: Siege, Runden bis zum Ende, Verluste, gewirkte Zauber je Art. Deterministisch, die Läufe stehen im CHANGELOG.
- **Selftests je Regel** (Host und eZ80): „Goblin flieht vor dem Riesen“, „Schütze schießt nur im Bereich“, „Shield nur ohne aktives Shield“, „Priorität wird halbiert und bleibt“, „Schläfer wacht bei Türauslöser auf“, „Beute landet beim Zauberer“.
- **Playtest** mit dem Nutzer nach 0c, 2c, 3b und 4b (kurze, gezielte Fragen wie in `docs/PLAYTEST-*.md`).

---

## 5. Technik und Risiken

| Thema | Plan |
|---|---|
| **Laufzeit** | Bench vor Phase 1 und nach jeder Phase (`loc --bench` auf der Hardware). Ziel: KI-Phase mit 8 Kreaturen unter 300 ms. Sichtliste je Besitzer, nicht je Feld; keine Divisionen in Schleifen (QUIRK T3); die Breitensuche (1,3 KB Stack) bleibt |
| **RAM** | Neue Bits in `Unit` prüfen (`MAX_UNITS` 32); `build.py` meldet die Reserve (heute 97 KB, Grenze 16 KB) |
| **Spielstand** | v9 gemeinsam für Wunden (0b) und Plan (Phase 1); alte Stände werden abgelehnt |
| **Szenariodatei und Kartenformat** | `LOCS` v2 (3a) und Kartenformat v5 (4b) sind nicht abwärtslesbar; die SD-Karte muss nach dem Bauen neu bespielt werden |
| **Schwierigkeit** | Mit Original-Kampf und -Werten (Con 63, Mana 120, Prioritäten, Spinne schlägt im Schnitt 30 statt 10) wird der Gegner deutlich härter. Die Arena eicht, bevor der Nutzer spielt. Es gibt **nur den Original-Schwierigkeitsgrad**, keine Stufen im Setup |
| **Routen auf eigenen Karten** | Das Original liefert nur Struktur und Dichte; die Wegpunkte entstehen neu |
| **Bekannte Lücken im Original** | Nur noch Feinheiten: die Reihenfolge gleich weit entfernter Nachbarfelder bei der Schrittwahl (unbestimmt, wir nehmen eine feste) und die offenen Punkte in §9 |

---

## 6. Reihenfolge und Umfang

```
0a Mana ──┬─ 0b Zustände ─ 0c Nahkampf ─ 0d Zauber ─ 0e Flächen/Drachenfeuer ─ 0f Beschwören/Designer ─ 0g Wertung
          │
          └─ 0h Sicht ─ 1 Fundament ─┬─ 2a Ziel/Flucht ─ 2b Fernangriff ─ 2c Gegenstände
                          ├─ 3a Werte/Bücher ─ 3b Zauberwahl ─ 3c Tränke/Drachen ─ 3d Zauberer läuft Routen
                          └─ 4a Leibwache/Portal ─ 4b Routen/Schlaf/Auslöser
                                                              5 Messen und Eichen (laufend)
```

- **Sofort möglich und unabhängig:** 0a (Mana).
- **0h (Sicht)** gehört vor Phase 1, weil die Sichtlisten der KI auf ihr beruhen. Sie hängt nicht am Kampfmodell und kann gleich nach 0a laufen.
- **Phase 1** hängt an 0a, 0b (Spielstand v9) und 0h und kann parallel zu 0c–0g laufen.
- **2a, 2b, 3b** rechnen mit `C_eff`, `Def_eff` und `A` und warten auf 0c und 0d; **2b** auch auf das Drachenfeuer aus 0e; **2a** auf das Gebunden-Verhalten aus 0c.
- **2d** (Abheben/Landen) braucht 0b (Flugregeln) und 1; **3a und 4a** brauchen kein neues Kampfmodell und liefern früh sichtbaren Nutzen (härterer Gegner, Portal-Sammeln).
- Gesamt: etwa **21 PRs**, davon Phase 0 mit acht größeren. Jeder PR hält die Regel „Selftests grün auf Host und eZ80, CHANGELOG, GDD“.

---

## 7. Dateien

| Bereich | Dateien |
|---|---|
| Regeln (Phase 0) | `data/spells.csv`, `data/weapons.csv`, `data/actions.csv`, `data/costs.csv`, `data/objects.csv`, `src/core/{spells,combat,items,brew,area,effect,world,wizard,game,ride,sight,view}.c`, `tools/gen_data.py`, `tests/selftest.c`, `docs/design/GDD.md` §4–§8, `host/arena.c` |
| KI (Phasen 1–4) | `src/core/ai*.c` (neu aufgeteilt), `src/core/world.h` (`Unit`-Bits), `src/core/save.c` (v9), `data/creatures.csv`, `data/ai_items.csv`, `data/scenarios/*.txt`, `tools/gen_scenarios.py`, `src/agon/mapfile.c`, `tools/gen_maps.py`, `data/maps/*.txt` |
| Messen | `host/arena.c` oder neues `host/aisim.c`, `tools/test.py`, `docs/TESTING.md` |

---

## 8. Entschieden

Am 2026-10-07 alle sieben Fragen der ersten Fassung mit **„wie im Original“** beantwortet:

1. Rückschlag 4 AP und 4 Ausdauer jedes Mal; „Gebunden“ ohne freien Schlag (R7, R8) → 0c.
2. Beschwören: `L` Kreaturen, Stufe wird verbraucht (R14) → 0f.
3. Designer-Preise nach `Basis + Inkrement × L` (R2) → 0f.
4. KI-Zauberer läuft Routen (K10.1) → 3d.
5. Allwissen: **gegenstandslos.** Die Aggressiven laufen nicht zum Spieler 1, sondern zu ihrem eigenen Zauberer (K10.7, korrigiert). Es gibt kein Schummeln zu entscheiden.
6. Schwierigkeitsstufen: nein, nur der Original-Grad.
7. Reihenfolge: wie in §6, Phase 0 zuerst.

**Frühere Annahmen, die K10 korrigiert hat:** Werfen gilt nicht dem Spieler 1, sondern bringt Beute zum eigenen Zauberer (R31); Aggressive sind Leibwache (R35); Auslöser feuern bei Geländeänderung, nicht beim Betreten (R34); der Gegner-Zauberer bewegt sich selbst (D62 fällt).

---

## 9. Noch offene Fragen an den Spectrum-Quellcode

Beantwortet seit der ersten Fassung (K10.5, K10.7, K10.8, K11): Routentabellen, Auslöser, Nachbarwahl, Priorität (dauerhaft halbiert), Bit 7 (Beute zum Zauberer), Bewegung des Gegner-Zauberers, **Abheben und Landen der KI**, **Sichtliste, Schusslinie und Zähler `$5B40`/`$5B41`**.

Offen:

1. **Mana-Schwelle `$D046` (40)** für Beschwörungen: je Szenario gleich? (3b)
2. **Obergrenze der Beschwörungen:** Wie viele Kreaturen kann der KI-Zauberer höchstens haben (Positionssätze)? Der Plan nimmt bisher unsere 5 (D62) an. (3b)
3. **Startpositionen des Gegner-Zauberers** (`$D043`): Wie wird aus der Anzahl gewählt, und wo steht der Gegner relativ zu seinem Haus? (3d)
