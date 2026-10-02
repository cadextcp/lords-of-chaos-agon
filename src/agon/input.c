#include "input.h"

#include "../core/chord.h"

uint8_t input_arrow(uint8_t vkey)
{
    switch (vkey) {
    case VK_UP: case VK_LOWER('w'): return ARROW_UP;
    case VK_DOWN: case VK_LOWER('s'): return ARROW_DOWN;
    case VK_LEFT: case VK_LOWER('a'): return ARROW_LEFT;
    case VK_RIGHT: case VK_LOWER('d'): return ARROW_RIGHT;
    default: return 0;
    }
}

uint8_t input_diagonal(uint8_t vkey)
{
    switch (vkey) {
    case VK_HOME: return ARROW_UP | ARROW_LEFT;
    case VK_PGUP: return ARROW_UP | ARROW_RIGHT;
    case VK_END: return ARROW_DOWN | ARROW_LEFT;
    case VK_PGDN: return ARROW_DOWN | ARROW_RIGHT;
    default: return 0;
    }
}
