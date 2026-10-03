#include "sound.h"

#include <agon/vdp.h>

/* One note on channel 0 (enabled by default), frequency in Hz, duration in
 * ms: VDU 23,0,&85,channel,0,volume,frequency;duration; sent through the
 * agondev wrapper (never through printf: the command contains 0x00). */
static void note(uint8_t vol, uint16_t hz, uint16_t ms)
{
    vdp_audio_play_note(0, vol, hz, ms);
}

void sound_play(uint8_t fx)
{
    switch (fx) {
    case SND_STEP:
        note(40, 220, 30);
        break;
    case SND_HIT:
        note(100, 110, 90);
        break;
    case SND_MISS:
        note(50, 330, 60);
        break;
    case SND_SPELL:
        note(80, 440, 120);
        break;
    case SND_PICKUP:
        note(70, 660, 70);
        break;
    case SND_PORTAL:
        note(90, 523, 200);
        break;
    case SND_DEATH:
        note(100, 98, 250);
        break;
    default:
        break;
    }
}
