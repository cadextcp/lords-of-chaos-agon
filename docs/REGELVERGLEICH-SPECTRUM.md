# Regelabgleich mit dem Spectrum-Original

Stand 2026-10-06. Quelle der Originalwerte: `lords-of-chaos-zx-agon/docs/REGELN.md` (aus dem Z80-Code gelesen, Szenario 1 geladen, Kapitelnummern „K“ unten beziehen sich darauf). Als Gegenprobe dienten das Amiga-Handbuch (`reference/`) und unser Code (`src/core`, `data/*.csv`). Gerechnete Vergleiche stehen in §5, der Vergleich der KI (K10, nachgereicht) in §6.5.

Kennzeichen in den Tabellen: ✅ gleich · 🔧 in diesem PR angeglichen · 📋 Vorschlag (R-Nummer, §6) · ⛔ bewusste eigene Entscheidung (D-Nummer im GDD).

---

## 1. Ergebnis in Kürze

- **Daten sind nah dran, Regeln nicht.** Die Kreaturentabelle (25 Kreaturen × 10 Werte), die Portalrunden aller drei Szenarien, der Fluchtbonus 10, die Mana-Regeneration (4 %) und die Trank-Dosen (Stufe + 3) stimmen. Es gab nur fünf Kleinigkeiten in der Kreaturentabelle.
- **Drei große Unterschiede** tragen den Rest: das **Kampfmodell** (Trefferchance plus Würfel gegen eine einzige Zufallszahl), die **Zauberformeln** (Mana-Tabelle, Reichweite, Schaden, Schild) und das **Ausdauer-/Wundenmodell**.
- **Kein großer Unterschied ist durch Spectrum-Hardware erzwungen.** Wir haben die Regeln bewusst anders gebaut (D16, D26–D30, D34, D40, D42 und weitere, fast alle auf Wunsch des Nutzers oder nach Playtest). Die Spectrum-Grenzen, die es gibt, betreffen etwas anderes (§4).
- **Werte lassen sich nicht einzeln herauslösen.** Die Boni des Originals (Trank +20/+25, Waffen +10, Schild +20 und mehr, Bolt-Stärke 4·L+25) sind auf sein Schadensmodell `RND(2·(C+1)) − Def` geeicht. In unser Modell (`50 + 5·(C − Def)` Prozent, dann Würfel) eingesetzt, wären sie zu stark. Sie gehören mit dem Modell zusammen übernommen oder gar nicht (R5).
- **Wichtigster Einzelfund: Unsere „Mana-Kosten“ sind sehr wahrscheinlich die XP-Preise des Zauberer-Designers.** Alle 45 Zeilen von `data/spells.csv` (`mana_base`, `mana_step`) sind identisch mit der Designer-Tabelle `$DC84` (Basis, Inkrement je Stufe); mit der Mana-Basis `$B4BE` des Originals stimmt keine einzige überein. Das Original rechnet Mana als `Basis × (L+1)` mit kleinen Basiswerten (R1). Die Gegenprobe am Amiga ist noch offen (Beobachtung O4).

---

## 2. In diesem PR angeglichen

Nur Werte und Mini-Regeln, die in jedem Kampfmodell gleich gelten und nicht gegen eine ausdrückliche Entscheidung stehen. Tests angepasst, `tools/test.py` auf Host und eZ80 grün.

| Was | Vorher | Original (K) | Jetzt |
|---|---|---|---|
| Dwarf, Goblin, Troll: Wood-Typ (zahlen im Wald nur Bodenkosten) | nein | ja (K2) | ja |
| Giant Bat: Trank-Verbrauch | 3 | 2 (K2) | 2 |
| Ghost: Use (Türen/Truhen bedienen) | nein | ja (K2) | ja |
| Zauberer: Tragkraft | 30 | 36 (K3.2) | 36 |
| Zauber wirken: AP | 10 | 8 (K8.1) | 8 |
| Nahkampf: AP | 10 | 8 | 8 |
| Fernwaffe/Flammenatem: AP | 12 | 8 | 8 |
| Werfen: AP | 10 | 8 | 8 |
| Aufheben: AP | 6 | 8 | 8 |
| Fallen lassen: AP | 2 | 0 | 0 |
| Essen: AP | 6 | 4 | 4 |
| Phiole füllen: AP | 6 | 4 | 4 |
| Aufsitzen: AP | 6 | 10 | 10 |
| Abfliegen: AP | 4 | 6 | 6 |
| Landen: AP | 4 | 0 | 0 |
| Gewichte: Schwert 4→10, Messer 1→3, Schild 4→8, Bogen 3→4, Keule 5→9, Axt 6→7, Wurfstern 1→4, Slayer/Magie-Slayer 6→9, leerer Kessel 8→15, leere Phiole 1→2, volle Phiole 1→4, Drachenkraut 1→2 | siehe links | Waffentabelle K6/K7 | angeglichen |
| Zauberer-Designer: Start-Constitution | 25 | 34 (K3.2) | 34 |
| Zauberer-Designer: Start-Mana | 90 | 80 | 80 |
| Designer-Preis je Punkt: Constitution / Mana | 2 / 9 | `⌊Startwert/Divisor⌋` = 3 / 8 (K3.3) | 3 / 8 |
| Apfel: Constitution | +4 | +10, dazu +40 Ausdauer (K7) | +10 |
| Essen gibt Ausdauer: 4 × Constitution-Gewinn | nein | ja (K7) | ja |
| Fliegen per Trank (Kreatur ohne Flügel): AP je Runde | 1 × Boden-AP | 2 × Boden-AP (K8.2) | 2 × |

