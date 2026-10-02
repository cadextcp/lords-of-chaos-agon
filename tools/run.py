#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Build, stage and start the game in the GUI emulator.

    uv run tools/run.py                       # play; close the window to stop
    uv run tools/run.py --dump --time 6       # auto-quit after 6 s, print loc.log
    uv run tools/run.py --time 8 --keys "dddw" --screenshot
                                              # scripted session: keys, then screenshot

autoexec.txt on the staged SD card starts the game, so it runs right after boot.
With --dump the game writes its screen grid to /loc/loc.log after every frame
(screenshot-free inspection). --keys and --screenshot use send_keys.py and
screenshot.py (Windows only).
"""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import agon_env as env  # noqa: E402
import build  # noqa: E402

BOOT_SECONDS = 4.0


def log(msg: str) -> None:
    print(f"[run] {msg}", flush=True)


def write_autoexec(args: list[str]) -> None:
    cmd = " ".join(["loc", *args])
    # MOS wants LF line endings in autoexec.txt
    (env.SDCARD / "autoexec.txt").write_bytes(f"cd /{env.GAME_DIR}\n{cmd}\n".encode())
    log(f"autoexec: cd /{env.GAME_DIR} && {cmd}")


def uv_tool(script: str, *args: str) -> None:
    subprocess.run(["uv", "run", str(env.ROOT / "tools" / script), *args], check=False)


def main() -> int:
    ap = argparse.ArgumentParser(description="Run the game in the GUI emulator")
    ap.add_argument("--no-build", action="store_true", help="skip building")
    ap.add_argument("--dump", action="store_true", help="game writes loc.log screen dumps")
    ap.add_argument("--bench", action="store_true", help="game measures redraw times -> loc.log")
    ap.add_argument("--time", type=float, help="quit the emulator after N seconds")
    ap.add_argument("--keys", help="keys to send after boot (see send_keys.py)")
    ap.add_argument("--list", action="store_true", help="--keys is a comma list of named keys")
    ap.add_argument("--screenshot", action="store_true", help="screenshot before quitting")
    args = ap.parse_args()

    if not env.GUI_EMULATOR.exists():
        log("emulator missing - run: uv run tools/setup.py")
        return 3
    if not args.no_build and (build.generate() != 0 or build.build_agon() != 0):
        return 1
    env.stage_game()
    logfile = env.SDCARD / env.GAME_DIR / "loc.log"
    logfile.unlink(missing_ok=True)
    write_autoexec(["--dump"] if args.dump else ["--bench"] if args.bench else [])

    cmd = [str(env.GUI_EMULATOR), "--sdcard", str(env.SDCARD.resolve()),
           "--firmware", env.FIRMWARE]
    log("starting " + " ".join(cmd))
    proc = subprocess.Popen(cmd, cwd=env.EMU_DIR)

    if args.time is None and not args.keys and not args.screenshot:
        return proc.wait()

    start = time.monotonic()
    if args.keys:
        time.sleep(BOOT_SECONDS)
        uv_tool("send_keys.py", *(["--list"] if args.list else []), args.keys)
    if args.time is not None:
        time.sleep(max(0.0, args.time - (time.monotonic() - start)))
    if args.screenshot:
        uv_tool("screenshot.py")
    proc.terminate()
    proc.wait(timeout=10)
    log(f"emulator stopped after {time.monotonic() - start:.1f}s")

    if logfile.exists():
        print(f"--- {logfile.relative_to(env.ROOT).as_posix()} ---")
        print(logfile.read_text(errors="replace"))
    else:
        log("no loc.log written (start with --dump)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
