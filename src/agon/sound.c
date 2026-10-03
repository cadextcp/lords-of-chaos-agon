#include "sound.h"

#include <agon/vdp.h>
#include <stdio.h>

/* VDU 23,0,135,ch,vol,freq(ms),dur(ms) - one channel, fixed volume.
 * Frequencies here are period-style values as the VDP expects them
 * (higher value = deeper tone in the MOS envelope API). */
static void note(uint8_t vol, uint16_t period, uint16_t duration)
{
    printf("\x17\x00\x87%c%c%c%c%c", 1, vol,
           (uint8_t)(period & 0xFF), (uint8_t)(period >> 8),
           (uint8_t)(duration & 0xFF), (uint8_t)(duration >> 8));
}

static bool ready;

void sound_play(uint8_t fx)
{
    if (!ready) {                        /* channel 1, volume envelope 1 */
        printf("\x17\x00\x85%c", 1);
        ready = true;
    }
    switch (fx) {
    case SND_STEP:
        note(60, 200, 40);
        break;
    case SND_HIT:
        note(120, 90, 90);
        break;
    case SND_MISS:
        note(50, 400, 60);
        break;
    case SND_SPELL:
        note(90, 140, 120);
        break;
    case SND_PICKUP:
        note(80, 250, 70);
        break;
    case SND_PORTAL:
        note(110, 180, 200);
        break;
    case SND_DEATH:
        note(120, 300, 250);
        break;
    default:
        break;
    }
}
