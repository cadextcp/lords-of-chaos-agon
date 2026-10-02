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
