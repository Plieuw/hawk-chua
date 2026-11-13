/* SPDX-License-Identifier: GPL-3.0-only
 * MOD FX (FX pressed once: the first FX page): the oscillators moved by a Chua circuit of its own, before the resonators
 * (osc -> MOD FX -> LPG -> resonators -> shimmer: each voice's oscillator before its gate, so the dry too, and
 * what the resonators ring), the circuit the
 * oscillator's knobs set (CHAOS, SHAPE: its alpha and beta), run at the RATE. After the Chua modulation core of the
 * feature/chua-mod-fx branch (ring, tremolo, the rectifier), with a vibrato added.
 *
 *   mfx_mode   0 OFF, 1 VIBRATO, 2 RING, 3 TREMOLO (ALGORITHM)
 *   mfx_rate   KNOB 1: VIBRATO, TREMOLO a note value of the tempo (the BPM or MIDI clock: 4 BARS .. 1/32, dotted and
 *              triplets: a cycle of the circuit each); RING the RATIO to the note (key follow: each resonator's bus
 *              its own circuit at its note x 1/4 .. 24, whole-number ratios, so the sidebands stay harmonic)
 *   mfx_slew   KNOB 2: the circuit slewed at audio rate (a lag: 0 off, up to a corner of ~0.9 Hz): its edges rounded,
 *              its jumps glided (the depth is the MIX's)
 *   mfx_rect   KNOB 3: the modulator ORIGINAL, FULL-WAVE, HALF +, HALF - (rectified: an octave, sharper) or
 *              FULL-WAVE 2X (rectified twice, its DC off between: two octaves)
 *   mfx_mix    KNOB 4: dry .. wet
 */
enum { MF_OFF, MF_VIBRATO, MF_RING, MF_TREMOLO, MF_SHIFT, MF_COUNT };
static const char *const MF_NAME[MF_COUNT] = {"OFF", "VIBRATO", "RING MOD", "TREMOLO", "SHIFT +/-"};
#define MF_KEYED(m) ((m) == MF_RING || (m) == MF_SHIFT)   /* (RING, SHIFT: the carrier at the note x the RATIO) */
#define MF_NRECT 5
static const char *const MF_RECT[MF_NRECT] = {"ORIGINAL", "FULL-WAVE", "HALF +", "HALF -", "FULL-WAVE 2X"};
static uint32_t mf_rect_of(uint32_t v) { return (v & 127u) * MF_NRECT / 128u; }   /* (KNOB 3's value: which) */
static uint8_t mf_rect_val(uint32_t i) { return (uint8_t)((i % MF_NRECT) * 128u / MF_NRECT + 12u); }   /* (and back) */

#define MF_VLEN 512u                             /* the vibrato's line (a power of two: ~11.6 ms; at most ~334 used) */
#define MF_CH NVOICE                             /* the channels: the voices (a note each; before their gates) */
#define MF_TINC 16777216u                        /* RING's reference circuit: 2^32 / 256 a sample (~172 Hz) */
#define MF_TMAX 384u                             /* (a cycle at most: then it is closed anyway) */
#define MF_NRATIO 23
static const char *const MF_RATIO_NAME[MF_NRATIO] = {"1/4", "1/3", "1/2", "2/3", "3/4", "1", "4/3", "3/2", "5/3", "2",
                                                      "5/2", "3", "4", "5", "6", "7", "8", "9", "10", "12", "16", "20",
                                                      "24"};
static const uint32_t MF_RATIO[MF_NRATIO] = {16384, 21845, 32768, 43691, 49152, 65536, 87381, 98304, 109227, 131072,
                                             163840, 196608, 262144, 327680, 393216, 458752, 524288, 589824, 655360,
                                             786432, 1048576, 1310720, 1572864};   /* Q16 */