Die Designer-Werte (Con 34, Mana 80) hat der Nutzer am 2026-10-06 ausdrücklich dem Spectrum zugesprochen (vorher Anker F6). Die Standardvorlage `wizard_apply_standard_set` kostet dadurch 531 statt 522 XP.

Folgen: Der Zauberer mit 40 AP schafft jetzt 5 Zauber pro Runde (mit den 34 AP des Designers wie im Original 4). Der Anker „etwa 4 Zauber pro Runde“ (PM 7) gilt damit für die Designer-Werte. Mit den schwereren Waffen (Schwert 10, Schild 8, Bogen 4) füllt sich die Tragkraft 36 deutlich schneller.

Nicht angeglichen, obwohl klein, weil es mit einer Entscheidung oder mit einem anderen Punkt zusammenhängt: Rückschlag-Kosten (R7), Tür-/Truhen-Kosten (nur bei uns, K8.1 hat sie nicht), Ausdauer pro Aktion (R11), Schild-Stärke und Trank-Boni (R5).

---

## 3. Bereits gleich

- Kreaturentabelle: AP, Flug-AP, Ausdauer, Konstitution, Combat, Defence, MR, Tragkraft, Siegpunkte und die Typ-/Reit-/Untoten-Flags aller 25 Kreaturen (maschinell gegengeprüft; Zauberer-Zeile ist eigener Wert).
- **Portal:** Erscheinungsrunde `12 + RND(4)`, `20 + RND(5)`, `44 + RND(8)` für die Szenarien 1–3 (`portal …` in `data/maps/*.txt`); Fluchtbonus 10.
- Mana-Regeneration 4 % des Maximums je Runde (`⌊max/25⌋`); Erschöpfte bekommen halbe AP; Speed-Trank verdoppelt AP und verdreifacht die Ausdauer-Erholung (bei uns 25 % → 75 %, im Original 1/6 → 1/2, das Verhältnis ist dasselbe).
- Wunde bei Einzeltreffer über 25 % der Maximal-Constitution.
- Bewegung: Typ-passendes Gelände kostet Bodenkosten (4); Flug 4 AP; Bewegungs-Ausdauer = halbe AP.
- Brauen: Kessel mit Stufe + 3 Schlucken; Drachen brauchen leeren Kessel plus Drachenkraut; Heiltrank heilt Constitution, Ausdauer und Wunden.
- Untote nehmen nur Schaden von Untoten, magischen Waffen und Zaubern.
- Blitz trifft auch die 8 Nachbarfelder; Magic Attack trifft auch eigene Kreaturen; Teleport setzt die AP auf 0.
- Sichtweite: 9 Felder am Boden, 11 in der Luft (im Original `$13`/`$17` in Entfernungseinheiten, gleiche Größenordnung).
- Zauberer-Designer: Startwerte Combat 5, Defence 5, MR 70, Ausdauer 34, AP 34 und 600 XP; unsere Punktkosten (2/2/4/2/4, Mana 9, AP 8) sind genau die Original-Kosten `⌊Startwert / Divisor⌋` (K3.3).

---

## 4. Spectrum-Grenzen und was sie für uns bedeuten

Das Original lief auf einem 8-Bit-Rechner mit 48/128 KB. Daraus folgen diese Eigenheiten, die wir **nicht** übernehmen müssen und größtenteils nicht übernommen haben:

