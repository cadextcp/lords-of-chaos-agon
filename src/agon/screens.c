#include "screens.h"

#include <agon/keyboard.h>
#include <agon/mos.h>
#include <stdio.h>
#include <string.h>

#include "../core/colors.h"
#include "../core/gen/data.h"
#include "../core/tutorial.h"
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

/* ---------- help pages from the SD card (M5, ADR 0011) ---------- */

#define HELP_MAX 3072
#define HELP_PAGES_MAX 72
#define LEXICON_MAX 6656

static uint8_t help_buf[HELP_MAX];
static uint8_t lex_buf[LEXICON_MAX];     /* lexicon.hlp is ~6 KB */
static uint24_t lex_len;                 /* bytes of lexicon.hlp read */
static uint16_t help_page[HELP_PAGES_MAX];
static uint8_t help_count;

/* Parse a .hlp image in place: page offsets into page_off, returns the
 * page count (0 when the image is invalid or truncated). */
static uint8_t help_parse(uint8_t *buf, uint24_t len, uint16_t *page_off)
{
    uint16_t pages, off, i;
    uint8_t n;
    if (len < 8 || memcmp(buf, "LOCH", 4) != 0 || buf[4] != 1)
        return 0;
    pages = (uint16_t)(buf[5] | (buf[6] << 8));
    if (pages == 0 || pages > HELP_PAGES_MAX)
        return 0;
    off = 7;
    for (n = 0; n < pages; n++) {
        if (off >= len)
            return 0;
        page_off[n] = off;
        off = (uint16_t)(off + 1 + buf[off]);        /* title */
        if (off >= len)
            return 0;
        {
            uint8_t lines = buf[off];
            off++;
            for (i = 0; i < lines; i++) {
                if (off >= len)
                    return 0;
                off = (uint16_t)(off + 1 + buf[off]);/* one text line */
                if (off > len)
                    return 0;
            }
        }
    }
    return (uint8_t)pages;
}

/* Copy the n-th text line of a page (n = 0 is the first body line). */
static bool help_line(char *out, uint8_t cap, uint8_t page, uint8_t n)
{
    uint16_t off;
    uint8_t title_len, lines, i, len;
    if (page >= help_count)
        return false;
    off = help_page[page];
    title_len = help_buf[off];
    off = (uint16_t)(off + 1 + title_len);
    lines = help_buf[off++];
    for (i = 0; i < lines; i++) {
        len = help_buf[off++];
        if (i == n) {
            if (len >= cap)
                len = (uint8_t)(cap - 1);
            memcpy(out, help_buf + off, len);
            out[len] = 0;
            return true;
        }
        off = (uint16_t)(off + len);
    }
    return false;
}

static void help_draw(uint8_t page)
{
    char buf[40];
    uint16_t off = help_page[page];
    uint8_t title_len, lines, i, row = 2;

    render_screen_clear();
    title_len = help_buf[off++];
    memcpy(buf, help_buf + off, title_len);
    buf[title_len] = 0;
    off = (uint16_t)(off + title_len);
    centred(0, C_BRIGHT_YELLOW, buf);
    lines = help_buf[off++];
    for (i = 0; i < lines && row < 28; i++) {
        uint8_t len = help_buf[off++];
        memcpy(buf, help_buf + off, len);
        buf[len] = 0;
        off = (uint16_t)(off + len);
        render_menu_text(1, row++, C_BRIGHT_WHITE, buf);
    }
    snprintf(buf, sizeof buf, "Blatt %u/%u   Esc zur\201ck", page + 1,
             help_count);
    centred(29, C_GREY, buf);
}