typedef struct {                                 /* a Hilbert pair's state (SHIFT): two chains of four all-passes */
    int32_t hx[8][2], hy[8][2], hd;
} mf_hil_t;
typedef struct {
    int16_t vl[MF_VLEN];
    mf_hil_t hs, hc;                             /* SHIFT: the sound's analytic pair, the carrier's */
    int32_t sic1, sic2;                          /* (SHIFT: the side's gate, as the voice's: synth.c lpg_side) */
    int32_t sd[CTL];                             /* (SHIFT: the side made this block: voice_mix L +, R -) */
    uint32_t ph;                                 /* RING: its carrier's phase (Q32: a cycle of the part's table) */
    int32_t sl, dc, dc2;                         /* (its slew, its rectifier's DC, the second's) */
    uint8_t act;                                 /* blocks it goes on after its input stopped (its tail) */
} mf_ch_t;
typedef struct {                                 /* a part's MOD FX (MULTI: A's, B's) */
    chua_t s;                                    /* the modulating circuit */
    int32_t dcl, dcl2, sl;                       /* the rectified modulator's DC (twice: 2X), the slew's state (Q16) */
    int32_t m[CTL];                              /* this block's modulator */
    uint32_t vw, vb;                             /* the vibrato's write position, this block's */
    uint32_t mode, n, rect;
    uint32_t ratio;                              /* (RING: the ratio's index) */
    uint32_t linc;                               /* (VIBRATO, TREMOLO: the slow circuit's step) */
    /* RING's carrier: one circuit a part at ~172 Hz (a cycle ~256 samples), each of its cycles (upward through its
     * last cycle's middle) kept as a table, its DC off; the voices read the newest whole one at their note x the
     * ratio: the circuit's own shape, harmonic by construction, at a table read a sample instead of a circuit each */
    chua_t rs;
    int16_t tab[2][MF_TMAX];
    uint16_t tlen[2], tn;
    uint8_t tcur;
    int32_t tsum, tthr, tprev;
    int32_t mix, sk;                             /* (smoothed: this block's end; mix0, sk0 its start, glided between per
                                                  * sample) */
    int32_t mix0, sk0;
    uint32_t rq, lincs;                          /* (the RATIO, Q16, and VIBRATO / TREMOLO's step, glided) */
    uint8_t on;
} mf_t;
static mf_t mfp[2];
#define mf (mfp[0])                              /* (part A's: the MOD page, the tests) */
static mf_ch_t mf_ch[MF_CH];                     /* (~8 KB: in RAM, the pool is full) */
static struct {                                  /* (the MOD page's pictures) */
    int16_t m[64];                               /* the modulator, slowly: every 256th sample (~0.37 s) */
    int16_t mm[256], x[256];                     /* the modulator and the sound it makes, every 2nd sample (in step) */
    volatile uint32_t w, xw, mw;
    uint32_t c;
} mf_view;

/* the voices of part pp (MULTI: each its half; else all) */
#define MF_LO(pp) (sys.multi ? (pp) * (NVOICE / 2u) : 0u)
#define MF_HI(pp) (sys.multi ? ((pp) + 1u) * (NVOICE / 2u) : NVOICE)
static void mf_reset(mf_t *M, uint32_t pp)
{
    memset(M, 0, sizeof *M);
    memset(&mf_ch[MF_LO(pp)], 0, (MF_HI(pp) - MF_LO(pp)) * sizeof mf_ch[0]);
    M->s.x = C1 / 8;
    M->rs.x = C1 / 8;
}

static uint32_t mf_ratio(void) { return (sys.mfx_rate & 127u) * MF_NRATIO / 128u; }   /* (RING: KNOB 1's ratio) */
static uint32_t mf_carrier(uint32_t inc, uint32_t rq)   /* (RING, SHIFT: the note's inc x the ratio (Q16), at most half the
                                                         * rate) */
{
    uint64_t f = ((uint64_t)inc * rq) >> 16;
    return f > 0x7FFFFFFFu ? 0x7FFFFFFFu : (uint32_t)f;
}
#define MF_NDIV 14
static const char *const MF_DIV_NAME[MF_NDIV] = {"4 BARS", "2 BARS", "1 BAR", "1/2", "1/2T", "1/4", "1/4T", "1/8.",
                                                  "1/8", "1/8T", "1/16.", "1/16", "1/16T", "1/32"};
