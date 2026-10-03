/*
 * Title/menu music (M5d, GDD 11.6): a three-channel note sequencer on
 * the VDP audio channels 1-3 (0 carries the effects). Non-blocking:
 * the title and menu loops call music_poll(); any keypress stops it
 * (music_stop). The note data comes from /loc/music/*.bin (ADR 0011,
 * tools/gen_music.py) - an own composition, nothing copied (D7).
 */
#ifndef LOC_MUSIC_H
#define LOC_MUSIC_H

#include <stdbool.h>

#include <stdint.h>

/* Load a song and start playing. False when the file is missing or
 * invalid (silence, the caller carries on). */
bool music_start(const char *file);
void music_stop(void);
/* Feed the channels whose note is due; cheap, call it in every loop. */
void music_poll(void);
bool music_playing(void);

#endif
