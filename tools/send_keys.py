#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pydirectinput", "pygetwindow"]
# ///
"""
Inject keystrokes into the running fab-agon-emulator GUI window (Windows).

Plain SendKeys does NOT reach the SDL window; pydirectinput sends scancodes
via SendInput, which the emulator receives like real keyboard input.
(Taken over from AgonPipeline/tools/send_keys.py.)

    uv run tools/send_keys.py ddwe                 # each char = one key press
    uv run tools/send_keys.py --list right,down,esc

Exit code 0 = keys sent, 2 = emulator window not found.
"""

import argparse
import sys
import time

import pydirectinput
import pygetwindow as gw

WINDOW_TITLE = "Fab Agon Emulator"


def main() -> int:
    ap = argparse.ArgumentParser(description="Send keys to the Agon emulator window")
    ap.add_argument("keys", help="key string, or comma-separated names with --list")
    ap.add_argument("--list", action="store_true", help="KEYS is a comma list (right,down,esc)")
    ap.add_argument("--delay", type=float, default=0.3, help="seconds between presses")
    ap.add_argument("--settle", type=float, default=0.8, help="seconds after focusing")
    args = ap.parse_args()

    wins = [w for w in gw.getWindowsWithTitle(WINDOW_TITLE) if w.title]
    if not wins:
        print("emulator window not found", file=sys.stderr)
        return 2
    wins[0].activate()
    time.sleep(args.settle)

    keys = args.keys.split(",") if args.list else list(args.keys)
    pydirectinput.PAUSE = 0.1
    for k in keys:
        pydirectinput.press(k)
        time.sleep(args.delay)
    print(f"[keys] sent: {keys}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
