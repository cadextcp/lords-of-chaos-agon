#include "mapfile.h"

#include <string.h>

#include "../core/lexicon.h"
#include "../core/spells.h"
#include "../core/wizard.h"

#include <agon/mos.h>

/* Largest possible map: header + 3 layers + roof (v4) + portal + units
 * + objects. */
#define MAPFILE_MAX (MAPBIN_HEADER + 3 * MAP_MAX_W * MAP_MAX_H                      + MAP_MAX_W * MAP_MAX_H + 4                      + 1 + 4 * MAX_UNITS + 1 + 4 * MAX_OBJECTS)
/* Largest scenario: 4 books x (2 + SPELL_COUNT x 2) bytes plus header. */
#define SCNFILE_MAX (6 + 4 * (2 + 2 * SPELL_COUNT))

static uint8_t buf[MAPFILE_MAX];
static uint8_t scnbuf[SCNFILE_MAX];
static uint16_t scn_len;      /* what scnbuf holds right now */

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

bool mapfile_exists(const char *path)
{
    uint8_t fh = mos_fopen(path, FA_READ);
    if (!fh)
        return false;
    mos_fclose(fh);
    return true;
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
    scn_len = (uint16_t)len;
    return spellbook_load(books, scnbuf, (uint16_t)len);
}

bool scnfile_ai(World *w, AiProfile *profiles)
{
    return scn_len != 0 && ai_scenario_load(w, profiles, scnbuf, scn_len);
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

bool savegame_write(const uint8_t *data, uint16_t len)
{
    /* write beside the old save and swap, so a failed write keeps it */
    uint8_t fh = mos_fopen("save.new", FA_WRITE | FA_CREATE_ALWAYS);
    uint24_t written;
    if (!fh)
        return false;
    written = mos_fwrite(fh, (char *)data, len);
    mos_fclose(fh);
    if (written != len) {
        mos_del("save.new");
        return false;
    }
    mos_del("save.dat");
    return mos_ren("save.new", "save.dat") == 0;
}

uint16_t savegame_read(uint8_t *buf, uint16_t cap)
{
    uint8_t fh = mos_fopen("save.dat", FA_READ);
    uint24_t len;
    if (!fh)
        return 0;
    len = mos_fread(fh, (char *)buf, cap);
    mos_fclose(fh);
    return (uint16_t)len;
}

/* Lexicon of discoveries "/lexicon.dat" (M5): a 17-byte blob, the same
 * careful pattern as wizards.dat (anything odd leaves the caller's data
 * alone). */
bool lexicon_save(const Lexicon *lex)
{
    uint8_t buf[17];
    uint16_t len = lexicon_export(lex, buf, sizeof buf);
    uint8_t fh;
    if (!len)
        return false;
    fh = mos_fopen("lexicon.dat", FA_WRITE | FA_CREATE_ALWAYS);
    if (!fh)
        return false;
    mos_fwrite(fh, (char *)buf, len);
    mos_fclose(fh);
    return true;
}

bool lexicon_load(Lexicon *lex)
{
    uint8_t buf[17];
    uint8_t fh = mos_fopen("lexicon.dat", FA_READ);
    uint24_t got;
    if (!fh)
        return false;
    got = mos_fread(fh, (char *)buf, sizeof buf);
    mos_fclose(fh);
    if (got != sizeof buf)
        return false;
    return lexicon_import(lex, buf, sizeof buf);
}