| Spectrum-Eigenheit | Ursache | Bei uns |
|---|---|---|
| Karte 36×36, immer Torus | Speicher, feste Szenariotabellen | 46×46 möglich (D64), Umbruch pro Karte |
| Werte höchstens 255 (Siegpunkte gedeckelt, Schaden 255 = „tot“) | ein Byte je Wert | VP sind 16 Bit; 255 bleibt als Schaden bei Magic Attack ein Designwert |
| Entfernung `2·max(dx,dy) + min(dx,dy)` | ganzzahlige Näherung der Euklid-Entfernung ohne Wurzel | Chebyshev; ein level-abhängiger Radius lässt sich auch damit bauen (R13) |
| Drei feste Szenarien, 1–4 Spieler, keine „Game Length“ | fest einkodierte Szenarioparameter | Szenarien als Daten; Rundenlimit nach Portalöffnung bleibt eine Regel (R23) |
| Kein Bomb-Potion, dafür Super-Trank (Ambergris) | Amiga-Fassung ist „re-designed“ | wir haben die Bombenphiole der Amiga-Fassung |
| AP als ein Zähler, daher anteilige Umrechnung Boden ↔ Luft | ein Wert je Kreatur | zwei Budgets je Ebene (D15); Original-Kosten Abfliegen 6 / Landen 0 übernommen |
| Platzierung beim Beschwören: bis zu 40 Zufallsversuche je Kreatur | Schleife statt Suche | feste Nachbarreihenfolge, gleiche Wirkung |

**Die großen Regelunterschiede (§6) sind keine dieser Grenzen.** Ob der Einzelwurf `RND(2·(C+1)) − Def` des Originals eine Spar-Variante für 8-Bit-Arithmetik ist oder ein bewusstes Design, lässt sich aus dem Code nicht entscheiden; er ist jedenfalls nicht zwingend, denn ein Z80 würde auch eine Trefferprozent-Rechnung schaffen. Unser Modell ist eine Grundsatzentscheidung des Nutzers (D&D-Vorbild, D28–D30, D42), kein Zwang.

---

## 5. Gerechnete Vergleiche

### 5.1 Nahkampf: erwarteter Schaden je Schlag

Original: `RND(min(255, 2·(C+1))) − Def`, nur positiv. Wir: Treffer mit `50 + 5·(C − Def)` % (10–90), Schaden Waffenwürfel + C/5 (waffenlos 1w4), Waffe zählt nicht in die Trefferchance (D42).

| Paarung | Original: Treffer | Original: Ø Schaden | Wir: Treffer | Wir: Ø Schaden |
|---|---:|---:|---:|---:|
| Goblin (9) → Goblin (9), faustlos | 50 % | 2,8 | 50 % | 1,9 |
| Goblin (9) mit Schwert → Goblin (9) | 75 % | 11,6 | 50 % | 5,5 |
| Troll (12) → Zwerg (6) | 73 % | 7,3 | 80 % | 3,7 |
| Riese (21) → Troll (16) | 61 % | 8,6 | 75 % | 5,0 |
| Bär (22) → Goblin (9) | 78 % | 14,5 | 90 % | 6,0 |
| Spinne (41) → Zauberer (Def 12) | 85 % | 30,4 | 90 % | 9,6 |
| Zauberer (10) mit Schwert → Goblin (9) | 76 % | 12,6 | 55 % | 6,5 |
| Zauberer (5), faustlos → Goblin (9) | 17 % | 0,2 | 30 % | 1,2 |
| Goldrache (50) → Zwerg (6) | 93 % | 44,7 | 90 % | 11,4 |

Beim Original wächst der Schaden **linear mit Combat** (grob C/2 je Schlag); bei uns wächst er mit C/5 und den Würfeln der Waffe. Starke Kreaturen (Spinne, Drache) schlagen im Original **drei- bis viermal** so hart. Dafür sind schwache Kämpfer mit schlechten Werten dort fast wirkungslos (Zauberer 5 gegen Goblin 9: 17 %). Unsere Kämpfe sind also länger und gleichmäßiger.

### 5.2 Zauber

Bolt gegen einen Goblin (Def 9, MR 46). Original: Angriffswert `4·L+25`, gegen **Defence**; wir: `(3+L)w6` gegen **Magieresistenz** (D40, D29).

| Stufe | Original: Treffer / Ø | Wir: Treffer / Ø |
|---|---|---|
| 1 | 83 % / 21,2 | 54 % / 7,6 |
| 4 | 88 % / 33,0 | 54 % / 13,2 |
| 8 | 91 % / 48,9 | 54 % / 20,8 |

Subversion gegen Goblin (MR 46): Original `RND(8·L+55) ≥ MR`, wir `50 + 5·(4·L − MR/4)` %.

| Stufe | Original | Wir |
|---|---:|---:|
| 1 | 27 % | 15 % |
| 4 | 47 % | 75 % |
| 8 | 61 % | 90 % |

Magic Shield: Original `+4·(L+1)+12` Defence für `L+1` Züge (Stufe 1: +20 für 2 Züge), wir `+2·L` für `2·L` Runden (Stufe 1: +2). Zielreichweite: Original `2·L+7` Einheiten (Stufe 1: 4 Felder gerade, 3 diagonal; Stufe 8: 11 bzw. 7), wir fest 6.

Mana (Stufe 1 / Stufe 4; Original `Basis·(L+1)`, wir `base + L·step`):

