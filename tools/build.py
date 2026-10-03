#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Build the game.

    uv run tools/build.py            # Agon binary  -> bin/loc.bin
    uv run tools/build.py --host     # host build   -> build/host/loc_host
    uv run tools/build.py --all      # both
    uv run tools/build.py --incremental  # skip make clean (headers not tracked!)

The Agon build runs agondev's make (inside WSL on Windows). The host build
compiles the platform-free core plus host/ with the system gcc (also WSL on
Windows), so the same code can be tested without the emulator.
"""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import agon_env as env  # noqa: E402

HOST_BIN = env.BUILD / "host" / "loc_host"
HOST_CFLAGS = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-O1", "-g"]


def log(msg: str) -> None:
    print(f"[build] {msg}", flush=True)


def generate() -> int:
    """Tile bank + generated C sources (src/core/gen/) from assets and data."""
    for script in ("build_tiles.py", "gen_data.py", "gen_maps.py", "gen_scenarios.py"):
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
    return 0


def build_host() -> int:
    HOST_BIN.parent.mkdir(parents=True, exist_ok=True)
    sources = sorted(str(p.relative_to(env.ROOT).as_posix())
                     for p in (env.ROOT / "src" / "core").rglob("*.c"))
    sources.append("host/main.c")
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
