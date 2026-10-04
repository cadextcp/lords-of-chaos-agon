#include "sound.h"

#include <agon/mos.h>
#include <agon/vdp.h>
#include <string.h>

#include "../core/gen/sfx.h"

/* Two effect channels so that e.g. a hit and a crit clang overlap;
 * music takes 1-3 and 5 (music.c). */
#define FX_VOICES 2
static const uint8_t FX_CH[FX_VOICES] = {0, 4};
#define SFX_BUFFER_BASE 0x6000
#define NO_SAMPLE 0xFF
#define STEPS 3

#define SQ VDP_AUDIO_WAVEFORM_SQUARE
#define TRI VDP_AUDIO_WAVEFORM_TRIANGLE
#define SAW VDP_AUDIO_WAVEFORM_SAWTOOTH
#define SIN VDP_AUDIO_WAVEFORM_SINEWAVE
#define NOI VDP_AUDIO_WAVEFORM_NOISE

/* One step: a sample, or (sample = NO_SAMPLE, or the file is missing) a
 * waveform tone of hz/ms. vol 0 ends the effect. */
typedef struct {
    uint8_t sample;
    uint8_t wave;
    uint8_t vol;
    uint16_t hz;
    uint16_t ms;
} Step;

typedef struct {
    uint8_t prio;
    Step step[STEPS];
} Effect;

static const Effect EFFECTS[SND_COUNT] = {
    [SND_STEP]      = {1, {{SFX_STEP, TRI, 40, 160, 50}}},
    [SND_HIT]       = {3, {{SFX_THUD, SQ, 110, 110, 90}}},
    [SND_MISS]      = {2, {{SFX_WHOOSH, NOI, 55, 500, 80}}},
    [SND_SPELL]     = {3, {{SFX_SPARKLE, SIN, 75, 784, 150}}},
    [SND_PICKUP]    = {2, {{SFX_BLIP, SQ, 60, 880, 60}}},
    [SND_PORTAL]    = {4, {{SFX_SUMMON, SIN, 90, 523, 200},
                           {NO_SAMPLE, SIN, 70, 784, 220}}},
    [SND_DEATH]     = {4, {{SFX_GROAN, SAW, 100, 150, 300}}},
    [SND_SWING]     = {2, {{SFX_WHOOSH, NOI, 70, 900, 90}}},
    [SND_BOW]       = {2, {{NO_SAMPLE, TRI, 60, 700, 30},
                           {SFX_WHOOSH, NOI, 55, 1200, 60}}},
    [SND_THROW]     = {2, {{SFX_WHOOSH, NOI, 65, 600, 80}}},
    [SND_DOOR]      = {2, {{SFX_CREAK, SAW, 65, 100, 250}}},
    [SND_CHEST]     = {2, {{SFX_LID, SQ, 85, 320, 80}}},
    [SND_SMASH]     = {3, {{SFX_CRASH, NOI, 105, 350, 200}}},
    [SND_ROUND]     = {1, {{NO_SAMPLE, SIN, 55, 392, 80},
                           {NO_SAMPLE, SIN, 55, 523, 120}}},
    [SND_WIN]       = {5, {{NO_SAMPLE, SQ, 70, 523, 110},
                           {NO_SAMPLE, SQ, 70, 659, 110},
                           {NO_SAMPLE, SQ, 70, 1047, 260}}},
    [SND_LOSE]      = {5, {{NO_SAMPLE, SAW, 65, 300, 160},
                           {NO_SAMPLE, SAW, 65, 250, 160},
                           {NO_SAMPLE, SAW, 65, 200, 300}}},
    [SND_CRIT]      = {4, {{SFX_CLANG, SQ, 110, 1200, 150}}},
    [SND_BOLT]      = {3, {{SFX_ZAP, SQ, 85, 900, 150}}},
    [SND_LIGHTNING] = {4, {{SFX_THUNDER, NOI, 115, 200, 400}}},
    [SND_SUMMON]    = {3, {{SFX_SUMMON, SIN, 90, 440, 300}}},
    [SND_TELEPORT]  = {3, {{SFX_ZAP, SQ, 60, 1500, 80},
                           {SFX_SPARKLE, SIN, 75, 1047, 150}}},
    [SND_CURSE]     = {3, {{NO_SAMPLE, SAW, 60, 220, 120},
                           {NO_SAMPLE, SAW, 60, 165, 200}}},
    [SND_DRINK]     = {2, {{SFX_BUBBLE, SIN, 75, 400, 150}}},
    [SND_EAT]       = {1, {{SFX_THUD, TRI, 45, 200, 60},
                           {SFX_THUD, TRI, 45, 170, 60}}},
    [SND_FLY]       = {1, {{SFX_WHOOSH, NOI, 50, 700, 80}}},
    [SND_MENU]      = {0, {{NO_SAMPLE, SIN, 40, 660, 25}}},
    [SND_CONFIRM]   = {1, {{SFX_BLIP, SIN, 55, 880, 60}}},
    [SND_BACK]      = {0, {{NO_SAMPLE, SIN, 40, 440, 30},
                           {NO_SAMPLE, SIN, 40, 330, 40}}},
    [SND_ERROR]     = {1, {{NO_SAMPLE, SQ, 45, 140, 120}}},
};

