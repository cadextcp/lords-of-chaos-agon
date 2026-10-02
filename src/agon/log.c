#include "log.h"

#include <agon/mos.h>
#include <stdio.h>
#include <string.h>

static uint8_t fh;

void log_open(bool enabled)
{
    fh = enabled ? mos_fopen("loc.log", FA_WRITE | FA_CREATE_ALWAYS) : 0;
}

void log_line(const char *line)
{
    if (!fh)
        return;
    mos_fwrite(fh, (char *)line, strlen(line));
    mos_fputc(fh, '\n');
}

void log_frame(const World *w, uint32_t view_hash)
{
    char line[MAP_MAX_W + 1];
    int16_t x, y;
    if (!fh)
        return;
    snprintf(line, sizeof line, "--- AP %u hash %08lX", w->units[0].ap,
             (unsigned long)view_hash);
    log_line(line);
    for (y = 0; y < w->h; y++) {
        for (x = 0; x < w->w; x++)
            line[x] = world_char(w, x, y);
        line[w->w] = '\0';
        log_line(line);
    }
}

void log_close(void)
{
    if (fh)
        mos_fclose(fh);
    fh = 0;
}
