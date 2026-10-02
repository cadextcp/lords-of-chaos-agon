"""
Shared paths, pinned versions and platform helpers for all tools/*.py.

The agondev toolchain only ships Linux binaries. On Windows every toolchain
call is routed through WSL (distro from $LOC_WSL_DISTRO, default "Ubuntu");
on Linux (CI) it runs natively. The emulator runs natively on both.
"""

from __future__ import annotations

import os
import platform
import shutil
import subprocess
from pathlib import Path, PureWindowsPath

ROOT = Path(__file__).resolve().parent.parent

EMU_VERSION = "1.2.5"
AGONDEV_VERSION = "v0.22"

IS_WINDOWS = platform.system() == "Windows"
IS_LINUX = platform.system() == "Linux"

EMU_DIR = ROOT / "emulator" / EMU_VERSION
AGONDEV_DIR = ROOT / "toolchain" / "agondev"
SDCARD = ROOT / "sdcard" / "staged"
BUILD = ROOT / "build"

# Directory on the SD card the game is installed to (MOS path, no drive).
GAME_DIR = "loc"
WSL_DISTRO = os.environ.get("LOC_WSL_DISTRO", "Ubuntu")
# Same MOS for GUI and headless runs (the CLI emulator always uses Console8
# MOS 2.3.3; the GUI defaults to "platform" MOS 3.x unless pinned).
FIRMWARE = "console8"
AGON_BIN = ROOT / "bin" / "loc.bin"


def exe(name: str) -> Path:
    return EMU_DIR / (name + (".exe" if IS_WINDOWS else ""))


GUI_EMULATOR = exe("fab-agon-emulator")
CLI_EMULATOR = exe("agon-cli-emulator")


def to_linux_path(p: Path | str) -> str:
    """C:\\a\\b -> /mnt/c/a/b on Windows; identity on Linux."""
    if not IS_WINDOWS:
        return str(p)
    w = PureWindowsPath(Path(p).resolve())
    drive = w.drive.rstrip(":").lower()
    return "/mnt/" + drive + "/" + "/".join(w.parts[1:])


def linux_cmd(cmd: list[str], cwd: Path | None = None) -> list[str]:
    """Wrap a Linux command line so it runs with agondev on PATH."""
    path = f"{to_linux_path(AGONDEV_DIR / 'bin')}:/usr/local/bin:/usr/bin:/bin"
    inner = ["env", f"PATH={path}", *cmd]
    if not IS_WINDOWS:
        return inner
    wsl = ["wsl.exe", "-d", WSL_DISTRO]
    if cwd is not None:
        wsl += ["--cd", to_linux_path(cwd)]
    return [*wsl, "--", *inner]


def run_linux(cmd: list[str], cwd: Path | None = None, check: bool = True,
              **kw) -> subprocess.CompletedProcess:
    full = linux_cmd(cmd, cwd)
    return subprocess.run(full, cwd=None if IS_WINDOWS else cwd, check=check, **kw)


def stage_game() -> Path:
    """Copy bin/loc.bin and build/tiles.bin to /loc on the staged SD card."""
    if not AGON_BIN.exists():
        raise FileNotFoundError(f"{AGON_BIN} missing - run: uv run tools/build.py")
    if not SDCARD.exists():
        raise FileNotFoundError(f"{SDCARD} missing - run: uv run tools/setup.py")
    dest = SDCARD / GAME_DIR
    dest.mkdir(exist_ok=True)
    shutil.copy2(AGON_BIN, dest / AGON_BIN.name)
    tiles = BUILD / "tiles.bin"
    if tiles.exists():
        shutil.copy2(tiles, dest / tiles.name)
    return dest
