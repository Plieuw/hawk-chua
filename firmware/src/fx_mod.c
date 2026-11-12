/* SPDX-License-Identifier: GPL-3.0-only
 * MOD FX (FX pressed twice: the MOD page): the oscillators moved by a Chua circuit of its own, before the resonators
 * (osc -> MOD FX -> resonators -> shimmer: the voices' buses, so the dry too, and what the resonators ring), the circuit the
 * oscillator's knobs set (CHAOS, SHAPE: its alpha and beta), run at the RATE. After the Chua modulation core of the
 * feature/chua-mod-fx branch (ring, single sideband shift, tremolo, the rectifier), with a vibrato added and the
 * Hilbert pair as two chains of four allpasses (Olli Niemitalo's) instead of a 127-tap FIR: ~16 multiplies a sample
 * for both the signal and the modulator, where the FIR took ~250.
 *
 *   mfx_mode   0 OFF, 1 VIBRATO, 2 RING, 3 SHIFT UP, 4 SHIFT DOWN, 5 TREMOLO (ALGORITHM)
 *   mfx_rate   KNOB 1: VIBRATO, TREMOLO 0.1 .. 13 Hz; RING, SHIFT 8 Hz .. 2 kHz (the circuit's main tone)
 *   mfx_slew   KNOB 2: the circuit slewed at audio rate (a lag: 0 off, up to a corner of ~0.9 Hz): its edges rounded,
 *              its jumps glided (the depth is the MIX's: the vibrato's sweep ~4 ms, the ring and shift full carrier)
 *   mfx_rect   KNOB 3: the modulator ORIGINAL, FULL-WAVE, HALF + or HALF - (rectified: an octave, sharper)
 *   mfx_mix    KNOB 4: dry .. wet
 */
enum { MF_OFF, MF_VIBRATO, MF_RING, MF_UP, MF_DOWN, MF_TREMOLO, MF_COUNT };
static const char *const MF_NAME[MF_COUNT] = {"OFF", "VIBRATO", "RING MOD", "SHIFT UP", "SHIFT DOWN", "TREMOLO"};
static const char *const MF_RECT[4] = {"ORIGINAL", "FULL-WAVE", "HALF +", "HALF -"};

#define MF_VLEN 512u                             /* the vibrato's line (a power of two: ~11.6 ms; at most ~334 used) */
#define MF_CH (RZ_POLY + 4u)                     /* the channels: the resonators' buses, BASIS's L and R a part each */
typedef struct {
    int32_t hx[8][2], hy[8][2], hd;              /* the two allpass chains (x[n-1..2], y[n-1..2]), the one-sample delay */
} mf_hil_t;
typedef struct {
    mf_hil_t h;
    int16_t vl[MF_VLEN];
    uint8_t act;                                 /* blocks it goes on after its input stopped (its tail) */
} mf_ch_t;
static struct {
    chua_t s;                                    /* the modulating circuit */
    int32_t dcm, dcl;                            /* the rectified modulator's DC (audio rate, slow) */
    int32_t sl;                                  /* the slew's state (Q16) */
    mf_hil_t hm;                                 /* the modulator's Hilbert pair */
    int32_t m[CTL], mr[CTL], mi[CTL];            /* this block's modulator (and its analytic pair: SHIFT) */
    uint32_t vw, vb;                             /* the vibrato's write position, this block's */
    uint32_t mode, n;
    int32_t mix, depth;
    uint8_t on;
} mf;
static mf_ch_t mf_ch[MF_CH] __attribute__((section(".pool")));
static struct {                                  /* (the MOD page's pictures) */
    int16_t m[64];                               /* the modulator, slowly: every 256th sample (~0.37 s) */
    int16_t mf[256], x[256];                     /* the modulator and the sound it makes, every 2nd sample (in step) */
    volatile uint32_t w, xw, mw;
    uint32_t c;
} mf_view;

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
/* the analytic pair of v: re, im 90 degrees apart (~40 Hz .. 20 kHz) */
static void mf_hilbert(mf_hil_t *H, int32_t v, int32_t *re, int32_t *im)
{
    int32_t a = mf_ap(H->hx, H->hy, MF_HA[0], v);
    int32_t b = mf_ap(H->hx + 4, H->hy + 4, MF_HA[1], v);
    *re = H->hd;
    H->hd = a;
    *im = b;
}

static void mf_reset(void)
{
    memset(&mf, 0, sizeof mf);
    memset(mf_ch, 0, sizeof mf_ch);
    mf.s.x = C1 / 8;
}

