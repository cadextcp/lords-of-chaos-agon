# Plattform-Audit: holen wir das Optimum aus dem Agon Light?

- **Stand:** 2026-10-05, Commit `e09300f`, Hardware Agon Light 2 mit Platform MOS 3 + VDP 2.16.0
- **Frage:** Nutzen wir die besten Muster der Plattform? Brauchen wir VDP-Tweaks? Lohnt Assembler?
- **Methode:** Codelesen (Core, Renderer, KI, Sicht), Linker-Map, vorhandene Messwerte aus
  ADR 0006/0009/0012 und `docs/AGON-QUIRKS.md`, Abgleich mit der MOS- und VDP-Dokumentation.

## Stand der Umsetzung (2026-10-05)

| Punkt | Stand |
|---|---|
| B1 Blocking-Bitmap zwischengespeichert | ✅ umgesetzt |
| B2 eigene Bitmap für Zauber-LOS | ✅ umgesetzt |
| B3 Sicht schneller | ✅ als **Shadowcasting** umgesetzt (D39), nicht als Pro-Einheit-Cache |
| B4 Zeichenstrom gebündelt | ✅ umgesetzt, im GUI-Emulator sichtgeprüft |
| B5 redundantes `select_bitmap` | ✅ umgesetzt (im selben Pfad) |
| B6 `anim_pair`-Tabelle | ✅ umgesetzt, als `const` zur Übersetzungszeit |
| B7 `printf` → `mos_putstring` | ✅ umgesetzt |
| B8 Viewport-Scroll beim Schwenk | ⬜ offen, braucht M2 — und `VDU 23,27,3` ignoriert den Viewport (siehe ADR 0006) |
| M1 Wo liegen die 100 ms? | ⬜ offen (der Redraw ist jetzt der größte Posten) |
| M2 Scrollt MODE 8? | ⬜ offen |
| M3 8- oder 24-Bit-Codegen? | ⬜ offen |
| M4 KI-Phase nachmessen | ✅ **160 → 20 ms**, Sicht **298 → 12 ms** (Hardware, 2026-10-05) |
| Bewegte Einheiten als Sprites | ⬜ offen |

Der Pro-Einheit-Sichtcache (B3, Weg 1) ist **nicht** gebaut: Shadowcasting
macht eine komplette Neuberechnung so billig, dass der Cache erst wieder
lohnt, wenn die Hardwaremessung das Gegenteil zeigt. Erst messen.

## Kurzfassung

Die Plattformnutzung ist **gut** — die Muster, die auf dem Agon richtig sind, sind alle da
(Abschnitt 1). Der Flaschenhals liegt nicht an fehlenden VDP-Fähigkeiten und nicht an C
statt Assembler, sondern an **drei Stellen, an denen Arbeit wiederholt wird, die sich
zwischenspeichern lässt**. Zwei davon sind risikolos und erklären zusammen vermutlich den
größten Teil der KI-Phase und der Eingabeverzögerung.

Die wichtigste strukturelle Erkenntnis: **alle Optimierungsziele des Projekts wurden am
Emulator gesetzt, und der Emulator unterschätzt die Hardware um Faktor 2 bis 5.** Dadurch
sind Entscheidungen, die damals als „reicht" abgehakt wurden, auf dem Gerät nicht mehr
tragfähig — allen voran die Sichtberechnung.

**Assembler ist derzeit die falsche Antwort.** Die gemessenen Hotspots sind algorithmisch;
ein Zwischenspeicher bringt dort Faktor 10 bis 100, Assembler bestenfalls Faktor 2.

## 1. Was schon richtig gemacht wird

Das ist keine Höflichkeit, sondern der Grund, warum der Audit so kurz ausfällt. Diese Muster
sind auf dem Agon die jeweils richtige Wahl und sitzen:

- **Kacheln als VDP-Buffer-Bitmaps**, gezeichnet mit `select_bitmap` + `draw_bitmap`
  (ADR 0006). Das ist das Standardmuster der Plattform; die Pixel liegen im VDP, nicht im
  eZ80-RAM, und gehen nur einmal beim Start über die Leitung.
- **Dirty-Field-Redraw** statt Vollbild: ein Schritt zeichnet 2 Felder, nicht 81.
- **Static-Cache** für Boden/Dekor/Feature pro Kartenfeld, invalidiert über eine
  Generationsnummer.
