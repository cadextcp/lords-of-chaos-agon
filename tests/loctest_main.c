/*
 * loctest: the core self-test as its own Agon program (QUIRK S6). Built
 * by tools/build.py from src/core, tests/selftest.c and this file into
 * build/loctest/bin/loctest.bin and staged next to loc.bin; tools/test.py
 * runs it headless in the CLI emulator, on the hardware it is `loctest`.
 */
#include <stdio.h>

#include "emu.h"
#include "selftest.h"

static void print_line(const char *line)
{
    printf("%s\r\n", line);
}

int main(void)
{
    uint16_t fails;
    selftest_set_verbose(false);         /* emulator console loses long tails */
    fails = core_selftest(print_line);
    printf(fails ? "=== TEST FAIL ===\r\n" : "=== TEST PASS ===\r\n");
    /* The fake VDP drops the first console bytes after the boot banner
     * (packet desync); a sacrificial line keeps the verdict readable. */
    printf("\r\n.");
    fflush(stdout);                      /* reach the UART before the exit */
    emu_exit(fails ? 1 : 0);
    return fails ? 1 : 0;
}
