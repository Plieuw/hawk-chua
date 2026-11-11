/* SPDX-License-Identifier: GPL-3.0-only
 * CHUA MOD FX: one fixed-point modulation engine for ring modulation and
 * generalized single-sideband frequency shifting.
 *
 * Signals are signed Q15 in int32_t arrays. The caller supplies the Chua
 * waveform as mod[]; rectification is applied here, so the same source can
 * drive either mode. Frequency shift uses analytic signals (31-tap Hilbert
 * transformers) rather than treating a non-sinusoidal Chua waveform as a
 * sine carrier. With a complex/non-sinusoidal modulator the result is a
 * spectral translation by all of that waveform's partials.
 *
 * mode: 0 ring, 1 shift up, 2 shift down
 * rectify: 0 raw, 1 full-wave, 2 positive half-wave, 3 negative half-wave
 * mix: 0..32768 (Q15)
 */
#include <stdint.h>
#include <string.h>

#define CM_HN 31u
#define CM_HD 15u

static const int16_t cm_hilbert[CM_HN] = {
    0, 0, -27, 0, -146, 0, -465, 0, -1174, 0, -2628, 0, -5905, 0, -20489,
    0, 20489, 0, 5905, 0, 2628, 0, 1174, 0, 465, 0, 146, 0, 27, 0, 0
};

typedef struct {
    int32_t x[CM_HN], m[CM_HN];
    int32_t m_dc_x, m_dc_y;
    uint8_t w;
} fx_chuamod_t;

static fx_chuamod_t cm_state;

static int32_t cm_sat15(int64_t x)
{
    x >>= 15;
    return x > 32767 ? 32767 : x < -32768 ? -32768 : (int32_t)x;
}

static int32_t cm_rectify(int32_t x, uint32_t mode)
{
    if (mode == 1u)
        return x < 0 ? -x : x;
    if (mode == 2u)
        return x < 0 ? 0 : x;
    if (mode == 3u)
        return x > 0 ? 0 : x;
    return x;
}

/* One 31-tap FIR quadrature component; the real component is delayed by
 * 15 samples to match the Hilbert transform's group delay. */
static int32_t cm_hilbert_run(const int32_t *h, uint32_t w)
{
    int64_t a = 0;
    uint32_t k;
    for (k = 0; k < CM_HN; k++) {
        uint32_t p = (w + CM_HN - k) % CM_HN;
        a += (int64_t)h[p] * cm_hilbert[k];
    }
    return cm_sat15(a);
}

static int32_t cm_delayed(const int32_t *h, uint32_t w)
{
    return h[(w + CM_HN - CM_HD) % CM_HN];
}

static void fx_chuamod_reset(void)
{
    memset(&cm_state, 0, sizeof cm_state);
}

/* Process one mono block. The 15-sample latency is identical in all modes,
 * including ring, so switching modes does not change alignment. */
static void fx_chuamod_process(const int32_t *in, const int32_t *mod,
                               int32_t *out, uint32_t n,
                               uint32_t mode, uint32_t rectify,
                               uint32_t mix)
{
    uint32_t i;
    if (mix > 32768u) mix = 32768u;
    mode %= 3u;
    rectify %= 4u;
    if (!mix) {
        for (i = 0; i < n; i++) out[i] = in[i];
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t w = cm_state.w;
        int32_t x = in[i], m = cm_rectify(mod[i], rectify);
        int32_t xd, md, xq, mq, wet;
        cm_state.x[w] = x;
        cm_state.m[w] = m;

        /* Remove the rectifier's DC component for frequency shifting.
         * The ring-mod path deliberately keeps it: that is part of the
         * characteristic full-wave/half-wave ring-mod sound. */
        if (mode) {
            int32_t y = m - cm_state.m_dc_x +
                        (int32_t)(((int64_t)cm_state.m_dc_y * 32600) >> 15);
            cm_state.m_dc_x = m;
            cm_state.m_dc_y = y;
            cm_state.m[w] = y;
        }

        xd = cm_delayed(cm_state.x, w);
        md = cm_delayed(cm_state.m, w);
        if (!mode) {
            wet = (int32_t)(((int64_t)xd * md) >> 15);
        } else {
            xq = cm_hilbert_run(cm_state.x, w);
            mq = cm_hilbert_run(cm_state.m, w);
            if (mode == 1u)       /* upper sideband: Xa * Ma */
                wet = (int32_t)(((int64_t)xd * md - (int64_t)xq * mq) >> 15);
            else                  /* lower sideband: Xa * conj(Ma) */
                wet = (int32_t)(((int64_t)xd * md + (int64_t)xq * mq) >> 15);
        }
        out[i] = (int32_t)(((int64_t)xd * (32768u - mix) +
                            (int64_t)wet * mix) >> 15);
        cm_state.w = (uint8_t)((w + 1u) % CM_HN);
    }
}
