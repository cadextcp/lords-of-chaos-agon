#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow"]
# ///
"""
Screenshot of the "Fab Agon Emulator" window (Windows) -> build/screenshots/.
(Adapted from AgonPipeline/tools/screenshot_emulator.py.)

    uv run tools/screenshot.py              # build/screenshots/emulator-<time>.png
    uv run tools/screenshot.py -o out.png
"""

import argparse
import ctypes
import ctypes.wintypes
import sys
import time
from pathlib import Path

from PIL import ImageGrab

ROOT = Path(__file__).resolve().parent.parent
WINDOW_TITLE = "Fab Agon Emulator"
user32 = ctypes.windll.user32


def find_window(prefix: str):
    found = []
    proc = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)

    def cb(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            n = user32.GetWindowTextLengthW(hwnd)
            buf = ctypes.create_unicode_buffer(n + 1)
            user32.GetWindowTextW(hwnd, buf, n + 1)
            if buf.value.startswith(prefix):
                found.append(hwnd)
                return False
        return True

    user32.EnumWindows(proc(cb), 0)
    return found[0] if found else None


def main() -> int:
    ap = argparse.ArgumentParser(description="Screenshot the emulator window")
    ap.add_argument("-o", "--output", type=Path, help="output PNG path")
    args = ap.parse_args()

    hwnd = find_window(WINDOW_TITLE)
    if hwnd is None:
        print("emulator window not found", file=sys.stderr)
        return 2
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)
    rect = ctypes.wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    img = ImageGrab.grab((rect.left, rect.top, rect.right, rect.bottom))

    out = args.output or ROOT / "build" / "screenshots" / time.strftime("emulator-%Y%m%d-%H%M%S.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    img.save(out)
    print(f"[screenshot] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
