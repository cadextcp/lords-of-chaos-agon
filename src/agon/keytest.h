/* Keyboard spike (issue #3): show/log every kbuf event + chord directions. */
#ifndef LOC_KEYTEST_H
#define LOC_KEYTEST_H

#include <stdbool.h>
#include <stdint.h>

/* MODE 8 text screen; ESC twice ends. Events also go to loc.log. */
void keytest_run(void);

#endif
