# loctest: the core self-test as its own Agon program (QUIRK S6). tools/build.py
# copies this file as Makefile into build/loctest/ next to src/ (src/core,
# selftest.c, main.c, emu.asm/emu.h) and runs make there.

NAME = loctest

LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0

include $(shell agondev-config --makefile)

CFLAGS += -Wall -Wextra -Isrc/core
# the same flags as the game, so the eZ80 checks see the game's code
CFLAGS += -O2
obj/selftest.o: CFLAGS += -Oz
