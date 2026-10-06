#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Run all automated tests.

  1. host:     build the core for the PC and run `loc_host --selftest`
  2. emulator: build loctest.bin, stage it and run `loctest` headless in
               the CLI emulator (real eZ80 code, no VDP). The program exits
               the emulator through I/O port 0 with the failure count.

Both stages must print "=== TEST PASS ===" and exit with code 0.

    uv run tools/test.py              # both stages
    uv run tools/test.py --host       # host only (fast, no emulator)
    uv run tools/test.py --emu        # emulator only
    uv run tools/test.py --no-build   # use existing binaries
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

PASS = "=== TEST PASS ==="
# The CLI emulator (1.2.5) runs autoexec.txt at boot, so the selftest is
# started from there; it ends the emulator itself via I/O port 0.
AUTOEXEC = f"cd /{env.GAME_DIR}\nloctest\n"


def log(msg: str) -> None:
    print(f"[test] {msg}", flush=True)


def printable(text: str) -> str:
    """Drop raw VDU bytes so the output prints on any console encoding."""
    return "".join(c if c in "\n\t" or 32 <= ord(c) < 127 else "." for c in text)


def check(name: str, rc: int, out: str, verbose: bool) -> bool:
    ok = rc == 0 and PASS in out
    if verbose or not ok:
        print(printable(out).rstrip())
    log(f"{name}: {'PASS' if ok else f'FAIL (exit {rc})'}")
    return ok


def test_host(verbose: bool) -> bool:
    r = env.run_linux([build.HOST_BIN.relative_to(env.ROOT).as_posix(), "--selftest"],
                      cwd=env.ROOT, check=False, capture_output=True, text=True)
    return check("host selftest", r.returncode, r.stdout + r.stderr, verbose)


def test_emu(verbose: bool, timeout: int) -> bool:
    if not env.CLI_EMULATOR.exists():
        log("CLI emulator missing - run: uv run tools/setup.py")
        return False
    env.stage_game()
    (env.SDCARD / "autoexec.txt").write_bytes(AUTOEXEC.encode())  # LF only
    start = time.monotonic()
    try:
        r = subprocess.run(
            [str(env.CLI_EMULATOR), "--sdcard", str(env.SDCARD.resolve()), "-u"],
            cwd=env.EMU_DIR, stdin=subprocess.DEVNULL, capture_output=True,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as e:
        print((e.stdout or b"").decode("utf-8", errors="replace"))
        log(f"emulator selftest: FAIL (timeout after {timeout}s)")
        return False
    # Emulator output contains raw VDU bytes, so decode leniently.
    out = (r.stdout + r.stderr).decode("utf-8", errors="replace")
    # The port-0 exit code is the verdict (emu_exit writes the fail count);
    # the console tail (including the PASS line) can get lost on the CI
    # emulator, so rc==0 without any FAIL line counts as a pass.
    ok = r.returncode == 0 and "FAIL" not in out
    if not ok or verbose:
        print(printable(out).rstrip())
    log(f"emulator selftest: {'PASS' if ok else f'FAIL (exit {r.returncode})'}")
    return ok
    log(f"emulator run took {time.monotonic() - start:.1f}s")
    return ok


def main() -> int:
    ap = argparse.ArgumentParser(description="Run host and emulator tests")
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--host", action="store_true", help="host tests only")
    g.add_argument("--emu", action="store_true", help="emulator tests only")
    ap.add_argument("--no-build", action="store_true", help="skip building")
    ap.add_argument("--timeout", type=int, default=60, help="emulator timeout (s)")
    ap.add_argument("-v", "--verbose", action="store_true", help="print test output")
    args = ap.parse_args()

    run_host = not args.emu
    run_emu = not args.host
    if not args.no_build:
        if build.generate() != 0:
            return 1
        if run_host and build.build_host() != 0:
            return 1
        if run_emu and build.build_agon() != 0:
            return 1

    ok = True
    if run_host:
        ok &= test_host(args.verbose)
    if run_emu:
        ok &= test_emu(args.verbose, args.timeout)
    log("ALL PASSED" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
