#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Compile data/scenarios/<name>.txt into binary scenario files (M4a, D22).

    uv run tools/gen_scenarios.py

Outputs (generated, not committed):
  build/scenarios/<name>.scn    loaded by the game from /loc/scenarios
  src/core/gen/scenarios.c      the same bytes as C arrays (selftest, host)

.scn format v2:
  "LOCS" | 2 | book_count
  | per book: owner u8 | entry_count u8 | entries: spell u8, level u8
  | profile_count | per profile: owner u8, name 10 bytes, mana, ap, sta, con,
    com, def, mr, carry, vp (u8 each), prio_count u8, (spell u8, priority u8)*
  | route_count u8, summon_routes u8 | per route: flags u8, n u8, (x, y)*
  | plan_count u8 | per plan: unit u8, route u8, step u8, flags u8
  | trigger_count u8 | per trigger: x u8, y u8, id u8

The map itself is referenced by name only; the caller loads
maps/<map>.map. Spell ids come from the generated gen/data.h enum.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCN = ROOT / "data" / "scenarios"
OUT_DIR = ROOT / "build" / "scenarios"
OUT_C = ROOT / "src" / "core" / "gen" / "scenarios.c"
OUT_H = ROOT / "src" / "core" / "gen" / "scenarios.h"
CORE = ROOT / "src" / "core"

OWNERS = {"p1": 0, "p2": 1, "p3": 2, "p4": 3}


def spell_ids() -> dict[str, int]:
    """SP_* enumerator values from the generated header."""
    text = re.sub(r"/\*.*?\*/", "", (CORE / "gen" / "data.h").read_text(encoding="utf-8"),
                  flags=re.S)
    values: dict[str, int] = {}
    for body in re.findall(r"typedef enum \{([^}]*)\} SpellId;", text, flags=re.S):
        nxt = 0
        for item in body.split(","):
            item = item.strip()
            if not item:
                continue
            if "=" in item:
                name, val = (x.strip() for x in item.split("=", 1))
                nxt = int(val, 0)
            else:
                name = item
            values[name] = nxt
            nxt += 1
    return values


def parse(path: Path) -> dict:
    m: dict = {"map": None, "books": [], "profiles": [], "routes": {}, "summon_routes": 0,
               "plans": [], "triggers": []}
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        word, _, rest = line.partition(" ")
        if word == "map":
            m["map"] = rest.strip()
        elif word == "book":
            owner, _, entries = rest.strip().partition(" ")
            book = []
            for pair in entries.split(","):
                f = pair.split()
                book.append((f[0], int(f[1]), int(f[2]) if len(f) > 2 else 0))
            m["books"].append((owner, book))
        elif word == "wizard":
            f = rest.split()
            prof = {"owner": f[0], "name": f[1]}
            for k in range(2, len(f), 2):
                prof[f[k]] = int(f[k + 1])
            m["profiles"].append(prof)
        elif word == "route":
            head, _, wps = rest.partition(":")
            f = head.split()
            m["routes"][int(f[0])] = (int(f[2], 0),
                                      [tuple(int(v) for v in p.split(",")) for p in wps.split()])
        elif word == "summon_routes":
            m["summon_routes"] = int(rest)
        elif word == "plan":
            f = rest.split()
            m["plans"].append((int(f[0]), int(f[2]), int(f[4]), int(f[6], 0)))
        elif word == "trigger":
            m["triggers"].append(tuple(int(v) for v in rest.split()))
        else:
            raise SystemExit(f"{path.name}: unexpected line: {raw!r}")
    if not m["map"]:
        raise SystemExit(f"{path.name}: missing map line")
    return m


def encode(m: dict, ids: dict[str, int]) -> bytes:
    out = bytearray(b"LOCS")
    out.append(2)
    out.append(len(m["books"]))
    prios: dict[str, list] = {}
    for owner, book in m["books"]:
        if owner not in OWNERS:
            raise SystemExit(f"unknown owner {owner!r}")
        entries = []
        for spell, level, prio in book:
            key = "SP_" + spell.upper()
            if key not in ids:
                raise SystemExit(f"unknown spell {spell!r} (see data/spells.csv)")
            if not 0 <= level <= 10:
                raise SystemExit(f"spell level must be 0..10: {spell} {level}")
            if prio:
                prios.setdefault(owner, []).append((ids[key], prio))
            if level:
                entries.append((ids[key], level))
        out.append(OWNERS[owner])
        out.append(len(entries))
        for s, l in entries:
            out += bytes((s, l))
    out.append(len(m["profiles"]))
    for p in m["profiles"]:
        out.append(OWNERS[p["owner"]])
        out += p["name"].encode("ascii")[:10].ljust(10, bytes(1))
        out += bytes(p[k] for k in ("mana", "ap", "sta", "con", "com", "def", "mr", "carry", "vp"))
        pr = prios.get(p["owner"], [])
        out.append(len(pr))
        for s, v in pr:
            out += bytes((s, v))
    routes = m["routes"]
    n = (max(routes) + 1) if routes else 0
    out.append(n)
    out.append(m["summon_routes"])
    for r in range(n):
        flags, wps = routes[r]
        out += bytes((flags, len(wps)))
        for x, y in wps:
            out += bytes((x, y))
    out.append(len(m["plans"]))
    for pl in m["plans"]:
        out += bytes(pl)
    out.append(len(m["triggers"]))
    for tr in m["triggers"]:
        out += bytes(tr)
    return bytes(out)


def main() -> int:
    ids = spell_ids()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT_C.parent.mkdir(parents=True, exist_ok=True)
    c = ["/* GENERATED by tools/gen_scenarios.py - do not edit. */",
         '#include "scenarios.h"', ""]
    names = []
    for path in sorted(SCN.glob("*.txt")):
        data = encode(parse(path), ids)
        out = OUT_DIR / (path.stem + ".scn")
        out.write_bytes(data)
        name = path.stem.upper()
        names.append(name)
        c.append(f"const uint8_t SCN_{name}[{len(data)}] = {{")
        c.append("    " + ", ".join(str(b) for b in data))
        c.append("};")
        c.append(f"const uint16_t SCN_{name}_LEN = {len(data)};")
        c.append("")
        print(f"[scn] {path.name} -> {out.relative_to(ROOT).as_posix()} ({len(data)} bytes)")
    h = ["/* GENERATED by tools/gen_scenarios.py - do not edit. */",
         "#ifndef LOC_GEN_SCENARIOS_H", "#define LOC_GEN_SCENARIOS_H", "",
         "#include <stdint.h>", ""]
    for n in names:
        h.append(f"extern const uint8_t SCN_{n}[];")
        h.append(f"extern const uint16_t SCN_{n}_LEN;")
    h += ["", "#endif", ""]
    (CORE / "gen" / "scenarios.h").write_text("\n".join(h), encoding="utf-8", newline="\n")
    OUT_C.write_text("\n".join(c), encoding="utf-8", newline="\n")
    print(f"[scn] {OUT_C.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
