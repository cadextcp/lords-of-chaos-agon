#include "music.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <string.h>

#include "sound.h"

#define MUSIC_CH 4            /* song channels -> VDP channels 1, 2, 3, 5 */
#define MUSIC_NOTES 160       /* per channel; longer tracks are rejected */
#define TAIL_MS 30            /* a note ends this early: the VDP drops a
                                 note sent to a busy channel (QUIRK A1) */
#define INSTR_SAMPLE 0x80     /* instrument byte: sample id | 0x80 */

static const uint8_t VDP_CH[MUSIC_CH] = {1, 2, 3, 5};

typedef struct {
    uint8_t vol;
    uint8_t count;
    uint8_t pos;
    bool done;
    uint32_t next_ms;         /* song time when the next note is due */
    uint16_t freq[MUSIC_NOTES];
    uint8_t units[MUSIC_NOTES];
} MusicChannel;

static MusicChannel chans[MUSIC_CH];
static uint8_t nch;
static uint16_t ms_unit;
static bool looping;
static bool playing;
static uint32_t start_cs;

/* Instrument of one channel: a sample (when loaded) or a waveform with
 * an ADSR envelope (attack/decay/release in 4 ms steps, sustain level). */
static void setup_channel(uint8_t c, const uint8_t *d)
{
    uint8_t ch = VDP_CH[c];
    uint16_t buf = 0xFFFF;
    vdp_audio_reset_channel(ch);
    if (d[0] & INSTR_SAMPLE)
        buf = sound_sample_buffer((uint8_t)(d[0] & 0x7F));
    if (buf != 0xFFFF) {
        vdp_audio_set_sample(ch, buf);
        vdp_audio_volume_envelope_disable(ch);
    } else {
        vdp_audio_set_waveform(ch, (d[0] & INSTR_SAMPLE) ? d[1] : d[0]);
        if (d[3] | d[4] | d[6])
            vdp_audio_volume_envelope_ADSR(ch, d[3] * 4, d[4] * 4, d[5], d[6] * 4);
        else
            vdp_audio_volume_envelope_disable(ch);
    }
}

bool music_start(const char *file)
{
    static uint8_t buf[512];              /* streamed: RAM is tight (S6) */
    uint8_t fh, c;
    music_stop();
    if (!music_on)
        return false;
    fh = mos_fopen(file, FA_READ);
    if (!fh)
        return false;
    if (mos_fread(fh, (char *)buf, 9) != 9 || memcmp(buf, "LOCM", 4) != 0 ||
        buf[4] != 2) {
        mos_fclose(fh);
        return false;
    }
    ms_unit = (uint16_t)(buf[5] | (buf[6] << 8));
    nch = buf[7];
    looping = (buf[8] & 1) != 0;
    if (nch == 0 || nch > MUSIC_CH || ms_unit == 0) {
        mos_fclose(fh);
        return false;
    }
    for (c = 0; c < nch; c++) {
        MusicChannel *m = &chans[c];
        uint8_t head[9];
        uint16_t count, k;
        if (mos_fread(fh, (char *)head, 9) != 9) {
            mos_fclose(fh);
            return false;
        }
        count = (uint16_t)(head[7] | (head[8] << 8));
        if (count == 0 || count > MUSIC_NOTES) {
            mos_fclose(fh);
            return false;
        }
        setup_channel(c, head);
        m->vol = head[2];
        m->count = (uint8_t)count;
        for (k = 0; k < count;) {         /* notes in pieces of 170 */
            uint16_t n = (uint16_t)(count - k) > 170 ? 170 : (uint16_t)(count - k);
            uint16_t j;
            if (mos_fread(fh, (char *)buf, (uint24_t)n * 3) != (uint24_t)n * 3) {
                mos_fclose(fh);
                return false;
            }
            for (j = 0; j < n; j++, k++) {
                m->freq[k] = (uint16_t)(buf[j * 3] | (buf[j * 3 + 1] << 8));
                m->units[k] = buf[j * 3 + 2];
            }
        }
        m->pos = 0;
        m->done = false;
        m->next_ms = 0;
    }
    mos_fclose(fh);
    start_cs = getsysvar_time();
    playing = true;
    music_poll();
    return true;
}

void music_stop(void)
{
    uint8_t c;
    if (!playing)
        return;
    playing = false;
    for (c = 0; c < nch; c++)
        vdp_audio_reset_channel(VDP_CH[c]);   /* silence at once */
}

void music_poll(void)
{
    uint8_t c;
    bool any = false;
    uint32_t now_ms;
    if (!playing)
        return;
    /* song time in ms from the centisecond clock (QUIRK A5, T6) */
    now_ms = (getsysvar_time() - start_cs) * 10;
    for (c = 0; c < nch; c++) {
        MusicChannel *m = &chans[c];
        uint16_t ms;
        if (m->done)
            continue;
        any = true;
        if (now_ms < m->next_ms)
            continue;
        ms = (uint16_t)(m->units[m->pos] * ms_unit);
        if (m->freq[m->pos])
            vdp_audio_play_note(VDP_CH[c], m->vol, m->freq[m->pos],
                                ms > TAIL_MS * 2 ? ms - TAIL_MS : ms / 2);
        m->next_ms += ms;
        if (++m->pos >= m->count) {
            if (looping)
                m->pos = 0;
            else
                m->done = true;
        }
    }
    if (!any)
        playing = false;                  /* a jingle has ended */
}

bool music_playing(void)
{
    return playing;
}

void audio_poll(void)
{
    music_poll();
    sound_poll();
}
