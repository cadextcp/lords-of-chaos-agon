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
