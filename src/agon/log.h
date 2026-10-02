/* Debug log on the SD card (loc.log in the current directory). */
#ifndef LOC_LOG_H
#define LOC_LOG_H

#include <stdbool.h>

void log_open(bool enabled);
void log_line(const char *line);
void log_screen(void);
void log_close(void);

#endif
