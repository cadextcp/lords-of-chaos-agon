#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Build the game.

    uv run tools/build.py            # Agon binaries -> bin/loc.bin, loctest.bin
    uv run tools/build.py --host     # host build   -> build/host/loc_host
    uv run tools/build.py --all      # both
    uv run tools/build.py --incremental  # skip make clean (headers not tracked!)

The Agon build runs agondev's make (inside WSL on Windows). The host build
compiles the platform-free core plus host/ with the system gcc (also WSL on
Windows), so the same code can be tested without the emulator.
"""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import agon_env as env  # noqa: E402

HOST_BIN = env.BUILD / "host" / "loc_host"
HOST_CFLAGS = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-g", "-Isrc/core"]
# Heap and stack share what lies between the end of .bss and 0xB0000
# (QUIRK S6). Below this the game misbehaves long before anything says so.
RAM_TOP = 0xB0000
RAM_RESERVE_MIN = 16 * 1024


def log(msg: str) -> None:
    print(f"[build] {msg}", flush=True)


def generate() -> int:
    """Tile bank + generated C sources (src/core/gen/) from assets and data."""
    for script in ("build_tiles.py", "gen_data.py", "gen_maps.py", "gen_variants.py",
                   "gen_scenarios.py", "gen_help.py", "gen_sfx.py", "gen_music.py", "build_font.py",
                   "build_title.py"):
        r = subprocess.run(["uv", "run", "--quiet", str(env.ROOT / "tools" / script)], cwd=env.ROOT)
        if r.returncode != 0:
            log(f"{script} FAILED")
            return 1
    return 0


def build_agon(clean: bool = True) -> int:
    if not (env.AGONDEV_DIR / "bin").exists():
        log("agondev missing - run: uv run tools/setup.py")
        return 3
    # agondev's Makefile has no header dependency tracking, so a changed
    # header (e.g. generated src/core/gen/tiles.h) would leave stale objects.
    # The project is small: always build from clean unless asked otherwise.
    if clean:
        env.run_linux(["make", "clean"], cwd=env.ROOT, check=False, capture_output=True)
    r = env.run_linux(["make"], cwd=env.ROOT, check=False)
    if r.returncode != 0:
        log("Agon build FAILED")
        return 1
    binary = env.ROOT / "bin" / "loc.bin"
    log(f"Agon binary: {binary.relative_to(env.ROOT)} ({binary.stat().st_size} bytes)")
    if check_ram() != 0:
        return 1
    return build_spike(clean) | build_loctest()


def check_ram() -> int:
    """The game's heap and stack live between the end of .bss and RAM_TOP;
    too little of it shows only as odd behaviour (QUIRK S6), so the build
    refuses it."""
    text = (env.ROOT / "bin" / "loc.map").read_text(errors="replace")
    m = re.search(r"^\.bss\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)", text, re.M)
    if not m:
        log("loc.map: no .bss line - RAM reserve unchecked")
        return 0
    end = int(m.group(1), 16) + int(m.group(2), 16)
    reserve = RAM_TOP - end
    log(f"RAM reserve (heap + stack): {reserve // 1024} KB")
    if reserve < RAM_RESERVE_MIN:
        log(f"RAM reserve below {RAM_RESERVE_MIN // 1024} KB - shrink code or buffers (QUIRK S6)")
        return 1
    return 0


def build_loctest() -> int:
    """The core self-test as its own Agon program (QUIRK S6): src/core,
    tests/selftest.c and tests/loctest_main.c, built in build/loctest."""
    root = env.LOCTEST_DIR
    src = root / "src"
    if src.exists():
        shutil.rmtree(src)
    shutil.copytree(env.ROOT / "src" / "core", src / "core")
    for name, dest in (("selftest.c", "selftest.c"), ("selftest.h", "selftest.h"),
                       ("loctest_main.c", "main.c")):
        shutil.copy2(env.ROOT / "tests" / name, src / dest)
    for name in ("emu.asm", "emu.h"):
        shutil.copy2(env.ROOT / "src" / "agon" / name, src / name)
    shutil.copy2(env.ROOT / "tests" / "loctest.mk", root / "Makefile")
    env.run_linux(["make", "clean"], cwd=root, check=False, capture_output=True)
    r = env.run_linux(["make"], cwd=root, check=False, capture_output=True)
    if r.returncode != 0:
        print(r.stdout.decode(errors="replace"), r.stderr.decode(errors="replace"))
        log("loctest build FAILED")
        return 1
    log(f"selftest binary: {env.LOCTEST_BIN.relative_to(env.ROOT).as_posix()} "
        f"({env.LOCTEST_BIN.stat().st_size} bytes)")
    return 0


def build_spike(clean: bool = True) -> int:
    """The VDP feature spike is its own Agon program (ADR 0012): it must not
    eat the game's RAM, but it belongs on the SD card for hardware runs."""
    if not env.SPIKE_DIR.exists():
        return 0
    if clean:
        env.run_linux(["make", "clean"], cwd=env.SPIKE_DIR, check=False, capture_output=True)
    r = env.run_linux(["make"], cwd=env.SPIKE_DIR, check=False, capture_output=True)
    if r.returncode != 0:
        print(r.stdout.decode(errors="replace"), r.stderr.decode(errors="replace"))
        log("vdptest build FAILED")
        return 1
    log(f"spike binary: {env.SPIKE_BIN.relative_to(env.ROOT).as_posix()} "
        f"({env.SPIKE_BIN.stat().st_size} bytes)")
    return 0


def build_host() -> int:
    HOST_BIN.parent.mkdir(parents=True, exist_ok=True)
    sources = sorted(str(p.relative_to(env.ROOT).as_posix())
                     for p in (env.ROOT / "src" / "core").rglob("*.c"))
    sources += ["tests/selftest.c", "host/main.c"]
    cmd = ["gcc", *HOST_CFLAGS, "-o", HOST_BIN.relative_to(env.ROOT).as_posix(), *sources]
    r = env.run_linux(cmd, cwd=env.ROOT, check=False)
    if r.returncode != 0:
        log("host build FAILED")
        return 1
    log(f"host binary: {HOST_BIN.relative_to(env.ROOT).as_posix()}")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Build loc.bin and/or the host build")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--host", action="store_true", help="host build only")
    g.add_argument("--all", action="store_true", help="Agon and host build")
    ap.add_argument("--incremental", action="store_true",
                    help="skip make clean (faster, but headers are not tracked)")
    args = ap.parse_args()

    if generate() != 0:
        return 1
    rc = 0
    if not args.host:
        rc |= build_agon(clean=not args.incremental)
    if args.host or args.all:
        rc |= build_host()
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
