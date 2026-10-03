#include "music.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <string.h>

#define MUSIC_CH 3            /* channels 1..3 of the VDP */
#define MUSIC_NOTES 96        /* per channel; longer tracks are rejected */
#define GAP_MS 15             /* silence between successive notes */

typedef struct {
    uint8_t wave;             /* VDP_AUDIO_WAVEFORM_* */
    uint8_t vol;
    uint8_t count;
    uint8_t pos;
    uint32_t next;            /* getsysvar_time when the next note is due */
    uint16_t freq[MUSIC_NOTES];
    uint8_t units[MUSIC_NOTES];
} MusicChannel;

static MusicChannel chans[MUSIC_CH];
static uint16_t ms_unit;
static bool playing;

/* Channel n of the song plays on VDP audio channel n+1. */
static void play_note(uint8_t ch, uint16_t freq, uint16_t ms)
{
    vdp_audio_play_note((int)(ch + 1), chans[ch].vol, freq, ms);
}

bool music_start(const char *file)
{
    static uint8_t buf[MUSIC_CH * (4 + MUSIC_NOTES * 3) + 8];
    uint8_t fh, nch, c, i;
    uint16_t off, count;
    uint24_t len;
    music_stop();
    fh = mos_fopen(file, FA_READ);
    if (!fh)
        return false;
    len = mos_fread(fh, (char *)buf, (uint24_t)sizeof buf);
    mos_fclose(fh);
    if (len < 8 || memcmp(buf, "LOCM", 4) != 0 || buf[4] != 1)
        return false;
    ms_unit = (uint16_t)(buf[5] | (buf[6] << 8));
    nch = buf[7];
    if (nch == 0 || nch > MUSIC_CH || ms_unit == 0)
        return false;
    off = 8;
    for (c = 0; c < nch; c++) {
        MusicChannel *m = &chans[c];
        uint16_t k;
        if (off + 4 > len)
            return false;
        m->wave = buf[off];
        m->vol = buf[off + 1];
        count = (uint16_t)(buf[off + 2] | (buf[off + 3] << 8));
        off += 4;
        if (count == 0 || count > MUSIC_NOTES || off + (uint24_t)count * 3 > len)
            return false;
        m->count = (uint8_t)count;
        for (k = 0; k < count; k++) {
            m->freq[k] = (uint16_t)(buf[off] | (buf[off + 1] << 8));
            m->units[k] = buf[off + 2];
            off += 3;
        }
        m->pos = 0;
        m->next = getsysvar_time();
        vdp_audio_set_waveform((int)(c + 1), m->wave);
        vdp_audio_volume_envelope_disable((int)(c + 1));
    }
    for (i = nch; i < MUSIC_CH; i++)
        chans[i].count = 0;
    playing = true;
    return true;
}

void music_stop(void)
{
    uint8_t c;
    if (!playing)
        return;
    playing = false;
    for (c = 0; c < MUSIC_CH; c++) {
        chans[c].count = 0;
        vdp_audio_play_note((int)(c + 1), 0, 0, 10);   /* fade the tail */
    }
}

void music_poll(void)
{
    uint8_t c;
    if (!playing)
        return;
    for (c = 0; c < MUSIC_CH; c++) {
        MusicChannel *m = &chans[c];
        uint16_t ms;
        if (m->count == 0)
            continue;
        if ((int32_t)(getsysvar_time() - m->next) < 0)
            continue;
        ms = (uint16_t)(m->units[m->pos] * ms_unit);
        if (ms < GAP_MS * 2)
            ms = GAP_MS * 2;
        if (m->freq[m->pos])
            play_note(c, m->freq[m->pos], (uint16_t)(ms - GAP_MS));
        m->next += m->units[m->pos] * ms_unit;
        m->pos = (uint8_t)((m->pos + 1) % m->count);
    }
}

bool music_playing(void)
{
    return playing;
}
