/* SPDX-License-Identifier: GPL-3.0-only
 * MOD FX (FX pressed twice: the MOD page): the resonators' output moved by a Chua circuit of its own, the circuit the
 * oscillator's knobs set (CHAOS, SHAPE: its alpha and beta), run at the RATE. After the Chua modulation core of the
 * feature/chua-mod-fx branch (ring, single sideband shift, tremolo, the rectifier), with a vibrato added and the
 * Hilbert pair as two chains of four allpasses (Olli Niemitalo's) instead of a 127-tap FIR: ~16 multiplies a sample
 * for both the signal and the modulator, where the FIR took ~250.
 *
 *   mfx_mode   0 OFF, 1 VIBRATO, 2 RING, 3 SHIFT UP, 4 SHIFT DOWN, 5 TREMOLO (ALGORITHM)
 *   mfx_rate   KNOB 1: VIBRATO, TREMOLO 0.1 .. 13 Hz; RING, SHIFT 8 Hz .. 2 kHz (the circuit's main tone)
 *   mfx_depth  KNOB 2: how far (the vibrato's sweep up to ~4 ms, the ring and shift's carrier, the tremolo's dip)
 *   mfx_rect   KNOB 3: the modulator ORIGINAL, FULL-WAVE, HALF + or HALF - (rectified: an octave, sharper)
 *   mfx_mix    KNOB 4: dry .. wet
 */
enum { MF_OFF, MF_VIBRATO, MF_RING, MF_UP, MF_DOWN, MF_TREMOLO, MF_COUNT };
static const char *const MF_NAME[MF_COUNT] = {"OFF", "VIBRATO", "RING MOD", "SHIFT UP", "SHIFT DOWN", "TREMOLO"};
static const char *const MF_RECT[4] = {"ORIGINAL", "FULL-WAVE", "HALF +", "HALF -"};

#define MF_VLEN 512u                             /* the vibrato's line (a power of two: ~11.6 ms) */
static struct {
    chua_t s;                                    /* the modulating circuit */
    int32_t dcm, dcl;                            /* the rectified modulator's DC (audio rate, slow) */
    int32_t hx[2][8][2], hy[2][8][2];            /* the two allpass chains (x[n-1..2], y[n-1..2]), signal and mod */
    int32_t hd[2];                               /* the second chain's one-sample delay, signal and mod */
    int16_t vl[MF_VLEN];
    uint32_t vw;
    int32_t pan_l, pan_r;                        /* the wet's share off L and R (Q15, from the block before) */
    uint8_t on;
} mf;
static struct { int16_t m[64]; volatile uint32_t w; } mf_view;   /* (the modulator, every 32nd sample: the MOD page) */

/* the allpass chains' coefficients (a^2, Q30): out = a (in + out[n-2]) - in[n-2]; path 1's output a sample later */
static const int32_t MF_HA[2][4] = {
    {514752760, 940832443, 1048613677, 1071056671},   /* 0.6923878^2 .. 0.9987488452737^2 */
    {173686865, 787083823, 1015061512, 1063647745},   /* 0.4021921162426^2 .. 0.9952884791278^2 */
};
static int32_t mf_ap(int32_t (*x)[2], int32_t (*y)[2], const int32_t *a, int32_t in)
{
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        int32_t o = (int32_t)(((int64_t)a[k] * (in + y[k][1])) >> 30) - x[k][1];
        x[k][1] = x[k][0];
        x[k][0] = in;
        y[k][1] = y[k][0];
        y[k][0] = o;
        in = o;
    }
    return in;
}
/* the analytic pair of v (which: 0 the signal, 1 the modulator): re, im 90 degrees apart (~40 Hz .. 20 kHz) */
static void mf_hilbert(uint32_t which, int32_t v, int32_t *re, int32_t *im)
{
    int32_t a = mf_ap(mf.hx[which], mf.hy[which], MF_HA[0], v);
    int32_t b = mf_ap(mf.hx[which] + 4, mf.hy[which] + 4, MF_HA[1], v);
    *re = mf.hd[which];
    mf.hd[which] = a;
    *im = b;
}

static void mf_reset(void)
{
    memset(&mf, 0, sizeof mf);
    mf.s.x = C1 / 8;
    mf.s.y = 0;
    mf.s.z = 0;
}

