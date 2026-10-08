# VDP feature spike (ADR 0012): a separate Agon program, so that the game
# binary does not pay for it. Built and staged by tools/build.py.

NAME = vdptest

LDHAS_ARG_PROCESSING = 0
LDHAS_EXIT_HANDLER = 0

include $(shell agondev-config --makefile)

CFLAGS += -Wall -Wextra