| Zauber | Original | Wir |
|---|---:|---:|
| Giant Bat | 4 / 10 | 7 / 13 |
| Harpy | 10 / 25 | 16 / 31 |
| Demon | 38 / 95 | 61 / 124 |
| Magic Bolt | 2 / 5 | 9 / 18 |
| Magic Shield | 4 / 10 | 9 / 18 |
| Teleport | 6 / 15 | 18 / 24 |

Vor allem die **billigen Zauber** (Bolt, Shield, Eye) kosten bei uns ein Vielfaches. Bei 80–90 Mana und 4 % Regeneration ist das ein spürbarer Unterschied im Spielfluss.

---

## 6. Vorschlagsliste

Aufwand: S ≤ 1 Tag, M einige Tage, L eine Woche und mehr. Ursache: „Grundsatz“ = wir haben es bewusst anders gebaut; „Daten“ = vermutlich Fehler bzw. Verwechslung bei uns; „Spectrum“ = vom Original-Rechner erzwungen.

### 6.1 Empfohlen, klein und modellunabhängig

| # | Regel | Original (K) | Wir | Ursache | Aufwand | Anmerkung |
|---|---|---|---|---|---|---|
| R9 | **Wunden zählen** | Zähler 0–7; jede Wunde kostet 2 Con je Zug; Curse setzt sofort 7 (K4, K5.3, K6.3) | ein Flag, −1 Con je Runde; Curse = eine Wunde | Grundsatz (Handbuch sagt nur „steady decrease“) | S | Spielstand v9. Curse wird dadurch deutlich gefährlicher (bei 7 Wunden −14 Con je Zug) |
| R10 | **Constitution-Malus stufenweise** | AP, Combat, Defence ÷ `⌊ConMax/ConAkt⌋` (halb unter 50 %, Drittel unter 33 %, …) (K4, K6.1) | halbe AP unter 50 %, Combat/Defence −2 | Grundsatz | S | Die Combat-/Defence-Folge hängt an R5; die AP-Folge ist unabhängig |
| R12 | **Flug kostet Ausdauer auch im Stand** | je Zug Ausdauer −= `⌊übrige AP/2⌋` (K4) | nur Schrittkosten | Grundsatz | S | Verhindert unbegrenztes Schweben; wirkt auf Pegasus, Gryphon, Drachen, Flug-Trank |
| R13 | **Reichweite nach Stufe** | Zielzauber `2·L+7`, Eye `3·L+10`, Teleport `2·L+30` Einheiten (K5.1) | fest 6 Felder | Grundsatz; die Spectrum-Entfernung selbst ist eine Näherung | S | Metrik bleibt Chebyshev, nur der Radius wächst mit der Stufe. Teleport wird deutlich weitreichender |
| R19 | **Pixies sind immer unsichtbar** | Kreatur `$93` bekommt „unsichtbar“ (K2) | nein | Grundsatz (Handbuch schweigt) | S | Prüfen, ob die Amiga-Fassung es auch tut (WinUAE); danach ein Flag in `creatures.csv` |
| R23 | **Portal schließt, Spiel endet** | Portal bleibt 12/15/10 Runden offen, danach Ende (K4); Anzahl Start-/Portalpositionen zufällig | Portal bleibt offen, kein Rundenlimit | Grundsatz (D3: Spiellänge unbekannt) | S–M | Der Spectrum hat dafür keine Option, es ist Szenariodatenwert. Gibt dem Spiel eine Uhr |
| R24 | **Subversion auf Reittiere** | Wizards und Reittiere mit Zauberer-Reiter immun; sonst Reittier und Reiter zusammen mit dem höheren MR (K5.3, K6.6) | alle Reittiere ausgenommen | Grundsatz (offener Punkt im HANDOVER) | S | Das ist die dort angekündigte Reit-Ausnahme |

### 6.2 Empfohlen, braucht erst eine Prüfung oder Entscheidung