bool screen_help(const char *file)
{
    struct keyboard_event_t e;
    uint8_t fh;
    uint24_t len;
    uint8_t page = 0;

    fh = mos_fopen(file, FA_READ);
    if (!fh)
        return false;
    len = mos_fread(fh, (char *)help_buf, sizeof help_buf);
    mos_fclose(fh);
    help_count = help_parse(help_buf, len, help_page);
    if (!help_count)
        return false;

    help_draw(page);
    for (;;) {
        while (!kbuf_poll_event(&e))
            ;
        if (!e.isdown)
            continue;
        if (e.vkey == VK_ESC || e.ascii == 13 || e.vkey == VK_SPACE)
            return true;
        if (e.vkey == VK_LEFT)
            page = page ? (uint8_t)(page - 1) : (uint8_t)(help_count - 1);
        else if (e.vkey == VK_RIGHT)
            page = (uint8_t)((page + 1) % help_count);
        else
            continue;
        help_draw(page);
    }
}

/* ---------- tutorial hints (loaded once per program run) ---------- */

#define TUT_HINT_MAX (TUT_COUNT - 1)   /* one hint line per step */
static char tut_hint[TUT_HINT_MAX][40];

bool tutorial_hints_load(const char *file)
{
    uint8_t fh;
    uint24_t len;
    uint16_t page_off[HELP_PAGES_MAX];
    uint8_t pages, i;

    fh = mos_fopen(file, FA_READ);
    if (!fh)
        return false;
    len = mos_fread(fh, (char *)help_buf, sizeof help_buf);
    mos_fclose(fh);
    pages = help_parse(help_buf, len, page_off);
    if (pages < 2)
        return false;
    memset(tut_hint, 0, sizeof tut_hint);
    help_count = pages;                  /* borrow the shared tables */
    memcpy(help_page, page_off, sizeof help_page);
    for (i = 0; i < TUT_HINT_MAX && i + 1 < pages; i++)
        help_line(tut_hint[i], sizeof tut_hint[i], (uint8_t)(i + 1), 0);
    help_count = 0;
    return true;
}

const char *tutorial_hint_line(uint8_t step)
{
    if (step >= TUT_HINT_MAX)
        return "";
    return tut_hint[step];
}

/* ---------- lexicon (M5) ---------- */

/* Three list sections: creatures, objects 0..19, objects 20..39. */
#define LEX_SECTIONS 3
#define LEX_TITLE_ROW 0
#define LEX_LIST_ROW 2
#define LEX_COLS 2
#define LEX_COL_X 1
#define LEX_COL_W 19

static const char *lexicon_section_title(uint8_t section)
{
    if (section == 0)
        return "Kreaturen";
    return section == 1 ? "Objekte I" : "Objekte II";
}

static uint8_t lexicon_section_entries(uint8_t section)
{
    if (section == 0)
        return CR_COUNT;
    return (uint8_t)((OBJ_COUNT + 1) / 2);
}

/* Section and index of an entry for the flat entry number (creatures
 * first, then objects). */
static void lexicon_entry(uint16_t entry, uint8_t *section, uint16_t *idx)
{
    if (entry < CR_COUNT) {
        *section = 0;
        *idx = entry;
    } else {
        *section = 1 + (entry - CR_COUNT) / ((OBJ_COUNT + 1) / 2);
        *idx = (entry - CR_COUNT) % ((OBJ_COUNT + 1) / 2);
    }
}

static uint16_t lexicon_entry_of(uint8_t section, uint16_t idx)
{
    if (section == 0)
        return idx;
    return (uint16_t)(CR_COUNT + (section - 1) * ((OBJ_COUNT + 1) / 2) + idx);
}

/* One list row: name or ??? plus a mark when discovered. */
static void lexicon_row(const Lexicon *lex, uint8_t section, uint16_t idx,
                        uint8_t col, uint8_t row, bool cursor)
{
    char buf[24];
    uint16_t entry = lexicon_entry_of(section, idx);
    bool seen = entry < CR_COUNT ? lexicon_seen_creature(lex, (uint8_t)entry)
                                 : lexicon_seen_object(lex,
                                       (uint8_t)(entry - CR_COUNT));
    const char *name = entry < CR_COUNT
        ? CREATURES[entry].name : OBJECTS[entry - CR_COUNT].name;
    snprintf(buf, sizeof buf, "%c%c%-16.16s", cursor ? '>' : ' ',
             seen ? '*' : ' ', seen ? name : "???");
    render_menu_text((uint8_t)(LEX_COL_X + col * LEX_COL_W), row,
                     seen ? C_BRIGHT_WHITE : C_GREY, buf);
}

