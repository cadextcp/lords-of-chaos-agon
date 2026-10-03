# Changelog

Format nach [Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionen nach Milestones (siehe `docs/ROADMAP.md`).

## [Unreleased] – M4 Classic komplett

### Geändert
- **Panel zeigt das Objekt in der Hand:** neue Zeile „Hand: <Name>" über der Boden-Liste (gelb); leere Hände zeigen „Hand: -". Antwortet auf die Frage „Wo sehe ich, was aktiv ist?" — Waffen wirken nur in der Hand, der Schild zählt getragen (D21).
- **Grafik v2 (Kreaturen-Review):** alle 26 Kreaturen (Greif komplett neu), 35 Objekte, alle Böden, alle 16 Wandstücke und Türen neu gezeichnet — Design-Export als PNG-Quelle in `assets/tiles/`; der Python-Generator überschreibt diese Kacheln nicht mehr (`V2_TILES`-Liste in `make_tiles.py`, Kreaturen-Generator ausser Betrieb). Schlüsselfarben für die Besitzer-Varianten bleiben erhalten (geprüft: Goblin 29/21, Zauberer 70/81 Pixel). Möbel, Kerzen, Dekor, Overlay- und Cursor-Kacheln bleiben v1.

### Behoben
- **Gebunden nur für eine Phase (GDD 6):** Eine Bodeneinheit neben einem Gegner konnte sich nie mehr wegbewegen, solange der Gegner stand – auf dem Agon fühlte sich das wie eine unsichtbare Mauer an (auch weil der Grund nicht gemeldet wurde). Jetzt bindet erst Nahkampfkontakt (heranziehen, angreifen, angegriffen werden) beide Seiten, die Bindung endet mit der eigenen Phase („im nächsten Zug ist Bewegung wieder möglich“). Beim Versuch wegzugehen erscheint „Gebunden: Gegner daneben - nur Angriff.“, bei Blob/Vine „Brei oder Ranken versperren den Weg.“
- **Szenario 1 startet mit dem Zauberer allein:** Zwerg, Fledermaus und Einhorn des Spielers waren Testeinheiten aus M3g/M4e und standen von Anfang an in der Karte (Garten, Haus). Kreaturen kommen aus dem Zauberbuch. Die Testkarte `testland` behält ihre Entwicklungseinheiten.
- **Nachbesserung M4h:** Die Zauberer-KI läuft nur noch zu Schätzen, die sie sieht (vorher ging sie zum ersten Schatz der Objektliste, auch unsichtbar); Magic Bolt trifft den nächsten sichtbaren Gegner statt den ersten in der Einheitenliste, mit genau einem Wurf pro Phase.
- **Nachbesserung M4f:** Designer: `-` senkt Attribute jetzt (volle XP-Rückgabe, nie unter den Startwert); Menüpunkt „Zauberer zurücksetzen“ ist korrekt benannt und fragt vor dem Löschen nach; `wizards.dat` hat Kopf (Magic, Version, Strukturgröße) und wird auf gültige Werte geprüft, sonst Stock-Zauberer; Szenario 0 im Kampagnenergebnis wird ignoriert (kein Shift um −1).
- **Nachbesserung M4e:** Karten mit Dach, aber ohne Portal, wurden vom Loader falsch geparst (v4 schreibt jetzt immer einen Portal-Block, `0xFF` = kein Portal); `world.c` bindet `ride.h` ein statt einer lokalen `extern`-Deklaration.
- **Review-Fixes M4a–c:**
  - Panel zeigte Kreaturen statt der Balken- und Status-Icons: Seit M4c liegen die Icon-Tile-IDs über 255, `BAR_ICON`/`STATUS_ICON` waren `uint8_t`.
  - Flugtrank: Wer ohne Flügel fliegt, bekam in der Luft 0 AP pro Runde und blieb nach Ablauf des Tranks oben. Jetzt gilt das Boden-Budget, nach Ablauf landet die Einheit (oder schwebt weiter, bis Platz ist).
  - Die Bombenphiole explodierte nie, `t` warf sie wie einen normalen Gegenstand. Phiolen fliegen jetzt bis zur ersten Einheit und zerplatzen dort.
  - Curse, Subversion, Magic Attack und Enchant brauchten weder Reichweite noch Sichtlinie (D17).
  - Kessel: Der Inhalt hing an einer Liste statt am Kessel-Objekt; nach dem Wegtragen blieb ein „Geisterkessel“. Jetzt ist das Objekt maßgeblich, volle Kessel lassen sich nicht tragen.
  - Getränke aus dem Kessel wirkten immer mit Stufe 2 statt mit der gebrauten Stufe (F1). Phiolen bleiben vorerst bei Stufe 2.
  - Drachenkraut braute nebenbei Heiltränke und wurde dabei verbraucht; es dient jetzt nur den Drachen.
  - Die KI sah unsichtbare Einheiten.
  - Rückschlag auch nach einem Fehlschlag und nach harmlosen Hieben gegen Untote (GDD §6); bisher schlug der Verteidiger nur nach einem Treffer zurück.
  - Verzauberte Waffen (Enchant) zählen doppelt (GDD §6.1).
  - Teleport streut symmetrisch und misst Chebyshev; Magic Attack und die Bombe rechnen mit Wrap-around und schreiben Kills dem richtigen Werfer bzw. Zaubernden zu.

### Hinzugefügt
- **Reiter sichtbar (M4k):** Wer ein Reittier reitet, wird als Reiter-Kachel hinter einer um 25 % verkleinerten Reittier-Kachel (18 statt 24 px, beim Laden aus der vollen Kachel berechnet) gezeichnet: Der Reiter bleibt deutlich sichtbar, das Tier steht darunter. Keine neue Grafik: beide vorhandenen Sprites werden zur Laufzeit kombiniert (`FieldLayers.ride`, Versatz je Reittier in `render.c`, auch im Panel-Porträt). Behoben dabei: Das Dach-Aufdecken (`apply_roof_rule`) verschob die Luft- und Reiter-Maske nicht mit, wenn es eine Ebene entfernte. Fliegende Paare tragen auf beiden Ebenen die Luft-Markierung.
- **M4j Politur (#52):**
  - **Kontextmenü mit Enter** (GDD §5.1.4): alle aktuell möglichen Aktionen mit Taste und AP-Kosten (unmögliche ausgeblendet), Buchstabe wirkt direkt, Esc schließt
  - **Big Map `m`** (GDD §11.1): 36×36 als 4×4-Blöcke im Kartenfenster, eigene Einheiten weiß, Feinde rot, Portal magenta
  - **Nachrichten-Log `l`**: Ringpuffer mit den letzten 10 Ereignissen (Kills, Zauber-Treffer, Rettung, Beschwörungen)
  - **Hilfe F1**: vollständige Tastenübersicht
  - **Sound** (GDD §11.4): Schritte, Treffer, Zauber, Aufheben, Portal, Tod über den Agon-Audiokanal (VDU 23,0,135)
  - **Umlaut-Font**: der Systemfont wird in einen VDP-Puffer kopiert und um ä/ö/ü/ß ergänzt (CP437-Codes), UI-Texte können echte Umlaute tragen
  - Bugfix: `run.py --no-menu` kollidierte nicht mehr mit `--bench`

- **M4i Speichern und Setup (#51):**
  - `src/core/save.[ch]`: binärer Spielstand (Welt, Zug-State, Portal/VP, Zauberbücher, Ladungen) mit Magic/Version/Größen-Gates; FNV-Hash für die Abnahme
  - **Autosave am Rundenende** in `save.dat` (GDD §2.3), Meldung „Gespeichert."
  - **5-Ladungen-Regel**: jeder Ladevorgang verbraucht eine Ladung, aufgebraucht = kein Laden mehr; **F8:** im Setup abschaltbar
  - **Setup-Panel** (Menü): Zufalls-Zauberer-Stärke 1–8 (+/−, erzeugt einen Slot-3-Zufalls-Zauberer, F9) und die Ladungen-Regel (L)
  - Hauptmenü: „Spielstand laden" stellt den exakten Zustand wieder her (Karte, Zug-State, Bücher)
  - Der Spielstand enthält auch Flächeneffekte und die erkundete Karte; Laden überspringt das Neuladen der Karte, die Ladungen-Regel prüft den Zähler der Datei, Schreiben geht über `save.new` und lässt die alte Datei bei Fehlern stehen

- **M4h KI-Ausbau (#50):**
  - **Wächter-Profil** (GDD §10): Untoten-Wachen der Szenario-Karten stehen auf ihrem Spawn-Posten, greifen Eindringlinge an und kehren zurück (`ai_set_post`/`ai_guard`, Postbereich 3 Felder); normale Unabhängige bleiben Jäger
  - **Zauberer-KI** nutzt jetzt Magic Bolt auf sichtbare Gegner und **sammelt Schätze** (nächster sichtbarer Schatz per Sichtstrahl, Aufheben auf dem Feld); Beschwörung/Portal-Flucht wie bisher
  - **Bench:** eine komplette KI-Zauberer-Phase kostet **160 ms** im Emulator (Budget 2 s, im `--bench`-Output sichtbar)
  - Abnahme: die KI entkommt gegen einen passiven Spieler zuverlässig durchs Portal (20-Runden-Dump)
  - **Fehler aus der Demo-Jagd gefixt:** `map_path` war im `--dump`/`--bench`-Modus uninitialisiert (Müll-Pfad beim Boot), und der SD-Lese-Puffer fasste v4-Karten (5304 B) nicht mehr — beide Karten laden jetzt wieder

- **M4g Szenarien 2 und 3 (#49):**
  - **Slayer's Dungeon** (eigene Karte, D2): Steinkorridore und Krypten, Untoten-Wache (Zombies, Geist, Vampir, Spectre), Schätze bis zum Slayer, Portal in der fernen Krypta (Runde 20–24)
  - **Ragaril's Domain** (eigene Karte): Zauberwald- und Schattenwald-Gürtel, Sumpfmoor, der Turmquartier im Nordosten; Ragaril (KI) befehligt Untote bis zum Dämon; Portal hinter dem Turm (Runde 44–51)
  - Szenario-Dateien für beide (Zauberbücher p1/p2 passend zur Stufe, GDD §9); das Hauptmenü listet alle drei Szenarien mit Stufenhinweis und lädt Karte + Bücher je nach Wahl
  - `run.py`: `--testland`/`--no-menu` ergänzt; das Spiel wertet jetzt **alle** Argumente aus statt nur argv[1] (Flag-Kombinationen funktionieren wieder)

- **M4f Zauberer und Kampagne (#48):**
  - `src/core/wizard.[ch]`: Designer-Datenmodell (Name, Level, XP, 5 Attribute mit linearen Kosten und Obergrenzen F6, Zauberbuch), 4 Slots
  - Hauptmenü (GDD §2.3): Szenario starten, Designer, Zauberer zurücksetzen, Beenden; Pfeile + Enter
  - Designer-Bildschirm: Attribute mit +/− erhöhen (XP-Konto im Kopf), Esc zurück
  - 4 Zauberer-Plätze in `wizards.dat` auf der SD (Laden beim Boot, Zurücksetzen auf den Stock-Zauberer)
  - Kampagne: **VP → XP 1:1**, erstes Abschließen eines Szenarios hebt die Stufe (Wiederholen bringt nur XP, GDD §9); **F5:** der Übertrag trägt nur Attribute, Zauberbuch und XP — der Zauberer startet unbewaffnet
  - Zufalls-Zauberer nach Stärke (Setup-Startwert, F9): XP-Budget und zufällige Buchstufen über den Partie-RNG

- **M4e Kampf komplett (#47):**
  - 7 neue Waffen als Objekte (Messer, Speer, Keule, Axt, Wurfstern, Slayer, Magie-Slayer) mit eigener Pixelart und den Werten aus `weapons.csv` (D7)
  - **Reiten** (`src/core/ride.[ch]`, Taste `b`): freundliche Reittiere (CF_MOUNT) nehmen Reiter (CF_RIDE: Zauberer, Pixie, Zwerg, Goblin, Troll) auf — das Paar ist eine Einheit (Flag `UF_RIDDEN`, Reiterart an Bord, Gepäck wandert mit), bewegt sich mit den AP des Reittiers, erscheint einmal in der Tab-Reihenfolge; Absteigen braucht ein freies Nachbarfeld (4 AP). **Reiter greifen von befreundetem Feld an** (D21-Ausnahme: `world_engaged` gilt nicht für sie)
  - **Dächer** im Kartenformat v4 (`roof`-Grid, R pro Feld): blockieren Sicht (auch Luft→Boden, GDD 3.2) und Landen; **F7:** das Dach ist von außen sichtbar und wird ausgeblendet, wenn eine eigene Einheit im Gebäude steht (dynamische View-Regel in beiden Kompositionspfaden)
  - Szenario-Karte: Dach über dem Starthaus, Einhorn daneben

- **M4d Flächeneffekte (#46):**
  - `src/core/area.[ch]`: Feld-Ebene mit 4 Flächenarten (Feuer, Gooey Blob, Tangle Vine, Flood), je bis 48 Felder mit eigener Stärke
  - F4-Ausbreitung am Rundenende (Chance Stärke × 10 % pro Feld, neue Felder Stärke − 1, alte − 1, Erlöschen bei 0), deterministisch über den Partie-RNG; Überschneidungen: gleiche Art frischt auf/ab (GDD-Feuerregel), verschiedene Arten überschreiben
  - Startwerte (D7, im GDD dokumentiert): Feuer 6 Schaden nur gegen Feinde (auf Gras/Holz/Bäumen), Blob 3 (außer Wasser, blockiert ab Stärke 2), Vine 2 (Gras/Wald, blockiert ab Stärke 2), Flood ertränkt Nicht-Wasserwesen mit 50 %/Runde
  - 8 animierte Overlay-Kacheln (Layer 7, GDD §11.3); Zauber wirken über den Zielmodus; Bench: 71 ms pro Tick mit 4 Flächen (Abnahme < 500 ms erfüllt)

- **M4c Tränke und Brauen (#45):**
  - `src/core/brew.[ch]`: Kessel (bis 4 pro Karte, leer/voll, Stufen) und Phiolen als Objekte; Brauen braucht leeren Kessel + Zutat auf dem Zaubererfeld, füllt `Stufe+3` Schlucke (GDD §7.2), verbraucht die Zutat und eine Zauberstufe
  - 7 Zutaten (Mistelzweig, Kleeblatt, Kristall, Schwefel, Feeenschwingel, Nitro, Drachenkraut; Apfel heilt mit), 12 neue Kacheln
  - Trinken `q` (Kessel oder Phiole, 4 AP), Füllen `v` (6 AP, Phiole trägt ihren Trank als eigenes Objekt); Wirkungen und F1-Dauern aus M4b; Bombenphiole explodiert beim Wurf im 3×3-Bereich
  - Drachen-Beschwörung braucht Kessel mit Drachenkraut unterm Zauberer; das Kraut wird bei Erfolg verbraucht `[PM 21]`
  - Kessel-Objekte aus Karten werden beim Laden registriert

- **M4b Wirkungen und sonstige Zauber (#44):**
  - `src/core/effect.[ch]`: zeitlich begrenzte Wirkungen pro Einheit (Art, Stärke, Rundenzahl, 4 Plätze), Tick am Rundenende; `UF_INVISIBLE`/`UF_MAGIC_WEAPON` folgen ihrer Wirkung
  - Wirkungen in den Werten: Schild/Schutz +Verteidigung, Stärke +Kampf, Schnell doppelt AP und dreifache Ausdauer-Regeneration, Flugg-Trank erlaubt Start ohne `ap_fly`; Status-Icons im Panel (Schild/Schwert/Stern/Blitz)
  - 7 Zauber (D22/F1–F3): Magic Shield (+2×Stufe Verteidigung für 2×Stufe Runden, wirkt auf den Zauberer), Magic Eye (Sicht von einem Punkt, eine Runde, durch Wände), Teleport (Reichweite 6, F3-Streuung, danach 0 AP), Curse (tödliche Wunde, Widerstand nach F2 +20), Subversion (Kreatur wechselt die Seite, nicht auf Zauberer/Reittiere), Magic Attack (trifft die ganze Kreaturart im Umkreis 2, auch eigene, Widerstand −10), Enchant (Waffen aller Einheiten auf dem Feld werden magisch, 2×Stufe Runden — Untote verwundbar)
  - Zauberliste führt alle Zauber in den Zielmodus; Selbstziel-Abbruch gilt nicht für Schild/Enchant (GDD-Ausnahme)
  - Nebenher: send_keys fokussiert das Emulatorfenster jetzt per Klick (Windows-Vordergrundsperre machte Tasten unzuverlässig)

- **M4a Classic-Lücken und Szenario-Format (#43):**
  - Untote nur durch Untote, magische Waffen (Magic Slayer, Enchant-Flag) und Zauber verletzbar `[PM 18]` — Bogen/Wurf/Nahkampf prüfen, Zauber umgehen die Regel
  - Unter 50 % Constitution: −2 Combat und − Defence (Startwert, GDD §4.1)
  - Nahrung: Apfel/Pilz heilen Constitution, magische Varianten Mana (`e`, 6 AP), Werte aus objects.csv; 6 neue Kacheln
  - Truhenspezialfall: Bump öffnet — mit Truhenschlüssel 8 AP (Schlüssel verschwindet), ohne 3× Aufbrechen; Loot-Tabelle wirft einen zufälligen Schatz aus; `r` liest Schriftrollen (8 AP, Hinweistext)
  - Szenario-Dateien `data/scenarios/*.txt` → kompilierte `.scn` (LOCS v1) mit Zauberbüchern aller Zauberer; `spellbook_default` entfällt; Zauberbücher kommen aus der Datei

## [Unreleased] – M3 Classic spielbar

### Geändert
- **Regeln aus dem M3-Review (GDD D21):**
  - Werfen, Bogen und Bolt nutzen dieselbe Trefferformel wie der Nahkampf (10–90 %, Defence inklusive Schild).
  - Schilde stapeln nicht mehr: Ein getragener Schild zählt, weitere nicht.
  - Wer stirbt (Kampf, Zauber, Verbluten), lässt alles Getragene auf sein Feld fallen, auch Schätze. Wer durchs Portal entkommt, nimmt es mit.
  - Bestätigt ohne Codeänderung: Ein Bodenverteidiger schlägt einen angreifenden Flieger zurück; der Blitz-Splash trifft auch eigene Einheiten und den Zaubernden.

### Behoben
- **Review-Fixes M3:**
  - Das Spiel hing in einer Endlosschleife, wenn der Mensch keine Einheiten mehr hatte (z. B. nach der Flucht durchs Portal). Die KI spielt jetzt zu Ende, bis kein Zauberer mehr da ist, höchstens 40 Runden (`TURN_AUTOPLAY_ROUNDS`). Danach kommt die Abrechnung und nur noch Esc wirkt.
  - Das Portal öffnet sich jetzt auch in Runden, die die KI allein spielt: Runden-Hook `turns.on_round` statt Aufruf in `main.c`.
  - Nach einem Tod war oft die falsche Einheit aktiv, und Fertig-Markierungen wurden vertauscht. Einheiten haben jetzt eine stabile `id`, das Fertig-Flag liegt an der Einheit, und `turn_revalidate` findet die aktive Einheit über die ID wieder. `turn_on_unit_removed` entfällt.
  - Kill-VP zählen jetzt den Wert des **Opfers** statt den des Killers. Tode laufen über `world_kill_unit` mit Protokoll, `game_credit_kills` rechnet ab. Treffer auf eigene Einheiten zählen nicht, mehrere Tote pro Blitz zählen einzeln, Fernkampf nennt den echten Schützen, und die KI bekommt ihre Kills ebenfalls gutgeschrieben.
  - Ein tödlicher Wurf konnte dem Spieler eine fremde Einheit als aktive geben.
  - Die KI las nach einem abgelehnten Nahkampf ein uninitialisiertes `CombatResult`; `combat_melee` nullt das Ergebnis jetzt immer.
  - Die KI behielt nach Toden veraltete Indizes, und Jäger konnten doppelt handeln. Jetzt wird über ID-Schnappschüsse iteriert (`ai_run_hunters`).
  - Die Fertig-Bits als `1u << i` waren auf dem eZ80 ab Einheit 24 undefiniert (24-Bit-`int`).
  - Die vier Kopien der Schadensanwendung sind zu `combat_damage` zusammengeführt.

### Hinzugefügt
- **M3g Szenario 1 „The Many Coloured Land“ (#33):**
  - Kartenformat v3 mit `portal x y rmin rmax`-Sektion (v2-Karten laden weiter); Welt trägt Portal-Position und Rundenspanne
  - 4 neue Schatz-Kacheln (Runenstein 6, Zauberstab 8, Rubin 20, Diamant 30 VP) neben Gold/Smaragd — die komplette Schatztabelle aus GDD §9.1
  - Szenario-Karte `data/maps/many_coloured_land.txt` (eigene Karte im Geist des Originals, D2): bunte Regionen, 6+ Schätze, KI-Zauberer, Portal Runde 12–15 bei (26,3)
  - `loc` startet jetzt Szenario 1; `loc --testland` öffnet die Entwicklungskarte
  - komplette Partie im Emulator durchspielbar und dokumentiert (p1 entkommt, p2-KI beschwört/flieht/zieht VP)

### Hinzugefügt
- **M3f Einfache KI (#32):**
  - `src/core/ai.[ch]`: Unabhängige jagen das nächste sichtbare Ziel (ein Strahl pro Kandidat — Hidden Movement wird respektiert), greifen an, wenn angrenzend; ohne Beute Umherstreifen
  - Zauberer-KI pro Phase: eigene Kreaturen jagen zuerst, Nahkampf gegen sichtbare Angrenzende, beschwört solange Begleitung < 3 und Mana reicht (günstigster Zauber), läuft zum Portal (auch vor dem Öffnen) und tritt hindurch, sobald es offen ist
  - AI-Registrierung über `turns.ai`-Callback (Standard: passen, für Tests); `turn_init` nullt jetzt den ganzen Zustand (fixt uninitialisierte Callback-Felder)
  - AP-Budgets begrenzen jede Phase natürlich; deterministisch über den Partie-RNG

- **M3e Portal und Siegpunkte (#31):**
  - `src/core/game.[ch]`: Portal erscheint deterministisch in der Rundenspanne (RNG), Betreten = Rettung für Zauberer mit getragenen Schätzen (VP), Kreaturen können nicht durch `[PM 29]`
  - Siegpunkte: Entkommen +10 (D19), Schätze aus objects.csv beim Übertritt, Kills (Kreaturwert, Zauberer doppelt im Nahkampf, AMI 4), Fernkampf einfach
  - Spielende, wenn kein Zauberer mehr lebt: Abrechnung als Meldungszeile; Runden-Hook öffnet das Portal mit Meldung
  - Portal als animierte 2-Frame-Kachel (View-Ebene über Terrain, blinkt mit)
  - Testland-Portal bei (26,3), Spanne Runde 12–15 (bis M3g in die Karte zieht)

- **M3d Basis-Objekte (#30):**
  - `data/objects.csv` + `data/weapons.csv` (Startwerte PM 35, D7), generierte `OBJECTS`/`WEAPONS`-Tabellen; 5 neue Objekt-Kacheln (Schwert, Bogen, Schild, Gold, Smaragd)
  - Inventar pro Einheit (6 Plätze) mit Trage-Limit aus der Kreaturtabelle, ein Objekt „in Benutzung“
  - Aufheben `g` (6 AP), Fallenlassen `d` (2 AP), Wechseln `w` (4 AP), Werfen `t` (10 AP, Flug bis 6 Felder, Wurfschaden, landet vor dem Ziel), Bogen `f` (12 AP, Reichweite 6, Sichtlinie, trifft Boden und Luft)
  - Waffenboni im Kampf: Schwert +4 Combat in der Hand, Schild +4 Defence immer beim Tragen (D18)
  - Testland: Schwert/Schild/Bogen/Gold/Smaragd beim Start-Haus

- **M3c Magic Bolt und Lightning (#29):**
  - `spell_bolt()`/`spell_lightning()`: Reichweite 6 (D17), Sichtlinie via neue öffentliche `sight_has_los()` (baut ihre Blockade-Bitmap selbst), Treffer nach Kampf-Modell (Defence zählt), Erd-/Lufteinheiten treffbar
  - Lightning: zusätzlich 8 Nachbarfelder, zerschlägt zerstörbares Terrain am Ziel, massives Ziel (Wand) wird abgelehnt
  - Zielmodus: Cursor gelb/blau/rot je Ziel, Pfeile/Akkorde bewegen, Enter/Leertaste wirkt, Esc oder Zielen auf den Zauberer bricht ohne Kosten ab; Panel zeigt das Zielfeld
  - `turn_revalidate()` als Sicherheitsnetz nach Flächen-Toden
  - eZ80-Selftest druckt nur noch Fehler (Emulator-Konsole verlor das Ende langer Ausgaben)

- **M3b Beschwörungen (#28):**
  - `Spellbook` pro Zauberer: Zauberstufen 1–8, jede Anwendung senkt die Stufe, bei 0 ist der Zauber verbraucht (GDD §7.1); Startbücher bis zur Szenario-Datei (M3g) im Core
  - `spell_summon()`: `Stufe` Kreaturen erscheinen auf freien Boden-Nachbarfeldern (Ring-Reihenfolge), kein Platz für alle → Mana verloren (GDD §7.2); 10 AP (`ACT_CAST`), Mana linear, nicht aus der Luft
  - Zauberliste als Overlay (`c`): Name, Stufe, Mana; Auswahl mit Buchstaben (Handler vor der WASD-Bewegung), `Esc` bricht ab
  - `SUMMON_KIND`-Tabelle generiert aus spells.csv/creatures.csv (0xFF = kein Beschwörungszauber)

- **M3a Kampf (#27):**
  - `src/core/combat.[ch]`: Nahkampf nach GDD D16 — Trefferchance 50+5×(Combat−Defence) (10–90 %), Schaden (Combat+Zufall)/4
  - Rückschlag automatisch, wenn der Verteidiger AP und Stamina hat `[PM 18]`; Aktionskosten aus `actions.csv`
  - Tödliche Wunde bei Einzeltreffer über 25 % Con `[PM 17]`, blutet −1 Con pro Runde bis zum Tod
  - Tod entfernt die Einheit (swap-mit-letzter); `turn_on_unit_removed()` repariert aktive Einheit und Fertig-Bits
  - Gebunden: Bodeneinheiten neben lebenden Gegnern können nicht mehr weg `[PM 17]`
  - Terrain-Angriff: Zähigkeit je Feature aus `data/features.csv` (Wände unzerstörbar), Feature fällt bei Schaden+Zufall(0..3) > Zähigkeit
  - Bump auf Gegner ist jetzt der Angriff (Meldungen für Treffer/Wunde/Tod/Rückschlag), Bump auf Terrain der Hieb

## [Unreleased] – M2 Core-Skelett

### Hinzugefügt
- **M2f Bump und Look-Modus (#18):**
  - `world_bump_kind()` klassifiziert gescheiterte Schritte (Tür, Einheit, Terrain, AP) für Bump-Interaktionen (GDD §5.1)
  - Bump auf geschlossene Tür öffnet sie (`world_open_door`: nur Kreaturen mit Händen `CF_USE`, 6 AP aus `actions.csv`, View-Cache und Sicht werden neu berechnet)
  - Bump auf Gegner/Terrain meldet „Kampf folgt in M3“
  - Look-Modus `x`: freier weißer Cursor per Pfeilen/Akkorden, Panel zeigt das untersuchte Feld bzw. die (sichtbare) Einheit, `describe_field()` nennt Einheit (mit „(Luft)“) oder Boden; Unerforscht bleibt dunkel; `Esc`/`x` beendet
  - Erste Haustür in Testland ist jetzt geschlossen (Bump-Demo)

- **M2e Luft- und Bodenebene (#17):**
  - `world_unit_at` mit Ebenen-Parameter (Boden/Luft, GDD D15): eine Boden- und eine Lufteinheit pro Feld
  - Fliegen: konstante 4/6 AP aus `costs.csv` (`AIR_AP_*`), ignoriert Terrain und Boden-Einheiten; Aufsteigen `<` / Landen `>` mit Aktionskosten, kein Landen auf Wasser
  - Rundenende füllt das Ebenen-Budget auf (am Boden `ap_max`, in der Luft `ap_fly`)
  - Sicht aus der Luft: Reichweite 11, keine Terrain-Blockade (GDD §3.4)
  - View: Flieger 3 px höher mit Bodenschatten über der Boden-Einheit (GDD §11.3), blaue Cursor-Farbe in der Luft, Panel-Icon „fliegt“ und AP-Balken am Luft-Budget
  - Testland hat eine p1-Riesenfledermaus; `loc --fly` startet Flieger in der Luft (Demo: ISO-Taste `<` ist im Emulator nicht sendbar, #3)

- **M2d Sichtlinie und Hidden Map (#16):**
  - `src/core/sight.[ch]`: Bresenham-Sicht pro Zielfeld (Chebyshev 9 Boden / 11 Luft, Endpunkte exklusiv, GDD D14), pro Spieler Bitfelder „erkundet“ (akkumuliert) und „sichtbar“
  - Unerforscht = schwarze Kachel (neue Pixelart), erinnert = Raster-Overlay; Gegner nur bei aktueller Sicht (Hidden Movement `[AMI 4]`), unsichtbare Gegner nie
  - Sicht wird nur nach eigenen Schritten und am Rundenende neu berechnet
  - `-O2` statt agondevs `-Oz` (Compose 28→18 ms, Voll-Redraw 52→44 ms), Sicht-Berechnung 792→162 ms durch Blocking-Bitmap und int8-Rays (ADR 0009)
  - View-Hash bewusst aktualisiert (neue Kachel verschiebt IDs)

- **M2c Rundenablauf (#15):**
  - `src/core/turn.[ch]`: Rundenzähler, Phase (unabhängige Kreaturen, dann Zauberer 1..n), aktive Einheit, seedbares RNG
  - `Tab`/`Shift+Tab` wählt die nächste bzw. vorige eigene Einheit mit AP, `Leertaste` beendet eine Einheit, `Shift+E` beendet den Zug (zweimal drücken als Bestätigung, `Esc` bricht ab)
  - Runde 1 erlaubt kein Bewegen, nur Zaubern `[PM 7]`; für Skript-Läufe per `loc --free-round1` abschaltbar
  - unabhängige Kreaturen streifen deterministisch umher (Platzhalter bis zur KI in M3), der KI-Zauberer passt
  - Rundenende: AP auffüllen, 25 % Stamina, 4 % Mana; erschöpfte Kreaturen (Stamina unter 25 %) erhalten nur halbe AP `[PM 12]`
  - Meldungszeile zeigt „Runde n – Zauberer-1: <Einheit>“, Panel und Kamera folgen der aktiven Einheit
  - VKey-Messung Tab/Leertaste/Shift (ADR 0007 Nachtrag)

- **M2a Terrains und Testland (#13):**
  - 7 neue Böden (hohes Gras, Wald, Zauberwald, Schattenwald, Sumpf, Wasser animiert, Geröll) und Fels als eigene Pixelart
  - Kosten, Sicht-Blockade, Terrain-Affinität und Ertrinken aus `data/costs.csv`
  - Testkarte 36×36 mit Wrap-around (zwei Häuser, Fluss mit Brücke, Wälder, Sumpf, Geröllfeld)
  - Kamera scrollt über die Kartenränder
  - `gen/maps.h` deklariert alle Karten

- **M2b Kreaturen (#14):**
  - `data/creatures.csv` aus der Kreaturtabelle `[PM 34]` (D12), mit Terrain-Affinität
  - alle 25 Kreaturen als eigene 24×24-Pixelart in 3/4-Ansicht (D13), jeweils in 5 Besitzerfarben (`tools/art/creatures.py`, Übersicht `docs/design/mockups/creatures.png`)
  - 16-Bit-Kachel-IDs, Kartenformat v2
  - Testland mit mehr Kreaturen

### Behoben
- Leistung: keine Divisionen mehr pro Feld auf Wrap-Karten (Berechnung 32 → 22 ms).
- Eingabe: Die Ereignis-Warteschlange wird vor der Tastenwiederholung geleert (falsche Richtungen nach Akkorden bei langsamen Frames).
- ez80-clang-Absturz bei `||`-Ketten über Enums, umgangen per Lookup-Tabelle (AGON-QUIRKS T7).

## M1 Grafik und Eingabe

### Geändert
- **Darstellungskonzept v2** (GDD D9–D11, ADR 0005): eigene 24×24-Pixelart in 3/4-Frontansicht, 9×9-Sicht, mehrere Ebenen pro Feld. Die 8×8-Glyphen-Demo ist entfernt.

### Hinzugefügt
- **Pixelart-Pipeline:**
  - Agon-64-Palette (`assets/palette/agon64.gpl`)
  - erste 44 Kacheln und 6 Icons
  - `tools/build_tiles.py` (→ `tiles.bin` RGBA2222, Besitzerfarben, Halb-Böden)
  - `tools/mockup.py`
- **Karten als Text:** `data/maps/wizard_house.txt` und `tools/gen_maps.py`.
- **Core:**
  - `world` mit Boden-, Dekor- und Feature-Ebene, Möbel-Blockade und AP-Kosten (Weg 3, sonst 4, diagonal ×1,5)
  - `view` mit Ebenen-Komposition, Wand-Auto-Tiling, Halb-Böden, Dirty-Feldern und Static-Cache
- **Agon-Renderer:**
  - Kacheln von SD in VDP-Buffer, Felder Ebene für Ebene
  - Panel mit 6 Balken und Icons, Meldungszeilen
  - Kerzen-Animation
  - `--bench`: Voll-Redraw 48 ms (ADR 0006)
- **Tests:** Welt-, View- und AP-Checks; schneller Pfad gleich Referenz; View-Hash plattformgleich.
- **Datenladen (#5, ADR 0008):**
  - Karten als validiertes Binärformat von SD (`/loc/maps/*.map`)
  - Regeltabellen aus CSV einkompiliert (`gen_data.py`): Zauber mit Mana-Formel, Bodenkosten, Aktionen
  - `gen_maps.py` liest Enum-Werte aus den C-Headern
  - Selftest prüft den Lader auch mit kaputten Daten
  - Speicher: etwa 63 KB von 448 KB
- **Info-Panel (#6):**
  - Einheiten mit Werten: Stamina, Constitution, Combat, Defence, Mana, Status-Flags (vorläufig bis M2)
  - Gehen kostet Stamina (AP/2), eine neue Runde stellt 25 % wieder her
  - 5 Status-Icons, Namen-Tabelle (`names.c`), „Am Boden“ zeigt Objekt, Möbel, Teppich bzw. Boden
  - Mockup-Panel an das Agon-Layout angeglichen
- **Sprite-Cursor und Animation (#4):**
  - Cursor als VDP-Sprite (4 Farben, blinkt)
  - `view_animate()` tauscht nur animierte Felder: Kerzen 16 → 6 ms pro Frame
- **Eingabe (#3, ADR 0007):**
  - `loc --keytest` (Tastatur-Spike)
  - Bewegung per Virtual Key
  - Pfeil-Akkorde für Diagonalen (80 ms), Pos1/Bild↑/Ende/Bild↓ als Diagonalen
  - eigene Tastenwiederholung (350/200 ms)
  - `send_keys.py` kann Akkorde und gehaltene Tasten senden; `run.py` setzt das deutsche Layout

## M0 Fundament (`v0.1.0`)

### Hinzugefügt
- **Projektgerüst:** `src/core` (plattformfrei), `src/agon` (VDP-Frontend), `host/` (PC-Frontend).
- **Tooling:**
  - `tools/setup.py` mit gepinntem fab-agon-emulator 1.2.5 und agondev v0.22 (SHA-256-geprüft)
  - `build.py`, `test.py`, `run.py`, `send_keys.py`, `screenshot.py`
- **Tests:** Core-Selftest, der identisch auf dem PC und im Headless-Emulator (eZ80) läuft; Exit-Code über I/O-Port 0.
- **Demo-Szene „Hello Glyph“:**
  - MODE 8 mit eigenen 8×8-Glyphen und Dirty-Cell-Rendering
  - Bewegung per Pfeiltasten/WASD
  - Bildschirm-Dump nach `loc.log`
- **CI:** GitHub Actions (Build, Host-Test, Emulator-Test, Artefakt `loc.bin`).
- **Docs:** GDD v0.4, Amiga-Beobachtungen, Roadmap, Architektur, ADRs 0001–0004.
- **Daten:** `data/spells.csv` (Mana-Formel), `data/costs.csv`, `data/actions.csv`.
