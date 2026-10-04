# Agon build (agondev). Compiles every source under src/ -> bin/loc.bin.
# Run via `uv run tools/build.py` (routes through WSL on Windows).

NAME = loc

# 0: simple argv processing (enough for `loc --selftest`)
LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0

include $(shell agondev-config --makefile)

CFLAGS += -Wall -Wextra
# agondev defaults to -Oz (size): -O2 overrides it. Measured in the M2d
# bench: view compose 28 -> 18 ms, full redraw 52 -> 44 ms; the binary
# stays far below the RAM budget (70 KB of 448 KB, ADR 0008).
CFLAGS += -O2

# Cold code is compiled for size (polish round, ADR 0012: the eZ80 RAM is
# nearly full). The selftest alone was 110 KB of -O2 code; menus, screens
# and the keyboard spike do no per-frame work. Hot paths (view, render,
# sight, world, ai, combat) keep -O2. The later -Oz wins over -O2.
obj/core/selftest.o obj/agon/screens.o obj/agon/keytest.o: CFLAGS += -Oz
