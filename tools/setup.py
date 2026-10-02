#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
One-time setup: download pinned toolchain + emulator, stage the SD card.

  * fab-agon-emulator  -> emulator/<version>/   (GUI + headless CLI)
  * agondev toolchain  -> toolchain/agondev/    (Linux binaries; on Windows
                                                 they run inside WSL)
  * SD card template   -> sdcard/staged/        (copied from the emulator bundle)

Every download is verified against a pinned SHA-256.

Usage:
    uv run tools/setup.py            # idempotent
    uv run tools/setup.py --force    # re-extract everything

Exit codes: 0 ok, 1 download/extract failed, 2 hash mismatch, 3 unsupported platform
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import sys
import tarfile
import urllib.request
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import agon_env as env  # noqa: E402

# ---------------------------------------------------------------------------
# Pinned versions. Bump deliberately (see docs/adr/0001-toolchain.md).
# ---------------------------------------------------------------------------
EMU_BASE = f"https://github.com/tomm/fab-agon-emulator/releases/download/{env.EMU_VERSION}"
EMU_ASSETS = {
    "windows": (
        f"fab-agon-emulator-v{env.EMU_VERSION}-windows-x64.zip",
        "f99d53ee490f48a1e5bfdb7e94c0f44edd9487036970e4f1fa1b276fd61c666f",
    ),
    "linux": (
        f"fab-agon-emulator-v{env.EMU_VERSION}-debian13-x86_64.tar.bz2",
        "9308da450c9e6532ec4070342d859bacb3996f14a9c31327e49e857c3ce32bbe",
    ),
}
AGONDEV_ASSET = (
    "agondev-linux_x86_64.tar.gz",
    "99d7094094210d02e29cbc6ceb2ab2b358f7bce7fd720005928bb28caabd4856",
)
AGONDEV_URL = (
    f"https://github.com/AgonPlatform/agondev/releases/download/"
    f"{env.AGONDEV_VERSION}/{AGONDEV_ASSET[0]}"
)

DOWNLOADS = env.ROOT / ".cache" / "downloads"


def log(msg: str) -> None:
    print(f"[setup] {msg}", flush=True)


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fp:
        for chunk in iter(lambda: fp.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path, sha: str) -> None:
    if dest.exists() and sha256_file(dest) == sha:
        log(f"cached + verified: {dest.name}")
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    log(f"downloading {url}")
    req = urllib.request.Request(url, headers={"User-Agent": "loc-agon-setup"})
    with urllib.request.urlopen(req) as resp, dest.open("wb") as fp:
        shutil.copyfileobj(resp, fp)
    actual = sha256_file(dest)
    if actual != sha:
        dest.unlink()
        log(f"SHA-256 mismatch for {dest.name}: expected {sha}, got {actual}")
        raise SystemExit(2)
    log(f"SHA-256 ok: {dest.name}")


def extract_python(archive: Path, target: Path) -> None:
    """Extract with Python (fine for the emulator bundle)."""
    target.mkdir(parents=True, exist_ok=True)
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(target)
    else:
        with tarfile.open(archive) as tf:
            tf.extractall(target, filter="tar")


def setup_emulator(force: bool) -> None:
    name, sha = EMU_ASSETS["windows" if env.IS_WINDOWS else "linux"]
    archive = DOWNLOADS / name
    download(f"{EMU_BASE}/{name}", archive, sha)
    if env.EMU_DIR.exists() and not force:
        log(f"emulator already extracted: {env.EMU_DIR}")
        return
    if env.EMU_DIR.exists():
        shutil.rmtree(env.EMU_DIR)
    tmp = env.EMU_DIR.with_name(env.EMU_DIR.name + ".tmp")
    if tmp.exists():
        shutil.rmtree(tmp)
    extract_python(archive, tmp)
    # Bundles contain a single top-level folder; flatten it.
    entries = list(tmp.iterdir())
    inner = entries[0] if len(entries) == 1 and entries[0].is_dir() else tmp
    shutil.move(str(inner), str(env.EMU_DIR))
    if tmp.exists():
        shutil.rmtree(tmp)
    log(f"emulator -> {env.EMU_DIR}")


def setup_agondev(force: bool) -> None:
    archive = DOWNLOADS / AGONDEV_ASSET[0]
    download(AGONDEV_URL, archive, AGONDEV_ASSET[1])
    marker = env.AGONDEV_DIR / ".version"
    if marker.exists() and marker.read_text().strip() == env.AGONDEV_VERSION and not force:
        log(f"agondev {env.AGONDEV_VERSION} already extracted")
        return
    if env.AGONDEV_DIR.exists():
        shutil.rmtree(env.AGONDEV_DIR)
    env.AGONDEV_DIR.mkdir(parents=True)
    # Extract with the Linux tar so exec bits and symlinks survive (WSL on Windows).
    env.run_linux(
        ["tar", "-xzf", env.to_linux_path(archive), "-C",
         env.to_linux_path(env.AGONDEV_DIR), "--strip-components=2"],
    )
    marker.write_text(env.AGONDEV_VERSION + "\n")
    log(f"agondev -> {env.AGONDEV_DIR}")


def stage_sdcard(force: bool) -> None:
    src = env.EMU_DIR / "sdcard"
    if not src.exists():
        log(f"no sdcard/ in emulator bundle ({src}); skipping")
        return
    if env.SDCARD.exists() and force:
        shutil.rmtree(env.SDCARD)
    shutil.copytree(src, env.SDCARD, dirs_exist_ok=True)
    log(f"sd card template -> {env.SDCARD}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--force", action="store_true", help="re-extract everything")
    ap.add_argument("--no-emulator", action="store_true", help="toolchain only")
    ap.add_argument("--no-toolchain", action="store_true", help="emulator only")
    args = ap.parse_args()

    if not (env.IS_WINDOWS or env.IS_LINUX):
        log("unsupported platform (Windows x64 or Linux x86_64 only)")
        return 3
    try:
        if not args.no_emulator:
            setup_emulator(args.force)
            stage_sdcard(args.force)
        if not args.no_toolchain:
            setup_agondev(args.force)
    except (OSError, RuntimeError) as e:
        log(f"failed: {e}")
        return 1
    log("done. next: uv run tools/build.py && uv run tools/test.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
