#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Build, stage and start the game in the GUI emulator.

    uv run tools/run.py                       # play; close the window to stop
    uv run tools/run.py --dump --time 6       # auto-quit after 6 s, print loc.log
    uv run tools/run.py --time 8 --free-round1 --keys "dddw" --screenshot
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


def write_autoexec(args: list[str], keyboard: int) -> None:
    cmd = " ".join(["loc", *args])
    # MOS wants LF line endings in autoexec.txt
    text = f"SET KEYBOARD {keyboard}\ncd /{env.GAME_DIR}\n{cmd}\n"
    (env.SDCARD / "autoexec.txt").write_bytes(text.encode())
    log(f"autoexec: SET KEYBOARD {keyboard} && cd /{env.GAME_DIR} && {cmd}")


def uv_tool(script: str, *args: str) -> None:
    subprocess.run(["uv", "run", str(env.ROOT / "tools" / script), *args], check=False)


def main() -> int:
    ap = argparse.ArgumentParser(description="Run the game in the GUI emulator")
    ap.add_argument("--no-build", action="store_true", help="skip building")
    ap.add_argument("--dump", action="store_true", help="game writes loc.log screen dumps")
    ap.add_argument("--bench", action="store_true", help="game measures redraw times -> loc.log")
    ap.add_argument("--keytest", action="store_true", help="keyboard spike: log every key event")
    ap.add_argument("--house", action="store_true", help="wizard house map (dev)")
    ap.add_argument("--testland", action="store_true", help="testland map (dev)")
    ap.add_argument("--no-menu", action="store_true",
                    help="skip the main menu (scripted runs)")
    ap.add_argument("--free-round1", action="store_true",
                    help="lift the round 1 movement lock (PM 7) for scripted runs")
    ap.add_argument("--fly", action="store_true",
                    help="p1 flyers start airborne (the ISO '<' key is not sendable, #3)")
    ap.add_argument("--keyboard", type=int, default=2,
                    help="MOS keyboard layout (SET KEYBOARD n), default 2 = German")
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
    mode = (["--dump"] if args.dump else ["--bench"] if args.bench
            else ["--keytest"] if args.keytest else [])
    if args.house:
        mode.append("--house")
    if args.testland:
        mode.append("--testland")
    if args.no_menu:
        # the game skips its menu in dump/bench mode anyway; for a plain
        # scripted run pass a marker the game accepts everywhere
        if not mode:
            mode.append("--dump")
    if args.free_round1:
        mode.append("--free-round1")
    if args.fly:
        mode.append("--fly")
    write_autoexec(mode, args.keyboard)

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