static void lexicon_draw_list(const Lexicon *lex, uint8_t section,
                              uint16_t cursor)
{
    uint16_t total, i;
    uint8_t entries = lexicon_section_entries(section);
    uint8_t half = (uint8_t)((entries + LEX_COLS - 1) / LEX_COLS);
    char buf[40];
    uint16_t seen_all = 0;
    uint16_t e;

    total = (uint16_t)(CR_COUNT + OBJ_COUNT);
    for (e = 0; e < total; e++)
        seen_all += e < CR_COUNT ? lexicon_seen_creature(lex, (uint8_t)e)
                                 : lexicon_seen_object(lex, (uint8_t)(e - CR_COUNT));
    render_screen_clear();
    snprintf(buf, sizeof buf, "LEXIKON   %u/%u entdeckt", seen_all, total);
    centred(LEX_TITLE_ROW, C_BRIGHT_YELLOW, buf);
    for (i = 0; i < entries; i++) {
        uint8_t col = (uint8_t)(i / half);
        uint16_t row_idx = (uint16_t)(i % half);
        if (col >= LEX_COLS)
            break;
        lexicon_row(lex, section, i, col,
                    (uint8_t)(LEX_LIST_ROW + row_idx), i == cursor);
    }
    snprintf(buf, sizeof buf, "%s   Blatt %u/3", lexicon_section_title(section),
             section + 1);
    centred(28, C_GREY, buf);
    centred(29, C_GREY, "Enter Detail   Esc zur\201ck");
}

/* Detail page: portrait, table values, description from lexicon.hlp. */
static void lexicon_draw_detail(uint16_t entry)
{
    char buf[40];
    uint16_t off;
    uint8_t pages, title_len, lines, i, row = 2;
    uint16_t page_off[HELP_PAGES_MAX];
    bool is_creature = entry < CR_COUNT;

    render_screen_clear();
    if (is_creature) {
        const CreatureDef *c = &CREATURES[entry];
        render_frame(0, 12, 31, 39, C_BRIGHT_BLUE);
        render_draw_tile((uint16_t)(CREATURE_TILE[entry]), 4, 16);
        snprintf(buf, sizeof buf, "%s", c->name);
        render_menu_text(5, 2, C_BRIGHT_WHITE, buf);
        snprintf(buf, sizeof buf, "Kampf %-3u  Vert. %-3u", c->combat,
                 c->defence);
        render_menu_text(5, 4, C_BRIGHT_WHITE, buf);
        snprintf(buf, sizeof buf, "Magieres. %u  Widerst. -", c->magic_res);
        render_menu_text(5, 5, C_GREY, buf);
        snprintf(buf, sizeof buf, "Kons %u  Ausd %u  AP %u", c->con, c->stamina,
                 c->ap);
        render_menu_text(5, 6, C_GREY, buf);
        snprintf(buf, sizeof buf, "Wert %u VP  Tragen %u", c->vp, c->carry);
        render_menu_text(5, 7, C_GREY, buf);
        buf[0] = 0;
        if (c->flags & CF_UNDEAD)
            strcat(buf, "Untot ");
        if (c->flags & CF_MOUNT)
            strcat(buf, "Reittier ");
        if (c->flags & CF_RIDE)
            strcat(buf, "reitet ");
        if (c->flags & CF_WEAPONS)
            strcat(buf, "Waffen ");
        if (c->flags & CF_USE)
            strcat(buf, "H\204nde");
        render_menu_text(5, 8, C_BRIGHT_CYAN, buf);
        row = 11;
    } else {
        uint8_t k = (uint8_t)(entry - CR_COUNT);
        const ObjectDef *o = &OBJECTS[k];
        render_frame(0, 12, 31, 39, C_BRIGHT_BLUE);
        render_draw_tile(o->tile, 4, 16);
        snprintf(buf, sizeof buf, "%s", o->name);
        render_menu_text(5, 2, C_BRIGHT_WHITE, buf);
        snprintf(buf, sizeof buf, "Gewicht %u  Wert %u VP", o->weight, o->vp);
        render_menu_text(5, 4, C_GREY, buf);
        if (o->weapon != WEAPON_NONE) {
            const WeaponDef *wd = &WEAPONS[o->weapon];
            snprintf(buf, sizeof buf, "Waffe: Kampf +%u Vert. +%u",
                     wd->combat, wd->defence);
            render_menu_text(5, 5, C_BRIGHT_CYAN, buf);
            if (wd->ranged)
                snprintf(buf, sizeof buf, "Fernkampf, Reichw. %u", wd->ranged);
            else
                snprintf(buf, sizeof buf, "Nahkampf");
            render_menu_text(5, 6, C_GREY, buf);
        }
        if (o->eat_con || o->eat_mana)
            snprintf(buf, sizeof buf, "Essen: +%u Kons +%u Mana", o->eat_con,
                     o->eat_mana);
        row = 9;
    }

    /* description text from the lexicon pages (page = entry index) */
    if (lex_len) {
        pages = help_parse(lex_buf, lex_len, page_off);
        if ((uint16_t)entry < pages) {
            off = page_off[entry];
            title_len = lex_buf[off];
            off = (uint16_t)(off + 1 + title_len);
            lines = lex_buf[off++];
            for (i = 0; i < lines && row < 26; i++) {
                uint8_t len = lex_buf[off++];
                memcpy(buf, lex_buf + off, len);
                buf[len] = 0;
                off = (uint16_t)(off + len);
                render_menu_text(1, row++, C_BRIGHT_WHITE, buf);
            }
        }
    }
    centred(29, C_GREY, "Esc zur\201ck");
}

