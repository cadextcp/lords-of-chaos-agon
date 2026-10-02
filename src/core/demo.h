/* M0 "hello glyph" scene: a seeded meadow and a wizard you can walk around. */
#ifndef LOC_DEMO_H
#define LOC_DEMO_H

#include <stdint.h>

typedef enum { DIR_NONE, DIR_N, DIR_S, DIR_W, DIR_E } Dir;

void demo_init(uint32_t seed);
/* Returns 1 if the wizard moved. */
uint8_t demo_move(Dir d);
uint8_t demo_wizard_x(void);
uint8_t demo_wizard_y(void);

#endif
