/*
 * Menu music and jingles (GDD 11.6, ADR 0012): a note sequencer for up to
 * four song channels on the VDP channels 1, 2, 3 and 5 (0 and 4 carry the
 * effects). Instruments are waveforms with an ADSR envelope or tunable
 * samples from /loc/sfx.bin. Non-blocking: the loops call audio_poll().
 * Notes are sent one at a time when due - the VDP drops a note sent to a
 * busy channel (QUIRK A1). Song data: /loc/music/NAME.bin from
 * tools/gen_music.py - own compositions, nothing copied (D7).
 */
#ifndef LOC_MUSIC_H
#define LOC_MUSIC_H

#include <stdbool.h>

#include <stdint.h>

/* Load a song and start playing (looping or once, as the file says).
 * False when music is switched off, or the file is missing or invalid. */
bool music_start(const char *file);
void music_stop(void);
/* Feed the channels whose note is due; cheap, call it in every loop. */
void music_poll(void);
bool music_playing(void);
/* music_poll + sound_poll: what every waiting loop calls. */
void audio_poll(void);

#endif