typedef struct {
    bool active;
    uint8_t fx;
    uint8_t step;
    uint32_t next;                       /* getsysvar_time of the next step */
} Voice;

static Voice voices[FX_VOICES];
static bool loaded[SFX_COUNT];
static uint16_t sample_ms[SFX_COUNT];
static uint16_t sample_base[SFX_COUNT];  /* tunable: pitch of the recording */

bool sound_on = true;
bool music_on = true;

uint16_t sound_sample_buffer(uint8_t sfx)
{
    return sfx < SFX_COUNT && loaded[sfx] ? (uint16_t)(SFX_BUFFER_BASE + sfx) : 0xFFFF;
}

uint16_t sound_sample_ms(uint8_t sfx, uint16_t hz)
{
    if (sfx >= SFX_COUNT || !loaded[sfx])
        return 0;
    if (!sample_base[sfx] || !hz)
        return sample_ms[sfx];
    /* a tunable sample plays faster when pitched up */
    return (uint16_t)((uint32_t)sample_ms[sfx] * sample_base[sfx] / hz);
}

bool sound_init(void)
{
    uint8_t fh, head[6], i, count;
    uint8_t chunk[256];
    for (i = 4; i <= 9; i++)             /* effects 0, 4; music 1-3, 5-9 */
        vdp_audio_enable_channel(i);
    memset(loaded, 0, sizeof loaded);
    fh = mos_fopen("sfx.bin", FA_READ);
    if (!fh)
        return false;
    if (mos_fread(fh, (char *)head, 6) != 6 || memcmp(head, "LOCX", 4) != 0 ||
        head[4] != 1) {
        mos_fclose(fh);
        return false;
    }
    count = head[5] < SFX_COUNT ? head[5] : SFX_COUNT;
    for (i = 0; i < count; i++) {
        uint8_t e[5];
        uint16_t base, len, left;
        uint16_t id = (uint16_t)(SFX_BUFFER_BASE + i);
        if (mos_fread(fh, (char *)e, 5) != 5)
            break;
        base = (uint16_t)(e[1] | (e[2] << 8));
        len = (uint16_t)(e[3] | (e[4] << 8));
        vdp_adv_clear_buffer(id);
        for (left = len; left;) {
            uint16_t n = left < sizeof chunk ? left : (uint16_t)sizeof chunk;
            if (mos_fread(fh, (char *)chunk, n) != n) {
                mos_fclose(fh);
                return false;
            }
            vdp_adv_write_block_data(id, n, (char *)chunk);
            left = (uint16_t)(left - n);
        }
        vdp_adv_consolidate(id);         /* one block (QUIRK S1) */
        vdp_audio_create_sample_from_buffer(0, id,
            VDP_AUDIO_SAMPLE_FORMAT_8BIT_SIGNED |
            ((e[0] & 1) ? VDP_AUDIO_SAMPLE_FORMAT_SAMPLE_TUNEABLE : 0));
        if (e[0] & 1)
            vdp_audio_set_buffer_frequency(0, id, base);
        sample_ms[i] = (uint16_t)(len / 16);   /* 16 kHz */
        sample_base[i] = (e[0] & 1) ? base : 0;
        loaded[i] = true;
    }
    mos_fclose(fh);
    return true;
}

