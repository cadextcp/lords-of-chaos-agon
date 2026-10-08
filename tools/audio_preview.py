#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Render the compiled music (build/music/*.bin) to WAV files, using the same
samples (tools/gen_sfx.py) and timing rules as src/agon/music.c - so the
composition can be judged on the PC. An approximation of the VDP: simple
waveforms, linear ADSR, no VDP mixer quirks.

    uv run tools/audio_preview.py            # all songs
    uv run tools/audio_preview.py title      # one song

Output: build/sfx/preview/music_<name>.wav (the effect samples are written
by gen_sfx.py next to it).
"""

from __future__ import annotations

import math
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_sfx  # noqa: E402

SR = gen_sfx.SR
MUSIC = ROOT / "build" / "music"
OUT = ROOT / "build" / "sfx" / "preview"
TAIL_MS = 30
LOOPS = 2                                   # looping songs: play twice


def wave_sample(kind: int, phase: float, rng) -> float:
    p = phase % 1.0
    if kind == 0:
        return 1.0 if p < 0.5 else -1.0
    if kind == 1:
        return 4 * abs(p - 0.5) - 1
    if kind == 2:
        return 2 * p - 1
    if kind == 3:
        return math.sin(2 * math.pi * p)
    return rng.uniform(-1, 1)


def render(path: Path, samples) -> list[float]:
    data = path.read_bytes()
    if data[:4] != b"LOCM" or data[4] != 2:
        raise SystemExit(f"{path.name}: not LOCM v2")
    ms_unit = data[5] | data[6] << 8
    nch, flags = data[7], data[8]
    off = 9
    tracks = []
    for _ in range(nch):
        instr, fb, vol, a, d, s, r = data[off:off + 7]
        count = data[off + 7] | data[off + 8] << 8
        off += 9
        notes = []
        for _ in range(count):
            f = data[off] | data[off + 1] << 8
            notes.append((f, data[off + 2]))
            off += 3
        tracks.append((instr, fb, vol, (a * 4, d * 4, s, r * 4), notes))
    reps = LOOPS if flags & 1 else 1
    total_ms = max(sum(u for _, u in t[4]) for t in tracks) * ms_unit * reps
    out = [0.0] * (total_ms * SR // 1000 + SR)
    import random
    rng = random.Random(1)
    for instr, fb, vol, (a, d, s, r), notes in tracks:
        t_ms = 0
        smp = None
        if instr & 0x80:
            name, pcm, tunable, base = samples[instr & 0x7F]
            smp = ([(b - 256 if b > 127 else b) / 127 for b in pcm], base or 0)
        for _ in range(reps):
            for freq, units in notes:
                dur = units * ms_unit
                play = dur - TAIL_MS if dur > 2 * TAIL_MS else dur // 2
                start = t_ms * SR // 1000
                n = play * SR // 1000
                g = vol / 127
                if freq and smp:
                    pcm, base = smp
                    step = freq / base if base else 1.0
                    pos = 0.0
                    for i in range(n):
                        if pos >= len(pcm) - 1:
                            break
                        out[start + i] += g * pcm[int(pos)]
                        pos += step
                elif freq:
                    ph = 0.0
                    rel_n = r * SR // 1000
                    for i in range(n + rel_n):
                        tm = i * 1000 / SR
                        if i >= n:                        # release
                            e = (s / 255) * (1 - (i - n) / max(1, rel_n))
                        elif a and tm < a:
                            e = tm / a
                        elif d and tm < a + d:
                            e = 1 - (1 - s / 255) * (tm - a) / d
                        else:
                            e = s / 255 if (a or d or r) else 1.0
                        ph += freq / SR
                        if start + i < len(out):
                            out[start + i] += g * e * wave_sample(fb if instr & 0x80 else instr, ph, rng)
                t_ms += dur
    peak = max(1e-9, max(abs(v) for v in out))
    return [v * 0.9 / peak for v in out]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    samples = gen_sfx.build()
    names = sys.argv[1:] or [p.stem for p in sorted(MUSIC.glob("*.bin"))]
    for name in names:
        x = render(MUSIC / f"{name}.bin", samples)
        dest = OUT / f"music_{name}.wav"
        with wave.open(str(dest), "wb") as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(SR)
            w.writeframes(b"".join(struct.pack("<h", round(v * 32767)) for v in x))
        print(f"[preview] {dest.relative_to(ROOT).as_posix()} ({len(x) / SR:.1f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
