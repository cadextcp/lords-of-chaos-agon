# Amiga-Beobachtungen (Referenz: Original in WinUAE)

Protokoll von Beobachtungen am Amiga-Original. Sie füllen die Lücken des Manuals, siehe GDD §13.

- **Quelle:** Lords of Chaos (1991, Blade), ADF lokal unter `Desktop\amiga\`, Kickstart 1.3, WinUAE. Spielsprache: Deutsch.
- **Status je Eintrag:** ✅ belegt · ❓ Vermutung, noch zu prüfen
- **Screenshots** liegen nur lokal unter `reference/amiga-screens/` und kommen nicht ins Repo.

---

## B1 – Kartenausschnitt und Kachel-Komposition (2026-10-02)

**Situation:** Szenario-Start, Zauberer-Haus (Screenshot `reference/amiga-screens/b1-house.png`).

| # | Beobachtung | Status |
|---|---|---|
| B1.1 | Das Kartenfenster zeigt **7×7 Kacheln**. Die Kacheln sind groß und detailliert; rechts ist das Info-Panel, darunter eine Textzeile. | ✅ |
| B1.2 | **Pro Kachel werden mehrere Elemente überlagert dargestellt**, z. B. Fußboden, Tür und Weg in einer Kachel. | ✅ |
| B1.3 | Der Innenboden des Hauses ist blau gekachelt (Steinboden). Außen ist schwarzer Grund mit grünen Grastupfen. | ✅ |
| B1.4 | Wände werden als **schmale graue Linien bzw. Steinblöcke** gezeichnet, die sich mit den Nachbarn verbinden (Auto-Tiling). Sie füllen nicht die ganze Kachel. | ✅ (Darstellung) |
| B1.5 | Wände belegen **eigene Kacheln**. Siehe B2.1. | ✅ |
| B1.6 | Möbel und Objekte sind sichtbar: Bett/Truhe oben links, Bücherregal, Tür (braun), Kessel/Objekt mit Wand-Detail. | ✅ |
| B1.7 | Unerforschte Bereiche sind schwarz. Die Textzeile zeigt beim Cursor „Unbeschrieben“, vermutlich für ein unerforschtes oder leeres Feld. | ❓ |
| B1.8 | Der Cursor ist ein grauer Rahmen über einer Kachel; der Mauszeiger ist ein gelber Pfeil. | ✅ |

**Folgen für das Design** (eingearbeitet in GDD §11.3):
- Unser Fenster zeigt etwa 27×24 Kacheln statt 7×7. Der volle Sichtradius (9/11) passt ohne Scrollen hinein; das ist eine bewusste Abweichung im Qud-Stil.
- Eine Zelle kann nur ein Glyph darstellen. Die Überlagerung wird per Layer-Priorität, Hintergrundfarbe und Auto-Tiling-Wandglyphen gelöst (GDD §11.3).
- Wände belegen Kacheln (B2.1). Das Kartenmodell braucht **keine** Kanten-Daten.

---

## B2 – Wände, Info-Panel, Haus-Inventar (2026-10-02)

**Situation:** Zauberer im Haus, Cursor auf einer Wandkachel (Screenshot `reference/amiga-screens/b2-wall-center.png`).

| # | Beobachtung | Status |
|---|---|---|
| B2.1 | **Wände verlaufen durch die Mitte der Kachel.** Die Wandlinie ist zentriert, verbindet sich mit den Nachbarwänden und belegt damit die ganze Kachel. Der Cursor auf so einer Kachel zeigt den Text „Wand“. | ✅ |
| B2.2 | Türen (braune Doppeltür) sitzen **in der Wandlinie** auf eigenen Kacheln. | ✅ (Darstellung) |
| B2.3 | Die Textzeile unter der Karte benennt das Element unter dem Cursor („Wand“), vergleichbar mit INFORM bzw. Look. | ✅ |
| B2.4 | Das Info-Panel zeigt **6 senkrechte Balken mit Icons**: Stiefel (grün), Blitz (gelb), Herz (rot), Schwert (grau), Schild (blau), Stern (pink). Belegung: AP, Stamina, Constitution, Combat, Defence, **Mana**. Der pinke Stern ist Mana (vom Spieler bestätigt). Das Manual nennt nur 5 Balken; der Mana-Balken ist Amiga-spezifisch. | ✅ |
| B2.5 | Oben rechts ist ein Vorschaukasten mit grünen und gelben Klammern. Er zeigt die Grafik der Kachel unter dem Cursor (hier ein Wandstück); in B1 war er leer. Was die Klammern bedeuten (grün/gelb), ist unklar. | ❓ |
| B2.6 | Der Auswahl-Cursor der gewählten Einheit ist ein **grüner Rahmen**, entspricht dem „walking cursor“ aus `[PM 7]`. | ✅ |
| B2.7 | Haus-Inventar sichtbar: Kerzen bzw. Kandelaber (rot), Kissen bzw. Betten (pink), blauer Kessel bzw. Schale, Tische, Bett, Bücherregal, Schrank, Wasserbecken (blau). Namen per Cursor-Text erfassen (→ O7). | ✅ (Namen offen) |

---

## B3 – Spectrum-Kartenbogen: Kartengröße und Mana-Tabelle (2026-10-02)

**Quelle:** Kartenbogen „Map created by Pavero, 2007, maps.speccy.cz“ (ZX-Spectrum-Fassung). Er enthält alle 5 Szenarien (3 Basis plus Expansion Kit One) als Übersichts- und Detailkarte sowie zwei Mana-Tabellen. Datei: `reference/spectrum-maps-pavero.png`, nur lokal, Drittwerk.

**Hinweis:** Die Amiga-Szenarien sind laut `[AMI 4]` „re-designed“. Die Werte gelten also für die Spectrum-Fassung und sind für Amiga eine **gute Näherung, aber nicht belegt**.

| # | Beobachtung | Status |
|---|---|---|
| B3.1 | **Kartengröße: 36×36 Kacheln** in allen 5 Szenarien. Gemessen: Detailkarte 867 px bei 24 px pro Kachel, Übersicht 291 px bei 8 px pro Kachel; der Rahmen ist bei allen Karten gleich groß. | ✅ (Spectrum) |
| B3.2 | **Wrap-around ist sichtbar**: Häuser am linken und rechten bzw. oberen und unteren Rand sind Hälften desselben Hauses über die Kante (Szenario 1: Pentakel-Häuser an den Ecken bzw. Rändern). | ✅ |
| B3.3 | **Wege** sind wie Wände als zentrierte, verbundene Linien gezeichnet (graue Punktlinien). Die Wege belegen Kacheln. | ✅ |
| B3.4 | Terrain-Typen, die auf den Karten sichtbar sind: Gras, Magic Wood (türkise Bäume), Felsen bzw. Findlinge, Sumpf bzw. Wasserpflanzen (türkise Wellen), Fluss (blau), Laub- und Obstbäume (grün bzw. rot), hohes Gras (gelb), Mauern (grau mit roten Ziegeln), Türen (gelb), Pentakel, Dungeon-Mauern (grün), Lava- bzw. Feuergänge (gelb-rot, Szenario 2), Inselwelt mit Meer (Iris). | ✅ (Namen per O7 zu bestätigen) |
| B3.5 | **Mana-Kosten für alle 45 Zauber und Stufen 0–8** liegen als Tabelle vor. **Jede Zeile ist exakt linear:** `Kosten(Stufe) = Basis + Stufe × Schritt`, per Skript für alle 45 Zeilen geprüft. Die Daten stehen in `data/spells.csv`. | ✅ (Spectrum) |
| B3.6 | Einige Tabellenwerte sind **rot** statt grün, z. B. Vampire ab Stufe 8, Demon ab 7 und die Drachen ab 6–8. Vermutung: Diese Kosten übersteigen das maximal erreichbare Mana bzw. sind nicht wirkbar. | ❓ |
| B3.7 | Die Tabelle enthält noch das **Super Potion** (8-Bit). Auf dem Amiga ist es entfallen; das **Bomb Potion** fehlt, seine Kosten werden in O4 beobachtet. | ✅ |

**Folgen für das Design:**
- Classic-Kartengröße ist **36×36**. Unser Kartenfenster (27×24) zeigt damit einen großen Teil der Welt. Sichtlinie (9/11) und Hidden Map begrenzen trotzdem, was man sieht.
- Die „Big Map“ kann bei uns die **ganze Welt** auf einen Bildschirm bringen: 36 Spalten bei 1 Zelle pro Kachel, vertikal scrollend oder mit 2 Kacheln pro Textzeile.
- Weil wir eigene Karten bauen (D2), ist 36×36 die Classic-Referenz. Größere Karten sind pro Szenario möglich, verschieben aber die Balance (Portal-Timing, AP).

---

## B4 – AP- und Stamina-Kosten für Bewegung auf Boden (2026-10-02)

**Situation:** Zauberer im Haus, Untergrund „Boden“. Drei Screenshots: Start, ein Schritt nach West, ein Schritt nach Südwest (`reference/amiga-screens/b4-ap-3/4/5.png`). Ausgewertet wurde die Füllhöhe der Balken pixelgenau per Skript (Farbe der hellen Balkenfüllung).

| Bild | AP | Stamina | CON / COM / DEF / Mana |
|---|---|---|---|
| Start | 20 px | 68 px | 50 / 12 / 12 / 140 px |
| nach Schritt **W** (orthogonal) | 12 px (−8) | 64 px (−4) | unverändert |
| nach Schritt **SW** (diagonal) | 0 px (−12) | 58 px (−6) | unverändert |

**Skala:** WinUAE verdoppelt die Pixel (Screenshot 720×568 ≈ 2× PAL). 2 Screenshot-Pixel entsprechen 1 Amiga-Pixel, Hypothese: **1 Amiga-Pixel = 1 Punkt**.

| # | Beobachtung | Status |
|---|---|---|
| B4.1 | **Orthogonaler Schritt auf Boden: 4 AP und 2 Stamina** | ✅ (unter der Skala-Hypothese) |
| B4.2 | **Diagonaler Schritt auf Boden: 6 AP und 3 Stamina**, also genau das 1,5-Fache. Das entspricht der Gollop-Regel aus Laser Squad und X-COM (4 bzw. 6). | ✅ (unter der Skala-Hypothese) |
| B4.3 | Die Rechnung ist konsistent: 10 AP minus 4 ergibt 6, die Diagonale kostet genau die restlichen 6, danach 0. | ✅ |
| B4.4 | Der Auswahl-Cursor (grüner Rahmen) bleibt nach dem Schritt **relativ zur Einheit** an derselben Stelle (vgl. `[PM 8]`). | ✅ |
| B4.5 | Die Skala ist zu verifizieren: Bei vollem AP-Balken (Rundenstart) dessen Pixelhöhe mit dem AP-Wert im Wizard Designer vergleichen. | ❓ |

---

## S1 – Sekundärquelle: Text zum AP-System (2026-10-02, ungeprüft)

Ein vom Spieler eingefügter Übersichtstext ohne Quellenangabe, vermutlich KI-generiert. Er wurde gegen Manual und eigene Messungen geprüft. **Er gilt nicht als Beleg**; seine Aussagen dienen nur als Hypothesen.

| Aussage | Bewertung |
|---|---|
| Kreatur-AP-Werte (Boden bzw. Flug) | ✅ deckt sich mit `[PM 34]` |
| Fliegen kostet terrainunabhängig, Speed Potion verdoppelt die AP, Teleport lässt 0 AP, höchstens etwa 4 Zauber pro Runde | ✅ deckt sich mit Manual `[PM 7, 10, 20, 23]` |
| „Kein Diagonalzuschlag“ | ❌ **widerlegt** durch B4.2 (4 gegen 6 AP) |
| Constitution unter 50 % halbiert die AP | ⚠️ Das Manual halbiert nur bei Erschöpfung (Stamina). Bei Constitution unter 50 % heißt es nur „affected“. |
| Zauberer hat 40 AP | ❓ Hypothese H1. Passt zu „4 Zauber pro Runde“, wenn Zaubern etwa 10 AP kostet (H2). Prüfen per B4.5 und O5. |
| Wege 3–4 AP, Tür und 6 Schritte ≈ 40 AP, etwa 3 Nahkampfangriffe pro Runde, Krokodil ohne Wasser-Malus | ❓ Hypothesen H3–H6, prüfen in O3 bzw. O8 |

---

## Offene Beobachtungs-Aufgaben

| ID | Frage | GDD-Bezug |
|---|---|---|
| ~~O1~~ | ~~Belegen Wände und Türen Kacheln oder Kanten?~~ → **Kacheln** (B2.1) | §3, §11.3 |
| O1b | Info-Panel: Was bedeuten die Balken-Icons (insbesondere pinker Stern = Mana?), der Vorschaukasten und die grün/gelben Klammern? | §11.1 |
| O2 | Kartengröße Amiga: Ist die Big Map auch 36×36 wie bei Spectrum (B3.1)? Nur Stichprobe nötig. | §3.1, §13 |
| ~~O3~~ | *Nicht mehr nötig (D7: eigenes Design, §5.3).* Optional als Plausibilitätscheck: AP-Kosten auf **anderem Terrain**: Gras, Weg, Wald, hohes Gras, Wasser bzw. Sumpf (Boden ist erledigt, B4). Jeweils ein orthogonaler Schritt, Screenshot vorher und nachher. Ideal: am Rundenanfang mit vollem AP-Balken, damit auch B4.5 (Skala) geklärt wird. | §13 |
| O4 | Mana-Kosten Amiga: 2–3 Zauber der Startliste mit `data/spells.csv` vergleichen (gleiche Formel?) sowie **Bomb-Potion-Kosten** ablesen | §7, §13 |
| O5 | Wizard Designer: Attribute, Startwerte, XP-Kosten, Obergrenzen | §7.3, §13 |
| O6 | Setup-Panel: angezeigte Rundenspannen für Spiellänge 1–5, Szenario 1 | §2.2, §9.1 |
| ~~O8~~ | *Nicht mehr nötig (D7).* Optional: Aktionskosten: AP-Balken vor und nach **einem Zauber** (H2: etwa 10 AP?), **Tür öffnen**, **Aufheben** und **Nahkampfangriff** | §5, §13 |
| O7 | Terrain-Katalog: alle Terrain- und Objekt-Typen von Szenario 1 mit Namen aus dem INFORM-Text | §3.3, §8 |
