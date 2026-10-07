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