- **Cursor als Sprite** statt als Feld-Redraw — Blinken kostet 0 ms statt eines Redraws.
- **Große Daten gestreamt** (Titelbild 75 KB, `tiles.bin`, `sfx.bin`) durch einen kleinen
  Stagingpuffer, statt sie im eZ80-RAM zu halten.
- **Keine Division im Pro-Feld-Code**: der Ursprung wird einmal pro Frame normalisiert
  (ADR 0006, M2b-Nachtrag). Das war schon einmal ein Faktor 3 bei der Sicht (ADR 0009).
- **`-O2` für heiße, `-Oz` für kalte Übersetzungseinheiten** im Makefile — bei 448 KB RAM
  genau die richtige Abwägung.
- **Audio pro Note getaktet**, weil der VDP Noten auf belegten Kanälen verwirft (ADR 0012, A1).
- **Kein `malloc`, keine Gleitkommazahlen, nur `stdint`-Typen** im Core.
- **Eigene Fonts aus VDP-Buffern** statt Pixel-für-Pixel-Text.

Dazu eine Teststruktur, die derselben Logik auf Host **und** eZ80 beim Rechnen zusieht. Das
ist für ein Agon-Projekt ungewöhnlich solide.

## 2. Der zentrale Befund: der Emulator unterschätzt die Hardware

| Messung | Emulator | Hardware | Faktor |
|---|---|---|---|
| Fenster komponieren (81 Felder) | 18–22 ms | **90 ms** | ~4–5 |
| Voller Redraw (81 Felder) | 44–50 ms | **152 ms** | ~3 |
| Sichtberechnung (2 eigene Einheiten) | 162 ms | **298 ms** | ~1,8 |
| KI-Phase | — | **160 ms** | — |

(Emulatorwerte aus ADR 0006/0009, Hardwarewerte aus `docs/AGON-QUIRKS.md`. **Achtung:** die
Läufe sind nicht deckungsgleich — Emulator auf „Testland", Hardware auf Szenario 1 mit
`many_coloured_land`. Die Faktoren sind daher Größenordnungen, keine exakten Verhältnisse.)

Die Folge in Zahlen: ein eigener Schritt löst `update_sight()` (298 ms) plus `view_update()`
(90 ms) plus Redraw aus. **Ein Schritt kostet also grob 0,4–0,5 s.** ADR 0009 hat 162 ms
ausdrücklich als „spürbar, aber spielbar" abgenommen — auf der Hardware ist derselbe Pfad
knapp doppelt so teuer und damit jenseits dessen, was sich flüssig anfühlt.

Das ist der eigentliche Hebel für „Bedienung optimieren": nicht die Tastenbehandlung, sondern
die Latenz zwischen Taste und Bild.

## 3. Befunde, nach Wirkung sortiert

### B1 — Die Blocking-Bitmap wird bei **jeder** LOS-Abfrage neu gebaut

**Risikolos, höchste Wirkung. Der klarste Fund des Audits.**

`sight_has_los()` und `sight_has_spell_los()` rufen beide als erstes `build_blk()`
(`src/core/sight.c:150`, `:169`). `build_blk()` baut die Blockier-Bitmap der **ganzen Karte**
neu: 36 Zeilen × 5 Spalten = 180 Aufrufe von `world_sight_byte()`, jeder mit einer
8er-Schleife → **1440 Durchläufe mit je zwei Tabellenzugriffen**, plus `memset` über 180 Byte.
Das ist pro Abfrage.

Und diese Abfragen stehen in Schleifen:

- `src/core/ai.c:53` — `ai_nearest_enemy()` prüft LOS **pro Kandidat** in einer Schleife über
  alle Einheiten.
- `src/core/ai.c:161` — dasselbe über alle Objekte.
- `src/core/ai.c:258` — dasselbe über Ziele.
- `src/core/items.c:269`, `src/core/spells.c:233`, `:314`, `:349`.

Bei 25 KI-Einheiten mit je ein paar Kandidaten sind das schnell 50–100 vollständige
Neuaufbauten der Kartenbitmap pro KI-Phase. **Vermutung (nicht gemessen):** das erklärt den
Großteil der 160 ms KI-Phase, und zwar als reine Verschwendung — die Bitmap hängt nur von
Boden und Feature ab, die sich während einer KI-Phase gar nicht ändern.

