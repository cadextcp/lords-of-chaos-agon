#include "umfont.h"

#include <agon/vdp.h>

/* 8x8 glyphs, rows top to bottom, MSB left (VDP font order). */
static uint8_t GLYPH_AE[8] = { 0x00, 0x66, 0x00, 0x3C, 0x66, 0x7E, 0x66, 0x00 };
static uint8_t GLYPH_OE[8] = { 0x00, 0x66, 0x00, 0x3C, 0x66, 0x66, 0x3C, 0x00 };
static uint8_t GLYPH_UE[8] = { 0x00, 0x66, 0x00, 0x66, 0x66, 0x66, 0x3C, 0x00 };
static uint8_t GLYPH_SS[8] = { 0x00, 0x3C, 0x40, 0x38, 0x44, 0x38, 0x44, 0x38 };

/* VDU 23,0,&90,n,b1..b8 redefines character n of the system font (the
 * agondev wrapper sends the bytes; a printf format cannot carry 0x00). */
void umfont_install(void)
{
    vdp_define_character(132, GLYPH_AE);   /* ae */
    vdp_define_character(148, GLYPH_OE);   /* oe */
    vdp_define_character(129, GLYPH_UE);   /* ue */
    vdp_define_character(225, GLYPH_SS);   /* ss */
}