void screen_lexicon(const Lexicon *lex)
{
    struct keyboard_event_t e;
    uint8_t section = 0, fh;
    uint16_t cursor[LEX_SECTIONS] = {0, 0, 0};
    bool detail = false;
    uint16_t entry = 0;
    uint24_t len;

    fh = mos_fopen("help/lexicon.hlp", FA_READ);
    if (fh) {
        len = mos_fread(fh, (char *)lex_buf, (uint24_t)sizeof lex_buf);
        mos_fclose(fh);
        if (help_parse(lex_buf, len, help_page) == 0)
            len = 0;
    } else {
        len = 0;
    }
    lex_len = len;

    lexicon_draw_list(lex, section, cursor[section]);
    for (;;) {
        while (!kbuf_poll_event(&e))
            ;
        if (!e.isdown)
            continue;
        if (e.vkey == VK_ESC) {
            if (detail) {
                detail = false;
                lexicon_draw_list(lex, section, cursor[section]);
                continue;
            }
            return;
        }
        if (detail) {
            if (e.ascii == 13 || e.vkey == VK_SPACE) {
                detail = false;
                lexicon_draw_list(lex, section, cursor[section]);
            }
            continue;
        }
        {
            uint8_t entries = lexicon_section_entries(section);
            if (e.vkey == VK_LEFT)
                section = section ? (uint8_t)(section - 1) : LEX_SECTIONS - 1;
            else if (e.vkey == VK_RIGHT)
                section = (uint8_t)((section + 1) % LEX_SECTIONS);
            else if (e.vkey == VK_UP)
                cursor[section] = cursor[section] ? cursor[section] - 1
                                                  : entries - 1;
            else if (e.vkey == VK_DOWN)
                cursor[section] = (uint16_t)((cursor[section] + 1) % entries);
            else if (e.ascii == 13 || e.vkey == VK_SPACE) {
                detail = true;
                entry = lexicon_entry_of(section, cursor[section]);
                lexicon_draw_detail(entry);
                continue;
            } else
                continue;
            lexicon_draw_list(lex, section, cursor[section]);
        }
    }
}
