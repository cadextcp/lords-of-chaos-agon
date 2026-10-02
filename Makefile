# Agon build (agondev). Compiles every source under src/ -> bin/loc.bin.
# Run via `uv run tools/build.py` (routes through WSL on Windows).

NAME = loc

# 0: simple argv processing (enough for `loc --selftest`)
LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0

include $(shell agondev-config --makefile)

CFLAGS += -Wall -Wextra
