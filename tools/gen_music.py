#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Compile data/music/<name>.txt into menu music and jingles (ADR 0011/0012).

    uv run tools/gen_music.py

Outputs (generated, not committed):
  build/music/<name>.bin   loaded from /loc/music on the SD card

Text format:
  tempo <quarter notes per minute>
  loop                                  the song repeats (else: plays once)
  channel <instrument> [volume]         instrument: square|triangle|sawtooth|
                                        sine|noise|vicnoise or a sample
                                        instrument from gen_sfx.py (pluck, drum)
  fallback <waveform>                   waveform when the samples are missing
  env <attack ms> <decay ms> <sustain 0-255> <release ms>   waveform envelope
  <note>:<eighths> ...                  a4, gs4, eb5, r = rest

.bin format v2 (little endian):
  "LOCM" | u8 version=2 | u16 ms_per_unit | u8 channel_count | u8 flags (1 = loop)
  per channel: u8 instrument (waveform 0-5, or 0x80 | sample id)
               | u8 fallback waveform | u8 volume
               | u8 attack/4 | u8 decay/4 | u8 sustain | u8 release/4
               | u16 note_count + notes: u16 frequency (0 = rest), u8 units
A unit is one eighth note: ms_per_unit = 30000 / tempo. Looping songs must
have the same length on every channel, or the voices drift apart.
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from gen_sfx import INSTRUMENTS  # noqa: E402

SRC = ROOT / "data" / "music"
OUT_DIR = ROOT / "build" / "music"

WAVEFORMS = {"square": 0, "triangle": 1, "sawtooth": 2, "sine": 3,
             "noise": 4, "vicnoise": 5}
BASE = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}
MAX_CHANNELS = 4
MAX_NOTES = 160


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


class Channel:
    def __init__(self, instr: int, vol: int):
        self.instr = instr
        self.fallback = WAVEFORMS["triangle"]
        self.vol = vol
        self.env = (0, 0, 255, 0)
        self.notes: list[tuple[int, int]] = []


def parse(path: Path) -> tuple[int, bool, list[Channel]]:
    tempo, loop = 120, False
    channels: list[Channel] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("#")[0].strip()
        if not line:
            continue
        parts = line.replace(":", " : ").split()
        key = parts[0]
        if key == "tempo":
            tempo = int(parts[1])
        elif key == "loop":
            loop = True
        elif key == "channel":
            name = parts[1] if len(parts) > 1 else ""
            if name in WAVEFORMS:
                instr = WAVEFORMS[name]
            elif name in INSTRUMENTS:
                instr = 0x80 | INSTRUMENTS[name]
            else:
                raise SystemExit(f"{path.name}: unknown instrument {name!r}")
            channels.append(Channel(instr, int(parts[2]) if len(parts) > 2 else 64))
        elif key == "fallback":
            channels[-1].fallback = WAVEFORMS[parts[1]]
        elif key == "env":
            a, d, s, r = (int(v) for v in parts[1:5])
            if not (0 <= s <= 255 and max(a, d, r) <= 1020):
                raise SystemExit(f"{path.name}: env out of range: {line!r}")
            channels[-1].env = (a, d, s, r)
        elif key == "vol":
            channels[-1].vol = int(parts[1])
        else:
            if not channels:
                raise SystemExit(f"{path.name}: note before any channel")
            i = 0
            while i < len(parts):
                if i + 2 >= len(parts) or parts[i + 1] != ":":
                    raise SystemExit(f"{path.name}: expected name:dur: {line!r}")
                channels[-1].notes.append((frequency(parts[i]), int(parts[i + 2])))
                i += 3
    return round(30000 / tempo), loop, channels


def compile_song(path: Path) -> bytes:
    ms, loop, channels = parse(path)
    if not 1 <= len(channels) <= MAX_CHANNELS:
        raise SystemExit(f"{path.name}: 1..{MAX_CHANNELS} channels, got {len(channels)}")
    lengths = {sum(u for _, u in ch.notes) for ch in channels}
    if loop and len(lengths) != 1:
        raise SystemExit(f"{path.name}: looping channels differ in length: {sorted(lengths)}")
    out = bytearray(b"LOCM")
    out += bytes((2,))
    out += ms.to_bytes(2, "little")
    out += bytes((len(channels), 1 if loop else 0))
    for ch in channels:
        if not 1 <= len(ch.notes) <= MAX_NOTES:
            raise SystemExit(f"{path.name}: 1..{MAX_NOTES} notes per channel")
        a, d, s, r = ch.env
        out += bytes((ch.instr, ch.fallback, ch.vol, a // 4, d // 4, s, r // 4))
        out += len(ch.notes).to_bytes(2, "little")
        for freq, units in ch.notes:
            if not 1 <= units <= 32:
                raise SystemExit(f"{path.name}: duration 1..32 units")
            out += freq.to_bytes(2, "little")
            out.append(units)
    return bytes(out)


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for path in sorted(SRC.glob("*.txt")):
        data = compile_song(path)
        (OUT_DIR / (path.stem + ".bin")).write_bytes(data)
        print(f"[music] {path.name} -> build/music/{path.stem}.bin ({len(data)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
