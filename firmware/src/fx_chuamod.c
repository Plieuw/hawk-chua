/* SPDX-License-Identifier: GPL-3.0-only
 * Chua modulation effects: ring modulation, single-sideband frequency shift,
 * and tremolo. All audio and modulation samples are signed Q15 in int32_t.
 *
 * The caller supplies mod[] from the Chua oscillator. For AUDIO RATE, mod[]
 * advances once per output sample. For HALF RATE, the caller advances the
 * Chua source at half its normal time step. For TREMOLO, it supplies the same
 * Chua source run at a slow time scale (the LFO rate). This keeps one source
 * waveform across all three effects and avoids a second, unrelated oscillator.
 *
 * Frequency shifting uses analytic signals made by a 127-tap Blackman-windowed
 * Hilbert FIR. At 48 kHz its useful quadrature band begins around 750 Hz.
 * The 63-sample group delay is present in all wet modes to keep switching aligned.
 *
 * API:
 *   fx_chuamod_reset()
 *   fx_chuamod_process(in, mod, out, n, mode, rectify, mix, depth)
 *
 * mode:    0 ring, 1 shift up, 2 shift down, 3 tremolo
 * rectify: 0 original, 1 full-wave, 2 positive half-wave, 3 negative half-wave
 * mix/depth: Q15, 0..32768
 */
#include <stdint.h>
#include <string.h>

#define CM_N        127u
#define CM_DELAY     63u
#define CM_DC_A      22       /* one-pole DC estimate: about 5 Hz at 48 kHz */
#define CM_GAIN_FULL 150339   /* 2.294 x, Q16: restore sine RMS after full-wave */
#define CM_GAIN_HALF 121242   /* 1.850 x, Q16: restore sine RMS after half-wave */

static const int16_t cm_hilbert_coeff[CM_N] = {
    0, 0, 0, 0, -1, 0, -3, 0,
    -6, 0, -10, 0, -15, 0, -22, 0,
    -31, 0, -42, 0, -56, 0, -74, 0,
    -96, 0, -122, 0, -154, 0, -192, 0,
    -237, 0, -291, 0, -355, 0, -430, 0,
    -519, 0, -626, 0, -754, 0, -909, 0,
    -1102, 0, -1348, 0, -1675, 0, -2133, 0,
    -2834, 0, -4067, 0, -6890, 0, -20839, 0,
    20839, 0, 6890, 0, 4067, 0, 2834, 0,
    2133, 0, 1675, 0, 1348, 0, 1102, 0,
    909, 0, 754, 0, 626, 0, 519, 0,
    430, 0, 355, 0, 291, 0, 237, 0,
    192, 0, 154, 0, 122, 0, 96, 0,
    74, 0, 56, 0, 42, 0, 31, 0,
    22, 0, 15, 0, 10, 0, 6, 0,
    3, 0, 1, 0, 0, 0, 0
};

typedef struct {
    int32_t x[CM_N];
    int32_t m[CM_N];
    int32_t dc_audio;
    int32_t dc_lfo;
    uint8_t w;
} fx_chuamod_state_t;

static fx_chuamod_state_t cm_state;

static int32_t cm_clip15(int64_t x)
{
    if (x > 32767) return 32767;
    if (x < -32768) return -32768;
    return (int32_t)x;
}

static int32_t cm_rectify(int32_t x, uint32_t mode)
{
    switch (mode) {
    case 1: return x < 0 ? -x : x;
    case 2: return x > 0 ? x : 0;
    case 3: return x < 0 ? x : 0;
    default: return x;
    }
}

/* Remove rectifier DC and restore the RMS of a sine-derived rectified wave.
 * Raw Chua output is already DC-blocked in the voice renderer. Tremolo uses
 * a much slower tracker so sub-Hz modulation is not mistaken for DC. */
static int32_t cm_prepare_mod(int32_t x, uint32_t rectify, uint32_t slow)
{
    int32_t y, gain;
    x = cm_rectify(x, rectify);
    if (rectify == 0u)
        return x;
    if (slow) {
        cm_state.dc_lfo += (int32_t)(((int64_t)(x - cm_state.dc_lfo)) >> 15);
        y = x - cm_state.dc_lfo;
    } else {
        cm_state.dc_audio += (int32_t)(((int64_t)(x - cm_state.dc_audio) * CM_DC_A) >> 15);
        y = x - cm_state.dc_audio;
    }
    gain = rectify == 1u ? CM_GAIN_FULL : CM_GAIN_HALF;
    return cm_clip15(((int64_t)y * gain) >> 16);
}

/* FIR output is Q15. w points at the newest sample. */
static int32_t cm_hilbert_run(const int32_t *h, uint32_t w)
{
    int64_t acc = 0;
    uint32_t k, p = w;
    for (k = 0; k < CM_N; ++k) {
        acc += (int64_t)h[p] * cm_hilbert_coeff[k];
        p = p ? p - 1u : CM_N - 1u;
    }
    return cm_clip15(acc >> 15);
}

static int32_t cm_delayed(const int32_t *h, uint32_t w)
{
    uint32_t p = w >= CM_DELAY ? w - CM_DELAY : w + CM_N - CM_DELAY;
    return h[p];
}

static void fx_chuamod_reset(void)
{
    memset(&cm_state, 0, sizeof cm_state);
}

static void fx_chuamod_process(const int32_t *in, const int32_t *mod,
                               int32_t *out, uint32_t n,
                               uint32_t mode, uint32_t rectify,
                               uint32_t mix, uint32_t depth)
{
    uint32_t i;
    if (mix > 32768u) mix = 32768u;
    if (depth > 32768u) depth = 32768u;
    mode %= 4u;
    rectify %= 4u;

    for (i = 0; i < n; ++i) {
        uint32_t w = cm_state.w;
        int32_t x = in[i];
        int32_t m = cm_prepare_mod(mod[i], rectify, mode == 3u);
        int32_t xd, md, wet, y;

        cm_state.x[w] = x;
        cm_state.m[w] = m;
        if (mix == 0u) {
            out[i] = in[i];
            cm_state.w = (uint8_t)(w + 1u == CM_N ? 0u : w + 1u);
            continue;
        }

        xd = cm_delayed(cm_state.x, w);
        md = cm_delayed(cm_state.m, w);

        if (mode == 0u) {
            wet = (int32_t)(((int64_t)xd * md) >> 15);
        } else if (mode == 1u || mode == 2u) {
            int32_t xq = cm_hilbert_run(cm_state.x, w);
            int32_t mq = cm_hilbert_run(cm_state.m, w);
            int64_t cross = (int64_t)xq * mq;
            int64_t direct = (int64_t)xd * md;
            /* analytic(x) * analytic(mod): upper sideband;
             * analytic(x) * conjugate(analytic(mod)): lower sideband. */
            wet = cm_clip15((mode == 1u ? direct - cross : direct + cross) >> 15);
        } else {
            /* The supplied Chua stream is the slow LFO. Convert bipolar Q15
             * modulation to a unipolar gain, then apply tremolo depth. */
            int32_t lfo = 16384 + (md >> 1);
            int32_t gain = 32768 - (int32_t)(((int64_t)depth *
                                               (32768 - lfo)) >> 15);
            wet = (int32_t)(((int64_t)xd * gain) >> 15);
        }

        y = (int32_t)(((int64_t)xd * (32768u - mix) +
                       (int64_t)wet * mix) >> 15);
        out[i] = cm_clip15(y);
        cm_state.w = (uint8_t)(w + 1u == CM_N ? 0u : w + 1u);
    }
}