static const uint32_t MF_DIV_CPQ[MF_NDIV] = {4096, 8192, 16384, 32768, 49152, 65536, 98304, 87381, 131072, 196608,
                                             174763, 262144, 393216, 524288};   /* cycles a quarter, Q16 */
static uint32_t mf_div(void) { return (sys.mfx_rate & 127u) * MF_NDIV / 128u; }   /* (VIBRATO, TREMOLO: KNOB 1's) */
static uint32_t arp_bpm_now(void);               /* (arp.c: the tempo, its own or MIDI clock's) */

/* a circuit's step a sample (Q24 time units, split in ns substeps) at inc (Q32 a sample: as a voice's, so a note's inc
 * gives the note) */
static uint32_t mf_steps(uint32_t inc, int32_t *hs, int32_t *ahs, int32_t *bhs)
{
    uint32_t h = (uint32_t)(((uint64_t)inc * (uint32_t)cc.period) >> 24), ns;
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

/* a sample of a circuit: stepped, read, rectified (its DC off: dc, k its speed), slewed (sl, sk: Q16) */
static inline int32_t mf_post(int32_t m, uint32_t rect, int32_t *dc, int32_t *dc2, uint32_t k, int32_t *sl, int32_t sk);
static inline int32_t mf_sample(chua_t *S, uint32_t ns, int32_t hs, int32_t ahs, int32_t bhs, uint32_t rect, int32_t *dc,
                         int32_t *dc2, uint32_t k, int32_t *sl, int32_t sk)
{
    uint32_t j;
    int32_t m;
    for (j = 0; j < ns; j++)
        chua_step(S, hs, ahs, bhs);
    if (chua_out_of_range(S)) {                  /* (as a voice would: started again) */
        S->x = C1 / 8;
        S->y = S->z = 0;
    }
    m = mf_clip(S->x >> 6);                      /* (x: about +-2.5 on the double scroll: Q20 >> 6, +-0.6 of full) */
    return mf_post(m, rect, dc, dc2, k, sl, sk);
}
/* a circuit's sample made a modulator: rectified (its DC off: dc, k its speed), slewed (sl, sk: Q16) */
static inline int32_t mf_post(int32_t m, uint32_t rect, int32_t *dc, int32_t *dc2, uint32_t k, int32_t *sl, int32_t sk)
{
    if (rect == 4u) {                            /* FULL-WAVE 2X: rectified, its DC off, rectified again (two octaves) */
        int32_t a = m < 0 ? -m : m, b;
        *dc += (a - *dc) >> k;
        a = mf_clip((a - *dc) * 2);
        b = a < 0 ? -a : a;
        *dc2 += (b - *dc2) >> k;
        m = mf_clip((b - *dc2) * 2);
    } else if (rect || k > 8u) {                        /* the rectifier (and RING always): its DC off, the level made up (a
                                                  * circuit cycling round one scroll is off the middle: a carrier must
                                                  * swing both ways, or the dry comes through) */
        int32_t r2 = rect == 1u ? (m < 0 ? -m : m) : rect == 2u ? (m > 0 ? m : 0) : rect == 3u ? (m < 0 ? m : 0) : m;
        *dc += (r2 - *dc) >> k;
        m = mf_clip(rect == 1u ? (r2 - *dc) * 2 : rect ? (r2 - *dc) * 7 / 4 : (r2 - *dc) * 3 / 2);
    }
    if (sk < 65536) {                            /* SLEW: a one-pole lag on the circuit, every sample */
        *sl += (int32_t)(((int64_t)(m * 65536 - *sl) * sk) >> 16);
        m = *sl >> 16;
    } else
        *sl = m * 65536;
    return m;
}

/* part pp's modulator for the block (once a block a part, its sound in sys, before its voices): 1 if MOD FX is on */
static int mf_gen(uint32_t n, uint32_t pp)
{
    mf_t *M = &mfp[pp & 1u];
    int view = !pp;                              /* (the MOD page shows A's) */
    uint32_t mode = sys.mfx_mode % MF_COUNT, rect = mf_rect_of(sys.mfx_rect), i;
    int32_t mix = (int32_t)(sys.mfx_mix & 127u) * 258;   /* Q15 (the depth is full: MIX sets how much) */
    {   /* the mod wheel's (or FREE EG's) targets VIBRATO, RING MOD: up, that one, as much as it is up (the page's
         * RATE, SLEW, RECTIFY with it) */
        uint32_t wv = sys.mw_vib & 127u, wr = sys.mw_ringm & 127u;
        if (wv | wr) {
            mode = wv >= wr ? MF_VIBRATO : MF_RING;
            mix = (int32_t)(wv >= wr ? wv : wr) * 258;
        }
    }
    uint32_t slew = sys.mfx_slew & 127u, ns = 1, rq;
    int32_t hs = 0, ahs = 0, bhs = 0, sk;
    /* (analog: the knobs glide, not step. MIX and SLEW ~20 ms, sample by sample; the RATIO and the note value ~45 ms;
     * turned off or to MIX 0: faded out first, then off) */
    if (mode == MF_OFF || !mix) {
        if (!M->on)
            return 0;
        if (M->mix < 200) {                      /* (faded out: off) */
            mf_reset(M, pp);
            return 0;
        }
        mode = M->mode;                          /* (fading: the mode it had, to nothing) */
        mix = 0;
    }
    if (!M->on) {                                /* (on: from nothing, faded in; the ratio and the rate where they are) */
        mf_reset(M, pp);
        M->on = 1;
        M->rq = MF_RATIO[mf_ratio()];
        M->lincs = 0;
        M->sk = -1;                              /* (the slew where it is: below) */
    }
    if (mode != M->mode)                         /* (another mode: the part's channels from silence) */
        memset(&mf_ch[MF_LO(pp)], 0, (MF_HI(pp) - MF_LO(pp)) * sizeof mf_ch[0]);
    M->ratio = mf_ratio();
    M->mode = mode;
    M->mix0 = M->mix;
    M->mix += (mix - M->mix) / 8;
    M->rect = rect;
    sk = slew ? (int32_t)(0xFFFFFFFFu / px_exp2((int32_t)(slew * 851968u / 127u))) : 65536;   /* (SLEW: the lag's
                                                  * coefficient, Q16: 13 octaves down at 127) */
    if (M->sk < 0)
        M->sk = sk;
    M->sk0 = M->sk;
    M->sk += (sk - M->sk) / 8;
    rq = MF_RATIO[M->ratio % MF_NRATIO];
    M->rq = (uint32_t)((int32_t)M->rq + ((int32_t)rq - (int32_t)M->rq) / 16);
    M->n = n = n > CTL ? CTL : n;
    if (!MF_KEYED(mode)) {                       /* VIBRATO, TREMOLO: one slow circuit for all, at the tempo's note value */
        uint32_t bpm = arp_bpm_now();            /* inc (Q32 a sample) = BPM / 60 x cycles a quarter / 44100 x 2^32 */
        uint32_t lt = (uint32_t)(((uint64_t)(bpm ? bpm : 120u) * MF_DIV_CPQ[mf_div()] * 1623u) >> 16);
        M->lincs = M->lincs ? (uint32_t)((int32_t)M->lincs + ((int32_t)lt - (int32_t)M->lincs) / 16) : lt;
        M->linc = M->lincs;
        ns = mf_steps(M->linc, &hs, &ahs, &bhs);
    }
    if (MF_KEYED(mode)) {                        /* RING, FM: the reference circuit, its cycles into the tables */
        ns = mf_steps(MF_TINC, &hs, &ahs, &bhs);
        for (i = 0; i < n; i++) {
            uint32_t j;
            int32_t x;
            int16_t *t = M->tab[M->tcur ^ 1u];
            for (j = 0; j < ns; j++)
                chua_step(&M->rs, hs, ahs, bhs);
            if (chua_out_of_range(&M->rs)) {
                M->rs.x = C1 / 8;
                M->rs.y = M->rs.z = 0;
            }
            x = mf_clip(M->rs.x >> 6);
            if ((M->tn >= MF_TMAX / 3u && M->tprev < M->tthr && x >= M->tthr) || M->tn >= MF_TMAX) {
                int32_t mean = M->tsum / (int32_t)M->tn;   /* (a cycle closed: its DC off, it is the newest) */
                for (j = 0; j < M->tn; j++)
                    t[j] = (int16_t)mf_clip(t[j] - mean);
                M->tlen[M->tcur ^ 1u] = M->tn;
                M->tcur ^= 1u;
                M->tthr = mean;
                M->tn = 0;
                M->tsum = 0;
                t = M->tab[M->tcur ^ 1u];
            }
            t[M->tn++] = (int16_t)x;
            M->tsum += x;
            M->tprev = x;
            if (view && !(mf_view.c++ & 255u))
                mf_view.m[mf_view.w++ & 63u] = 0;
        }
    } else
        for (i = 0; i < n; i++) {
            int32_t m = mf_sample(&M->s, ns, hs, ahs, bhs, rect, &M->dcl, &M->dcl2, 12u, &M->sl, M->sk);
            M->m[i] = m;
            if (!view)
                continue;
            if (!(mf_view.c++ & 255u))
                mf_view.m[mf_view.w++ & 63u] = (int16_t)m;
            if (i & 1u)
                mf_view.mm[mf_view.mw++ & 255u] = (int16_t)m;
        }
    M->vb = M->vw;
    M->vw += n;
    return 1;
}

/* SHIFT's Hilbert pairs (Olli Niemitalo's: ~40 Hz .. 20 kHz 90 degrees apart): a^2 in Q30; out = a (in + out[n-2]) -
 * in[n-2]; path 1's output a sample later */
static const int32_t MF_HA[2][4] = {
    {514752760, 940832443, 1048613677, 1071056671},   /* 0.6923878^2 .. 0.9987488452737^2 */
    {173686865, 787083823, 1015061512, 1063647745},   /* 0.4021921162426^2 .. 0.9952884791278^2 */
};
static inline int32_t mf_ap(int32_t (*x)[2], int32_t (*y)[2], const int32_t *a, int32_t in)
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
static inline void mf_hilbert(mf_hil_t *H, int32_t v, int32_t *re, int32_t *im)
{
    int32_t a = mf_ap(H->hx, H->hy, MF_HA[0], v), b = mf_ap(H->hx + 4, H->hy + 4, MF_HA[1], v);
    *re = H->hd;
    H->hd = a;
    *im = b;
}

/* a channel (ch: a voice, its oscillator before the gate) moved, in place: VIBRATO, TREMOLO by the block's modulator,
 * RING by its own circuit at inc (its note's, Q32 a sample) x the RATIO; view: the voice the screen traces (both cores
 * render voices: only that one writes the MOD page's trace) */
static void mf_apply(const mf_t *M, uint32_t ch, int32_t *w, uint32_t n, uint32_t inc, int view)
{
    mf_ch_t *C = &mf_ch[ch];
    uint32_t i, any = 0, mode = M->mode;
    int32_t mix = M->mix0, dm = (M->mix - M->mix0) / (int32_t)(n ? n : 1u), dk = (M->sk - M->sk0) / (int32_t)(n ? n : 1u);
    if (!M->on)
        return;
    n = n > M->n ? M->n : n;
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
    /* (a loop a mode: nothing decided per sample) */
    if (mode == MF_SHIFT) {                      /* SHIFT +/-: the sound's spectrum moved up by the carrier (the part's
                                                  * newest circuit cycle at the note x the ratio) and down by it at once:
                                                  * the mid (their sum, a ring mod) on, the side (up - down) into C->sd:
                                                  * up on the left, down on the right */
        uint32_t rect = M->rect, L = M->tlen[M->tcur], f = mf_carrier(inc, M->rq);
        const int16_t *t = M->tab[M->tcur];
        int32_t sk = M->sk0;
        for (i = 0; i < n; i++) {
            int32_t x = w[i], m = 0, xr, xi, mr, mi, mid, side;
            if (L) {
                uint32_t pos = (uint32_t)(((uint64_t)C->ph * L) >> 16), i0 = pos >> 16, i1 = i0 + 1u >= L ? 0u : i0 + 1u;
                m = t[i0] + (((t[i1] - t[i0]) * (int32_t)(pos >> 1 & 32767u)) >> 15);
            }
            m = mf_post(m, rect, &C->dc, &C->dc2, 10u, &C->sl, sk);
            C->ph += f;
            if (view && (i & 1u))
                mf_view.mm[mf_view.mw++ & 255u] = (int16_t)m;
            mf_hilbert(&C->hs, x, &xr, &xi);
            mf_hilbert(&C->hc, m * 2, &mr, &mi);
            mid = (int32_t)(((int64_t)xr * mr) >> 15);           /* ((up + down) / 2) */
            side = (int32_t)(-((int64_t)xi * mi) >> 15);         /* ((up - down) / 2) */
            w[i] = x + (int32_t)(((int64_t)(mid - x) * mix) >> 15);
            C->sd[i] = (int32_t)(((int64_t)side * mix) >> 15);
            mix += dm;
            sk += dk;
        }
    } else if (mode == MF_RING) {                       /* x times the carrier (x 2: its swing about full): the part's newest
                                                  * cycle of the circuit read at the note x the ratio */
        uint32_t rect = M->rect, L = M->tlen[M->tcur], f = mf_carrier(inc, M->rq);
        const int16_t *t = M->tab[M->tcur];
        int32_t sk = M->sk0;
        for (i = 0; i < n; i++) {
            int32_t x = w[i], m = 0, wet;
            if (L) {
                uint32_t pos = (uint32_t)(((uint64_t)C->ph * L) >> 16), i0 = pos >> 16, i1 = i0 + 1u >= L ? 0u : i0 + 1u;
                m = t[i0] + (((t[i1] - t[i0]) * (int32_t)(pos >> 1 & 32767u)) >> 15);
            }
            m = mf_post(m, rect, &C->dc, &C->dc2, 10u, &C->sl, sk);
            C->ph += f;
            if (view && (i & 1u))
                mf_view.mm[mf_view.mw++ & 255u] = (int16_t)m;
            wet = (int32_t)(((int64_t)x * m) >> 14);
            w[i] = x + (int32_t)(((int64_t)(wet - x) * mix) >> 15);
            mix += dm;
            sk += dk;
        }
    } else if (mode == MF_VIBRATO) {             /* the line read late by 4 ms +- up to 3.6 ms (linear in between) */
        uint32_t p = M->vb;
        int16_t *vl = C->vl;
        for (i = 0; i < n; i++, p++) {
            int32_t x = w[i], a0, a1, wet;
            uint32_t d16 = (uint32_t)(176 * 65536 + M->m[i] * 316), di = d16 >> 16, fr = d16 >> 1 & 32767u;
            vl[p & (MF_VLEN - 1u)] = (int16_t)mf_clip(x);
            a0 = vl[(p - di) & (MF_VLEN - 1u)];
            a1 = vl[(p - di - 1u) & (MF_VLEN - 1u)];
            wet = a0 + (((a1 - a0) * (int32_t)fr) >> 15);
            w[i] = x + (int32_t)(((int64_t)(wet - x) * mix) >> 15);
            mix += dm;
        }
    } else                                       /* TREMOLO: the level dipped by the slow circuit */
        for (i = 0; i < n; i++) {
            int32_t x = w[i], wet = (int32_t)(((int64_t)x * (16384 + (M->m[i] >> 1))) >> 15);
            w[i] = x + (int32_t)(((int64_t)(wet - x) * mix) >> 15);
            mix += dm;
        }
}

static void mf_scope(const int32_t *s, uint32_t n)   /* (what it made: the MOD page's lower trace) */
{
    uint32_t i;
    for (i = 1; i < n; i += 2)
        mf_view.x[mf_view.xw++ & 255u] = (int16_t)mf_clip(s[i]);
}

static void mf_block(int32_t *w, uint32_t n)     /* (one channel at A 440, the block's modulator with it: the tests) */
{
    if (mf_gen(n, 0))
        mf_apply(&mfp[0], 0, w, n, 42852281u, 1);
}
