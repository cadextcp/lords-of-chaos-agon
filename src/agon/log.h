/* Debug log on the SD card (loc.log in the current directory). */
#ifndef LOC_LOG_H
#define LOC_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include "../core/world.h"

void log_open(bool enabled);
void log_line(const char *line);
/* ASCII map (world_char) plus active unit AP and view hash. */
void log_frame(const World *w, uint32_t view_hash);
void log_close(void);

#endif
