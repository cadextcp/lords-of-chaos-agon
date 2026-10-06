# Plan: Schlauere KI nach dem Spectrum-Vorbild

> Stand 2026-10-06 · Wunsch des Nutzers: „Mach einen Plan für die schlauere KI.“
> Grundlage: `docs/REGELVERGLEICH-SPECTRUM.md` (Vorschläge R1–R35) und das KI-Kapitel K10 in `lords-of-chaos-zx-agon/docs/REGELN.md`.
> Entscheidungen des Nutzers vom 2026-10-06: **Kampfmodell wie im Original** (Weg B des Berichts, GDD D67). **Mana wie im Original** (Quellcode bestätigt: Giant Bat Stufe 1 kostet 4, die Zauberliste zeigt `Basis × (L+1)`, R1).
> Dieses Dokument ist ein **Plan**, es ändert noch keinen Code.

---

## 1. Ziel

„Schlauer“ heißt hier, was das Original an der KI kann und unsere heute nicht:

- Kreaturen **wägen Stärken ab**: Sie greifen nur an, was sie verletzen können, und fliehen vor Stärkeren (R28).
- Sie **schießen** mit Bogen und Drachenfeuer (R29) und **nutzen Gegenstände**: essen, Heiltrank, Waffe wechseln, Beute nach Wert aufheben (R30).
- Der KI-Zauberer hat **eigene Werte und große Bücher** (R33) und **wirkt mit Prioritäten**: Schild, Tränke über den Kessel, Drachen, Flächenzauber, Bolt und Blitz mit Bedingungen (R32).
- Kreaturen **schlafen, laufen Routen, werden aggressiv, ziehen bei Portalöffnung zum Portal** (R34, R35).

Bleiben soll, was bei uns besser ist: Wegsuche per Breitensuche (D62), Türen und Truhen, Plündern des Hauses, Wildtiere (D35, D37) und **kein Schummeln** (D20: die KI kennt nur, was sie sieht, plus das Haus des Gegners, wie ein Spieler die Karte kennt).

Nicht Ziel: Spectrum-Löcher nachbauen (Zauber-KI fest auf Spieler 2, leere Handler für Enchant, Subversion, Curse, Magic Attack, Teleport, Eye).

---

## 2. Wo wir stehen

| Bereich | Heute (`ai.c`, 1142 Zeilen) | Original (K10) |
|---|---|---|
| Kreatur im Kampf | nächstes sichtbares Ziel, angreifen oder zulaufen, drei Aktionen | Stärkefilter `2·C ≥ 1,5·Def`, Flucht vor Bedrohungen, bis 50 Schleifendurchgänge |
| Fernwaffen | keine | Bogen, magischer Bogen, Drachenfeuer mit Reichweite und Filter |
| Gegenstände | Waffe/Schild aufnehmen, Schätze sammeln | Wunschtabelle, Essen, Tränke, Waffenwechsel, Werfen |
| Zauberer | Bolt auf das nächste Ziel, bis 5 Kreaturen beschwören, Haus plündern, Portal | Prioritätstabelle je Szenario, Schild, Tränke, Drachen, Fire/Blob/Vine/Flood, Bolt/Lightning mit Bedingungen |
| Zauberer-Werte | Zeile `wizard` (Con 30, Mana 80) | Torquemada Con 63 / Mana 120, Elbo Smogg 63 / 140, Ragaril 50 / 200 |
| Bewegung | Jagd, Wachposten, Wildtier-Regeln, Breitensuche | Routen, Schlaf, Auslöserfelder, Aggressivität, Portal-Sammeln |
| Messung | `host/arena.c` (alle gegen alle, Balancing) | – |

Laufzeit auf der Hardware (Bench 46×46): KI-Phase 80 ms bei frischem Start.

---

## 3. Voraussetzung: die Regeln des Originals (Phase 0)

