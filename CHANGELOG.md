# Changelog

Format nach [Keep a Changelog](https://keepachangelog.com/de/1.1.0/), Versionen nach Milestones (siehe `docs/ROADMAP.md`).

## [Unreleased] – M3 Classic spielbar

### Hinzugefügt
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
