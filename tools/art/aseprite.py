#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Aseprite sources for the tiles (ADR 0013).

    uv run tools/art/aseprite.py import [--only FAMILY]   # PNG -> .aseprite
    uv run tools/art/aseprite.py export [--only FAMILY]   # .aseprite -> PNG
    uv run tools/art/aseprite.py check                    # round trip exact?
    uv run tools/art/aseprite.py list                     # families and tiles

Every tile family is one sprite in assets/aseprite/<family>.aseprite: the
tiles sit in a grid of COLS columns, each one a slice named after its PNG
(slice data = "tiles" or "icons", the folder it is exported to). Edit
there - in Aseprite by hand or through the aseprite MCP server (.mcp.json)
- then `export` writes the PNGs. The PNGs stay committed: build_tiles.py,
mockup.py and CI read them and need no Aseprite. `check` exports into a
temporary folder and compares every pixel with the committed PNGs.

A new PNG without a sprite (e.g. drawn by a script) goes in with
`import --only FAMILY`, which rebuilds that family from the PNGs - export
first if the sprite holds unexported edits.

Aseprite: ASEPRITE_PATH, else the usual install folders, else PATH.
"""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TILES = ROOT / "assets" / "tiles"
ICONS = ROOT / "assets" / "icons"
SPRITES = ROOT / "assets" / "aseprite"
PALETTE = ROOT / "assets" / "palette" / "agon64.gpl"
COLS = 8
# Families by name prefix; creatures (with their _f1/_f2 frames) are one
# family, the 8x8 icons another, the rest "misc".
PREFIXES = ("edge", "floor", "wall", "fence", "door", "gate", "window", "decor",
            "obj", "fx", "area", "cursor", "portal")


def creature_ids() -> set[str]:
    rows = (ROOT / "data" / "creatures.csv").read_text(encoding="utf-8").splitlines()
    return {r.split(",", 1)[0] for r in rows if r and not r.startswith(("#", "id,"))}


def family_of(png: Path, creatures: set[str]) -> str:
    if png.parent == ICONS:
        return "icons"
    stem = png.stem
    head = stem.split("_", 1)[0]
    if head in PREFIXES:
        return head
    base = stem.rsplit("_f", 1)[0] if stem.rsplit("_f", 1)[-1].isdigit() else stem
    if base in creatures:
        return "creatures"
    return "misc"


def families() -> dict[str, list[Path]]:
    cr = creature_ids()
    out: dict[str, list[Path]] = {}
    for png in sorted(TILES.glob("*.png")) + sorted(ICONS.glob("*.png")):
        out.setdefault(family_of(png, cr), []).append(png)
    return dict(sorted(out.items()))


def aseprite_exe() -> str:
    env = os.environ.get("ASEPRITE_PATH")
    candidates = [env] if env else []
    candidates += [r"C:\Program Files\Aseprite\Aseprite.exe",
                   r"C:\Program Files (x86)\Steam\steamapps\common\Aseprite\Aseprite.exe",
                   "/Applications/Aseprite.app/Contents/MacOS/aseprite",
                   os.path.expanduser("~/.steam/steam/steamapps/common/Aseprite/aseprite")]
    for c in candidates:
        if c and Path(c).is_file():
            return c
    found = shutil.which("aseprite")
    if found:
        return found
    raise SystemExit("Aseprite not found: set ASEPRITE_PATH (see ADR 0013)")


def lua_str(s: str | Path) -> str:
    return '"' + str(s).replace("\\", "/").replace('"', '\\"') + '"'


def run_lua(script: str, quiet: bool = False) -> str:
    with tempfile.NamedTemporaryFile("w", suffix=".lua", delete=False, encoding="utf-8") as f:
        f.write(script)
        path = f.name
    try:
        r = subprocess.run([aseprite_exe(), "--batch", "--script", path],
                           capture_output=True, text=True)
        out = (r.stdout or "") + (r.stderr or "")
        if r.returncode != 0 or "ERROR" in out:
            raise SystemExit(f"aseprite failed:\n{out}")
        if out.strip() and not quiet:
            print(out.strip())
        return out
    finally:
        os.unlink(path)


def import_family(name: str, pngs: list[Path]) -> None:
    from PIL import Image          # here: setup.py imports this file without Pillow
    size = Image.open(pngs[0]).size
    w, h = size
    for p in pngs:
        if Image.open(p).size != size:
            raise SystemExit(f"{p.name}: size differs within family {name}")
    rows = (len(pngs) + COLS - 1) // COLS
    out = SPRITES / f"{name}.aseprite"
    lines = [
        f"local spr = Sprite({COLS * w}, {rows * h}, ColorMode.RGB)",
        f"spr:setPalette(Palette{{ fromFile = {lua_str(PALETTE)} }})",
        f"spr.gridBounds = Rectangle(0, 0, {w}, {h})",
        "spr.layers[1].name = 'tiles'",
        "local img = spr.cels[1].image",
    ]
    for i, p in enumerate(pngs):
        x, y = (i % COLS) * w, (i // COLS) * h
        folder = "icons" if p.parent == ICONS else "tiles"
        lines += [
            f"img:drawImage(Image{{ fromFile = {lua_str(p)} }}, Point({x}, {y}))",
            f"do local s = spr:newSlice(Rectangle({x}, {y}, {w}, {h})); "
            f"s.name = {lua_str(p.stem)}; s.data = {lua_str(folder)} end",
        ]
    lines += [f"spr:saveAs({lua_str(out)})", "spr:close()"]
    run_lua("\n".join(lines) + "\n")
    print(f"[ase] {name}: {len(pngs)} tiles -> {out.relative_to(ROOT).as_posix()}")


def export_sprites(names: list[str], dest: Path) -> dict[str, str]:
    """Every slice of the sprites as <dest>/<name>.png; returns the folder
    (slice data, "tiles" or "icons") of each name."""
    lines = []
    for name in names:
        src = SPRITES / f"{name}.aseprite"
        if not src.exists():
            raise SystemExit(f"{src.relative_to(ROOT).as_posix()} missing - run import")
        lines += [
            f"do local spr = Sprite{{ fromFile = {lua_str(src)} }}",
            "  local full = Image(spr.spec)",
            "  full:drawSprite(spr, 1)",
            "  for _, s in ipairs(spr.slices) do",
            "    local b = s.bounds",
            "    local t = Image(b.width, b.height, ColorMode.RGB)",
            "    t:drawImage(full, Point(-b.x, -b.y))",
        ]
        lines += [
            f"    t:saveAs({lua_str(dest)} .. '/' .. s.name .. '.png')",
            "    print('SLICE ' .. s.name .. ' ' .. s.data)",
            "  end",
            "  spr:close()",
            "end",
        ]
    folders: dict[str, str] = {}
    for line in run_lua("\n".join(lines) + "\n", quiet=True).splitlines():
        if line.startswith("SLICE "):
            _, name, folder = line.split()
            folders[name] = folder
    return folders


def same(a: Path, b: Path) -> bool:
    from PIL import Image
    ia, ib = Image.open(a).convert("RGBA"), Image.open(b).convert("RGBA")
    if ia.size != ib.size:
        return False
    ba, bb = ia.tobytes(), ib.tobytes()
    for i in range(0, len(ba), 4):
        if ba[i + 3] == 0 and bb[i + 3] == 0:
            continue                       # transparent: colour does not matter
        if ba[i:i + 4] != bb[i:i + 4]:
            return False
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description="Aseprite sources for the tiles (ADR 0013)")
    ap.add_argument("cmd", choices=("import", "export", "check", "list"))
    ap.add_argument("--only", help="one family")
    args = ap.parse_args()
    fam = families()
    names = [args.only] if args.only else list(fam)
    for n in names:
        if n not in fam:
            raise SystemExit(f"unknown family {n!r}: {', '.join(fam)}")
    if args.cmd == "list":
        for n in names:
            print(f"{n:10} {len(fam[n]):3}  {' '.join(p.stem for p in fam[n])}")
        return 0
    if args.cmd == "import":
        SPRITES.mkdir(parents=True, exist_ok=True)
        for n in names:
            import_family(n, fam[n])
        return 0
    if args.cmd == "export":
        # through a temporary folder: only PNGs whose pixels changed are
        # replaced, so a re-export does not churn the committed files
        with tempfile.TemporaryDirectory() as tmp:
            folders = export_sprites(names, Path(tmp))
            changed = 0
            for q in sorted(Path(tmp).glob("*.png")):
                dst = (ICONS if folders.get(q.stem) == "icons" else TILES) / q.name
                if dst.exists() and same(dst, q):
                    continue
                shutil.copyfile(q, dst)
                changed += 1
                print(f"[ase] wrote {dst.relative_to(ROOT).as_posix()}")
        print(f"[ase] export: {changed} PNG(s) changed")
        return 0
    with tempfile.TemporaryDirectory() as tmp:     # check
        export_sprites(names, Path(tmp))
        bad, missing = [], []
        for n in names:
            for p in fam[n]:
                q = Path(tmp) / p.name
                if not q.exists():
                    missing.append(p.name)
                elif not same(p, q):
                    bad.append(p.name)
        extra = {q.name for q in Path(tmp).glob("*.png")} - {p.name for n in names for p in fam[n]}
        total = sum(len(fam[n]) for n in names)
        if bad or missing or extra:
            for label, items in (("differs", bad), ("not in a sprite", missing),
                                 ("only in a sprite", sorted(extra))):
                for i in items:
                    print(f"[ase] {label}: {i}")
            print(f"[ase] check FAILED ({len(bad)} differ, {len(missing)} missing, "
                  f"{len(extra)} extra of {total})")
            return 1
        print(f"[ase] check ok: {total} PNGs match their sprites")
    return 0


if __name__ == "__main__":
    sys.exit(main())