| # | Regel | Original (K) | Wir | Ursache | Aufwand | Anmerkung |
|---|---|---|---|---|---|---|
| R1 | **Mana-Kosten** | `Basis × (L+1)`, Basis z. B. Bat 2, Harpy 5, Bolt 1, Shield 2 (K5.1, K5.2) | `base + L·step` mit den Zahlen der XP-Tabelle | **Daten** (45/45 Zeilen identisch mit `$DC84`) | S (Formel + neue Spalte) | **Erst O4 in WinUAE:** Giant Bat Stufe 1 kostet 4 Mana (Original) oder 7 (unsere Tabelle)? Dann übernehmen; KI-Wahl „günstigster Zauber“ und alle Mana-Tests ziehen mit |
| R2 | **Designer-Preise für Zauber** | Stufe L→L+1 kostet `Basis + Inkrement·L` XP, bis Stufe 8, alle 45 Zauber (K3.3, K5.2) | nur Beschwörungen kaufbar, erste Stufe `design_cost`, jede weitere 50 % davon | Grundsatz (F6, Nutzer-Anker) | S | Unsere heutigen `mana_base`/`mana_step` sind genau diese XP-Zahlen und können nach R1 in die XP-Spalten wandern. Die Nutzer-Anker der Beschwörungen (Bat 4, Harpy 12, Spider 28, Ghost 22, Vampire 50, Spectre 44, Demon 58) weichen von den Spectrum-Basiswerten ab (5, 11, 22, 14, 29, 29, 40); Entscheidung nötig |
| R3 | **Attributpreise steigen mit dem Wert** | Preis je Punkt `⌊Wert/Divisor⌋` (Mana 10, AP 4, Stamina 8, Con 10, Combat 2, Defence 2, MR 16); Maxima Mana 200, AP 40, Stamina 90, Con 90, Combat 30, Defence 30, MR 100 (K3.3) | feste Preise (2/2/4/2/4, Mana 9, AP 8); Maxima Mana 250, AP 120, Stamina 100, Con 60 | Grundsatz | S | Unsere festen Preise sind die Original-Preise beim Startwert, steigen aber nicht. Maxima so hoch machen Fernkämpfer-Zauberer möglich, die es im Original nicht gibt |
| R4 | **Startwerte des Zauberers** | Con 34, Mana 80 | – | – | – | **Erledigt** (§2). Offen bleibt der Zufallszauberer des Originals (Combat/Defence 6, MR 90, Zauberlevel `RND(3)`) |

### 6.3 Ein Block: das Kampfmodell (Entscheidung nötig, Aufwand L)

R5, R6, R15, R16 und R17 hängen zusammen. Sie ändern nichts einzeln, sondern entweder alle oder keines.

| # | Regel | Original (K) | Wir | Ursache |
|---|---|---|---|---|
| R5 | **Nahkampfschaden** | `Schaden = RND(min(255, 2·(C_eff+1))) − Def_eff`, ≤ 0 = Fehlschlag; Waffe zählt in C und Def; Schild +13 (K6.1, K6.2) | Treffer `50+5·(C−Def)` %, dann Waffenwürfel + C/5; Waffe nicht in der Trefferchance (D16, D28, D42) | Grundsatz |
| R6 | **Schadenszauber** | Bolt `A = 4L+25`, Blitz `A = 4L+30`, gegen Defence; Schaden `RND(2(A+1)) − Def` (K5.3) | Würfel `(3+L)w6` bzw. `(5+L)w6` gegen **Magieresistenz** (D28, D29, D32, D40) | Grundsatz |
| R15 | **Tränke** | Strength Combat +20, Protection Defence +25, fest; Dauer `⌊(3(L−1)+10)/Verbrauch⌋` Züge (K8.2) | `2·L` bzw. Stufe, Dauer `8·L/Verbrauch` Runden (F1) | Grundsatz |
| R16 | **Magic Shield** | Defence `+4(L+1)+12` für `L+1` Züge (K5.3) | `+2·L` für `2·L` Runden | Grundsatz |
| R17 | **Flächenschaden und Ausbreitung** | Feuer `25+2F`, Blob `16+2(B−1)`, Vine 8 je Runde; Ausbreitung `RND(70−3·Lv)+1 ≤ Entzündbarkeit`, Überleben `RND(T) < ⌊S/4⌋+40`; Vine/Flood auf 9×9-Quadrat (K5.3, K8.3) | Feuer 6, Blob 3, Vine 2; Ausbreitung `Stufe·10 %` (F4) | Grundsatz; Geländetabellen mit Entzündbarkeit je Kachel fehlen bei uns |

Einordnung der drei Wege:

- **A: Unser Modell behalten.** Kein Aufwand. Die Original-Boni bleiben unübernommen; die Zahlen aus §5 stehen als Referenz. Kämpfe bleiben lang und gleichmäßig.
- **B: Original-Modell mit allen Werten.** Aufwand L: `combat.c`, `items.c`, `spells.c`, `brew.c`, `area.c`, die Zauber-KI und fast alle Kampf-Selftests ändern sich. Kämpfe werden kürzer und tödlicher, starke Kreaturen sehr stark. Spielstand ändert sich nicht.
- **C: Regelprofil „Original“ und „Modern“.** Ein Schalter im Setup wählt die Formeln (Klassik-Profil entspricht dem Namen „Classic“ des Projekts). Aufwand L plus Pflege zweier Balancings; macht Vergleichsspiele mit dem Original möglich. Meine Empfehlung, falls beide Wege gewünscht sind; sonst A.

### 6.4 Bewusst anders, Ursache eigene Entscheidung (kein Änderungsvorschlag)

