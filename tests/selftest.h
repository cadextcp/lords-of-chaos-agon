/*
 * Core self-test. Compiled into BOTH test builds: the host test runner
 * (`loc_host --selftest`) and its own Agon program `loctest.bin` (run
 * headless in the CLI emulator, or `loctest` on the hardware). Running the
 * same assertions on the eZ80 catches 24-bit-int and codegen issues that
 * host tests alone would miss. It is no longer part of loc.bin: 137 KB of
 * code and data the game needs for itself (QUIRK S6).
 */
#ifndef LOC_SELFTEST_H
#define LOC_SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

#include "world.h"

typedef void (*selftest_log_fn)(const char *line);

/* Returns the number of failed checks (0 = pass). */
uint16_t core_selftest(selftest_log_fn log);
/* Print passing checks too? Default true (host). The Agon build sets
 * false: the emulator console loses the tail of long outputs. */
void selftest_set_verbose(bool verbose);

#endif
