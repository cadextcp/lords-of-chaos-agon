/* Load a binary map (.map, ADR 0008) from the SD card into the world. */
#ifndef LOC_MAPFILE_H
#define LOC_MAPFILE_H

#include <stdbool.h>

#include "../core/spells.h"
#include "../core/world.h"

/* Path relative to the current directory, e.g. "maps/wizard_house.map".
 * False if the file is missing, too large or invalid (world unchanged). */
bool mapfile_load(World *w, const char *path);

#endif

/* Load a compiled scenario (.scn, M4a) with the spellbooks of all
 * wizards. False when missing or invalid. */
bool scnfile_load(Spellbook *books, const char *path);
/* Wizard slots: all 4 in one file "/wizards.dat" (M4f). load returns
 * false when the file is missing, has another layout or holds invalid
 * values (the caller then falls back to the stock wizards). */
bool wizards_save(void);
bool wizards_load(void);
