# AGENTS.md

Agent instructions live in [CLAUDE.md](CLAUDE.md) (German). Key points:

- Build/test: `uv run tools/setup.py`, `uv run tools/build.py --all`, `uv run tools/test.py` (must pass).
- `src/core` is platform-free C99 (no VDP/MOS, `stdint` types only — `int` is 24-bit on eZ80); host code lives in `host/`.
- Never commit `reference/`, `emulator/`, `toolchain/`, `sdcard/`, `.cache/`.
- Game design source of truth: `docs/design/GDD.md`.
