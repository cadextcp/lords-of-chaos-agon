#include "mapfile.h"

#include <agon/mos.h>

/* Largest possible map: header + 3 layers of 36x36 + units + objects. */
#define MAPFILE_MAX (MAPBIN_HEADER + 3 * MAP_MAX_W * MAP_MAX_H + 1 + 4 * MAX_UNITS + 1 + 3 * MAX_OBJECTS)

static uint8_t buf[MAPFILE_MAX];

bool mapfile_load(World *w, const char *path)
{
    uint8_t fh = mos_fopen(path, FA_READ);
    uint24_t len;
    if (!fh)
        return false;
    len = mos_fread(fh, (char *)buf, sizeof buf);
    mos_fclose(fh);
    if (len == 0 || len >= sizeof buf)   /* empty or larger than any valid map */
        return false;
    return world_load_bin(w, buf, (uint16_t)len);
}
