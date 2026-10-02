#include "log.h"

#include <agon/mos.h>
#include <stdint.h>
#include <string.h>

#include "../core/screen.h"

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

void log_screen(void)
{
    log_line("--- SCREEN ---");
    screen_dump(log_line);
}

void log_close(void)
{
    if (fh)
        mos_fclose(fh);
    fh = 0;
}