**Behebung:** `blk` hinter der schon vorhandenen Generationsnummer zwischenspeichern.

```c
static uint8_t blk[MAP_MAX_H][SIGHT_COLS];
static const World *blk_world;
static uint8_t blk_gen;   /* World.generation is uint8_t */

static void ensure_blk(const World *w)
{
    if (blk_world == w && blk_gen == w->generation)
        return;
    build_blk(w);
    blk_world = w;
    blk_gen = w->generation;
}
```

`view.c` hat mit `cache_world`/`cache_gen` genau dasselbe Muster schon vorgemacht — es ist
also bereits projektübliche Praxis.

**Zu prüfen:** ändert `world->generation` sich wirklich bei allem, was Sicht blockiert
(Feuer brennt Wald weg, Mauer eingestürzt, Baum gefällt)? Falls nicht, muss die
Invalidierung dort ergänzt werden. Der Selftest sollte einen Fall bekommen, der Gelände
ändert und danach LOS prüft.

**Kosten:** 0 Byte RAM zusätzlich (`blk` existiert schon), ~6 Zeilen Code.

### B2 — `sight_has_spell_los()` scannt zusätzlich die ganze Karte

Jeder Zauber-LOS-Test baut nicht nur `blk` neu, sondern läuft danach noch über **alle 1296
Felder** und ruft pro Feld `world_has_roof()` (mit Multiplikation) und
`world_feature_blocks_sight()` (`src/core/sight.c:169`). Das ist der teuerste LOS-Pfad, und
er hängt an `main.c:124` — also am **Zielen mit dem Cursor**, wo die Latenz direkt zu spüren ist.

**Behebung:** eine zweite Bitmap `blk_spell` neben `blk`, aus demselben Generations-Cache
gefüllt. Kosten: 180 Byte RAM.

### B3 — Sicht wird komplett neu berechnet, obwohl sich eine Einheit bewegt hat

**Höchste Wirkung auf das Spielgefühl, mittleres Risiko.**

`sight_compute()` (`src/core/sight.c:83`) verwirft `visible` komplett und rechnet für **jede**
eigene Einheit das ganze Sichtfeld neu: pro Einheit 19×19 = 361 Felder (fliegend 23×23 = 529),
und pro Feld einen Bresenham-Lauf über bis zu 9 Schritte. Das ist O(r³) pro Einheit.

Gemessen: 298 ms für **zwei** Einheiten, also ~149 ms pro Einheit, ~413 µs pro Feld. Mit 25
eigenen Einheiten läge das im Sekundenbereich.

Aufgerufen wird es über `update_sight()` „bei jedem eigenen Schritt und am Rundenende"
(`src/agon/main.c:399`). Bei einem Schritt ändert sich aber nur das Sichtfeld **einer**
Einheit.

Drei Wege, in steigender Wirkung und steigendem Risiko:

1. **Pro-Einheit-Cache, verhaltensgleich.** Sichtfeld jeder Einheit einzeln merken und
   OR-verknüpfen; nur für bewegte Einheiten neu rechnen, alles verwerfen wenn
   `world->generation` sich ändert. Ein Schritt kostet dann eine Einheit statt aller.
   **RAM:** als Vollkartenbitmap 180 Byte/Einheit (25 Einheiten = 4,5 KB); als Kästchen um
   die Einheit (23 Zeilen × 3 Byte + Ursprung) ~72 Byte/Einheit = 1,8 KB. Das passt ins
   Budget (Abschnitt 6).
2. **Felder in Entfernungsreihenfolge abarbeiten** und den `already-visible`-Übersprung
   dadurch häufiger greifen lassen. Billig, verhaltensgleich, aber nur ein kleiner Faktor.
3. **Recursive Shadowcasting** über 8 Oktanten statt Strahl-pro-Feld: O(r²) statt O(r³),
   realistisch Faktor 10–20. **Aber:** Shadowcasting liefert eine andere Sichtmenge als
   Strahlenwerfen. Das ändert Spielverhalten und den View-Hash und ist damit eine
   GDD-Entscheidung (§14), keine reine Optimierung. **Nicht ohne Abstimmung.**

Empfehlung: Weg 1. Er ist verhaltensgleich, der Selftest kann ihn gegen `sight_compute()` als
Referenz prüfen (genau wie der Static-Cache gegen `view_compose()` geprüft wird), und er
trifft den häufigsten Fall — einen Schritt.

