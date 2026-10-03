#include "umfont.h"

#include <agon/vdp.h>
#include <stdio.h>

/* 8x8 glyphs, rows top to bottom, MSB left (VDP bitmap order). */
static const uint8_t GLYPH_AE[8] = { 0x00, 0x66, 0x00, 0x3C, 0x66, 0x7E, 0x66, 0x00 };
static const uint8_t GLYPH_OE[8] = { 0x00, 0x66, 0x00, 0x3C, 0x66, 0x66, 0x3C, 0x00 };
static const uint8_t GLYPH_UE[8] = { 0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x00 };
static const uint8_t GLYPH_SS[8] = { 0x00, 0x3C, 0x40, 0x38, 0x44, 0x38, 0x44, 0x38 };

#define FONT_BUFFER 1099

/* VDU 23,0,0x95,5,buffer;  copy system font
 * VDU 23,27,n,8,bytes...   redefine character n (aggregated stream) */
static void define_glyph(uint8_t code, const uint8_t *rows)
{
    uint8_t i;
    printf("\x17\x1b%c%c", code, 8);
    for (i = 0; i < 8; i++)
        putchar(rows[i]);
}

void umfont_install(void)
{
    printf("\x17\x00\x95%c%c;", 5, FONT_BUFFER & 0xFF);
    printf("\x17\x00\x95%c%c;", 0, FONT_BUFFER & 0xFF);
    define_glyph(132, GLYPH_AE);         /* ae */
    define_glyph(148, GLYPH_OE);         /* oe */
    define_glyph(129, GLYPH_UE);         /* ue */
    define_glyph(225, GLYPH_SS);         /* ss */
}