| # | Regel | Original (K) | Wir | Begründung bei uns |
|---|---|---|---|---|
| R7 | **Rückschlag** | Verteidiger zahlt 4 AP und 4 Ausdauer, jedes Mal, wenn er noch kann (K6.2) | kostenlos, einmal pro Runde (D27, D29) | Playtest: Verteidiger wurden zum Rückschlag gezwungen und starteten erschöpft |
| R8 | **Gebunden** | Wer neben einem Gegner steht, kann nicht ziehen, bevor er ihn getötet hat (K8.1; auch Amiga-Handbuch) | Wegziehen erlaubt, Gegner bekommt einen freien Schlag (D26) | Review-Fix des Nutzers; Entscheidung beim Nutzer, ob das so bleibt |
| R14 | **Beschwören** | `L` Kreaturen je Wurf, Stufe wird verbraucht, Mana `Basis·(L+1)` (K5.3) | eine Kreatur, Stufe = Kreaturstärke, unbegrenzt wirkbar (D34) | Nutzerregel; nicht von Spectrum-Grenzen verursacht |
| R20 | **AP beim Ebenenwechsel** | AP werden anteilig umgerechnet (K2) | zwei Budgets je Ebene (D15) | Eigene, einfachere Lösung |

### 6.5 KI der Computer-Zauberer und Kreaturen (K10)

Das Original hat zwei KI-Teile: Kreaturen (NPCs und Beschwörte) und einen einzigen Gegner-Zauberer als Spieler 2. Unsere KI (`ai.c`, D20, D35, D37, D62) ist anders gebaut: Jäger mit Sichtlinie, Wildtiere, ein defensiver Zauberer, der sein Haus plündert, Wegsuche per Breitensuche.

**Gegner-Zauberer** (K10.2) und unser KI-Zauberer. Bei uns bekommt der KI-Zauberer die Zahlen der Zeile `wizard` in `creatures.csv` (AP 40, Ausdauer 60, Constitution 30, Combat 10, Defence 12, MR 80, Mana 80) und ein kleines Buch aus `data/scenarios/*.txt`.

| | Torquemada (Sz. 1) | Elbo Smogg (Sz. 2) | Ragaril (Sz. 3) | wir |
|---|---|---|---|---|
| Mana | 120 | 140 | 200 | 80 |
| AP / Ausdauer | 34 / 68 | 34 / 68 | 34 / 78 | 40 / 60 |
| Constitution | 63 | 63 | 50 | 30 |
| Combat / Defence | 9 / 10 | 9 / 10 | 13 / 18 | 10 / 12 |
| MR / Tragkraft | 90 / 48 | 90 / 48 | 90 / 48 | 80 / 36 |
| Buch | 24 Zauber mit Stufe (K10.2) | Beschwörungen, Tränke, Blob, Vine, Bolt, Lightning, Shield | Beschwörungen, Tränke, Bolt, Lightning, Shield | Sz. 1 `goblin 2, magic_bolt 1`; Sz. 2 `zombie 3, ghost 2, magic_bolt 2`; Sz. 3 `vampire 3, spectre 2, demon 1, magic_bolt 3, curse 1` |

Die Gegner des Originals haben also **doppelt so viel Constitution und 1,5- bis 2,5-mal so viel Mana** wie unser KI-Zauberer. Ihre Bücher sind groß: Beschwörungen (Sz. 1: Goblin, Troll, Centaur, Elephant, Bear, Crocodile, Bat, Harpy, Vampire, Spectre), alle sieben Tränke auf Stufe 2 (Sz. 1), Fire 5, Blob 3, Vine 5, Flood 3, Bolt 3, Lightning 2, Shield 3.