Die KI-Schwellen des Originals rechnen mit seinen Werten: `C_eff` und `Def_eff` (K6.1), den Angriffswert `A = 4L+25` und den Waffen-Boni in Combat und Defence. Mit unserem Prozent-und-Würfel-Modell (D16, D28–D30, D40, D42) gäbe `2·C ≥ 1,5·Def` keinen Sinn. **Darum kommen die Regeln zuerst.** Das ist ein eigener, großer Block; hier steht nur sein Umfang, damit die KI-Phasen ihre Abhängigkeiten sehen.

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 0a | **Mana wie im Original:** neue Spalte `mana` je Zauber (`Basis`, K5.2), `spell_mana = Basis × (L+1)`; die bisherigen `mana_base`/`mana_step` heißen `xp_base`/`xp_step` und gehören dem Designer. KI-Wahl „günstigster Zauber“ und Mana-Tests ziehen mit | R1 | S |
| 0b | **Zustände:** Wunden als Zähler 0–7 mit 2 Con je Zug, Curse = 7 Wunden; Constitution-Faktor `⌊ConMax/ConAkt⌋` auf AP, Combat, Defence; Flug kostet Ausdauer auch im Stand. Spielstand v9 | R9, R10, R12 | M |
| 0c | **Nahkampf:** `Schaden = RND(min(255, 2·(C_eff+1))) − Def_eff`; Waffentabelle K7 mit Combat-, Defence- und Wurfwerten, magische doppelt; Wegfall der Würfel (D28), der kritischen Treffer (D30) und der Trefferchance (D16, D42); Gelände angreifen nach K6.4 | R5 | M–L |
| 0d | **Zauber, Tränke, Schild:** Bolt `4L+25`, Blitz `4L+30` gegen **Defence**; Strength +20, Protection +25, Dauer `⌊(3(L−1)+10)/Verbrauch⌋`; Magic Shield `4(L+1)+12` für `L+1` Züge; Reichweite `2L+7` (Eye `3L+10`, Teleport `2L+30`) | R6, R13, R15, R16, R18 | M |
| 0e | **Flächen und Drachenfeuer:** Feuer `25+2F`, Blob `16+2(B−1)`, Vine 8; Ausbreitung und Überleben nach K5.3; Geländetabellen mit Entzündbarkeit; **Drachenfeuer als Aktion** (K6.4: Reichweite 12, Angriff 35) | R17, R26 | L |

Offen innerhalb von Phase 0 (siehe §8): Rückschlag und „Gebunden“ (R7, R8), Beschwören (R14), Designer-Preise (R2).

Wirkung auf bestehende Entscheidungen: D16, D27–D30, D32, D40, D42 werden „ersetzt durch D67“. Das GDD (§6, §7) und fast alle Kampf-Selftests müssen neu geschrieben werden; `host/arena.c` ist danach neu zu eichen.

---

## 4. KI-Phasen

### Phase 1 – Fundament (1 PR, M)

Nichts davon ändert das Verhalten sichtbar; es schafft Platz für die nächsten Phasen.

- **Datei aufteilen:** `ai.c` wächst sonst auf über 2000 Zeilen. Vorschlag: `ai.c` (Schleife, gemeinsames), `ai_creature.c` (Taktik), `ai_wizard.c` (Zauberer), `ai_items.c` (Gegenstände). Alles bleibt plattformfrei unter `src/core` (CLAUDE.md-Regel).
- **Sichtliste der Gegner** wie `D_D0E4`: einmal je KI-Einheit aus der vorhandenen `Sight` des Besitzers gebaut, je Eintrag Index, `C_eff`, `Def_eff`, Entfernung. Die Sichtlinie bleibt unser Shadowcasting (D39).
- **Entscheidungsschleife je Kreatur** nach K10.3: höchstens 50 Durchgänge, feste Reihenfolge der Sofortaktionen (Kessel trinken, Zaubern, Aufheben, Phiole, Essen, Werfen, Waffe wechseln), danach Fernangriff, Flucht, Nahkampfziel, Gegenstand, Route/Jagd, Schritt.
- **Plan je Einheit:** wenige Bits in `Unit` (schläft, aggressiv, Route, Schritt, Auslöser-ID), analog `post_x`, `grudge`. Das gehört in denselben Spielstand v9 wie die Wunden.
- **Daten:** `creatures.csv` bekommt die Spalte `aggr` (K2, Byte 8); `data/ai_items.csv` die Wunschwerte je Objekt (K10.6, auf unsere Objekte abgebildet); die Szenariodatei bekommt Zauberer-Werte und Prioritäten (Phase 3).

