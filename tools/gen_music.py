#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Compile data/music/<name>.txt into title/menu music (M5d, ADR 0011).

    uv run tools/gen_music.py

Outputs (generated, not committed):
  build/music/<name>.bin   loaded from /loc/music on the SD card

.bin format (little endian):
  "LOCM" | u8 version=1 | u16 ms_per_unit | u8 channel_count
  per channel: u8 waveform (0-5, vdp.h order) | u8 volume | u16 note_count
               + notes: u16 frequency (0 = rest), u8 units

Note names: a4, gs4, cb5, r (rest). A unit is one eighth note; the tempo
line sets the quarter-notes-per-minute, ms_per_unit = 30000 / tempo * ...:
one eighth = quarter / 2 -> ms = 60000 / bpm / 2 * ... = 30000 / bpm.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "data" / "music"
OUT_DIR = ROOT / "build" / "music"

WAVEFORMS = {"square": 0, "triangle": 1, "sawtooth": 2, "sine": 3,
             "noise": 4, "vicnoise": 5}
BASE = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def midi(name: str) -> int:
    """a4 -> 69, gs4 -> 68, eb5 -> 75 ... ('s' or '#' = sharp, 'b' = flat)"""
    if name == "r":
        return -1
    semis = BASE[name[0]]
    i = 1
    while i < len(name) and name[i] in "s#b":
        semis += 1 if name[i] in "s#" else -1
        i += 1
    octave = int(name[i:])
    return 12 * (octave + 1) + semis


def frequency(name: str) -> int:
    m = midi(name)
    if m < 0:
        return 0
    return round(440.0 * 2 ** ((m - 69) / 12))


def parse(path: Path) -> tuple[int, list[list[tuple[int, int]]], list[int], list[int]]:
    tempo = 120
    channels: list[list[tuple[int, int]]] = []
    waves: list[int] = []
    vols: list[int] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#")[0].strip()
        if not line:
            continue
        parts = line.replace(":", " : ").split()
        if parts[0] == "tempo":
            tempo = int(parts[1])
        elif parts[0] == "channel":
            if len(parts) < 2 or parts[1] not in WAVEFORMS:
                raise SystemExit(f"{path.name}: waveform expected: {line!r}")
            channels.append([])
            waves.append(WAVEFORMS[parts[1]])
            vols.append(int(parts[2]) if len(parts) > 2 else 64)
        elif parts[0] == "vol":
            if vols:
                vols[-1] = int(parts[1])
        else:
            if not channels:
                raise SystemExit(f"{path.name}: note before any channel")
            i = 0
            while i < len(parts) - 2:
                if parts[i + 1] != ":":
                    raise SystemExit(f"{path.name}: expected name:dur: {line!r}")
                channels[-1].append((frequency(parts[i]), int(parts[i + 2])))
                i += 3
    ms_per_unit = round(30000 / tempo)
    return ms_per_unit, channels, waves, vols


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for path in sorted(SRC.glob("*.txt")):
        ms, channels, waves, vols = parse(path)
        if not channels or len(channels) > 3:
            raise SystemExit(f"{path.name}: 1..3 channels, got {len(channels)}")
        out = bytearray(b"LOCM")
        out += bytes((1,))
        out += ms.to_bytes(2, "little")
        out.append(len(channels))
        for ch, wave, vol in zip(channels, waves, vols):
            if not ch:
                raise SystemExit(f"{path.name}: empty channel")
            out += bytes((wave, vol))
            out += len(ch).to_bytes(2, "little")
            for freq, units in ch:
                if not 1 <= units <= 16:
                    raise SystemExit(f"{path.name}: duration 1..16 units")
                out += freq.to_bytes(2, "little")
                out.append(units)
        (OUT_DIR / (path.stem + ".bin")).write_bytes(bytes(out))
        print(f"[music] {path.name} -> build/music/{path.stem}.bin "
              f"({len(channels)} channels, {ms} ms/unit, {len(out)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
