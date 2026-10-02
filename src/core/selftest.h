/*
 * Core self-test. Compiled into BOTH builds: the host test runner and the
 * Agon binary (`loc --selftest`, run headless in the CLI emulator). Running
 * the same assertions on the eZ80 catches 24-bit-int and codegen issues
 * that host tests alone would miss.
 */
#ifndef LOC_SELFTEST_H
#define LOC_SELFTEST_H

#include <stdint.h>

typedef void (*selftest_log_fn)(const char *line);

/* Returns the number of failed checks (0 = pass). */
uint16_t core_selftest(selftest_log_fn log);

#endif
