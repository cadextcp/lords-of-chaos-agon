/*
 * Sound effects over the Agon audio system (GDD 11.4, M4j): short notes
 * on channel 0 through the agondev wrapper vdp_audio_play_note
 * (VDU 23,0,&85,...).
 */
#ifndef LOC_SOUND_H
#define LOC_SOUND_H

#include <stdint.h>

typedef enum {
    SND_STEP,        /* footstep */
    SND_HIT,         /* melee hit */
    SND_MISS,        /* melee miss */
    SND_SPELL,       /* spell cast */
    SND_PICKUP,      /* object taken */
    SND_PORTAL,      /* escape / portal open */
    SND_DEATH        /* a unit died */
} SoundFx;

void sound_play(uint8_t fx);

#endif
