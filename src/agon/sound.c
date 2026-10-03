#include "sound.h"

#include <agon/vdp.h>

/* All effects run on channel 0 (music will take 1-3, M5d). Every effect
 * picks a waveform, an ADSR envelope and a short note sequence; the
 * envelopes are disabled again afterwards so the next effect starts
 * clean. Durations are in ms, ADSR in ms/ms/percent/ms (VDP audio). */
#define FX_CHANNEL 0

/* One note on the effect channel. */
static void note(uint8_t vol, uint16_t hz, uint16_t ms)
{
    vdp_audio_play_note(FX_CHANNEL, vol, hz, ms);
}

/* Note pair/triple: the VDP queues per-channel notes back to back, so a
 * sequence plays without blocking the program. */
static void seq3(uint8_t vol, uint16_t hz1, uint16_t hz2, uint16_t hz3,
                 uint16_t ms)
{
    vdp_audio_play_note(FX_CHANNEL, vol, hz1, ms);
    vdp_audio_play_note(FX_CHANNEL, vol, hz2, ms);
    vdp_audio_play_note(FX_CHANNEL, vol, hz3, ms);
}

void sound_play(uint8_t fx)
{
    vdp_audio_volume_envelope_disable(FX_CHANNEL);
    switch (fx) {
    case SND_STEP:                       /* soft muffled thump */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_TRIANGLE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 5, 20, 40, 30);
        note(30, 160, 40);
        break;
    case SND_SWING:                      /* airy swoosh */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_NOISE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 10, 40, 30, 60);
        note(45, 900, 90);
        break;
    case SND_HIT:                        /* dull thud with a ring */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SQUARE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 2, 30, 50, 80);
        seq3(90, 220, 110, 90, 45);
        break;
    case SND_MISS:                       /* short hiss */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_VICNOISE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 5, 30, 20, 50);
        note(40, 500, 70);
        break;
    case SND_DEATH:                      /* descending groan */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SAWTOOTH);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 10, 60, 40, 120);
        seq3(95, 200, 150, 98, 90);
        break;
    case SND_SPELL:                      /* shimmering triad */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SINEWAVE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 30, 40, 60, 80);
        seq3(70, 523, 659, 784, 70);
        break;
    case SND_BOW:                        /* pluck and zip */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SAWTOOTH);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 3, 20, 30, 60);
        seq3(60, 300, 900, 1200, 35);
        break;
    case SND_THROW:                      /* whoosh upwards */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_TRIANGLE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 10, 30, 40, 70);
        seq3(55, 300, 450, 620, 45);
        break;
    case SND_PICKUP:                     /* bright blip */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SQUARE);
        note(60, 660, 60);
        note(60, 880, 50);
        break;
    case SND_DOOR:                       /* creak */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SAWTOOTH);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 40, 40, 50, 80);
        seq3(50, 90, 120, 100, 70);
        break;
    case SND_CHEST:                      /* lid pops open */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SQUARE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 5, 15, 40, 40);
        seq3(70, 180, 320, 480, 40);
        break;
    case SND_SMASH:                      /* crash */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_NOISE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 2, 60, 30, 100);
        note(90, 350, 140);
        break;
    case SND_PORTAL:                     /* rising arpeggio */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SINEWAVE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 20, 30, 70, 100);
        seq3(85, 392, 523, 659, 80);
        note(85, 784, 160);
        break;
    case SND_ROUND:                      /* two-note chime */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SINEWAVE);
        note(50, 392, 70);
        note(50, 523, 90);
        break;
    case SND_WIN:                        /* fanfare */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SQUARE);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 10, 20, 70, 120);
        seq3(90, 523, 659, 784, 100);
        note(90, 1047, 220);
        break;
    case SND_LOSE:                       /* sad descent */
        vdp_audio_set_waveform(FX_CHANNEL, VDP_AUDIO_WAVEFORM_SAWTOOTH);
        vdp_audio_volume_envelope_ADSR(FX_CHANNEL, 30, 60, 50, 200);
        seq3(80, 300, 250, 200, 130);
        break;
    default:
        break;
    }
}
