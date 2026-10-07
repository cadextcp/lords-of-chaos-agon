# Offene Fragen (autonome Arbeit zu D67)

> Wird beim Umsetzen von `docs/PLAN-KI.md` gepflegt. Statt abzubrechen, wird hier notiert, welche Annahme getroffen wurde. Der Nutzer antwortet gesammelt.

## Entschieden (2026-10-07, Nutzer)

- `MAX_UNITS` bleibt 32.
- Level 1 behaelt die zwei festen Startplaetze (kein Umbau auf vier Haeuser).

## Offen

| Nr | Phase | Frage | Getroffene Annahme |
|---|---|---|---|
| F1 | 0a | Die Bomb Potion (nur Amiga) hat keine Mana-Basis im Original. | Bleibt `known=0`, Kosten 0 wie bisher. |
| F2 | 0b | „Flug gesperrt“ (Gelände-Bit 3) kennt das Original nur in Szenario 2 (Dungeon, Wände). Unsere Karten haben keinen Dungeon. | Noch keine Sperre; kommt mit 0h (Flieger über Wänden) und dem Dungeon-Szenario, falls gewünscht. |
| F3 | 0b | Wunden eines **Reiters** (`rider_*`) werden nicht gespeichert; sie gehen beim Absitzen verloren. | Wie bisher, wird mit 0g (Reiter mit eigenen Werten, K6.6) nachgezogen. |
| F4 | 0b | „Kein Abheben“ (Bit 4) gilt im Original für Innenräume; bei uns ist das „unter Dach“. | Dach (`world_has_roof`) sperrt Abheben und Landen. |
| F5 | 0c | Zähigkeiten der Möbel und Türen (Skala des Originals 40–200) sind nur in groben Zügen übernommen: Türen 60/80, Möbel 60–65, Baum 80, Fels 200, Zaun 40. | Bleibt, bis das Playtest-Feedback anderes sagt. Folge: Kreaturen unter Combat 40 brechen keine Tür auf. |
| F6 | 0c | Nahkampf zwischen Boden und Luft: Das Original trifft nur auf gleicher Höhe (Eigenheit, K10.3). Bei uns darf ein Flieger Bodenziele weiter angreifen, bis 2d (Abheben/Landen der KI) steht. | Wie bisher; wird in 2d umgestellt. |
| F7 | 0c | Kritische Treffer (D30) und `reacted` (D29) sind weg; die Felder `crit`/`return_crit` in `CombatResult` bleiben für die Anzeige, sind aber immer `false`. | Aufräumen später (S). |
| F8 | 0d | Zaubern aus der Luft (CAST-A, Teleport nur unter Flying-Trank) kennt das Original; bei uns kann ein fliegender Zauberer nicht zaubern. | Bleibt gesperrt; die Teleport-Bedingung ist damit gegenstandslos. Soll das Zaubern aus der Luft kommen, ist das eine eigene Aufgabe (Ziele je Höhe). |
| F9 | 0d | Der Super-Trank (Ambergris) hat keine Zutat und keine Phiole. Wir haben statt dessen die Bombe der Amiga-Fassung (Nitro). | Beides bleibt; Ambergris bräuchte eine neue Objektkachel. |
| F10 | 0d | Magic Attack trifft im Original auch Reiter einzeln; bei uns steckt der Reiter im Reittier. | Das Reittier stirbt, der Reiter wird abgeworfen (D60). |
| F11 | 0e | Verbranntes Gelände wird bei uns zu `FL_PATH` (Erde), im Original zu Tile 0 (Boden, nicht brennbar). | Reicht optisch als Brandspur; ein eigenes „verbrannt“-Kachelpaar wäre Kunst. |
| F12 | 0e | Die Entzündbarkeits- und Empfänglichkeitswerte unserer Geländefamilien (`terrain_effects.csv`) sind aus den Kachelfamilien des Originals abgeleitet, nicht 1:1. Möbel und Türen brennen mit 8–12. | Bleibt bis zum Playtest. |
| F13 | 0e | Blob und Feuer auf stark brennbarem Gelände mit hoher Stufe sterben nie aus (so rechnet das Original auch), begrenzt nur durch 48 Felder je Fläche. | Wie im Original. |
| F14 | 0e | Flieger über Feuer bleiben unverletzt (nicht im Original gelesen, aber „Feuer am Boden“). | Eigene Zusatzregel. |
| F15 | 0f | Der Setup-Bildschirm kennt eine „Zufallsstärke“ für Zufallszauberer; im Original gibt es nur einen. | Der Wert bleibt in der Oberfläche, wirkt aber nicht mehr. Soll er verschwinden, ist das eine kleine UI-Änderung. |
| F16 | 0f | Alte Wizard-Dateien auf der SD-Karte mit Ausdauer/Constitution über 90 oder AP über 40 werden als ungültig abgelehnt (neue Höchstwerte). | Slot wird zurückgesetzt; kein Migrationspfad. |
| F17 | 0g | Schatzwerte: Unsere Objekte (Gold 40, Rubin 20 …) weichen von den Originalwerten ab (16/12/8). „Schätze roh“ habe ich so verstanden, dass die Tabellenwerte ungeändert zählen; unsere Tabelle bleibt. | Unsere Werte bleiben. Soll die Schatztabelle auf 16/12/8 schrumpfen, ist das eine Datenänderung in `objects.csv`. |
| F18 | 0g | Die Anzeige zeigt für Reiter nur die AP des Reittiers; die Reiter-AP sind im Spiel nicht sichtbar. | Panel-Anzeige (zweite AP-Zeile für den Reiter) kommt mit dem Playtest, wenn gewünscht. |
| F19 | 0g | Der Wizard-Level für `4·Level+15` kommt für Spieler 1 aus dem Slot, für KI-Zauberer gilt Level 1 (19). Das Original setzt das Byte je Zauberer im Szenario. | Mit der Szenariodatei v2 (3a) pro Zauberer festlegen. |
| F20 | 0h | Gegenstände unter Blätterdach sieht der Flieger nie (K11.2); die Anzeige der Felder richtet sich aber nach dem Gelände, nicht nach `vis_obj` (nur Einheiten, Namen und Aufhebe-Liste nutzen es). | Reicht, bis jemand es im Spiel vermisst. |
| F21 | 0h | Das Original hat „Blätterdach“ und „überdacht“ als Kachelflags. Bei uns: Blätterdach = Gelände mit `blocks_sight_ground` (Wald, hohes Gras, Zauberwald, Totenwald) oder Baum; überdacht = Dach (`world_has_roof`). | Wie beschrieben. |
| F22 | 0h | Würfe (`t`) fliegen weiter geradeaus nach dem Weg-Prinzip, nicht über die Schusslinie K11.6; nur der Bogen und das Drachenfeuer nutzen `sight_shot_clear`. | Wirft man aus der Luft durch eine Wand, passiert das auch im Original nur im Dungeon-Szenario. |
| F23 | KI 1 | Das Original lässt einen Flieger nie am Boden kämpfen, weil das Nahkampfziel „gleiche Höhe“ verlangt, aber Landen ein Nahkampfziel braucht (Eigenheit, K10.3). Ich habe das nicht nachgebaut: Ein Flieger sieht Bodenziele als Nahkampfziele, landet daneben und kämpft im selben Zug. | Bleibt so (sonst wären Fledermäuse und Harpyien gegen Bodentruppen wirkungslos). |
| F24 | KI 1 | Die Phasen 1 und 2 des Plans sind in einem PR zusammengefasst (Schleife und Taktik greifen ineinander); Phase 3 und 4 folgen als eigene PRs. | Reine Arbeitsplanung. |
| F25 | KI 1 | Die alten D62-Verhalten der Kreaturen (die ersten zwei bewachen das Haus, die anderen marschieren zum Rivalenhaus) entfallen: Ohne Routen (4b) sind alle Leibwache beim Zauberer. | Mit 4b kommen Routen; bis dahin ist der KI-Zauberer defensiv. |
| F26 | KI 1 | Die KI nutzt die Beschwörungsobergrenze 5 (D62) und seinen alten Zauberer-Ablauf noch bis Phase 3. | wie geplant |
