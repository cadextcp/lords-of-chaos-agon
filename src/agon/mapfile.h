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