| # | Regel | Original (K10) | Wir | Ursache | Aufwand |
|---|---|---|---|---|---|
| R28 | **Kampfeinschätzung der Kreaturen** | Ziel nur, wenn `2·C_eff(eigen) ≥ 1,5·Def_eff(Ziel)`, nächstes Ziel gewinnt. **Flucht**, wenn `2·Def_eff(eigen) < 1,5·C_eff(Gegner)`, auf das Feld mit der größten Summe der Abstände zu allen Bedrohungen; Flieger ignorieren Bodengegner; **aggressive Kreaturen fliehen nie** (K10.3, K10.4) | greifen immer das nächste sichtbare Ziel an, fliehen nie (außer erschreckte Wildtiere, D37) | Grundsatz (nicht Spectrum-bedingt) | M (an R5 gekoppelt, weil die Schwellen Original-Werte rechnen) |
| R29 | **Fernangriff der KI** | Kreaturen mit Bogen und Drachen feuern, wenn `2·A ≥ 1,5·Def_eff`, Entfernung unter der Reichweite und 8 AP da sind; Untote nur mit magischem Bogen (K10.4) | die KI benutzt keinen Bogen (`items_fire` kommt in `ai.c` nicht vor) | Grundsatz | M |
| R30 | **Gegenstände der Kreaturen** | Wunschtabelle je Kachel (Schätze 100–127, magische Waffen 70–105, Tränke 90–127); Aufheben nach `⌊Wert/(D+1)⌋`; Essen bei `1,5·Con < ConMax` oder Ausdauer ≤ Max/4; Heiltrank bei `1,5·Con < ConMax`; Waffenwechsel nach Wert; bis 10 Gegenstände (K10.6) | nur Waffe/Schild aufnehmen, Schätze sammeln (D62); kein Essen, kein Trank, kein Wechsel | Grundsatz | M |
| R31 | **Werfen auf den Zauberer von Spieler 1** | Nicht-Wizards werfen Wurfgegenstände (Bit 7) auf den Wizard von Spieler 1, wenn in Wurfweite (K10.6) | nein | Grundsatz; im Original auch **allwissend** (Spieler 1 ist bekannt) | S, geringer Wert |
| R32 | **Zauberwahl des KI-Zauberers** | Prioritätstabelle je Szenario und Zauber, halbiert nach jedem Wurf; Bedingungen: Bolt/Lightning nur wenn `2·(4L+25) ≥ 1,5·Def_eff`, Lightning nicht bei eigenen Kreaturen unter 4; Fire/Blob im Radius `2L+6`; Vine/Flood nicht bei eigenen Kreaturen unter `2L+1`; Shield nur ohne aktives Shield; Tränke und Drachen mit Kessel und Zutat; Beschwören nur mit Mana ≥ 40 danach (außer Gegner sichtbar) und ≥ L freien Nachbarfeldern; ohne sichtbare Gegner nur bei ≥ halben AP (K10.5) | Bolt auf den nächsten Gegner, Beschwören bis 5 Kreaturen (teuerste zuerst, ein Viertel Mana Reserve), sonst nichts: **kein Schild, keine Tränke, keine Flächenzauber, keine Drachen** | Grundsatz | L |
| R33 | **Eigene Werte und Bücher der Gegner-Zauberer** | siehe Tabelle oben (K10.2) | Standardwerte der Zauberer-Zeile, Mini-Bücher | Grundsatz (Datenlücke) | S–M: Format der Szenariodatei um Werte erweitern, `gen_scenarios.py` und `mapfile.c` ändern. **Empfohlen**, wirkt sofort auf den Schwierigkeitsgrad |
| R34 | **Schlaf, Auslöser und Routen** | Kreaturen der Szenariotabelle schlafen, bis sie einen Gegner sehen oder ein Wesen ein Auslöserfeld betritt; nicht aggressive laufen Wegpunkt-Routen (11/7/6 Routen je Szenario) (K10.7) | Wachposten (`post`) und Wildtiere; keine Routen | Grundsatz | M–L (Kartenformat: Wegpunkte und Auslöser) |
| R35 | **Aggressivität und Portal** | `RND(100) < Aggressivität`: die Kreatur läuft zum Zauberer von Spieler 1 und bleibt unter Entfernung 5 stehen, sie flieht nie; **ab der Portalrunde laufen alle KI-Kreaturen zum Portal** (K10.7) | siehe R21: Monster jagen sichtbare Ziele; bei offenem Portal geht nur der Zauberer dorthin | Grundsatz; das „Läuft zum Spieler 1“ ist allwissend und passt nicht zu unserem „kein Schummeln“ (D20) | S–M |

Nicht übernehmen, obwohl es dort steht (**Spectrum-Grenzen**):

- Die Zauber-KI ist **fest auf Spieler 2 verdrahtet** (Mana von Spieler 2 liegt an einer festen Adresse, K10.1), und es gibt nur im Ein-Spieler-Modus einen KI-Zauberer. Unser KI-Zauberer läuft für jeden Besitzer.
- Enchant, Subversion, Curse, Magic Attack, Teleport und Magic Eye wirkt die KI des Originals **nie** (ihr Handler ist ein leeres `ret`, `$A45F`). Das ist ein Loch in der Umsetzung, kein Spielentwurf. Unsere KI wirkt diese Zauber auch nicht, hier sind beide gleich.
- Eine Kreatur bricht ihre Entscheidungsschleife nach 50 Durchgängen ab; die Schrittwahl sortiert die 8 Nachbarfelder nur nach Entfernung zum Ziel. Unsere Breitensuche (D62) kommt um Mauern herum und bleibt.
- Die Sicht der KI ist im Original an die Sichtliste gebunden, die aggressive Kreaturen aber umgeht. Wir bleiben beim Verzicht auf Schummeln.

Was bei uns **besser** gelöst ist und bleibt: Wegsuche um Hindernisse, Türen und Truhen öffnen, Plündern des eigenen Hauses (D62), Herden und Revierverhalten der Wildtiere (D35, D37).

### 6.6 Offen und noch nicht bewertet

