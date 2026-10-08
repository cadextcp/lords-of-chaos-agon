/*
 * Umlaut font (M4j): redefines four characters of the system font as the
 * German glyphs (ae, oe, ue, ss) at their CP437 codes (132/148/129/225). Every UI string spells umlauts as AE/OE/UE today;
 * strings that contain the real characters render correctly now.
 */
#ifndef LOC_UMFONT_H
#define LOC_UMFONT_H

void umfont_install(void);

#endif
