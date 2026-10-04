#!/usr/bin/env python3
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
Synthesise the sound effect and instrument samples (polish round, ADR 0012).
Everything is generated from scratch here - no recordings, nothing copied.

    uv run tools/gen_sfx.py

Outputs (generated, not committed):
  build/sfx/sfx.bin         all samples, streamed to /loc/sfx.bin on the SD
  build/sfx/preview/*.wav   16-bit previews to listen to on the PC
  src/core/gen/sfx.h        SFX_* ids (sample order)

sfx.bin (little endian):
  "LOCX" | u8 version=1 | u8 count
  per sample: u8 flags (bit 0 = tunable instrument) | u16 base_hz
              | u16 length | length x int8 PCM at 16 kHz

The VDP plays 8-bit signed PCM at 16 kHz (QUIRK A6). Tunable samples are
instruments: play_note's frequency shifts their pitch relative to base_hz.
"""

from __future__ import annotations

import math
import random
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "build" / "sfx"
HEADER = ROOT / "src" / "core" / "gen" / "sfx.h"
SR = 16000


# ---------- building blocks ----------

def silence(sec: float) -> list[float]:
    return [0.0] * int(sec * SR)


def noise(n: int, rng: random.Random) -> list[float]:
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def lowpass(x: list[float], cutoff: float) -> list[float]:
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / SR)
    y, out = 0.0, []
    for v in x:
        y += a * (v - y)
        out.append(y)
    return out


def highpass(x: list[float], cutoff: float) -> list[float]:
    lp = lowpass(x, cutoff)
    return [a - b for a, b in zip(x, lp)]


def bandpass_sweep(x: list[float], f_of_t, q: float = 3.0) -> list[float]:
    """RBJ band-pass whose centre follows f_of_t(t in 0..1)."""
    out = []
    x1 = x2 = y1 = y2 = 0.0
    n = len(x)
    b0 = b2 = a1 = a2 = 0.0
    for i, v in enumerate(x):
        if i % 32 == 0:
            f = f_of_t(i / n)
            w = 2 * math.pi * f / SR
            alpha = math.sin(w) / (2 * q)
            a0 = 1 + alpha
            b0, b2 = alpha / a0, -alpha / a0
            a1, a2 = -2 * math.cos(w) / a0, (1 - alpha) / a0
        y = b0 * v + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, v
        y2, y1 = y1, y
        out.append(y)
    return out


def sweep(n: int, f0: float, f1: float, shape: str = "sine",
          exp: bool = True) -> list[float]:
    out, ph = [], 0.0
    for i in range(n):
        t = i / max(1, n - 1)
        f = f0 * (f1 / f0) ** t if exp else f0 + (f1 - f0) * t
        ph += f / SR
        p = ph % 1.0
        if shape == "sine":
            out.append(math.sin(2 * math.pi * p))
        elif shape == "square":
            out.append(1.0 if p < 0.5 else -1.0)
        elif shape == "saw":
            out.append(2 * p - 1)
        else:                                   # triangle
            out.append(4 * abs(p - 0.5) - 1)
    return out


def env_exp(n: int, tau: float, attack: float = 0.002) -> list[float]:
    na = max(1, int(attack * SR))
    return [(i / na if i < na else 1.0) * math.exp(-i / (tau * SR)) for i in range(n)]


def mul(a: list[float], b: list[float]) -> list[float]:
    return [x * y for x, y in zip(a, b)]


def mix(*parts: tuple[float, list[float]], offset: list[int] | None = None) -> list[float]:
    n = max(len(p) + (offset[i] if offset else 0) for i, (_, p) in enumerate(parts))
    out = [0.0] * n
    for i, (g, p) in enumerate(parts):
        o = offset[i] if offset else 0
        for k, v in enumerate(p):
            out[o + k] += g * v
    return out


def normalise(x: list[float], peak: float = 0.95) -> list[float]:
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade_out(x: list[float], sec: float = 0.01) -> list[float]:
    n = min(len(x), int(sec * SR))
    for i in range(n):
        x[len(x) - n + i] *= 1 - i / n
    return x


# ---------- the effects ----------

def s_step(r: random.Random) -> list[float]:
    n = int(0.08 * SR)
    thump = mul(sweep(n, 90, 50), env_exp(n, 0.025))
    scuff = mul(lowpass(noise(n, r), 900), env_exp(n, 0.015))
    return mix((1.0, thump), (0.6, scuff))


def s_whoosh(r: random.Random) -> list[float]:
    n = int(0.24 * SR)
    band = bandpass_sweep(noise(n, r), lambda t: 350 + 1700 * math.sin(math.pi * t) ** 2, 2.0)
    shape = [math.sin(math.pi * i / n) ** 2 for i in range(n)]
    return mul(band, shape)


def s_clang(r: random.Random) -> list[float]:
    n = int(0.45 * SR)
    partials = [(1180, 1.0, 0.30), (1725, 0.7, 0.22), (2390, 0.5, 0.18),
                (3110, 0.35, 0.12), (4020, 0.2, 0.08)]
    out = [0.0] * n
    for f, a, tau in partials:
        s = mul(sweep(n, f, f * 0.998), env_exp(n, tau))
        out = [o + a * v for o, v in zip(out, s)]
    hit = mul(highpass(noise(n, r), 2000), env_exp(n, 0.006))
    return mix((1.0, out), (0.8, hit))


def s_thud(r: random.Random) -> list[float]:
    n = int(0.2 * SR)
    body = mul(sweep(n, 170, 55), env_exp(n, 0.07))
    slap = mul(lowpass(noise(n, r), 1500), env_exp(n, 0.02))
    return mix((1.0, body), (0.7, slap))


def s_groan(r: random.Random) -> list[float]:
    n = int(0.65 * SR)
    out, ph = [], 0.0
    for i in range(n):
        t = i / n
        f = 170 * (80 / 170) ** t * (1 + 0.04 * math.sin(2 * math.pi * 6 * i / SR))
        ph += f / SR
        out.append(2 * (ph % 1.0) - 1)
    voice = lowpass(lowpass(out, 900), 1400)
    breath = lowpass(noise(n, r), 1200)
    shape = [min(1.0, i / (0.06 * SR)) * (1 - i / n) ** 1.5 for i in range(n)]
    return mul(mix((1.0, voice), (0.25, breath)), shape)


def s_thunder(r: random.Random) -> list[float]:
    n = int(0.85 * SR)
    brown, y = [], 0.0
    for v in noise(n, r):
        y = 0.98 * y + 0.2 * v
        brown.append(y)
    rumble = mul(lowpass(brown, 300), env_exp(n, 0.35, attack=0.02))
    crack = [0.0] * n
    t = 0
    for _ in range(6):
        t += int(r.uniform(0.01, 0.05) * SR)
        burst = mul(highpass(noise(int(0.06 * SR), r), 800), env_exp(int(0.06 * SR), 0.012))
        for k, v in enumerate(burst):
            if t + k < n:
                crack[t + k] += v * (1 - t / n)
    return mix((1.0, rumble), (0.9, crack))


def s_zap(r: random.Random) -> list[float]:
    n = int(0.28 * SR)
    tone = mul(sweep(n, 2200, 180, "square"), env_exp(n, 0.12))
    ring = sweep(n, 70, 70)
    fizz = mul(highpass(noise(n, r), 3000), env_exp(n, 0.05))
    return mix((0.8, [a * (0.6 + 0.4 * b) for a, b in zip(tone, ring)]), (0.3, fizz))


def s_creak(r: random.Random) -> list[float]:
    n = int(0.5 * SR)
    out, ph, f = [], 0.0, 95.0
    for i in range(n):
        f = max(70.0, min(140.0, f + r.uniform(-1.5, 1.6)))
        ph += f / SR
        stick = 0.5 + 0.5 * math.sin(2 * math.pi * 28 * i / SR) ** 8
        out.append((2 * (ph % 1.0) - 1) * stick)
    band = bandpass_sweep(out, lambda t: 700 + 300 * t, 1.5)
    shape = [math.sin(math.pi * i / n) ** 0.5 for i in range(n)]
    return mul(band, shape)


def s_lid(r: random.Random) -> list[float]:
    n = int(0.22 * SR)
    knock = mul(sweep(n, 420, 380), env_exp(n, 0.03))
    low = mul(sweep(n, 120, 60), env_exp(n, 0.05))
    click = mul(highpass(noise(n, r), 2500), env_exp(n, 0.004))
    second = [0.0] * int(0.07 * SR) + mul(sweep(n, 520, 500), env_exp(n, 0.02))
    return mix((0.8, knock), (0.8, low), (0.6, click), (0.4, second[:n]))


def s_sparkle(r: random.Random) -> list[float]:
    n = int(0.5 * SR)
    out = [0.0] * n
    for k in range(9):
        f = r.uniform(1500, 4200)
        onset = int(k * 0.045 * SR)
        m = n - onset
        tone = mul(sweep(m, f, f * 1.01), env_exp(m, 0.08))
        for i, v in enumerate(tone):
            out[onset + i] += v * (1 - k / 12)
    trem = [0.7 + 0.3 * math.sin(2 * math.pi * 18 * i / SR) for i in range(n)]
    return mul(out, trem)


def s_summon(r: random.Random) -> list[float]:
    n = int(0.8 * SR)
    a = sweep(n, 180, 720)
    b = sweep(n, 270, 1080)
    wind = bandpass_sweep(noise(n, r), lambda t: 300 + 1200 * t, 1.2)
    shape = [math.sin(math.pi * i / n) ** 1.2 for i in range(n)]
    trem = [0.75 + 0.25 * math.sin(2 * math.pi * 9 * i / SR) for i in range(n)]
    return mul(mul(mix((1.0, a), (0.6, b), (0.5, wind)), shape), trem)


def s_blip(r: random.Random) -> list[float]:
    a = mul(sweep(int(0.045 * SR), 880, 880), env_exp(int(0.045 * SR), 0.03, 0.003))
    b = mul(sweep(int(0.07 * SR), 1320, 1320), env_exp(int(0.07 * SR), 0.035, 0.003))
    return a + b


def s_bubble(r: random.Random) -> list[float]:
    n = int(0.42 * SR)
    out = [0.0] * n
    t = 0
    while t < n - int(0.04 * SR):
        m = int(0.035 * SR)
        f0 = r.uniform(280, 420)
        b = mul(sweep(m, f0, f0 * 2.2), env_exp(m, 0.012, 0.003))
        for i, v in enumerate(b):
            out[t + i] += v
        t += int(r.uniform(0.04, 0.08) * SR)
    return out


def s_crash(r: random.Random) -> list[float]:
    n = int(0.5 * SR)
    burst = mul(lowpass(noise(n, r), 3000), env_exp(n, 0.12, 0.003))
    low = mul(sweep(n, 110, 45), env_exp(n, 0.1))
    debris = [0.0] * n
    for _ in range(14):
        t = int(r.uniform(0.05, 0.42) * SR)
        m = int(0.015 * SR)
        click = mul(highpass(noise(m, r), 1500), env_exp(m, 0.004))
        for i, v in enumerate(click):
            if t + i < n:
                debris[t + i] += v * 0.6
    return mix((1.0, burst), (0.9, low), (1.0, debris))


# ---------- instruments (tunable) ----------

def i_pluck(r: random.Random) -> tuple[list[float], int]:
    """Karplus-Strong string, base pitch = SR / period."""
    period = 31                                  # 16000 / 31 = 516 Hz (~C5): the
    n = int(1.0 * SR)                            # lead sits around octave 5
    buf = [r.uniform(-1, 1) for _ in range(period)]
    out = []
    for i in range(n):
        v = buf[i % period]
        nxt = buf[(i + 1) % period]
        buf[i % period] = 0.998 * 0.5 * (v + nxt)
        out.append(v)
    return lowpass(out, 5000), round(SR / period)


def i_drum(r: random.Random) -> tuple[list[float], int]:
    n = int(0.25 * SR)
    body = mul(sweep(n, 150, 50), env_exp(n, 0.09))
    click = mul(highpass(noise(n, r), 1500), env_exp(n, 0.006))
    return mix((1.0, body), (0.5, click)), 100


# name, generator, tunable
SAMPLES = [
    ("step", s_step, False),
    ("whoosh", s_whoosh, False),
    ("clang", s_clang, False),
    ("thud", s_thud, False),
    ("groan", s_groan, False),
    ("thunder", s_thunder, False),
    ("zap", s_zap, False),
    ("creak", s_creak, False),
    ("lid", s_lid, False),
    ("sparkle", s_sparkle, False),
    ("summon", s_summon, False),
    ("blip", s_blip, False),
    ("bubble", s_bubble, False),
    ("crash", s_crash, False),
    ("pluck", i_pluck, True),
    ("drum", i_drum, True),
]

INSTRUMENTS = {name: i for i, (name, _, tune) in enumerate(SAMPLES) if tune}


def to_int8(x: list[float]) -> bytes:
    return bytes((max(-127, min(127, round(v * 127))) & 0xFF) for v in x)


def write_wav(path: Path, x: list[float]) -> None:
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, round(v * 32767))))
                               for v in x))


def build() -> list[tuple[str, bytes, bool, int]]:
    """All samples as (name, int8 bytes, tunable, base_hz) - also used by
    the audio preview."""
    out = []
    for i, (name, gen, tunable) in enumerate(SAMPLES):
        rng = random.Random(1000 + i)            # deterministic output
        res = gen(rng)
        data, base = (res if tunable else (res, 0))
        data = fade_out(normalise(data))
        out.append((name, to_int8(data), tunable, base))
    return out


def main() -> int:
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "preview").mkdir(exist_ok=True)
    samples = build()
    blob = bytearray(b"LOCX")
    blob += bytes((1, len(samples)))
    for name, data, tunable, base in samples:
        if len(data) > 0xFFFF:
            raise SystemExit(f"{name}: too long")
        blob += struct.pack("<BHH", 1 if tunable else 0, base, len(data))
        blob += data
        write_wav(OUT_DIR / "preview" / f"{name}.wav",
                  [(b - 256 if b > 127 else b) / 127 for b in data])
    (OUT_DIR / "sfx.bin").write_bytes(bytes(blob))

    lines = ["/* GENERATED by tools/gen_sfx.py - do not edit. Sample ids in",
             " * /loc/sfx.bin order (ADR 0012). */",
             "#ifndef LOC_GEN_SFX_H", "#define LOC_GEN_SFX_H", ""]
    for i, (name, _, _) in enumerate(SAMPLES):
        lines.append(f"#define SFX_{name.upper()} {i}")
    lines += ["", f"#define SFX_COUNT {len(SAMPLES)}", "", "#endif", ""]
    HEADER.parent.mkdir(parents=True, exist_ok=True)
    HEADER.write_bytes("\n".join(lines).encode())
    secs = sum(len(d) for _, d, _, _ in samples) / SR
    print(f"[sfx] {len(samples)} samples, {secs:.1f} s, build/sfx/sfx.bin "
          f"({len(blob)} bytes), previews in build/sfx/preview/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
