/*
 * Logical colour indices. The first 16 entries of the Agon 64-colour
 * palette are assumed to match the classic 16-colour set (to be verified
 * in the M1 rendering spike, see docs/ROADMAP.md).
 */
#ifndef LOC_COLORS_H
#define LOC_COLORS_H

enum {
    C_BLACK = 0,
    C_RED,
    C_GREEN,
    C_YELLOW,
    C_BLUE,
    C_MAGENTA,
    C_CYAN,
    C_WHITE,
    C_GREY,
    C_BRIGHT_RED,
    C_BRIGHT_GREEN,
    C_BRIGHT_YELLOW,
    C_BRIGHT_BLUE,
    C_BRIGHT_MAGENTA,
    C_BRIGHT_CYAN,
    C_BRIGHT_WHITE
};

#endif