### B4 — VDU-Bytes gehen einzeln hinaus statt gebündelt

`draw_tile()` macht `vdp_adv_select_bitmap()` + `vdp_draw_bitmap()`
(`src/agon/render.c:134`). Das sind zwei Bibliotheksaufrufe, die zusammen 12 Byte erzeugen
(5 Byte für `VDU 23,27,&20,id;` + 7 Byte für `VDU 23,27,3,x;y;`). Ein voller Redraw mit ~260
Bitmaps sind ~3120 Byte, jedes Byte über einen eigenen Weg durch die libagon und MOS.

**MOS kann das in einem Aufruf.** `RST 18h` hat einen Längenmodus: `BC` = Länge, der
Delimiter in `A` greift **nur bei `BC = 0`**. Beliebige Binärbytes inklusive Nullbytes sind
also erlaubt. In agondev ist das `mos_puts(buffer, size, 0)`.

Das ist Hebel 1 aus ADR 0006 („VDU-Bytes eines Frames in **einen** Puffer sammeln") — damals
notiert, nie gebaut. Vorschlag: ein Zeichenstrom-Puffer in `render.c`, in den `draw_tile()`
schreibt, und ein `render_flush()` am Ende von `render_fields()`.

**Wichtig, und der Grund für Abschnitt 7:** wie viel das bringt, hängt davon ab, wo die 152 ms
eines Vollbilds tatsächlich liegen. Bei 1,152 MBaud sind 3120 Byte nur ~27 ms reine
Übertragung — dann liegen ~125 ms im eZ80-Overhead und Bündeln bringt sehr viel. Bei
384 kBaud wären es ~81 ms Leitungszeit — dann bringt **Bytes sparen** (B5) mehr als Bündeln.
Das muss gemessen werden, bevor man baut.

### B5 — Redundantes `select_bitmap`

Jedes `draw_tile()` wählt die Bitmap neu, auch wenn es dieselbe wie beim vorigen Aufruf ist.
Bei einem Vollbild wiederholen sich Bodenkacheln stark; bei ~30 verschiedenen Kachel-IDs auf
260 Zeichnungen sinkt der Strom von 3120 auf ~1970 Byte (−37 %), wenn man

- entweder die letzte gewählte ID merkt und gleiche Aufrufe überspringt (trivial, wirkt nur
  bei Nachbarschaft), oder
- die Zeichnungen eines Vollbilds **nach Kachel-ID gruppiert** ausgibt (ein `select` pro ID,
  danach nur noch `draw`).

Gruppieren ist nur erlaubt, wo sich Ebenen nicht überlappen. Innerhalb eines Feldes ist die
Reihenfolge bindend (Boden → Dekor → Einheit → Overlay), und fliegende Einheiten ragen ins
Feld darüber. Eine **ebenenweise** Ausgabe über das ganze Fenster (alle Böden, dann alle
Dekore, …) erhält die Reihenfolge und erlaubt das Gruppieren pro Ebene. Das ist eine echte
Umstellung von `render_fields()`, kein Einzeiler — aber der View liefert die Ebenen schon
getrennt, die Information ist also da.

### B6 — `anim_swap()` als Linearsuche im heißesten Pro-Feld-Pfad

`view_update()` ruft für **jede Ebene jedes Feldes** `is_animated()` auf, und das ruft zweimal
`anim_swap()`, das linear über 7 Einträge sucht (`src/core/view.c:43`). Bei 81 Feldern × ~6
Ebenen × ~28 Vergleichen sind das ~13 600 Vergleiche pro Compose — in einer Funktion, die auf
Hardware 90 ms braucht.

**Behebung:** Tabelle statt Suche. `tools/build_tiles.py` erzeugt schon `gen/tiles.h` und
kennt alle Kachel-IDs; es kann eine `ANIM_PARTNER[TILE_COUNT]`-Tabelle mitgenerieren
(0 = nicht animiert, sonst die Partner-ID). Dann ist `is_animated()` ein Array-Zugriff und
`anim_swap()` einer. **RAM:** 291 × 2 Byte = 582 Byte in `.rodata`, nicht in `.bss`.

Nebeneffekt: `view_update()` berechnet `animated[][]` in jedem Frame neu, obwohl es nur von
den Kachel-IDs abhängt, die ohnehin gerade verglichen werden — die Tabelle macht das
nahezu kostenlos.

### B7 — `printf()` für Panel- und Menütext

`text_at()` benutzt `printf("%s", s)` (`src/agon/render.c:301`). Das zieht die vollständige
Formatierungsmaschine der libc für etwas, das `mos_puts(s, len, 0)` direkt kann. Das Panel
wird pro Frame mehrfach geschrieben (Name, Hand, AP, Mana, 6 Balken, „Am Boden"-Liste).

Kleiner Posten im Vergleich zu B1–B3, aber billig zu holen, und er senkt auch den Codeumfang
(Abschnitt 6). `snprintf()` zum Bauen der Zeilen kann bleiben — teuer ist der Ausgabeweg.

### B8 — Kameraschwenk zeichnet das Vollbild (offen, braucht Messung)

Ein Vollbild-Redraw (152 ms) fällt nur beim Scrollen an. Der VDP kann **Grafik-Viewports
scrollen**: `VDU 23,7,extent,direction,movement` mit `extent = 2` für den aktuellen
Grafik-Viewport, pixelweise wenn `movement != 0`. Mit `VDU 24` ist das Kartenfenster ohnehin
als Viewport beschreibbar.

Wenn das in MODE 8 funktioniert, kostet ein Schwenk um ein Feld das Scrollen plus **eine neue
Zeile oder Spalte** (9 Felder statt 81) — grob 152 ms → 20–30 ms.

**Die Dokumentation sagt nicht, ob Viewport-Scrolling in den Bitmap-Modi korrekt arbeitet.**
Genau dafür gibt es `spikes/vdptest/`. Das ist der eine VDP-Tweak, der sich zu untersuchen
lohnt — siehe Abschnitt 7.

## 4. VDP: was hilft, was nicht

**Buffered Commands API (`VDU 23,0,&A0`) hilft hier nicht.** Sie kann Befehlsfolgen in einem
Puffer ablegen (Befehl 0) und aufrufen (Befehl 1) und Bytes darin verändern (Befehl 5). Für
Kacheln an **wechselnden** Koordinaten muss man die Koordinaten vor jedem Aufruf patchen —
das kostet ungefähr so viele Bytes wie das Zeichenkommando selbst. Der Gewinn entsteht nur
bei Folgen, die **unverändert** wiederholt werden. Issue #2 („Buffered Commands") sollte
deshalb auf B4 (Bündeln) umgestellt oder geschlossen werden; es ist der teurere Weg zum
kleineren Gewinn.

**Doppelpuffer bleibt richtig verworfen** (ADR 0012, 4). Mit Dirty-Field-Redraw müsste jede
Änderung doppelt gezeichnet werden. Daran ändert sich nichts.

**Palette/Copper bleibt verworfen** — `VDU 19` wirkt in MODE 8 nicht (ADR 0012, P1), und ein
Moduswechsel steht nicht zur Debatte.

**Sprites sind unterbenutzt.** 8 Sprites kosten ~3 ms pro Bild (ADR 0012, S1) — gemessen im
Emulator, auf Hardware noch offen. Der Cursor ist schon ein Sprite, und das hat den Blinkpfad
von einem Feld-Redraw auf 0 ms gedrückt. Dieselbe Logik gilt für alles, was sich über
unverändertem Untergrund bewegt: gleitende Schritte, Projektile, Schadenszahlen. Ein als
Sprite bewegtes Objekt braucht **keinen** Feld-Redraw — und Feld-Redraws sind auf Hardware
das Teure. Das ist der zweite ergiebige VDP-Hebel neben B8, und er steht als Restposten schon
im Handover („KI-Bewegungen gleiten noch nicht").

**Was wir nicht brauchen:** affine Transformationen (Befehl 40), Bitmap-Spiegelung über
Puffer-Reverse (Befehl 24), zusätzliche Grafikmodi. Nichts davon adressiert einen gemessenen
Engpass.

## 5. Assembler: ehrliche Einordnung

**Jetzt nicht.** Die Rangfolge der Gewinne ist eindeutig:

| Maßnahme | realistischer Faktor |
|---|---|
| B1 Blocking-Bitmap zwischenspeichern | 10–100 auf dem LOS-Pfad |
| B3 Pro-Einheit-Sichtcache | ~N (Zahl eigener Einheiten) auf dem Schrittpfad |
| B4/B5 Strom bündeln und gruppieren | 1,5–3 auf dem Zeichenpfad |
| Assembler in denselben Schleifen | **1,5–2** |

Assembler in `path_clear()` zu schreiben, bevor B1 und B3 drin sind, heißt Arbeit zu
beschleunigen, die danach gar nicht mehr stattfindet.

**Es gibt aber eine Zwischenstufe, die wahrscheinlich mehr bringt als handgeschriebener
Assembler und viel billiger ist: nachsehen, was der Compiler wirklich erzeugt.**

`path_clear()` ist bewusst in `int8_t` geschrieben, mit dem Kommentar „8-bit math throughout
… to dodge the eZ80's 24-bit int arithmetic". Nach C-Regeln wird `int8_t`-Arithmetik aber auf
`int` hochgezogen — und `int` ist auf dem eZ80 **24 Bit** (QUIRK T3). Ob der Rückcast auf
`int8_t` den Compiler dazu bringt, tatsächlich 8-Bit-Code zu erzeugen, oder ob er in 24 Bit
rechnet und am Ende abschneidet, **ist offen**. Die Messung stützt den Verdacht: ~413 µs pro
Feld sind bei ≤9 Bresenham-Schritten etwa 7600 Takte — deutlich mehr, als die Schleife
aussieht.

agondev ist clang-basiert, also liefert `-S` das Listing. Eine Stunde mit dem Assemblerlisting
von `path_clear()`, `blocked()` und `compose_fast()` sagt mehr als jede Schätzung — und wenn
dort 24-Bit-Arithmetik steht, ist die Abhilfe oft eine Zeile C (konsequent `uint8_t`-Lokale,
Maskierung statt Cast), nicht eine Assemblerfunktion.

**Wenn es danach noch Assembler sein soll**, sind das die drei Kandidaten, in dieser
Reihenfolge:

1. **Der VDU-Strom-Blit** aus B4 — `ldir` ist genau das, wofür der Z80 gebaut ist, und der
   Puffer wird pro Frame gefüllt.
2. **Die Bresenham-Innenschleife** von `path_clear()`, falls sie nach B1/B3 überhaupt noch
   heiß ist (Register halten statt Stack).
3. **`world_sight_byte()`** — reine Bitschieberei über zwei Tabellen. Wird durch B1 allerdings
   kalt.

Die Aufrufkonvention ist in QUIRK T4 schon dokumentiert (erstes Argument bei `(iy+3)`), es
gibt also Vorerfahrung im Projekt. Und QUIRK T7 erinnert daran, dass das Backend v0.22 Bugs
hat — ein Grund mehr, das Listing anzusehen.

## 6. Die bindende Schranke: eZ80-RAM

Aus `bin/loc.map` (Commit `e09300f`), exakt:

| Abschnitt | Größe |
|---|---|
| `.text` | 251 405 Byte |
| `.rodata` | 56 119 Byte |
| `.data` | 395 Byte |
| `.bss` | 124 309 Byte |
| **Summe** | **432 228 Byte** |
| Region `USERRAM` (`0x040000`–`0x0B0000`) | 458 752 Byte |
| **frei** (`___heapbot 0x0A9A45` → `__stack 0x0B0000`) | **26 043 Byte** |

Diese 26 KB sind **gleichzeitig der Stack** (`__stack = 0x0B0000`, nach unten wachsend;
dynamische Allokation findet nicht statt). QUIRK S6 erinnert daran, dass 12 KB zusätzliche
statische Puffer das Budget schon einmal gesprengt haben.

Realistisch nutzbar für neue statische Daten: **grob 15–20 KB.** Der Bedarf der Vorschläge:

| Vorschlag | RAM |
|---|---|
| B1 `blk`-Cache | 0 (existiert schon) |
| B2 `blk_spell` | 180 Byte |
| B3 Pro-Einheit-Sichtcache, Kästchenform | ~1,8 KB |
| B4 VDU-Strompuffer | 1–4 KB |
| B6 `ANIM_PARTNER` | 582 Byte, `.rodata` |

Das passt zusammen — aber nicht mit großer Reserve. Zwei Gegenfinanzierungen liegen bereit:
B7 (`printf` raus) senkt `.text`, und `.text` ist mit 251 KB der größte Posten überhaupt.
Vor jedem Einbau: `bin/loc.map` ansehen, nicht raten. Die CI meldet die Größe von `loc.bin`
bereits (ROADMAP) — eine Schwelle für den freien Rest wäre die logische Ergänzung.

## 7. Was zuerst gemessen werden muss

Drei der Befunde sind Vermutungen aus dem Codelesen, und zwei Entscheidungen hängen an einer
Zahl, die wir nicht haben. Das ist billig zu klären, und die Hardware hängt am USB.

**M1 — Wo liegen die 152 ms eines Vollbilds?** Entscheidet zwischen B4 (Bündeln) und B5
(Bytes sparen). Drei Messungen in `spikes/vdptest/`:

1. `n` × `select_bitmap` einzeln über `vdp_adv_select_bitmap()` → eZ80-Weg pro Byte.
2. Dieselben Bytes als ein `mos_puts()` → Leitung + VDP-Parsing ohne eZ80-Overhead.
3. Dasselbe mit `select` + `draw` einer 24×24-Bitmap → Differenz zu (2) ist die
   Rasterisierung im VDP.

Daraus fällt die effektive Byterate des Pfades eZ80 → VDP heraus, und damit die Antwort.

**M2 — Scrollt `VDU 23,7,2,…` in MODE 8 korrekt?** Entscheidet B8. Viewport mit `VDU 24`
setzen, Kacheln zeichnen, um 24 px scrollen, Ergebnis ansehen. Ein Testfall in `vdptest`.

**M3 — Rechnet `path_clear()` in 8 oder in 24 Bit?** Entscheidet, ob Abschnitt 5 eine
C-Änderung oder eine Assemblerfunktion wird. `-S` und lesen.

**M4 — KI-Phase mit und ohne B1.** B1 ist so billig, dass „einbauen und nachmessen" der
schnellste Weg zur Zahl ist. Erwartung: 160 ms → deutlich unter 50 ms.

Und unabhängig davon: die Hardwarewerte stammen von Commit `037521f` (2026-10-03). Seitdem
sind die Polish-Runde, Flächeneffekte, Sound und der Reit-Fix dazugekommen. **Ein frischer
`loc --bench` auf dem Gerät ist die Voraussetzung für alles andere** — ADR 0012 hat die
Hardwarebestätigung ohnehin noch offen.

## 8. Vorgeschlagene Reihenfolge

Playtesten muss darauf nicht warten — die Punkte unten ändern Latenz, nicht Spielinhalt.

**Zuerst, risikolos, klein:**

1. Frischer `loc --bench` + `vdptest` auf der Hardware (Grundlinie, ADR 0012 schließen).
2. **B1** `blk`-Cache. Mit Selftest-Fall für Geländeänderung. Nachmessen (M4).
3. **B2** `blk_spell` dazu.
4. **B6** `ANIM_PARTNER`-Tabelle aus `build_tiles.py`.

**Danach, mittlere Eingriffe:**

5. **M1** messen, dann **B4** und/oder **B5** bauen.
6. **B3** Pro-Einheit-Sichtcache, Selftest gegen `sight_compute()` als Referenz.
7. **B7** `printf` → `mos_puts` im Panelpfad.

**Dann erst:**

8. **M2** messen, bei Erfolg **B8** (Scroll beim Schwenk).
9. **M3** lesen, Codegen in C reparieren; Assembler nur, wenn das Listing es rechtfertigt.
10. Bewegte Einheiten als Sprites (Abschnitt 4) — zugleich ein Spielgefühl-Gewinn.

**Nicht ohne Abstimmung:** Shadowcasting (B3, Weg 3) ändert Sichtmengen und damit Spielregeln
— das wäre eine GDD-Entscheidung, keine Optimierung.

## Quellen

- MOS-API, `RST 18h` Längen- und Delimitermodus:
  <https://agonplatform.github.io/agon-docs/mos/API/>
- VDP Buffered Commands API:
  <https://agonplatform.github.io/agon-docs/vdp/Buffered-Commands-API/>
- VDU-Befehle (`VDU 23,7` Scroll, `VDU 24` Grafik-Viewport):
  <https://agonplatform.github.io/agon-docs/vdp/VDU-Commands/>
- Softwareübersicht der Plattform (Index, kein technischer Leitfaden):
  <https://github.com/sabotrax/agon-software>
- Projektintern: ADR 0006, 0009, 0012, `docs/AGON-QUIRKS.md`, `bin/loc.map`