### Phase 2 – Taktik der Kreaturen (3 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 2a | **Ziel und Flucht:** Nahkampfziel nur bei `2·C_eff ≥ 1,5·Def_eff`, nächstes gewinnt; Bedrohung bei `2·Def_eff < 1,5·C_eff(Gegner)`; Fluchtfeld mit der größten Abstandssumme; Ausnahmen für Flieger, Untote, Aggressive (K10.4) | R28 | M |
| 2b | **Fernangriff:** Bogen und Drachenfeuer mit Reichweite, Filter und 8 AP; Untote nur mit magischem Bogen oder Drache auf brennbarem Gelände | R29 | M |
| 2c | **Gegenstände:** Aufheben nach `⌊Wert/(D+1)⌋` bis zur Tragkraft; Essen bei `1,5·Con < ConMax` oder Ausdauer ≤ Max/4; Heiltrank; Waffenwechsel; Werfen auf den Zauberer von Spieler 1, soweit sichtbar | R30, R31 | M |

### Phase 3 – Der KI-Zauberer (3 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 3a | **Werte und Bücher aus der Szenariodatei:** Format `LOCS` v2 mit Name, Mana, AP, Ausdauer, Constitution, Combat, Defence, MR, Tragkraft, Siegpunkten und je Zauber Stufe und Priorität (K10.2). `gen_scenarios.py`, `mapfile.c` und der Szenario-Start wenden sie auf den Gegner an. **Sofortiger Effekt auf den Schwierigkeitsgrad** | R33 | S–M |
| 3b | **Zauberwahl:** Prioritätsliste, Halbieren nach dem Wurf, Bedingungen je Zauber (Bolt/Lightning mit Stärkefilter und Eigenschutz, Flächenzauber mit Radius `2L+6`, Shield nur ohne aktives, Beschwören mit Mana-Rest und freien Nachbarfeldern), „ohne sichtbare Gegner nur bei halben AP“ | R32 | L |
| 3c | **Tränke und Drachen:** leerer Kessel und Zutat auf dem Feld, vorher ablegen, brauen, aus dem Kessel trinken, Phiole füllen (`brew.c` wird vom KI-Code genutzt) | R32 | M |

Das Verhalten „bleibt im Haus, plündert zuerst, geht erst bei ≤ 1 Kreatur oder Wutanfall hinaus“ (D62) bleibt als Grundlage. Die Prioritäten ersetzen nur **was** er wirkt, nicht **wo** er ist.

### Phase 4 – Bewegung und Szenario (2 PRs)

| PR | Inhalt | R | Aufwand |
|---|---|---|---|
| 4a | **Aggressivität und Portal:** `RND(100) < aggr` beim Erscheinen; Aggressive rücken auf das **bekannte** gegnerische Haus und die zuletzt gesehene Position vor (kein Allwissen) und fliehen nie; ab der Portalrunde ziehen alle KI-Kreaturen zum Portal | R21, R35 | S–M |
| 4b | **Schlaf, Auslöser, Routen:** Kartenformat v5 mit Wegpunktlisten und Auslöserfeldern (`gen_maps.py`, Textkarten bekommen `route`- und `trigger`-Zeilen); Schläfer wachen bei Sicht oder Auslöser auf; nicht Aggressive laufen ihre Route | R34 | M–L |

4b hängt am Format der Routentabellen im Original, das noch nicht gelesen ist (§9). Wenn es zu teuer wird, genügen eigene Routen auf den Szenariokarten.

### Phase 5 – Messen und Eichen (1–2 PRs, M)

