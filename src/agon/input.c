#include "input.h"

#include "../core/chord.h"

/* Movement by arrow keys only (GDD 5.2): WASD is NOT mapped - w, a, s
 * and d are action keys there (wield, plus d = drop; a/s unused), and
 * the arrow branch of the event loop would shadow them. */
uint8_t input_arrow(uint8_t vkey)
{
    switch (vkey) {
    case VK_UP: return ARROW_UP;
    case VK_DOWN: return ARROW_DOWN;
    case VK_LEFT: return ARROW_LEFT;
    case VK_RIGHT: return ARROW_RIGHT;
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
