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

PrintWindow with PW_RENDERFULLCONTENT reads the window directly, so an
occluded or unfocused emulator is captured as-is - the foreground-lock
denied SetForegroundWindow and the grab got whatever covered the window
(a browser, once). Falls back to the old foreground grab if PrintWindow
yields nothing.
"""

import argparse
import ctypes
import ctypes.wintypes
import sys
import time
from pathlib import Path

from PIL import Image, ImageGrab

ROOT = Path(__file__).resolve().parent.parent
WINDOW_TITLE = "Fab Agon Emulator"
user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
PW_RENDERFULLCONTENT = 0x2


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


def grab_window(hwnd) -> Image.Image | None:
    rect = ctypes.wintypes.RECT()
    user32.GetWindowRect(hwnd, ctypes.byref(rect))
    w, h = rect.right - rect.left, rect.bottom - rect.top
    if w <= 0 or h <= 0:
        return None
    hdc = user32.GetWindowDC(hwnd)
    mem = gdi32.CreateCompatibleDC(hdc)
    bmp = gdi32.CreateCompatibleBitmap(hdc, w, h)
    gdi32.SelectObject(mem, bmp)
    # PrintWindow wants a client pointer for the flag in some builds; 64-bit
    # needs the result checked, 0 means it refused (older Windows).
    ok = user32.PrintWindow(hwnd, mem, PW_RENDERFULLCONTENT)
    img = None
    if ok:
        class BMIHEADER(ctypes.Structure):
            _fields_ = [("biSize", ctypes.c_uint32), ("biWidth", ctypes.c_int32),
                        ("biHeight", ctypes.c_int32), ("biPlanes", ctypes.c_uint16),
                        ("biBitCount", ctypes.c_uint16), ("biCompression", ctypes.c_uint32),
                        ("biSizeImage", ctypes.c_uint32), ("biXPels", ctypes.c_int32),
                        ("biYPels", ctypes.c_int32), ("biClrUsed", ctypes.c_uint32),
                        ("biClrImportant", ctypes.c_uint32)]

        bi = BMIHEADER(ctypes.sizeof(BMIHEADER), w, -h, 1, 32, 0, 0, 0, 0, 0, 0)
        buf = ctypes.create_string_buffer(w * h * 4)
        if gdi32.GetDIBits(mem, bmp, 0, h, buf, ctypes.byref(bi), 0):
            img = Image.frombuffer("RGBA", (w, h), buf.raw, "raw", "BGRA", 0, 1)
    gdi32.DeleteObject(bmp)
    gdi32.DeleteDC(mem)
    user32.ReleaseDC(hwnd, hdc)
    if img is not None and img.getextrema() == ((0, 0), (0, 0), (0, 0), (255, 255)):
        return None          # all black: renderer did not paint into it
    return img


def main() -> int:
    ap = argparse.ArgumentParser(description="Screenshot the emulator window")
    ap.add_argument("-o", "--output", type=Path, help="output PNG path")
    args = ap.parse_args()

    hwnd = find_window(WINDOW_TITLE)
    if hwnd is None:
        print("emulator window not found", file=sys.stderr)
        return 2
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    img = grab_window(hwnd)
    if img is None:             # old path: raise it, grab the screen region
        user32.SetForegroundWindow(hwnd)
        time.sleep(0.5)
        rect = ctypes.wintypes.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(rect))
        img = ImageGrab.grab((rect.left, rect.top, rect.right, rect.bottom))

    out = args.output or ROOT / "build" / "screenshots" / time.strftime("emulator-%Y%m%d-%H%M%S.png")
    out.parent.mkdir(parents=True, exist_ok=True)
    img.convert("RGB").save(out)
    print(f"[screenshot] {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
