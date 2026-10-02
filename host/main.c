/*
 * Host (PC) frontend for the platform-free core.
 *
 *   loc_host --selftest     run core self-test, exit code = failures
 *   loc_host --dump [seed]  print the demo scene as ASCII and exit
 *   loc_host [seed]         line-based play: w/a/s/d + Enter, q quits
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/core/demo.h"
#include "../src/core/screen.h"
#include "../src/core/selftest.h"

static void print_line(const char *line)
{
    puts(line);
}

static Dir key_to_dir(int c)
{
    switch (c) {
    case 'w': return DIR_N;
    case 's': return DIR_S;
    case 'a': return DIR_W;
    case 'd': return DIR_E;
    default:  return DIR_NONE;
    }
}

int main(int argc, char **argv)
{
    uint32_t seed = 42;
    int c;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0) {
        uint16_t fails = core_selftest(print_line);
        puts(fails ? "=== TEST FAIL ===" : "=== TEST PASS ===");
        return fails ? 1 : 0;
    }
    if (argc > 1 && strcmp(argv[1], "--dump") == 0) {
        if (argc > 2)
            seed = (uint32_t)strtoul(argv[2], NULL, 0);
        demo_init(seed);
        screen_dump(print_line);
        return 0;
    }
    if (argc > 1)
        seed = (uint32_t)strtoul(argv[1], NULL, 0);

    demo_init(seed);
    screen_dump(print_line);
    while ((c = getchar()) != EOF && c != 'q') {
        if (demo_move(key_to_dir(c)))
            screen_dump(print_line);
    }
    return 0;
}
