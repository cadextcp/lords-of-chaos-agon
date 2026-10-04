/*
 * Sound effects over the Agon audio system (GDD 11.4, ADR 0012).
 *
 * The VDP does not queue notes: a note sent to a busy channel is dropped
 * (QUIRK A1). Effects are therefore short step lists that sound_poll()
 * plays one step at a time on two effect channels (0 and 4); a new
 * effect takes a free channel or interrupts one of equal or lower
 * priority (channel reset). Steps are 8-bit samples from /loc/sfx.bin
 * (tools/gen_sfx.py) with a waveform tone as fallback when the file is
 * missing. Never send audio through printf: the commands contain 0x00.
 */
#ifndef LOC_SOUND_H
#define LOC_SOUND_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SND_STEP,        /* footstep */
    SND_HIT,         /* melee/spell hit connects */
    SND_MISS,        /* attack whiffs */
    SND_SPELL,       /* spell cast (generic) */
    SND_PICKUP,      /* object taken */
    SND_PORTAL,      /* escape through the portal / portal opens */
    SND_DEATH,       /* a unit died */
    SND_SWING,       /* blade swung */
    SND_BOW,         /* arrow released */
    SND_THROW,       /* something thrown */
    SND_DOOR,        /* door creaks open */
    SND_CHEST,       /* chest pried open */
    SND_SMASH,       /* terrain smashed */
    SND_ROUND,       /* new round chime */
    SND_WIN,         /* end screen: victory (fallback; the jingle is music) */
    SND_LOSE,        /* end screen: defeat */
    SND_CRIT,        /* critical hit: metal clang */
    SND_BOLT,        /* magic bolt */
    SND_LIGHTNING,   /* magic lightning */
    SND_SUMMON,      /* a creature appears */
    SND_TELEPORT,    /* teleport */
    SND_CURSE,       /* curse / subversion */
    SND_DRINK,       /* potion or cauldron drunk, brewing */
    SND_EAT,         /* food eaten */
    SND_FLY,         /* take off / land / mount */
    SND_MENU,        /* menu cursor moved */
    SND_CONFIRM,     /* menu choice taken */
    SND_BACK,        /* menu left with Esc */
    SND_ERROR,       /* an action was refused */
    SND_COUNT
} SoundFx;

/* Enable the extra channels and stream /loc/sfx.bin into VDP buffers.
 * False when the file is missing (effects fall back to waveforms). */
bool sound_init(void);
void sound_play(uint8_t fx);
/* Advance the running effects; cheap, call it in every loop. */
void sound_poll(void);
/* The sample id loaded into VDP buffer form, or 0xFFFF when missing
 * (music.c builds its instruments from them). */
uint16_t sound_sample_buffer(uint8_t sfx);
/* How long a sample sounds at pitch hz (0 = recorded pitch); 0 when the
 * sample is missing. */
uint16_t sound_sample_ms(uint8_t sfx, uint16_t hz);

/* Player settings (setup menu), kept in /loc/settings.dat. */
extern bool sound_on;
extern bool music_on;
void sound_settings_load(void);
void sound_settings_save(void);

#endif