/* this block's step of the circuit (Q24 time units a sample, split in ns substeps): the RATE as a frequency */
static uint32_t mf_steps(int32_t *hs, int32_t *ahs, int32_t *bhs)
{
    uint32_t mode = mf.mode, r = sys.mfx_rate & 127u, inc, h, ns;
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

/* the block's modulator (once a block, before the voices' buses are moved): 1 if MOD FX is on */
static int mf_gen(uint32_t n)
{
    uint32_t mode = sys.mfx_mode % MF_COUNT, rect = (sys.mfx_rect & 127u) >> 5, i, ns, k;
    int32_t mix = (int32_t)(sys.mfx_mix & 127u) * 258, depth = 32767;   /* Q15 (the depth: full; MIX sets how much) */
    {   /* the mod wheel's (or FREE EG's) targets VIBRATO, RING MOD, FREQ SHIFT: up, that one, as much as it is up (the
         * page's RATE, SLEW, RECTIFY with it; FREQ SHIFT down if the page is on SHIFT DOWN) */
        uint32_t wv = sys.mw_vib & 127u, wr = sys.mw_ringm & 127u, ws = sys.mw_shift & 127u;
        if (wv | wr | ws) {
            mode = wv >= wr && wv >= ws ? MF_VIBRATO : wr >= ws ? MF_RING : mode == MF_DOWN ? MF_DOWN : MF_UP;
            mix = (int32_t)(wv >= wr && wv >= ws ? wv : wr >= ws ? wr : ws) * 258;
        }
    }
    uint32_t slew = sys.mfx_slew & 127u;
    int32_t sk = slew ? (int32_t)(0xFFFFFFFFu / px_exp2((int32_t)(slew * 851968u / 127u))) : 65536;   /* (Q16: 13 octaves down) */
    int32_t hs, ahs, bhs;
    if (mode == MF_OFF || !mix) {
        if (mf.on)
            mf_reset();
        return 0;
    }
    if (!mf.on) {
        mf_reset();
        mf.on = 1;
    }
    mf.mode = mode;
    mf.mix = mix;
    mf.depth = depth;
    mf.n = n = n > CTL ? CTL : n;
    ns = mf_steps(&hs, &ahs, &bhs);
    for (i = 0; i < n; i++) {
        int32_t m;
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
        if (slew) {                              /* SLEW: a one-pole lag on the circuit, every sample */
            mf.sl += (int32_t)(((int64_t)(m * 65536 - mf.sl) * sk) >> 16);
            m = mf.sl >> 16;
        } else
            mf.sl = m * 65536;
        mf.m[i] = m;
        if (mode == MF_UP || mode == MF_DOWN)    /* (the carrier's analytic pair: once, for every channel) */
            mf_hilbert(&mf.hm, (int32_t)(((int64_t)m * depth) >> 14), &mf.mr[i], &mf.mi[i]);
        if (i & 1u)
            mf_view.mf[mf_view.mw++ & 255u] = (int16_t)m;
        if (!(mf_view.c++ & 255u))
            mf_view.m[mf_view.w++ & 63u] = (int16_t)m;
    }
    mf.vb = mf.vw;
    mf.vw += n;
    return 1;
}

/* a channel (ch: a resonator's bus, or BASIS's L or R) moved by this block's modulator, in place */
static void mf_apply(uint32_t ch, int32_t *w, uint32_t n)
{
    mf_ch_t *C = &mf_ch[ch];
    uint32_t i, any = 0, mode = mf.mode;
    int32_t mix = mf.mix, depth = mf.depth;
    if (!mf.on)
        return;
    n = n > mf.n ? mf.n : n;
    for (i = 0; i < n && !any; i++)
        any = w[i] != 0;
    if (any)
        C->act = 4;                              /* (4 blocks of tail: past the vibrato's longest delay) */
    else if (!C->act)
        return;                                  /* (silent and done: nothing to move) */
    else if (!--C->act) {
        memset(C, 0, sizeof *C);                 /* (its tail out: from silence next time) */
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t x = w[i], m = mf.m[i], wet;
        switch (mode) {
        case MF_VIBRATO: {                       /* the line read late by 4 ms +- up to 3.6 ms (linear in between) */
            uint32_t d16 = (uint32_t)(176 * 65536 + (int32_t)(((int64_t)m * depth) >> 15) * 316), di, fr, p = mf.vb + i;
            int32_t a0, a1;
            C->vl[p & (MF_VLEN - 1u)] = (int16_t)mf_clip(x);
            di = d16 >> 16;
            fr = d16 >> 1 & 32767u;
            a0 = C->vl[(p - di) & (MF_VLEN - 1u)];
            a1 = C->vl[(p - di - 1u) & (MF_VLEN - 1u)];
            wet = a0 + (((a1 - a0) * (int32_t)fr) >> 15);
            break;
        }
        case MF_RING:                            /* x times the circuit (DEPTH: how much of the carrier) */
            wet = (int32_t)(((int64_t)x * (32768 - depth + (int32_t)(((int64_t)m * depth) >> 15) * 2)) >> 15);
            break;
        case MF_UP:
        case MF_DOWN: {                          /* x's spectrum moved by the circuit's: the upper or lower sideband */
            int32_t xr, xi;
            mf_hilbert(&C->h, x, &xr, &xi);
            wet = (int32_t)((mode == MF_UP ? (int64_t)xr * mf.mr[i] - (int64_t)xi * mf.mi[i] : (int64_t)xr * mf.mr[i] + (int64_t)xi * mf.mi[i]) >> 15);
            break;
        }
        default: {                               /* TREMOLO: the level dipped by the slow circuit */
            int32_t lfo = 16384 + (m >> 1), g = 32768 - (int32_t)(((int64_t)depth * (32768 - lfo)) >> 15);
            wet = (int32_t)(((int64_t)x * g) >> 15);
            break;
        }
        }
        w[i] = x + (int32_t)(((int64_t)(wet - x) * mix) >> 15);
    }
}

static void mf_scope(const int32_t *s, uint32_t n)   /* (what it made: the MOD page's lower trace) */
{
    uint32_t i;
    for (i = 1; i < n; i += 2)
        mf_view.x[mf_view.xw++ & 255u] = (int16_t)mf_clip(s[i]);
}

static void mf_block(int32_t *w, uint32_t n)     /* (one channel, the block's modulator with it: the tests) */
{
    if (mf_gen(n))
        mf_apply(0, w, n);
}
