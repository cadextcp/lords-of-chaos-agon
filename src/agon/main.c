/*
 * Agon frontend.
 *
 *   loc              play the M0 demo scene (MODE 8, custom glyphs)
 *   loc --selftest   run the core self-test without touching the VDP and
 *                    exit the emulator with the result (headless CI)
 *   loc --dump       like play, but write the screen grid to loc.log after
 *                    every frame (screenshot-free inspection)
 */
#include <agon/keyboard.h>
#include <agon/vdp.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/demo.h"
#include "../core/screen.h"
#include "../core/selftest.h"
#include "emu.h"
#include "log.h"
#include "render.h"

#define KEY_ESC 27
/* MOS ASCII codes for the cursor keys */
#define KEY_LEFT 8
#define KEY_RIGHT 21
#define KEY_UP 11
#define KEY_DOWN 10

static void print_line(const char *line)
{
    printf("%s\r\n", line);
}

static Dir key_to_dir(uint8_t ascii)
{
    switch (ascii) {
    case KEY_UP:    case 'w': case 'W': return DIR_N;
    case KEY_DOWN:  case 's': case 'S': return DIR_S;
    case KEY_LEFT:  case 'a': case 'A': return DIR_W;
    case KEY_RIGHT: case 'd': case 'D': return DIR_E;
    default: return DIR_NONE;
    }
}

static int selftest(void)
{
    uint16_t fails = core_selftest(print_line);
    printf(fails ? "=== TEST FAIL ===\r\n" : "=== TEST PASS ===\r\n");
    emu_exit(fails ? 1 : 0);
    return fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    struct keyboard_event_t e;
    bool dump = false;
    bool running = true;

    if (argc > 1 && strcmp(argv[1], "--selftest") == 0)
        return selftest();
    if (argc > 1 && strcmp(argv[1], "--dump") == 0)
        dump = true;

    log_open(dump);
    log_line("BOOT");
    render_init();
    demo_init(42);
    render_flush();
    if (dump)
        log_screen();

    kbuf_init(16);
    while (running) {
        if (!kbuf_poll_event(&e) || !e.isdown)
            continue;
        if (e.ascii == KEY_ESC) {
            running = false;
        } else if (demo_move(key_to_dir(e.ascii))) {
            render_flush();
            if (dump)
                log_screen();
        }
    }
    kbuf_deinit();

    render_shutdown();
    log_line("EXIT");
    log_close();
    return 0;
}
