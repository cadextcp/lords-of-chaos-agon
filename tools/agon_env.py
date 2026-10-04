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
SPIKE_DIR = ROOT / "spikes" / "vdptest"          # VDP feature spike (ADR 0012)
SPIKE_BIN = SPIKE_DIR / "bin" / "vdptest.bin"


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
    """Copy loc.bin, tiles.bin, maps, scenarios and help to /loc on the SD."""
    if not AGON_BIN.exists():
        raise FileNotFoundError(f"{AGON_BIN} missing - run: uv run tools/build.py")
    if not SDCARD.exists():
        raise FileNotFoundError(f"{SDCARD} missing - run: uv run tools/setup.py")
    dest = SDCARD / GAME_DIR
    dest.mkdir(exist_ok=True)
    shutil.copy2(AGON_BIN, dest / AGON_BIN.name)
    if SPIKE_BIN.exists():
        shutil.copy2(SPIKE_BIN, dest / SPIKE_BIN.name)
    tiles = BUILD / "tiles.bin"
    if tiles.exists():
        shutil.copy2(tiles, dest / tiles.name)
    maps = BUILD / "maps"
    if maps.exists():
        (dest / "maps").mkdir(exist_ok=True)
        for m in maps.glob("*.map"):
            shutil.copy2(m, dest / "maps" / m.name)
    scenarios = BUILD / "scenarios"
    if scenarios.exists():
        (dest / "scenarios").mkdir(exist_ok=True)
        for f in scenarios.glob("*.scn"):
            shutil.copy2(f, dest / "scenarios" / f.name)
    helpdir = BUILD / "help"
    if helpdir.exists():
        (dest / "help").mkdir(exist_ok=True)
        for f in helpdir.glob("*.hlp"):
            shutil.copy2(f, dest / "help" / f.name)
    fonts = BUILD / "fonts"
    if fonts.exists():
        (dest / "fonts").mkdir(exist_ok=True)
        for f in fonts.glob("*.fnt"):
            shutil.copy2(f, dest / "fonts" / f.name)
    sfx = BUILD / "sfx" / "sfx.bin"
    if sfx.exists():
        shutil.copy2(sfx, dest / sfx.name)
    for name in ("title.bin", "win.bin", "lose.bin"):
        pic = BUILD / name
        if pic.exists():
            shutil.copy2(pic, dest / pic.name)
    music = BUILD / "music"
    if music.exists():
        (dest / "music").mkdir(exist_ok=True)
        for f in music.glob("*.bin"):
            shutil.copy2(f, dest / "music" / f.name)
    return dest