- **KI-Duell im Host-Programm:** `host/arena.c` kann alle gegen alle. Neu: Szenario laden, KI-Zauberer (Spieler 2) gegen einen einfachen Test-Gegner (Jäger-Bot) über N Partien mit festen Startwerten, Ausgabe: Siege, Runden bis zum Ende, Verluste, geworfene Zauber je Art. Die Läufe sind deterministisch und stehen im CHANGELOG-Eintrag der Phase.
- **Selftests je Regel** (Host und eZ80), z. B. „Goblin flieht vor dem Riesen“, „Bogenschütze schießt nur im Bereich“, „Shield nur ohne aktives Shield“, „Prioritätswahl halbiert nach dem Wurf“.
- **Playtest** mit dem Nutzer nach 2c, 3b und 4a (kurze, gezielte Fragen, wie in `docs/PLAYTEST-*.md`).
- **Optional:** Schwierigkeit im Setup (leicht/normal/schwer als Faktor auf KI-Mana und -Constitution). Nur auf Wunsch.

---

## 5. Technik und Risiken

| Thema | Plan |
|---|---|
| **Laufzeit** | Bench vor Phase 1 und nach jeder Phase (`loc --bench`, Hardware). Ziel: KI-Phase mit 8 Kreaturen unter 300 ms. Die Sichtliste wird je Besitzer gebaut, nicht je Feld. Keine Divisionen in den Schleifen (QUIRK T3) |
| **RAM** | Breitensuche belegt 1,3 KB Stack und bleibt. Neue Bits in `Unit` prüfen (`MAX_UNITS` 32); `build.py` meldet die Reserve (heute 97 KB) |
| **Spielstand** | v9 gemeinsam für Wunden (0b) und Plan-Bits (Phase 1). Alte Stände werden abgelehnt wie bisher |
| **Szenariodatei** | `LOCS` v2 (3a) ist abwärts nicht lesbar; die Szenarien auf der SD müssen neu gebaut werden (`build.py` tut das) |
| **Schwierigkeit** | Mit Originalwerten (Con 63, Mana 120, Prioritäten) wird der Gegner deutlich härter als heute. Erst Phase 0c/0d, dann 3a/3b, damit die Eichung auf einem fertigen Kampfmodell steht |
| **Balance** | Das Kampfmodell des Originals ist tödlicher (Spinne gegen Zauberer im Schnitt 30 statt 10 Schaden je Schlag, Bericht §5.1). Die Arena (Phase 5) muss vor dem Playtest eichen |
| **Unsicheres im Original** | Das Landen/Abfliegen der KI, die Schrittwahl, die Routentabellen und die Auslöser sind in K10 nur grob beschrieben (§9). Wo unklar, gilt unsere eigene Lösung und wird im GDD vermerkt |

---

## 6. Reihenfolge und Umfang

```
0a Mana ─┬─ 0b Zustände ─ 0c Nahkampf ─ 0d Zauber/Tränke ─ 0e Flächen + Drachenfeuer
         │
         └─ 1 Fundament ─┬─ 2a Ziel/Flucht ─ 2b Fernangriff ─ 2c Gegenstände
                         ├─ 3a Werte/Bücher ─ 3b Zauberwahl ─ 3c Tränke/Drachen
                         └─ 4a Aggressiv/Portal ─ 4b Schlaf/Routen
                                                          5 Messen und Eichen (laufend)
```

- **Sofort möglich und unabhängig:** 0a (Mana). Es bringt ohne Kampfumbau schon den Original-Spielfluss (billige Zauber).
- **Phase 1** kann parallel zu 0b–0e laufen, sobald 0a und 0b (Spielstand v9) stehen.
- **2a, 2b, 3b** rechnen mit `C_eff`, `Def_eff` und `A` und warten auf 0c und 0d. **2b** wartet außerdem auf das Drachenfeuer aus 0e.
- **3a und 4a** brauchen kein neues Kampfmodell und liefern früh sichtbaren Nutzen (härterer Gegner, Portalsammeln). Wenn schnelle Wirkung gewünscht ist, ziehen sie nach 1.
- Gesamt: etwa **16 PRs**, davon Phase 0 mit fünf größeren. Jeder PR hält die Regel „Selftests grün auf Host und eZ80, CHANGELOG, GDD“.

---

## 7. Dateien

