#include "mapfile.h"

#include <string.h>

#include "../core/spells.h"
#include "../core/wizard.h"

#include <agon/mos.h>

/* Largest possible map: header + 3 layers of 36x36 + units + objects. */
#define MAPFILE_MAX (MAPBIN_HEADER + 3 * MAP_MAX_W * MAP_MAX_H + 1 + 4 * MAX_UNITS + 1 + 3 * MAX_OBJECTS)
/* Largest scenario: 4 books x (2 + SPELL_COUNT x 2) bytes plus header. */
#define SCNFILE_MAX (6 + 4 * (2 + 2 * SPELL_COUNT))

static uint8_t buf[MAPFILE_MAX];
static uint8_t scnbuf[SCNFILE_MAX];

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

bool scnfile_load(Spellbook *books, const char *path)
{
    uint8_t fh = mos_fopen(path, FA_READ);
    uint24_t len;
    if (!fh)
        return false;
    len = mos_fread(fh, (char *)scnbuf, sizeof scnbuf);
    mos_fclose(fh);
    if (len == 0 || len >= sizeof scnbuf)
        return false;
    return spellbook_load(books, scnbuf, (uint16_t)len);
}

/* File layout: "LOCW", version, sizeof(Wizard) (u16 LE), then the slots.
 * Anything else (old layout, corruption) falls back to stock wizards. */
#define WIZ_FILE_VERSION 1
#define WIZ_FILE_HEADER 7

bool wizards_save(void)
{
    uint8_t fh = mos_fopen("wizards.dat", FA_WRITE | FA_CREATE_ALWAYS);
    uint8_t hdr[WIZ_FILE_HEADER] = {'L', 'O', 'C', 'W', WIZ_FILE_VERSION,
                                    (uint8_t)(sizeof(Wizard) & 0xFF),
                                    (uint8_t)(sizeof(Wizard) >> 8)};
    if (!fh)
        return false;
    mos_fwrite(fh, (char *)hdr, sizeof hdr);
    mos_fwrite(fh, (char *)wizard_slots, sizeof wizard_slots);
    mos_fclose(fh);
    return true;
}

bool wizards_load(void)
{
    static Wizard tmp[WIZARD_SLOTS];
    uint8_t hdr[WIZ_FILE_HEADER];
    uint8_t fh = mos_fopen("wizards.dat", FA_READ);
    uint8_t i;
    if (!fh)
        return false;
    if (mos_fread(fh, (char *)hdr, sizeof hdr) != sizeof hdr ||
        memcmp(hdr, "LOCW", 4) != 0 || hdr[4] != WIZ_FILE_VERSION ||
        (uint16_t)(hdr[5] | (hdr[6] << 8)) != sizeof(Wizard) ||
        mos_fread(fh, (char *)tmp, sizeof tmp) != sizeof tmp) {
        mos_fclose(fh);
        return false;
    }
    mos_fclose(fh);
    for (i = 0; i < WIZARD_SLOTS; i++)
        if (!wizard_valid(&tmp[i]))
            return false;
    memcpy(wizard_slots, tmp, sizeof tmp);
    return true;
}