| # | Regel | Original (K) | Wir | Anmerkung |
|---|---|---|---|---|
| R11 | **Ausdauer je Aktion** | Schwelle der Erschöpfung unter `Max/6`, Erholung `Max/6`, Nahkampf 8, Zaubern/Werfen/Schießen/Aufheben 0 (K4, K8.1) | Schwelle und Erholung 25 %; **jede** Aktion kostet halbe AP aufgerundet; die Ausdauer-Spalte in `actions.csv` wird gar nicht gelesen | Ertrinken (D48) ist auf die 25 % gebaut. Eine Umstellung braucht D48 neu (M) |
| R18 | **Zauberformeln einzeln** | Enchant `L+3` Runden, Curse `RND(8L+55)+10 ≥ MR`, Magic Attack `D < 2L+1` mit Schaden 255, Teleport-Streuung `⌊(D−2L)/2⌋+1` (K5.3) | Enchant `2·L` Runden, Resist-Rolle F2, Magic Attack Radius 2, Streuung `Distanz/4` | Alle S, aber alle an R1/R13 und das Resist-Modell gekoppelt |
| R21 | **NPC-Aggressivität** | Prozentwert je Kreatur: Spinne, Vampir, Dämon 99, Spectre 80, Zombie 40, Riese, Gorilla, Löwe, Bär 20, Fledermaus, Geist 10 (K2) | Wildtier-Klassen (D35, D37); Monster jagen immer | Tauglich als Wahrscheinlichkeit, dass ein Monster der Neutralen jagt. M |
| R22 | **Siegpunkte** | Wizard-Kill = `2×SP`, beschworene Kreatur = `SP`; Opferwert eines Wizards `(4·Level+15)/2`; Schätze zählen roh (K3, K6.5) | nur Wizard im **Nahkampf** doppelt (Amiga-Beobachtung AMI 4); Wizard als Opfer 20; Schätze Gold 40, Rubin 20, … | Skalenfrage: Original-Schatz 16/12/8, Kill 2–18; unser Gold 40 ist hoch. Amiga-Beobachtung hat Vorrang |
| R25 | **Reiter** | Reiter handelt nach „Select Rider“ mit eigenen AP, Ausdauer und Werten; Einzelzauber und Nahkampf treffen nur das Reittier; Magic Attack trifft Reiter einzeln (K6.6) | Reiter handelt aus dem Sattel, aber mit AP und Ausdauer des Reittiers (D60) | Der Original-Zauberer kann reiten und danach mit vollen AP zaubern. Das macht Reittiere deutlich stärker. Der Schadensteil stimmt schon (Reittier wird getroffen, Reiter wird abgeworfen) |
| R26 | **Werfen, Bogen, Drachenfeuer** | Wurfweite `min(36, ⌊2·C/Gewicht⌋+5)` Einheiten; Bogen 16, Magischer Bogen 22, Drachenfeuer 12; Angriffswerte 15/30/35 (K6.4) | Wurf bis 6 Felder, Bogen bis 6 Felder | An R5 gekoppelt. Gewicht und Stärke zählen im Original für die Wurfweite |
| R27 | **Gelände angreifen** | `RND(2·C_eff) ≥ Zähigkeit` (Wand 80), 6 AP + 6 Ausdauer, nur wenn `1,5·C_eff ≥ Zähigkeit` (K6.4) | Waffenwürfel + Zufall gegen Möbel-Zähigkeit, Kosten wie Nahkampf | Unsere Karten haben Möbel statt Geländekacheln mit eigener Zähigkeit |

Nicht verglichen: die KI der Computergegner und Torquemadas (im Original nicht gelesen), Szenario-Population und -Hooks, Sichtlinienformel, Engaged-Bit (REGELN §9). Die Terrain-Tabellen der drei Szenarien (K8.4) sind per Kachel, wir nutzen eigene Gelände-Familien (`costs.csv`); die Werte sind ähnlich (Boden 4, Wald 6–8, Wasser 12), ein Eins-zu-eins-Vergleich lohnt erst nach R17.

---

## 7. Empfohlene Reihenfolge

1. **O4 in WinUAE** (Giant Bat und Magic Bolt Mana auf Stufe 1 ablesen). Das entscheidet R1/R2 und kostet eine Viertelstunde.
2. **Eine Entscheidung zum Kampfmodell** (A, B oder C, §6.3). Sie bestimmt, ob R5, R6, R15, R16, R17 überhaupt anfallen.
3. **Die kleinen Regeln R9, R10 (AP-Teil), R12, R13, R19, R23, R24** in einem oder zwei PRs; sie sind unabhängig voneinander und vom Kampfmodell.
4. **R33 (Werte und Bücher der Gegner-Zauberer)**: Datenänderung, die den Schwierigkeitsgrad sofort anhebt; unabhängig vom Kampfmodell.
5. **R3** (steigende Attributpreise) mit dem Nutzer klären; R4 ist erledigt.
6. R11, R18, R21, R22, R25–R27 danach einzeln; die KI-Regeln R28–R32, R34, R35 nach der Entscheidung zum Kampfmodell (R28, R29, R32 rechnen mit Original-Werten).
