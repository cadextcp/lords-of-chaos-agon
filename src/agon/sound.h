/*
 * Sound effects over the Agon audio system (GDD 11.4, M5c): channel 0
 * carries the effects, 1-3 stay free for music (M5d). Waveforms, ADSR
 * envelopes and short note sequences through the agondev wrappers
 * (VDU 23,0,&85,...) - never through printf: the commands contain 0x00.
 */
#ifndef LOC_SOUND_H
#define LOC_SOUND_H

#include <stdint.h>

typedef enum {
    SND_STEP,        /* footstep */
    SND_HIT,         /* melee/spell hit connects */
    SND_MISS,        /* attack whiffs */
    SND_SPELL,       /* spell cast */
    SND_PICKUP,      /* object taken */
    SND_PORTAL,      /* escape through the portal */
    SND_DEATH,       /* a unit died */
    SND_SWING,       /* blade swung */
    SND_BOW,         /* arrow released */
    SND_THROW,       /* something thrown */
    SND_DOOR,        /* door creaks open */
    SND_CHEST,       /* chest pried open */
    SND_SMASH,       /* terrain smashed */
    SND_ROUND,       /* new round chime */
    SND_WIN,         /* end screen: victory */
    SND_LOSE         /* end screen: defeat */
} SoundFx;

void sound_play(uint8_t fx);

#endif