/* this block's step of the circuit (Q24 time units a sample, split in ns substeps): the RATE as a frequency */
static uint32_t mf_steps(int32_t *hs, int32_t *ahs, int32_t *bhs)
{
    uint32_t mode = sys.mfx_mode % MF_COUNT, r = sys.mfx_rate & 127u, inc, h, ns;
    int slow = mode == MF_VIBRATO || mode == MF_TREMOLO;
    /* slow: 0.1 Hz x 2^(7 oct) = 12.8 Hz; fast: 8 Hz x 2^(8 oct) = 2 kHz: inc (Q32 a sample) = f / 44100 x 2^32 */
    uint32_t f16 = (uint32_t)(((uint64_t)(slow ? 6554u : 524288u) * px_exp2((int32_t)(r * (slow ? 458752u : 524288u) / 127u))) >> 16);   /* Hz, Q16 */
    inc = (uint32_t)(((uint64_t)f16 * 97391u) >> 16);   /* (2^32 / 44100 = 97391.5: Q16 Hz to Q32 a sample) */
    h = (uint32_t)(((uint64_t)inc * (uint32_t)cc.period) >> 24);
    ns = (h + CHUA_HMAX - 1u) / CHUA_HMAX;
    if (ns > CHUA_NSUB) {
        ns = CHUA_NSUB;
        h = CHUA_HMAX * CHUA_NSUB;
    }
    ns = ns < 1u ? 1u : ns;
    *hs = (int32_t)(h / ns);
    *ahs = (int32_t)(((int64_t)cc.alpha * *hs) >> 16);
    *bhs = (int32_t)(((int64_t)cc.beta * *hs) >> 16);
    return ns;
}

static inline int32_t mf_clip(int32_t x) { return x > 32767 ? 32767 : x < -32768 ? -32768 : x; }

/* the wet of a block in place (the resonators' sum, Q15-ish), n samples */
static void mf_block(int32_t *w, uint32_t n)
{
    uint32_t mode = sys.mfx_mode % MF_COUNT, rect = (sys.mfx_rect & 127u) >> 5, i, ns, k;
    int32_t mix = (int32_t)(sys.mfx_mix & 127u) * 258, depth = (int32_t)(sys.mfx_depth & 127u) * 258;   /* Q15 */
    int32_t hs, ahs, bhs;
    if (mode == MF_OFF || !mix) {
        if (mf.on)
            mf_reset();
        return;
    }
    if (!mf.on) {
        mf_reset();
        mf.on = 1;
    }
    ns = mf_steps(&hs, &ahs, &bhs);
    for (i = 0; i < n; i++) {
        int32_t x = w[i], m, wet;
        for (k = 0; k < ns; k++)
            chua_step(&mf.s, hs, ahs, bhs);
        if (chua_out_of_range(&mf.s)) {          /* (as a voice would: started again) */
            mf.s.x = C1 / 8;
            mf.s.y = mf.s.z = 0;
        }
        m = mf_clip(mf.s.x >> 6);                /* (x: about +-2.5 on the double scroll: Q20 >> 6, +-0.6 of full) */
        if (rect) {                              /* the rectifier: its DC off, the level made up */
            int32_t r2 = rect == 1u ? (m < 0 ? -m : m) : rect == 2u ? (m > 0 ? m : 0) : (m < 0 ? m : 0);
            if (mode == MF_VIBRATO || mode == MF_TREMOLO) {
                mf.dcl += (r2 - mf.dcl) >> 12;
                r2 -= mf.dcl;
            } else {
                mf.dcm += (r2 - mf.dcm) >> 7;
                r2 -= mf.dcm;
            }
            m = mf_clip(rect == 1u ? r2 * 2 : r2 * 7 / 4);
        }
        switch (mode) {
        case MF_VIBRATO: {                       /* the line read late by 4 ms +- up to 3.6 ms (linear in between) */
            uint32_t d16 = (uint32_t)(176 * 65536 + (int32_t)(((int64_t)m * depth) >> 15) * 316), di, fr;
            int32_t a0, a1;
            mf.vl[mf.vw & (MF_VLEN - 1u)] = (int16_t)mf_clip(x);
            di = d16 >> 16;
            fr = d16 >> 1 & 32767u;
            a0 = mf.vl[(mf.vw - di) & (MF_VLEN - 1u)];
            a1 = mf.vl[(mf.vw - di - 1u) & (MF_VLEN - 1u)];
            wet = a0 + (((a1 - a0) * (int32_t)fr) >> 15);
            mf.vw++;
            break;
        }
        case MF_RING:                            /* x times the circuit (DEPTH: how much of the carrier) */
            wet = (int32_t)(((int64_t)x * (32768 - depth + (int32_t)(((int64_t)m * depth) >> 15) * 2)) >> 15);
            break;
        case MF_UP:
        case MF_DOWN: {                          /* x's spectrum moved by the circuit's: the upper or lower sideband */
            int32_t xr, xi, mr, mi, ms = (int32_t)(((int64_t)m * depth) >> 14);
            mf_hilbert(0, x, &xr, &xi);
            mf_hilbert(1, ms, &mr, &mi);
            wet = (int32_t)((mode == MF_UP ? (int64_t)xr * mr - (int64_t)xi * mi : (int64_t)xr * mr + (int64_t)xi * mi) >> 15);
            break;
        }
        default: {                               /* TREMOLO: the level dipped by the slow circuit */
            int32_t lfo = 16384 + (m >> 1), g = 32768 - (int32_t)(((int64_t)depth * (32768 - lfo)) >> 15);
            wet = (int32_t)(((int64_t)x * g) >> 15);
            break;
        }
        }
        w[i] = x + (int32_t)(((int64_t)(wet - x) * mix) >> 15);
        if (!(i & 31u))
            mf_view.m[mf_view.w++ & 63u] = (int16_t)m;
    }
}
