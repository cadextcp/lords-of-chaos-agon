#include "music.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <string.h>

#include "sound.h"

#define MUSIC_CH 4            /* song channels, two VDP channels each */
#define MUSIC_NOTES 160       /* per channel; longer tracks are rejected */
#define TAIL_MS 30            /* waveform notes end this early */
#define LATENCY_MS 150        /* the VDP starts a note up to ~150 ms after
                                 it was sent (audio buffering, measured) */
#define INSTR_SAMPLE 0x80     /* instrument byte: sample id | 0x80 */

/* The VDP drops a note sent to a busy channel (QUIRK A1), and a sample
 * sounds to its end whatever the note length. Every song channel
 * therefore alternates between two VDP channels: a note may ring out
 * while the next one starts. Effects use 0 and 4. */
static const uint8_t VDP_CH[MUSIC_CH][2] = {{1, 6}, {2, 7}, {3, 8}, {5, 9}};

typedef struct {
    uint8_t head[7];          /* instrument, fallback, volume, ADSR */
    uint8_t count;
    uint8_t pos;
    bool done;
    uint8_t last;             /* which of the two VDP channels played last */
    uint32_t busy[2];         /* song ms until each VDP channel is free */
    uint16_t pend_hz;         /* a note held back after a reset (0 = none) */
    uint16_t pend_ms;
    uint32_t next_ms;         /* song ms when the next note is due */
    uint16_t freq[MUSIC_NOTES];
    uint8_t units[MUSIC_NOTES];
} MusicChannel;

static MusicChannel chans[MUSIC_CH];
static uint8_t nch;
static uint16_t ms_unit;
static bool looping;
static bool playing;
static uint32_t start_cs;

static bool is_sample(const MusicChannel *m)
{
    return (m->head[0] & INSTR_SAMPLE) &&
           sound_sample_buffer((uint8_t)(m->head[0] & 0x7F)) != 0xFFFF;
}

/* Instrument on one VDP channel: a sample (when loaded) or a waveform
 * with an ADSR envelope (attack/decay/release in 4 ms steps). */
static void setup_vdp(const MusicChannel *m, uint8_t ch)
{
    const uint8_t *d = m->head;
    vdp_audio_reset_channel(ch);
    if (is_sample(m)) {
        vdp_audio_set_sample(ch, sound_sample_buffer((uint8_t)(d[0] & 0x7F)));
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
    static uint8_t buf[510];              /* streamed: RAM is tight (S6) */
    uint8_t fh, c;
    music_stop();
    if (!music_on || !sound_channels)     /* channels 3-9 need sound_init */
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
        memcpy(m->head, head, sizeof m->head);
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
        setup_vdp(m, VDP_CH[c][0]);
        setup_vdp(m, VDP_CH[c][1]);
        m->pos = 0;
        m->done = false;
        m->last = 1;
        m->busy[0] = m->busy[1] = 0;
        m->pend_hz = 0;
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
    for (c = 0; c < nch; c++) {           /* silence at once */
        vdp_audio_reset_channel(VDP_CH[c][0]);
        vdp_audio_reset_channel(VDP_CH[c][1]);
    }
}

/* Send one note on the VDP channel that is free (alternating). When both
 * still ring, the one that ends first is reset and the note follows on
 * the next poll: a note sent right after resetting a channel that plays
 * a sample is sometimes dropped (measured, ADR 0012). */
static void play(MusicChannel *m, uint8_t c, uint16_t hz, uint16_t slot_ms,
                 uint32_t now_ms)
{
    uint8_t k = (uint8_t)(m->last ^ 1);
    uint16_t play_ms, ring_ms;
    if (m->busy[k] > now_ms) {
        if (m->busy[m->last] < m->busy[k])
            k = m->last;
        if (m->busy[k] > now_ms) {
            setup_vdp(m, VDP_CH[c][k]);
            m->busy[k] = 0;
            m->last = (uint8_t)(k ^ 1);   /* so that k is picked next */
            m->pend_hz = hz;
            m->pend_ms = slot_ms;
            return;
        }
    }
    if (is_sample(m)) {
        /* let it ring into the next note, but end before the channel is
         * used again - at the earliest one unit after the next note */
        uint16_t cap = (uint16_t)(slot_ms + ms_unit - LATENCY_MS);
        ring_ms = sound_sample_ms((uint8_t)(m->head[0] & 0x7F), hz);
        play_ms = ring_ms < cap ? ring_ms : cap;
        ring_ms = play_ms;
    } else {
        play_ms = slot_ms > TAIL_MS * 2 ? (uint16_t)(slot_ms - TAIL_MS)
                                        : (uint16_t)(slot_ms / 2);
        ring_ms = (uint16_t)(play_ms + m->head[6] * 4);   /* + release */
    }
    vdp_audio_play_note(VDP_CH[c][k], m->head[2], hz, play_ms);
    m->busy[k] = now_ms + ring_ms + LATENCY_MS;
    m->last = k;
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
        uint16_t slot;
        if (m->done)
            continue;
        any = true;
        if (m->pend_hz) {                  /* held back by a reset */
            uint16_t hz = m->pend_hz;
            m->pend_hz = 0;
            play(m, c, hz, m->pend_ms, now_ms);
        }
        if (now_ms < m->next_ms)
            continue;
        slot = (uint16_t)(m->units[m->pos] * ms_unit);
        if (m->freq[m->pos])
            play(m, c, m->freq[m->pos], slot, now_ms);
        m->next_ms += slot;
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
