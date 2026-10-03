#include "screens.h"

#include <agon/keyboard.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "input.h"
#include "render.h"

/* Strings use the umlaut font codes (umfont.c): \204 ae, \224 oe,
 * \201 ue, \341 ss. */

static void centred(uint8_t row, uint8_t colour, const char *s)
{
    size_t n = strlen(s);
    uint8_t col = n >= 40 ? 0 : (uint8_t)((40 - n) / 2);
    render_menu_text(col, row, colour, s);
}

static void line(uint8_t row, const char *label, uint16_t value,
                 uint8_t colour)
{
    char buf[40];
    snprintf(buf, sizeof buf, "%-22s%5u", label, value);
    render_menu_text(8, row, colour, buf);
}

bool screen_end(const EndInfo *info)
{
    struct keyboard_event_t e;
    char buf[40];
    bool win = info->outcome == OUT_WIN;
    uint8_t row = 11;

    render_screen_clear();
    render_frame(8, 8, 311, 231, win ? C_BRIGHT_YELLOW : C_RED);
    if (win) {
        centred(3, C_BRIGHT_YELLOW, "*** Gl\201ckwunsch! ***");
        snprintf(buf, sizeof buf, "%.16s entkommt durchs Portal!",
                 info->name);
        centred(5, C_BRIGHT_GREEN, buf);
    } else {
        centred(3, C_BRIGHT_RED, "G A M E   O V E R");
        snprintf(buf, sizeof buf, "%.16s ist gefallen.", info->name);
        centred(5, C_BRIGHT_RED, buf);
    }
    if (info->scenario)
        centred(7, C_BRIGHT_CYAN, info->scenario);

    line(row++, "Runden", info->rounds, C_BRIGHT_WHITE);
    line(row++, "Besiegte Gegner", info->kills, C_BRIGHT_WHITE);
    line(row++, "Beute (Punkte)", info->loot_vp, C_BRIGHT_WHITE);
    line(row++, "Siegpunkte", info->vp, C_BRIGHT_YELLOW);
    if (info->campaign) {
        row++;
        line(row++, "Erfahrung (neu)", info->xp_gain, C_BRIGHT_GREEN);
        line(row++, "Erfahrung (gesamt)", info->xp_total, C_BRIGHT_GREEN);
        snprintf(buf, sizeof buf, "Stufe %u%s", info->level,
                 info->level_up ? "  - aufgestiegen!" : "");
        render_menu_text(8, row, info->level_up ? C_BRIGHT_YELLOW : C_GREY,
                         buf);
    } else if (!win) {
        row++;
        render_menu_text(8, row, C_GREY, "Beute und Punkte sind verloren.");
    }
    centred(25, C_GREY, "Enter: Hauptmen\201   Esc: Beenden");

    for (;;) {                           /* drain, then wait for a key */
        while (!kbuf_poll_event(&e))
            ;
        if (!e.isdown)
            continue;
        if (e.vkey == VK_ESC)
            return false;
        if (e.ascii == 13 || e.vkey == VK_SPACE)
            return true;
    }
}