/* Send step v->step of the voice's effect; false when the effect ends. */
static bool voice_step(Voice *v, uint8_t ch)
{
    const Step *s;
    uint16_t ms;
    if (v->step >= STEPS)
        return false;
    s = &EFFECTS[v->fx].step[v->step];
    if (s->vol == 0)
        return false;
    /* the VDP starts notes with up to ~150 ms latency: the previous step
     * may still sound, and a busy channel drops the note (A1) - a reset
     * frees it at once (vdptest A6/A9) */
    vdp_audio_reset_channel(ch);
    vdp_audio_volume_envelope_disable(ch);
    if (s->sample != NO_SAMPLE && loaded[s->sample]) {
        ms = sample_ms[s->sample];
        vdp_audio_set_sample(ch, SFX_BUFFER_BASE + s->sample);
        vdp_audio_play_note(ch, s->vol, 0, ms);
    } else {
        ms = s->ms;
        vdp_audio_set_waveform(ch, s->wave);
        vdp_audio_play_note(ch, s->vol, s->hz, ms);
    }
    /* the next note is only accepted once this one ended (QUIRK A1):
     * round up to the 2-cs clock and keep one tick of margin */
    v->next = getsysvar_time() + ms / 10 + 3;
    v->step++;
    return true;
}

void sound_play(uint8_t fx)
{
    uint8_t i, pick = FX_VOICES;
    uint32_t now = getsysvar_time();
    if (!sound_on || fx >= SND_COUNT)
        return;
    for (i = 0; i < FX_VOICES; i++)       /* a channel that is silent */
        if (!voices[i].active && (int32_t)(now - voices[i].next) >= 0) {
            pick = i;
            break;
        }
    if (pick == FX_VOICES) {              /* else the weakest, if not stronger */
        uint8_t best = 0xFF;
        for (i = 0; i < FX_VOICES; i++) {
            uint8_t p = voices[i].active ? EFFECTS[voices[i].fx].prio : 0;
            if (p <= EFFECTS[fx].prio && p < best) {
                best = p;
                pick = i;
            }
        }
        if (pick == FX_VOICES)
            return;                       /* voice_step resets the channel */
    }
    voices[pick].active = true;
    voices[pick].fx = fx;
    voices[pick].step = 0;
    if (!voice_step(&voices[pick], FX_CH[pick]))
        voices[pick].active = false;
}

void sound_poll(void)
{
    uint8_t i;
    uint32_t now = getsysvar_time();
    for (i = 0; i < FX_VOICES; i++) {
        Voice *v = &voices[i];
        if (!v->active || (int32_t)(now - v->next) < 0)
            continue;
        if (!voice_step(v, FX_CH[i]))
            v->active = false;            /* next still guards the tail */
    }
}

/* ---------- settings ---------- */

void sound_settings_load(void)
{
    uint8_t fh, b[6];
    fh = mos_fopen("settings.dat", FA_READ);
    if (!fh)
        return;
    if (mos_fread(fh, (char *)b, 6) == 6 && memcmp(b, "LOCP", 4) == 0) {
        music_on = (b[4] & 1) != 0;
        sound_on = (b[4] & 2) != 0;
    }
    mos_fclose(fh);
}

void sound_settings_save(void)
{
    uint8_t fh, b[6] = {'L', 'O', 'C', 'P', 0, 1};
    b[4] = (uint8_t)((music_on ? 1 : 0) | (sound_on ? 2 : 0));
    fh = mos_fopen("settings.dat", FA_WRITE | FA_CREATE_ALWAYS);
    if (!fh)
        return;
    mos_fwrite(fh, (char *)b, 6);
    mos_fclose(fh);
}
