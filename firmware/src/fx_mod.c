/* SPDX-License-Identifier: GPL-3.0-only
 * MOD FX (FX pressed once: the first FX page): the oscillators moved by a Chua circuit of its own, before the resonators
 * (osc -> MOD FX -> resonators -> shimmer: the voices' buses, so the dry too, and what the resonators ring), the circuit the
 * oscillator's knobs set (CHAOS, SHAPE: its alpha and beta), run at the RATE. After the Chua modulation core of the
 * feature/chua-mod-fx branch (ring, tremolo, the rectifier), with a vibrato added.
 *
 *   mfx_mode   0 OFF, 1 VIBRATO, 2 RING, 3 TREMOLO (ALGORITHM)
 *   mfx_rate   KNOB 1: VIBRATO, TREMOLO 0.1 .. 13 Hz; RING 8 Hz .. 2 kHz (the circuit's main tone)
 *   mfx_slew   KNOB 2: the circuit slewed at audio rate (a lag: 0 off, up to a corner of ~0.9 Hz): its edges rounded,
 *              its jumps glided (the depth is the MIX's: the vibrato's sweep ~4 ms, the ring the full carrier)
 *   mfx_rect   KNOB 3: the modulator ORIGINAL, FULL-WAVE, HALF + or HALF - (rectified: an octave, sharper)
 *   mfx_mix    KNOB 4: dry .. wet
 */
enum { MF_OFF, MF_VIBRATO, MF_RING, MF_TREMOLO, MF_COUNT };
static const char *const MF_NAME[MF_COUNT] = {"OFF", "VIBRATO", "RING MOD", "TREMOLO"};
static const char *const MF_RECT[4] = {"ORIGINAL", "FULL-WAVE", "HALF +", "HALF -"};

#define MF_VLEN 512u                             /* the vibrato's line (a power of two: ~11.6 ms; at most ~334 used) */
#define MF_CH (RZ_POLY + 4u)                     /* the channels: the resonators' buses, BASIS's L and R a part each */
typedef struct {
    int16_t vl[MF_VLEN];
    uint8_t act;                                 /* blocks it goes on after its input stopped (its tail) */
} mf_ch_t;
static struct {
    chua_t s;                                    /* the modulating circuit */
    int32_t dcm, dcl;                            /* the rectified modulator's DC (audio rate, slow) */
    int32_t sl;                                  /* the slew's state (Q16) */
    int32_t m[CTL];                              /* this block's modulator */
    uint32_t vw, vb;                             /* the vibrato's write position, this block's */
    uint32_t mode, n;
    int32_t mix;
    uint8_t on;
} mf;
static mf_ch_t mf_ch[MF_CH];                     /* (~8 KB: in RAM, the pool is full) */
static struct {                                  /* (the MOD page's pictures) */
    int16_t m[64];                               /* the modulator, slowly: every 256th sample (~0.37 s) */
    int16_t mf[256], x[256];                     /* the modulator and the sound it makes, every 2nd sample (in step) */
    volatile uint32_t w, xw, mw;
    uint32_t c;
} mf_view;

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
    int32_t mix = (int32_t)(sys.mfx_mix & 127u) * 258;   /* Q15 (the depth is full: MIX sets how much) */
    {   /* the mod wheel's (or FREE EG's) targets VIBRATO, RING MOD: up, that one, as much as it is up (the page's
         * RATE, SLEW, RECTIFY with it) */
        uint32_t wv = sys.mw_vib & 127u, wr = sys.mw_ringm & 127u;
        if (wv | wr) {
            mode = wv >= wr ? MF_VIBRATO : MF_RING;
            mix = (int32_t)(wv >= wr ? wv : wr) * 258;
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
    int32_t mix = mf.mix;
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
            uint32_t d16 = (uint32_t)(176 * 65536 + m * 316), di, fr, p = mf.vb + i;
            int32_t a0, a1;
            C->vl[p & (MF_VLEN - 1u)] = (int16_t)mf_clip(x);
            di = d16 >> 16;
            fr = d16 >> 1 & 32767u;
            a0 = C->vl[(p - di) & (MF_VLEN - 1u)];
            a1 = C->vl[(p - di - 1u) & (MF_VLEN - 1u)];
            wet = a0 + (((a1 - a0) * (int32_t)fr) >> 15);
            break;
        }
        case MF_RING:                            /* x times the circuit (x 2: its swing about full) */
            wet = (int32_t)(((int64_t)x * m) >> 14);
            break;
        default: {                               /* TREMOLO: the level dipped by the slow circuit */
            wet = (int32_t)(((int64_t)x * (16384 + (m >> 1))) >> 15);
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