| Bereich | Dateien |
|---|---|
| Regeln (Phase 0) | `data/spells.csv`, `data/weapons.csv`, `data/actions.csv`, `src/core/{spells,combat,items,brew,area,effect,world,wizard}.c`, `tools/gen_data.py`, `tests/selftest.c`, `docs/design/GDD.md` §6–§7, `host/arena.c` |
| KI (Phasen 1–4) | `src/core/ai*.c` (neu aufgeteilt), `src/core/world.h` (`Unit`-Bits), `src/core/save.c` (v9), `data/creatures.csv`, `data/ai_items.csv`, `data/scenarios/*.txt`, `tools/gen_scenarios.py`, `src/agon/mapfile.c`, `tools/gen_maps.py`, `data/maps/*.txt` |
| Messen | `host/arena.c` oder neues `host/aisim.c`, `tools/test.py`, `docs/TESTING.md` |

---

## 8. Offene Entscheidungen

1. **Rückschlag und „Gebunden“ (R7, R8):** Im Original zahlt der Verteidiger jedes Mal 4 AP und 4 Ausdauer und kann nicht ziehen, bevor der Feind tot ist. Wir haben den freien Rückschlag (D27, D29) und den freien Schlag beim Wegziehen (D26) bewusst anders gelöst. Gilt „wie im Original“ auch hier? Das ändert, wie die KI Flucht und Bedrohung bewertet.
2. **Beschwören (R14):** Original: `L` Kreaturen je Wurf, die Stufe wird verbraucht, die KI wirkt nur mit „≥ L freien Nachbarfeldern“. Wir: eine Kreatur, Stufe = Stärke, unbegrenzt (D34, Nutzerregel). Bleibt D34?
3. **Designer-Preise (R2):** Spectrum-Staffel `Basis + Inkrement × L` für alle 45 Zauber oder die Nutzer-Anker aus F6 (nur Beschwörungen kaufbar, +50 %)?
4. **KI-Zauberer:** defensiv im Haus (D62) bleiben oder wie im Original Routen laufen und jagen?
5. **Allwissen:** Das Original lässt Aggressive zum Zauberer von Spieler 1 laufen. Vorschlag: nein, nur zum bekannten Haus und zur letzten gesehenen Position (§4, Phase 4a).
6. **Schwierigkeitsstufen** im Setup, oder nur der feste Original-Schwierigkeitsgrad?
7. **Reihenfolge:** zuerst Phase 0 komplett, oder nach 0a/0b schon 1 und 3a/4a (schneller sichtbarer Nutzen)?

---

## 9. Fragen an den Quellcode des Spectrum-Originals

Damit die Lücken in K10 schließen, bevor die jeweilige Phase beginnt:

1. **Landen und Abfliegen der KI** (`$9E3A`): Wann hebt eine Kreatur ab, wann landet sie? (Phase 2/4)
2. **Routentabellen** (`$D00F`–`$D017`): Aufbau von Wegpunkten, Routenflags und Planeinträgen; die 11/7/6 Routen als Koordinatenlisten für die drei Szenarien. (4b)
3. **Auslöserfelder** (`$D017`) und die Schläfer-Liste (`$D015`): Einträge je Szenario (x, y, ID) und wer schläft. (4b)
4. **Schrittwahl** (`$9AF3`/`$9A48`): Nach welchem Maß werden die 8 Nachbarfelder sortiert, nur Entfernung oder auch Geländekosten und Hindernisse? (Phase 1)
5. **Zauber-Priorität** (`$A4DA`): Wird `D_D3DC` beim Halbieren dauerhaft überschrieben, oder zu Beginn jedes KI-Zugs zurückgesetzt? Das entscheidet, ob die Prioritäten über das Spiel verfallen. (3b)
6. **Bit 7 der Gegenstandstabelle** (`$A5E3`): Was bedeutet es wirklich (Wurfgegenstand oder etwas anderes)? (2c)
7. **Mana-Schwelle `$D046` (40)** für Beschwörungen: ist sie je Szenario verschieden? (3b)
8. **Bewegt sich der Gegner-Zauberer selbst** (Route, Jagd), oder bleibt er stehen? Das entscheidet Frage 4 aus §8. (3b)
