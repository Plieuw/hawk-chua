/* SPDX-License-Identifier: GPL-3.0-only */
/* NOISE: thirty noise oscillators after the programs of Befaco's Noise Plethora (its manual describes each one; these
 * are this firmware's own fixed-point versions of those descriptions, written for a keyboard: the note sets the main
 * frequency, X and Y the program's two controls). Each voice runs its own copy, through the same ADSR and low pass
 * gate as the CHUA circuit.
 *
 *   A  TEXTURES        0 RADIO OH NO   1 RWALK FLANGE   2 XMOD RING SQR  3 XMOD RING SINE  4 CROSS MOD RING
 *                      5 RESONOISE     6 GRAIN GLITCH   7 GRAIN GLITCH 2 8 GRAIN GLITCH 3  9 BASURILLA
 *   B  HH CLUSTERS     0 CLUSTER SAW   1 PW CLUSTER     2 CR CLUSTER     3 SINE FM CLUST   4 TRI FM CLUST
 *                      5 PRIME CLUSTER 6 PRIME C NOISE  7 FIBONACCI      8 PARTIAL CLUST   9 PHASING CLUST
 *   C  HARSH & WILD    0 BASURA TOTAL  1 ATARI          2 WAKING FILOM.  3 P S H           4 ARRAY ROCKS
 *                      5 EXIST. PAIN   6 WHO KNOWS      7 SATAN WORKOUT  8 RWALK BITCRUSH  9 RWALK LFREE
 *   D  ORDER           0 BIG BANG (this firmware's own: the standard waves, entropy, a bang, the series stretched)
 *                      1 PIANO (this firmware's own: a piano's tone from its partials, struck, see below)
 *
 * Building blocks: phase accumulators (Q32 cycles), square / pulse / saw / triangle / sine (px_sin), xorshift noise,
 * random walkers in a box, a Chamberlin state-variable band-pass, a granular cell (a 1024-sample buffer read back
 * faster in grains), a flanger, a 16-bit LFSR, sample & hold, a bit crusher and a small "dirty" reverb (three allpasses
 * and a damped comb). Signals Q15. Each voice has 4096 samples of buffer in the pool (np_buf).
 *
 * The FM oscillator (the MOD page): per voice a modulator at a ratio of the note, its wave morphing sine ->
 * triangle -> saw -> square, moves the pitch of every audio-rate oscillator of the program, linear FM (1 + m x depth;
 * no through zero: down to 1/16 of the frequency). Its depth is DEPTH (the MOD page: always there) plus HEIGHT times
 * its own AD envelope (the MOD ENV page: LENGTH, SKEW, HEIGHT), started by each note. With both 0 nothing of it runs.
 *
 *   np_start(k, seed)                 a voice's state at a new note
 *   np_render(k, alg, inc, out, n, ext)  voice k, program alg (0..29), the note's increment (ext: FREE EG FM's factor
 *                                     a sample, Q16, or 0): n samples Q15 into out */
#define NP_COUNT 32
#define NP_BIGBANG 30u                           /* (the programs' numbers: osc = program + 1) */
#define NP_PIANO 31u
static const char *const NP_NAME[NP_COUNT] = {
    "RADIO OH NO", "RWALK FLANGE", "XMOD RING SQR", "XMOD RING SINE", "CROSS MOD RING",
    "RESONOISE", "GRAIN GLITCH", "GRAIN GLITCH 2", "GRAIN GLITCH 3", "BASURILLA",
    "CLUSTER SAW", "PW CLUSTER", "CR CLUSTER", "SINE FM CLUST", "TRI FM CLUST",
    "PRIME CLUSTER", "PRIME C NOISE", "FIBONACCI", "PARTIAL CLUST", "PHASING CLUST",
    "BASURA TOTAL", "ATARI", "WAKING FILOM.", "P S H", "ARRAY ROCKS",
    "EXIST. PAIN", "WHO KNOWS", "SATAN WORKOUT", "RWALK BITCRUSH", "RWALK LFREE",
    "BIG BANG", "PIANO",
};
static const char *const NP_X[NP_COUNT] = {        /* what X and Y do, per program (the screen) */
    "OSC 2", "WALK RATE", "OSC 2", "OSC 2", "RATIOS", "MOD RATIO", "GRAIN", "GRAIN", "GRAIN", "LFO RATE",
    "SPREAD", "DETUNE", "DETUNE", "DETUNE", "DETUNE", "SPREAD", "SPREAD", "SPREAD", "SPREAD", "SPREAD",
    "LFSR BEND", "OSC 2", "BOX", "RATE", "MOD RATIO", "SWEEP RATE", "SWEEP RATE", "FM", "BOX", "BOX",
    "WAVE", "HAMMER",
};
static const char *const NP_Y[NP_COUNT] = {
    "PWM", "FLANGE", "FM", "FM", "FM", "FOLD", "FM", "FM", "FM", "WIDTH",
    "VOICES", "PW", "FM", "FM", "FM", "FM", "NOISE FM", "TONE", "TONE", "DRIFT",
    "REVERB", "FM", "FM", "REVERB", "SCRAMBLE", "DEPTH", "DEPTH", "CRUSH", "REVERB", "REVERB",
    "ENTROPY", "STRINGS",
};
/* the output level per program: measured (tests/basis_test.c, NP_LEVELS=1 prints them) so that each sounds about as
 * loud as the circuit at X = Y = 64, a four-note chord (Q12) */
static const uint16_t NP_GAIN[NP_COUNT] = {
    1747, 1703, 1041, 2220, 1945, 510, 880, 879, 1531, 4523,
    3027, 1975, 1996, 2339, 2423, 2275, 2289, 2350, 2309, 1750,
    4035, 1169, 1285, 3241, 1548, 959, 382, 925, 2334, 1740,
    4200, 5600,
};

#define NP_BUF 4096u
/* a voice's buffer: the reverb's allpasses and comb, the granular cell, the flanger (a program uses a few of them) */
#define NB_AP1 0u
#define NB_AP2 241u
#define NB_AP3 624u
#define NB_COMB 1181u
#define NB_GRAIN 2800u
#define NB_FLANGE 3824u
#define NL_AP1 241u
#define NL_AP2 383u
#define NL_AP3 557u
#define NL_COMB 1617u
#define NL_GRAIN 1024u
#define NL_FLANGE 256u
static int16_t np_buf[NVOICE][NP_BUF] __attribute__((section(".pool")));

typedef struct {
    uint32_t ph[16], mp[6];                      /* oscillators, modulators */
    int32_t walk[16];                            /* random walkers (Q16, -1 .. 1) */
    uint32_t rng;
    int32_t last[4];                             /* the last outputs (for feedback, cross modulation) */
    int32_t lo[4], bd[4];                        /* band-passes: low, band */
    int32_t sh, lp, cnt, hold;                   /* sample & hold, one-pole, counters */
    uint32_t gw, gr, gstart, gn;                 /* the granular cell: write, read (Q16), grain start, samples left */
    uint32_t a1, a2, a3, cp;                     /* the reverb's positions */
    int32_t clp;                                 /* its comb's damping */
    uint32_t fw;                                 /* the flanger's write */
    uint32_t lfsr;
    int16_t wt[64];                              /* ARRAY ROCKS: the wavetable */
    int32_t hist[16];                            /* the last outputs every 4 samples (the screen's trace) */
    uint32_t hn;
    uint32_t menv, mage;                         /* its envelope (0..ENV_MAX), the voice's age it was started for */
    uint8_t mstage;                              /* 0 rising, 1 falling, 2 done */
    int32_t bang;                                /* BIG BANG: the heat left of the note's bang, Q15 */
    int32_t xs, ys, gs;                          /* X, Y (Q15) and the level (Q12) as smoothed (synth.c: ksm_glide) */
    uint32_t kage;                               /* the note they were taken at once for */
    uint8_t kep;                                 /* ksm_epoch then */
} np_t;
static np_t np_state[NVOICE];
#define NP_PNK 24u
static const uint16_t NP_PN_B[128] = {300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 280, 261, 243, 227, 212, 197, 184, 172, 160, 157, 153, 150, 147, 144, 141, 138, 136, 133, 130, 137, 144, 152, 160, 169, 178, 188, 198, 209, 220, 234, 248, 263, 279, 297, 315, 334, 355, 377, 400, 426, 454, 483, 514, 548, 583, 621, 661, 704, 750, 798, 850, 930, 1017, 1113, 1218, 1332, 1458, 1595, 1745, 1909, 2089, 2285, 2500, 2689, 2893, 3112, 3347, 3600, 3873, 4166, 4481, 4821, 5185, 5578, 6000, 6476, 6990, 7545, 8143, 8789, 9487, 10240, 11052, 11929, 12876, 13897, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000, 15000};
typedef struct {
    uint32_t ph[NP_PNK], inc[NP_PNK];            /* each partial's phase, its increment (Q32) */
    int32_t ef[NP_PNK], es[NP_PNK];              /* its two stages' levels (Q29: prompt, aftersound) */
    int32_t df[NP_PNK], ds[NP_PNK];              /* their fall a block (Q30) */
    uint32_t bp[NP_PNK], bi[NP_PNK];             /* its beating's phase, its rate a block */
    uint32_t age, n;                             /* the note it was struck for; partials sounding */
    int32_t thump, click, tl, tb;                /* the strike's knock and click (Q15), the knock's filter */
} np_piano_t;
static np_piano_t np_pn[NVOICE];

/* ---------------------------------------------------------------- helpers --- */
static inline uint32_t np_rnd(uint32_t *s) { uint32_t x = *s; x ^= x << 13; x ^= x >> 17; x ^= x << 5; return *s = x; }
static inline int32_t np_noise(uint32_t *s) { return (int32_t)(np_rnd(s) >> 16) - 32768; }
static inline int32_t np_sq(uint32_t ph) { return ph < 0x80000000u ? 32767 : -32767; }
static inline int32_t np_pulse(uint32_t ph, uint32_t pw) { return ph < pw ? 32767 : -32767; }
static inline int32_t np_saw(uint32_t ph) { return (int32_t)(ph >> 16) - 32768; }
static inline int32_t np_tri(uint32_t ph)
{
    int32_t t = (int32_t)(ph >> 15);             /* 0 .. 131071 */
    return t < 65536 ? t - 32768 : 98303 - t;
}
/* a cheap sine (some programs take a dozen per sample): 256 points, linear between them, Q15 */
static const int16_t NP_SIN[257] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
    0,
};
static inline int32_t np_sin(uint32_t ph)
{
    uint32_t i = ph >> 24;
    int32_t a = NP_SIN[i], f = (int32_t)((ph >> 9) & 32767u);
    return a + (((NP_SIN[i + 1u] - a) * f) >> 15);
}
static inline uint32_t np_mul(uint32_t inc, uint32_t r16) { return (uint32_t)(((uint64_t)inc * r16) >> 16); }
/* FM: the increment moved by mod (Q15) x index (Q15, 1.0 = +-100 % of the increment) */
static inline uint32_t np_fm(uint32_t inc, int32_t mod, int32_t idx)
{
    int32_t d = (int32_t)(((int64_t)(int32_t)(inc >> 1) * mod * idx) >> 29);
    return d < -(int32_t)(inc - (inc >> 4)) ? inc >> 4 : inc + (uint32_t)d;   /* (never at a standstill: an */
}                                                                           /* oscillator stuck would go silent) */
/* a ratio from a knob: 2^(lo + (hi - lo) k / 32767) octaves, Q16 */
static inline uint32_t np_ratio(int32_t k, int32_t lo_oct, int32_t hi_oct)
{
    return px_exp2(lo_oct * 65536 + (int32_t)(((int64_t)(hi_oct - lo_oct) * 65536 * k) / 32767));
}
static inline int32_t np_clip(int32_t v) { return v > 32767 ? 32767 : v < -32767 ? -32767 : v; }

/* a random walker in a box of +-box (Q16): steps of +-step, reflected at the walls */
static inline void np_walk(int32_t *w, uint32_t *rng, int32_t step, int32_t box)
{
    int32_t v = *w + (int32_t)(((int64_t)np_noise(rng) * step) >> 15);
    if (v > box) v = 2 * box - v;
    if (v < -box) v = -2 * box - v;
    *w = clamp(v, -box, box);
}
/* a Chamberlin band-pass: f = 2 sin(pi fc / FS) ~ 2 pi inc / 2^32 (Q16, clamped stable), damping q Q16 */
static inline int32_t np_bp(int32_t *lo, int32_t *bd, int32_t in, uint32_t inc, int32_t q)
{
    int32_t f = (int32_t)(((uint64_t)inc * 411775u) >> 32), hi;
    f = f > 48000 ? 48000 : f;
    *lo += (int32_t)(((int64_t)f * *bd) >> 16);
    hi = in - *lo - (int32_t)(((int64_t)q * *bd) >> 16);
    *bd += (int32_t)(((int64_t)f * hi) >> 16);
    *bd = clamp(*bd, -(1 << 20), 1 << 20);
    *lo = clamp(*lo, -(1 << 20), 1 << 20);
    return *bd;
}
/* the small reverb: three allpasses (0.6) then a comb whose feedback (room, Q15) is damped; returns the wet */
static int32_t np_verb(np_t *S, int16_t *b, int32_t in, int32_t room)
{
    int32_t d, v, c;
    d = b[NB_AP1 + S->a1]; v = in + (d * 19661 >> 15); b[NB_AP1 + S->a1] = (int16_t)np_clip(v); in = d - (v * 19661 >> 15);
    S->a1 = S->a1 + 1u == NL_AP1 ? 0u : S->a1 + 1u;
    d = b[NB_AP2 + S->a2]; v = in + (d * 19661 >> 15); b[NB_AP2 + S->a2] = (int16_t)np_clip(v); in = d - (v * 19661 >> 15);
    S->a2 = S->a2 + 1u == NL_AP2 ? 0u : S->a2 + 1u;
    d = b[NB_AP3 + S->a3]; v = in + (d * 19661 >> 15); b[NB_AP3 + S->a3] = (int16_t)np_clip(v); in = d - (v * 19661 >> 15);
    S->a3 = S->a3 + 1u == NL_AP3 ? 0u : S->a3 + 1u;
    c = b[NB_COMB + S->cp];
    S->clp += (c - S->clp) * 3 / 8;              /* (damping: the tail darkens) */
    b[NB_COMB + S->cp] = (int16_t)np_clip(in / 2 + (int32_t)(((int64_t)S->clp * room) >> 15));
    S->cp = S->cp + 1u == NL_COMB ? 0u : S->cp + 1u;
    return c + in / 2;
}
/* the granular cell: in goes in; read back at rate (Q16) in grains of len samples, each from len samples back */
static int32_t np_grain(np_t *S, int16_t *b, int32_t in, uint32_t rate, uint32_t len)
{
    int32_t o;
    b[NB_GRAIN + (S->gw & (NL_GRAIN - 1u))] = (int16_t)np_clip(in);
    S->gw++;
    if (!S->gn) {                                /* a new grain */
        S->gn = len;
        S->gr = (S->gw - len - 1u) << 16;
    }
    S->gn--;
    o = b[NB_GRAIN + ((S->gr >> 16) & (NL_GRAIN - 1u))];
    S->gr += rate;
    return o;
}

static void np_start(uint32_t k, uint32_t seed)   /* (seed: from note_on's rng(); under a held note, from the */
{                                                  /* voice: the second core must not touch rng()) */
    np_t *S = &np_state[k];
    uint32_t i;
    seed ^= 0x9E3779B9u * (k + 1u);
    memset(S, 0, sizeof *S);
    memset(np_buf[k], 0, sizeof np_buf[k]);
    memset(&np_pn[k], 0, sizeof np_pn[k]);       /* (PIANO: struck at the note) */
    S->rng = seed | 1u;
    S->lfsr = 0xACE1u;
    S->bang = 32767;
    for (i = 0; i < 16u; i++)
        S->ph[i] = np_rnd(&S->rng);              /* (clusters: no click of all phases at 0) */
    for (i = 0; i < 64u; i++)
        S->wt[i] = (int16_t)np_noise(&S->rng);
}

/* BIG BANG (D0): the harmonic series, sixteen partials, their weights the standard waves' (sine, triangle, saw,
 * square: WAVE, X, between them), band-limited by itself (a partial over the top is left out); then ENTROPY (Y): the
 * partials' frequencies walk off the series (+-50 % at the most) and noise comes in, order to disorder; BANG
 * (CHAOS's knob): each note starts at full entropy, a singularity, and cools to ENTROPY's in 30 ms .. 4 s (0: no
 * bang); EXPAND (OUTPUT's knob): the series stretched, partial n at n^(1 + 0 .. 0.5): in tune .. piano .. bell */
static const int16_t NP_BB_W[4][16] = {
    {20000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {16211, 0, -1801, 0, 648, 0, -331, 0, 200, 0, -134, 0, 96, 0, -72, 0},
    {12732, -6366, 4244, -3183, 2546, -2122, 1819, -1592, 1415, -1273, 1157, -1061, 979, -909, 849, -796},
    {25465, 0, 8488, 0, 5093, 0, 3638, 0, 2829, 0, 2315, 0, 1959, 0, 1698, 0},
};
static const int32_t NP_LOG2[16] = {0, 65536, 103872, 131072, 152170, 169408, 183983, 196608, 207744, 217706, 226717, 234944, 242512, 249519, 256042, 262144};   /* log2 n, Q16 */

static const uint16_t NP_PRIMES[16] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53};
static const uint16_t NP_FIB[16] = {1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597};
static const uint32_t NP_MODR[6] = {65536, 92682, 131072, 180879, 196608, 32768};   /* 1, 1.41, 2, 2.76, 3, 0.5 */

/* the MOD ENV's BEND: the envelope's level x (Q15) curved, 64 straight; above, towards x^3 (rises slowly, then
 * fast; falls fast, then slowly: exponential); below, towards 1 - (1 - x)^3 (the other way: logarithmic) */
static int32_t np_bend(int32_t x, uint32_t bend)
{
    int32_t b = (int32_t)(bend & 127u) - 64, t, m;
    if (!b)
        return x;
    if (b > 0)
        t = (((x * x) >> 15) * x) >> 15;
    else {
        int32_t u = 32768 - x;
        t = 32768 - ((((u * u) >> 15) * u) >> 15);
    }
    m = b < 0 ? -b : b + 1;                      /* 0 .. 64 */
    return x + (((t - x) * m) >> 6);
}

/* PIANO (D1): a piano's tone from what it is made of, as BIG BANG builds the standard waves from the series: up to
 * 24 partials of the struck strings, each where a piano has it and doing what a piano's does.
 *   where       partial n at n sqrt(1 + B n^2) (the steel's stiffness: sharp, the more the higher; B per note as a
 *               piano's, ~1.3e-4 at the break .. 1.5e-2 at the top; STIFF x 0 .. 2.5)
 *   how loud    as the hammer leaves it: its place on the string (an eighth: the 8th, 16th left out), its felt (a
 *               low-pass, the harder the hit and the HAMMER, the more partials: soft and round .. bright and hard),
 *               the sound board's tilt
 *   how long    each in two stages, as a piano's (the strings of a note pull on the bridge together, then apart): the
 *               prompt sound, falling fast, and the aftersound, long (the fundamental ~20 s in the bass, ~1 s at the top,
 *               DECAY x 1/4 .. 4); the high partials die sooner
 *   beating     the two or three strings a note, tuned a hair apart (STRINGS: one .. a grand's .. honky-tonk): each
 *               partial swells and fades at its own rate, the higher the faster
 *   the strike  the hammer's thump on the board (a short knock, low) and its felt on the steel (a click), as hard as
 *               the note
 * Struck at each note (a held key rings on, a released one is damped by the gate's release); its knobs where BIG
 * BANG's are: X HAMMER, Y STRINGS, CHAOS's DECAY, OUTPUT's STIFF */
static int32_t np_fall(uint32_t t60b)            /* a level's fall a block to -60 dB in t60b blocks, Q30 */
{
    uint32_t e = 653126u / (t60b < 2u ? 2u : t60b);   /* (log2(1000) / t, Q16) */
    return (int32_t)(px_exp2(-(int32_t)e) << 14);
}
/* n / d, the quotient under 2^32 (shift and subtract: the target has no 64-bit divide; a strike's few) */
static uint32_t np_udiv(uint64_t n, uint32_t d)
{
    uint64_t r = 0;
    uint32_t q = 0;
    int i;
    if (!d)
        return 0xFFFFFFFFu;
    for (i = 63; i >= 0; i--) {
        r = r << 1 | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            if (i < 32)
                q |= 1u << i;
        }
    }
    return q;
}
static void np_piano_strike(uint32_t k, np_t *S, uint32_t inc)
{
    np_piano_t *P = &np_pn[k];
    const voice_t *v = &voice[k];
    uint32_t note = v->note & 127u, j, vel = (uint32_t)v->amp >> 8;   /* (Q15 -> 0 .. 127) */
    int32_t hard = (int32_t)(sys.np_x & 127u), str = (int32_t)(sys.np_y & 127u);
    int32_t dec = (int32_t)(sys.chaos & 127u) - 64, stf = (int32_t)(sys.output & 127u);
    uint64_t bq = ((uint64_t)NP_PN_B[note] * (uint32_t)stf * 86237u) >> 10;   /* B, Q32 (x 4295 / 51: STIFF 51 a piano's) */
    uint32_t s1 = px_isqrt64((1ull << 32) + bq), tr = note < 21u ? 0u : note > 108u ? 87u : note - 21u;
    /* the fundamental's T60 in blocks: ~20 s at A0, halving every ~20 semitones, x 2^(DECAY / 32) */
    uint32_t t1 = (uint32_t)(((uint64_t)27562u * px_exp2(-(int32_t)tr * 3277 + dec * 2048)) >> 16);
    /* the felt: the hammer's contact (longer in the bass, shorter when struck harder) sets a corner, Hz, above
     * which the partials fall 6 dB an octave: ~350 Hz at A0 .. ~2.6 kHz at C8 at a mezzo forte; x 0.55 .. 1.75 with
     * the strike, x 2^((HAMMER - 64) / 40) */
    uint32_t fh = (uint32_t)(((uint64_t)350u * px_exp2((int32_t)tr * 2185 + (hard - 64) * 1638)) >> 16);
    int32_t sum = 0;
    uint64_t sq = 0;
    fh = fh * (55u + vel * 120u / 127u) / 100u;
    fh = fh < 120u ? 120u : fh > 9000u ? 9000u : fh;
    P->age = v->age;
    P->n = 0;
    /* the bass: past the 14th partial every 2nd .. 6th (each as strong as those it stands for), up to ~3.5 kHz:
     * a low string's brilliance, what a small speaker plays of it */
    uint32_t hz = (uint32_t)(((uint64_t)inc * 44100u) >> 32), nmax = 3500u / (hz ? hz : 1u);
    uint32_t sd = nmax > 24u ? (nmax - 14u + 9u) / 10u : 1u;
    sd = sd > 6u ? 6u : sd;
    for (j = 0; j < NP_PNK; j++) {
        uint32_t n1 = j < 14u ? j + 1u : 14u + (j - 13u) * sd, sk = px_isqrt64((1ull << 32) + bq * n1 * n1);
        uint64_t r = np_udiv((uint64_t)n1 * sk << 16, s1);      /* n sqrt(1 + B n^2) / sqrt(1 + B), Q16 */
        uint64_t c = ((uint64_t)inc * r) >> 16;
        uint32_t f = (uint32_t)((c * 44100u) >> 32), comb, t;
        int32_t a, lp;
        if (c >= 0x60000000u || f > 9000u)       /* (past 9 kHz: not there) */
            break;
        P->inc[j] = (uint32_t)c;
        {
            int32_t cs = np_sin((uint32_t)(n1 * (0x80000000u / 8u)));   /* |sin(n pi / 8)|, Q15 */
            comb = (uint32_t)(cs < 0 ? -cs : cs);
            comb = comb > 32767u ? 32767u : comb;
        }
        lp = (int32_t)(fh * 32768u / px_isqrt64((uint64_t)fh * fh + (uint64_t)f * f));   /* 1 / sqrt(1 + (f/fh)^2) */
        a = (int32_t)(((int64_t)comb * lp) >> 15);
        a = (int32_t)(((int64_t)a * (int32_t)(32768u - (8100u << 15) / (f * f + 8100u))) >> 15);   /* (the board: weak
                                                  * under ~90 Hz, the lowest fundamentals less than their partials) */
        if (j >= 14u && sd > 1u)                 /* (the power of the sd it stands for) */
            a = (int32_t)(((int64_t)a * (int32_t)px_isqrt64((uint64_t)sd << 30)) >> 15);
        sum += a;
        sq += (uint64_t)((int64_t)a * a);
        P->ef[j] = a << 14;                      /* (Q15 -> Q29: the prompt sound, most of it) */
        P->es[j] = (a << 14) / 3;                /* (the aftersound) */
        /* their T60s: the partial's (shorter with n and with its frequency), the prompt a fifth of it */
        t = t1 * 1000u / (1000u + (n1 - 1u) * 60u + (f / 50u) * (f / 50u) / 4u);   /* (t1 < 2^17) */
        P->df[j] = np_fall(t / 5u);
        P->ds[j] = np_fall(t + t / 4u);
        /* the beating: the strings apart by 0 .. 0.6 .. 6 cents (STRINGS 0, 64, 127): f x c / 1731, Hz -> a block */
        {
            uint32_t cents100 = (uint32_t)(str * str) * 37u / 1000u + (np_rnd(&S->rng) & 15u);   /* (cents x 100) */
            uint64_t bhz = ((uint64_t)(f * cents100) * 24812u) >> 16;   /* Hz, Q16 (x 65536 / 173100) */
            P->bi[j] = (uint32_t)((bhz * (uint32_t)((65536ull * CTL << 16) / 44100u)) >> 16);   /* (Q16 Hz x 2^16 x CTL / FS: Q32 a block) */
            P->bp[j] = np_rnd(&S->rng);
        }
        P->ph[j] = 0;                            /* (struck: from 0, a sine's start) */
        P->n = j + 1u;
    }
    if (sum > 0) {   /* (as loud whatever the spectrum: its power the same, its sum never over the top; the treble
                      * softer, 4.5 dB an octave over C4, as a piano's short strings and light hammers give less) */
        uint32_t rms = px_isqrt64(sq);
        int32_t g = rms ? (int32_t)(11000u * 32768u / rms) : 32768, gm = (24000 << 15) / sum;
        int32_t tilt = note > 60u ? (int32_t)(px_exp2(-(int32_t)(note - 60u) * 4083) >> 1) : 32768;   /* Q15 */
        g = g < gm ? g : gm;
        g = (int32_t)(((int64_t)g * tilt) >> 15);
        for (j = 0; j < P->n; j++) {
            P->ef[j] = (int32_t)(((int64_t)P->ef[j] * g) >> 15);
            P->es[j] = (int32_t)(((int64_t)P->es[j] * g) >> 15);
        }
    }
    P->thump = (int32_t)(vel * vel / 2u) + 600;  /* the knock: as hard as the strike */
    P->click = (int32_t)(vel * vel / 4u) * (int32_t)(64 + hard) / 64;
    if (note > 72u)                              /* (the treble's felt thinner: its click softer) */
        P->click = (int32_t)(((int64_t)P->click * (int32_t)(px_exp2(-(int32_t)(note - 72u) * 2731) >> 1)) >> 15);
}
static void np_piano(uint32_t k, np_t *S, uint32_t inc, int32_t *out, uint32_t n, const uint32_t *fk, uint32_t fm)
{
    np_piano_t *P = &np_pn[k];
    const voice_t *v = &voice[k];
    uint32_t i, j;
    int32_t g0[NP_PNK], gd[NP_PNK];
    if (v->age != P->age && (v->stage == ST_ATTACK || v->stage == ST_SUSTAIN))   /* a note: struck */
        np_piano_strike(k, S, inc);
    for (j = 0; j < P->n; j++) {                 /* each partial's gain over this block: its stages, its beating */
        int32_t bt = 32768 - ((32768 - (np_sin(P->bp[j]) >> 0)) >> 2), g1;   /* (0.5 .. 1: a swell) */
        int32_t gs = (int32_t)(((int64_t)P->es[j] * bt) >> 15);
        g0[j] = (P->ef[j] + gs) >> 14;           /* Q15 */
        P->ef[j] = (int32_t)(((int64_t)P->ef[j] * P->df[j]) >> 30);
        P->es[j] = (int32_t)(((int64_t)P->es[j] * P->ds[j]) >> 30);
        P->bp[j] += P->bi[j];
        g1 = (P->ef[j] + (int32_t)(((int64_t)P->es[j] * bt) >> 15)) >> 14;
        gd[j] = (g1 - g0[j]) / (int32_t)(n ? n : 1u);
    }
    while (P->n && (P->ef[P->n - 1u] | P->es[P->n - 1u]) < 16384)   /* (the top ones gone: not computed) */
        P->n--;
    for (i = 0; i < n; i++) {
        int32_t s = 0, t;
        for (j = 0; j < P->n; j++) {
            P->ph[j] += fm ? np_mul(P->inc[j], fk[i]) : P->inc[j];
            s += (np_sin(P->ph[j]) * g0[j]) >> 15;
            g0[j] += gd[j];
        }
        if (P->thump > 32) {                     /* the knock on the board: noise through a low resonance */
            t = np_noise(&S->rng);
            P->tb += (int32_t)(((int64_t)(t - P->tl - (P->tb >> 1)) * 1200) >> 15);
            P->tl += (int32_t)(((int64_t)P->tb * 1200) >> 15);
            s += (int32_t)(((int64_t)P->tl * P->thump) >> 15) / 2;
            P->thump -= P->thump >> 9;           /* (~25 ms) */
            if (P->click > 32) {                 /* the felt on the steel: a click, high */
                s += (int32_t)(((int64_t)(t - S->lp) * P->click) >> 15) / 4;
                S->lp = t;
                P->click -= P->click >> 5;       /* (~1.5 ms) */
            }
        }
        out[i] = np_clip(s);
    }
}

/* ---------------------------------------------------------------- render --- */
static void np_render(uint32_t k, uint32_t alg, uint32_t inc, int32_t *out, uint32_t n, const uint32_t *ext)
{
    np_t *S = &np_state[k];
    int16_t *b = np_buf[k];
    int32_t x = (int32_t)(sys.np_x & 127u) * 258, y = (int32_t)(sys.np_y & 127u) * 258;   /* Q15 0 .. 32766 */
    int32_t g0, g1;                              /* the level: from, to (this block) */
    uint32_t i, j;
    {   /* the knobs glide (no zipper noise as they turn: synth.c, ksm_glide); a new note, or a change made in silence,
         * takes them at once */
        int32_t gt = (int32_t)NP_GAIN[alg % NP_COUNT] * (alg >= NP_BIGBANG ? 64 : (int32_t)(sys.np_level & 127u)) / 64;
                                                 /* (BIG BANG, PIANO: their own knobs where LEVEL was: at 64) */
        if (voice[k].age != S->kage || S->kep != ksm_epoch) {
            S->kage = voice[k].age;
            S->kep = ksm_epoch;
            S->xs = x;
            S->ys = y;
            S->gs = gt;
            g0 = gt;
        } else {
            int32_t dx = x - S->xs, dy = y - S->ys, dg = gt - S->gs;
            S->xs = dx < 24 && dx > -24 ? x : S->xs + dx / 16;
            S->ys = dy < 24 && dy > -24 ? y : S->ys + dy / 16;
            dg = dg < 16 && dg > -16 ? dg : dg / 16;
            g0 = S->gs;
            S->gs += dg;
        }
        g1 = S->gs;
        x = S->xs;
        y = S->ys;
    }
    uint32_t fk[CTL], hgt = sys.menv_target ? 0u : sys.menv_height & 127u, fm = hgt;   /* (on a parameter: keys.c) */
    if (inc > 0x20000000u)                       /* (an eighth of the rate: room for the ratios above it) */
        inc = 0x20000000u;
    if (n > CTL)
        n = CTL;
    {   /* the MOD ENV: an AD envelope, started by each note (not gated: it runs its course), per sample. LENGTH the
         * whole of it, SKEW where the peak falls (0: at once, a fall; 64: the middle; 127: a rise, then off) */
        uint32_t len = ENV_FRAMES[sys.menv_len & 127u], a = len * (sys.menv_skew & 127u) / 127u;   /* (<= 441000 x 127: 32 bits) */
        /* (the divisors never 0, even where the guard says so: this compiler may divide before it tests, and on the
         * second core a divide by 0 traps; SKEW 0 or 127 made one) */
        uint32_t da = a ? a : 1u, dd = len - a ? len - a : 1u;
        uint32_t ia = a ? ENV_MAX / da : ENV_MAX, id = len - a ? ENV_MAX / dd : ENV_MAX;
        const voice_t *v = &voice[k];
        if ((v->stage == ST_ATTACK || v->stage == ST_SUSTAIN) && v->age != S->mage) {   /* a note: from 0 again */
            S->mage = v->age;
            S->mstage = 0;
            S->menv = 0;
        }
        for (i = 0; i < n; i++) {
            if (S->mstage == 0) {
                if (ENV_MAX - S->menv <= ia) {
                    S->menv = ENV_MAX;
                    S->mstage = 1;
                } else
                    S->menv += ia;
            } else if (S->mstage == 1) {
                if (S->menv <= id) {
                    S->menv = 0;
                    S->mstage = 2;
                } else
                    S->menv -= id;
            }
            fk[i] = (uint32_t)np_bend((int32_t)(S->menv >> 15), sys.menv_bend);   /* (Q15, BEND, for the pitch below) */
        }
    }
    {   /* the MOD ENV straight on the pitch (no oscillator, no VCA between): HEIGHT^3 the octaves at its peak
         * (0 .. 3: fine near 0), up or down (bit 7: down), the sweep back to the note */
        int32_t hq = (int32_t)(((uint64_t)hgt * hgt * hgt * 6441246u) >> 26);   /* Q16 octaves (x 3 / 127^3) */
        if (sys.menv_height & 0x80u)
            hq = -hq;
        if (voice[k].vfm)                        /* (velocity: a soft note's sweep shallower) */
            hq = (hq * (256 - (int32_t)voice[k].vfm)) >> 8;
        if (voice + k == render_tv) {            /* (the screen: the sweep now, 0..32767 of 3 octaves) */
            int32_t r = ((hq >> 2) * np_bend((int32_t)(S->menv >> 15), sys.menv_bend)) >> 13;   /* (Q16 octaves) */
            r = r < 0 ? -r : r;
            sys.menv_view = (uint16_t)(r / 6 > 32767 ? 32767 : r / 6);
        }
        if (fm) {   /* per sample the factor on every audio-rate increment, Q16 (the envelope is slow: every 4) */
            uint32_t pf = 65536u;
            for (i = 0; i < n; i++) {
                if (!(i & 3u))
                    pf = px_exp2((int32_t)(((int64_t)hq * (int32_t)fk[i]) >> 15));
                fk[i] = pf > 0x80000u ? 0x80000u : pf;
            }
        }
    }
    if (ext) {   /* FREE EG FM (synth.c): its factor each sample, on the MOD ENV's (Q16) */
        for (i = 0; i < n; i++)
            fk[i] = fm ? np_mul(fk[i], ext[i]) : ext[i];
        fm = 1;
    }
#define NPF(v) (fm ? np_mul((v), fk[i]) : (v))   /* an audio-rate increment, the FM on it */
    switch (alg) {
    /* ---------------------------------------------------------- A: textures --- */
    case 0: {   /* RADIO OH NO: four squares cross-modulating in pairs, summed */
        uint32_t r = np_ratio(x, -1, 2), i1 = np_mul(inc, r), i2 = np_mul(inc, 49152u), i3 = np_mul(i1, 87381u);
        uint32_t pw = 0x80000000u + (uint32_t)((y - 16384) * 98304);
        for (i = 0; i < n; i++) {
            int32_t o0 = np_sq(S->ph[0]), o1 = np_pulse(S->ph[1], pw), o2 = np_sq(S->ph[2]), o3 = np_pulse(S->ph[3], pw);
            S->ph[0] += NPF(np_fm(inc, o1, 16384));
            S->ph[1] += NPF(np_fm(i1, o0, 16384));
            S->ph[2] += NPF(np_fm(i2, o3, 16384));
            S->ph[3] += NPF(np_fm(i3, o2, 16384));
            out[i] = (o0 + o1 + o2 + o3) / 4;
        }
        break;
    }
    case 1: {   /* RWALK FLANGE: four random walkers tune pulse oscillators that FM a sine; a flanger after it */
        int32_t step = 4 + (x >> 7) * 4, depth = 8 + (y >> 10);
        for (i = 0; i < n; i++) {
            int32_t m = 0, s, d, fl;
            for (j = 0; j < 4u; j++) {
                np_walk(&S->walk[j], &S->rng, step, 65536);
                S->mp[j] += NPF(np_mul(inc, (uint32_t)(65536 + (S->walk[j] >> 1)) * (j + 1u) / 2u));
                m += np_pulse(S->mp[j], 0x80000000u);
            }
            S->ph[0] += NPF(np_fm(inc, m / 4, 20000));
            s = np_sin(S->ph[0]);
            b[NB_FLANGE + (S->fw & (NL_FLANGE - 1u))] = (int16_t)s;
            S->ph[1] += 3000u * 97u;                 /* the flanger's LFO, ~0.3 Hz */
            d = 2 + ((np_tri(S->ph[1]) + 32768) * depth >> 16);
            fl = b[NB_FLANGE + ((S->fw - (uint32_t)d) & (NL_FLANGE - 1u))];
            S->fw++;
            out[i] = (s + fl * y / 32767) / 2;
        }
        break;
    }
    case 2:     /* XMOD RING SQR: two squares FM each other; their ring modulation */
    case 3: {   /* XMOD RING SINE: the same with sines */
        uint32_t i1 = np_mul(inc, np_ratio(x, -2, 2));
        for (i = 0; i < n; i++) {
            int32_t o0 = alg == 2u ? np_sq(S->ph[0]) : np_sin(S->ph[0]);
            int32_t o1 = alg == 2u ? np_sq(S->ph[1]) : np_sin(S->ph[1]);
            S->ph[0] += NPF(np_fm(inc, o1, y));
            S->ph[1] += NPF(np_fm(i1, o0, y));
            out[i] = (o0 * o1) >> 15;
        }
        break;
    }
    case 4: {   /* CROSS MOD RING: sine, saw, square, triangle FM each other in a chain (last to first); their product */
        uint32_t r = 65536u + (uint32_t)x * 4u, i1 = np_mul(inc, r), i2 = np_mul(i1, r), i3 = np_mul(i2, r);
        for (i = 0; i < n; i++) {
            int32_t w0 = np_sin(S->ph[0]), w1 = np_saw(S->ph[1]), w2 = np_sq(S->ph[2]), w3 = np_tri(S->ph[3]), p;
            S->ph[0] += NPF(np_fm(inc, w3, y));
            S->ph[1] += NPF(np_fm(i1, w0, y));
            S->ph[2] += NPF(np_fm(i2, w1, y));
            S->ph[3] += NPF(np_fm(i3, w2, y));
            p = (w0 * w1) >> 15;
            p = (p * w2) >> 15;
            out[i] = (p * w3) >> 14;
        }
        break;
    }
    case 5: {   /* RESONOISE: a square FMs a sine, folded, through a band-pass whose cutoff white noise moves */
        uint32_t i1 = np_mul(inc, np_ratio(x, -1, 2));
        int32_t fold = 32768 + y * 8;                /* gain 1 .. 9 before the fold, Q15 */
        for (i = 0; i < n; i++) {
            int32_t m = np_sq(S->ph[1]), s, v;
            S->ph[1] += NPF(i1);
            S->ph[0] += NPF(np_fm(inc, m, 16384));
            s = (int32_t)(((int64_t)np_sin(S->ph[0]) * fold) >> 15);
            v = np_tri((uint32_t)s * 65536u + 0x40000000u);   /* the fold: a triangle of the scaled wave */
            out[i] = np_bp(&S->lo[0], &S->bd[0], v, np_mul(inc * 3u, (uint32_t)(65536 + np_noise(&S->rng))), 6000) * 2;
        }
        break;
    }
    case 6:     /* GRAIN GLITCH: a square through a granular cell, fed back as FM; the XOR of the two */
    case 7:     /* GRAIN GLITCH 2: the same, the cell's output */
    case 8: {   /* GRAIN GLITCH 3: a saw through it, the cell's output */
        uint32_t len = 32u + (uint32_t)(x >> 5);     /* 32 .. 1055 -> within the cell */
        len = len > NL_GRAIN - 2u ? NL_GRAIN - 2u : len;
        for (i = 0; i < n; i++) {
            int32_t o = alg == 8u ? np_saw(S->ph[0]) : np_sq(S->ph[0]);
            int32_t g = np_grain(S, b, o, 98304u, len);   /* read back 1.5x: a fifth up, glitching at each grain */
            S->ph[0] += NPF(np_fm(inc, g, y));
            out[i] = alg == 6u ? (o * g) >> 15 : g;     /* (XOR of two squares: their product) */
        }
        break;
    }
    case 9: {   /* BASURILLA: three white noises gated by three pulse LFOs */
        uint32_t base = np_mul(inc >> 6, np_ratio(x, -2, 3)), pw = 0x08000000u + (uint32_t)y * 0x7000u;
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            S->mp[0] += NPF(base);
            S->mp[1] += NPF(np_mul(base, 89784u));    /* x 1.37 */
            S->mp[2] += NPF(np_mul(base, 117309u));   /* x 1.79 */
            for (j = 0; j < 3u; j++)
                if (S->mp[j] < pw)
                    s += np_noise(&S->rng);
            out[i] = s / 2;
        }
        break;
    }
    /* -------------------------------------------------------- B: clusters --- */
    case 10: {  /* CLUSTER SAW: 16 saws a linear spread apart; Y how many */
        uint32_t nv = 2u + (uint32_t)(y * 14 / 32767), sp = (uint32_t)x / 8u;   /* (0 .. 0.0625 of f apart) */
        static const uint32_t RD[8] = {1431655766u, 1073741825u, 858993460u, 715827883u, 613566757u, 536870913u,
                                       477218589u, 429496730u};   /* (2^32 / d + 1: / 3 .. 10 as a multiply, exact
                                                                   * for |s| <= 16 x 32768) */
        uint64_t rd = RD[nv / 2u - 1u];
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            for (j = 0; j < nv; j++) {
                S->ph[j] += NPF(np_mul(inc, 65536u + sp * j));
                s += np_saw(S->ph[j]);
            }
            out[i] = s < 0 ? -(int32_t)(((uint64_t)(uint32_t)-s * rd) >> 32) : (int32_t)(((uint64_t)(uint32_t)s * rd) >> 32);
        }
        break;
    }
    case 11: {  /* PW CLUSTER: six detuned pulses, their pulse width Y */
        uint32_t pw = 0x10000000u + (uint32_t)y * 0x6000u, ci[6];
        for (j = 0; j < 6u; j++)                     /* (the increments: once a block) */
            ci[j] = np_mul(inc, (uint32_t)(65536 + ((int32_t)j * 2 - 5) * (x >> 6)));
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            for (j = 0; j < 6u; j++) {
                S->ph[j] += NPF(ci[j]);
                s += np_pulse(S->ph[j], pw);
            }
            out[i] = s / 4;
        }
        break;
    }
    case 12:    /* CR CLUSTER: six detuned sines, a slow FM on all */
    case 13:    /* SINE FM CLUST: six detuned sines, each FMed by its own sine */
    case 14: {  /* TRI FM CLUST: six detuned triangles, each FMed by its own sine */
        uint32_t cv[6], mv[6];
        for (j = 0; j < 6u; j++) {                   /* (the increments: once a block) */
            cv[j] = np_mul(inc, (uint32_t)(65536 + ((int32_t)j * 2 - 5) * (x >> 6)));
            mv[j] = np_mul(cv[j], NP_MODR[j]);
        }
        for (i = 0; i < n; i++) {
            int32_t s = 0, slow;
            S->mp[5] += NPF(inc >> 4);
            slow = np_sin(S->mp[5]);
            for (j = 0; j < 6u; j++) {
                uint32_t ci = cv[j];
                int32_t m;
                if (alg == 12u)
                    m = slow;
                else {
                    S->mp[j] += NPF(mv[j]);
                    m = np_sin(S->mp[j]);
                }
                S->ph[j] += NPF(np_fm(ci, m, y));
                s += alg == 14u ? np_tri(S->ph[j]) : np_sin(S->ph[j]);
            }
            out[i] = s / 4;
        }
        break;
    }
    case 15:    /* PRIME CLUSTER: 16 saws detuned by the primes, one sine FMs them all */
    case 16: {  /* PRIME C NOISE: 16 triangles detuned by the primes, white noise FMs them */
        uint32_t sp = (uint32_t)x >> 6, ci[16];      /* a prime's share, 0 .. 512 / 65536 */
        for (j = 0; j < 16u; j++)                    /* (the increments: once a block) */
            ci[j] = np_mul(inc, 65536u + sp * NP_PRIMES[j]);
        for (i = 0; i < n; i++) {
            int32_t s = 0, m;
            if (alg == 15u) {
                S->mp[0] += NPF(inc >> 1);
                m = np_sin(S->mp[0]);
            } else
                m = np_noise(&S->rng);
            for (j = 0; j < 16u; j++) {
                S->ph[j] += NPF(np_fm(ci[j], m, y >> 1));
                s += alg == 15u ? np_saw(S->ph[j]) : np_tri(S->ph[j]);
            }
            out[i] = s / 6;
        }
        break;
    }
    case 17:    /* FIBONACCI: 16 saws detuned along the Fibonacci series */
    case 18: {  /* PARTIAL CLUST: 16 saws, each the one below times (1 + spread) */
        uint32_t sp = (uint32_t)x >> 7, mul = 65536u + (uint32_t)x / 2u, ci[16];
        int32_t tone = 2000 + y * 3 / 2;             /* the one-pole low-pass, Q15 */
        uint32_t c = inc;
        for (j = 0; j < 16u; j++) {
            if (alg == 17u)
                ci[j] = np_mul(inc, 65536u + sp * NP_FIB[j] / 8u);
            else {
                ci[j] = c;
                c = np_mul(c, mul);
                if (c > 0x30000000u)
                    c = 0x30000000u;
            }
        }
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            for (j = 0; j < 16u; j++) {
                S->ph[j] += NPF(ci[j]);
                s += np_saw(S->ph[j]);
            }
            S->lp += (int32_t)(((int64_t)(s / 6 - S->lp) * tone) >> 15);
            out[i] = S->lp;
        }
        break;
    }
    case 19: {  /* PHASING CLUST: 16 squares a spread apart, a slow LFO drifting each */
        uint32_t sp = (uint32_t)x >> 7;
        for (i = 0; i < n; i++) {
            int32_t s = 0, lfo;
            S->mp[0] += 9000u;                       /* ~0.1 Hz */
            lfo = np_sin(S->mp[0]);
            for (j = 0; j < 16u; j++) {
                int32_t dr = (int32_t)(((int64_t)lfo * y * (int32_t)(j + 1u)) >> 30);   /* up to +-16 / 65536 x j */
                S->ph[j] += NPF(np_mul(inc, (uint32_t)(65536 + (int32_t)(sp * j) + dr * 4)));
                s += np_sq(S->ph[j]);
            }
            out[i] = s / 8;
        }
        break;
    }
    /* ---------------------------------------------------- C: harsh & wild --- */
    case 20: {  /* BASURA TOTAL: a bent LFSR clocked by an oscillator its own bits FM; reverb */
        for (i = 0; i < n; i++) {
            uint32_t prev = S->ph[0];
            int32_t bit = S->lfsr & 1u ? 32767 : -32767, w;
            S->ph[0] += NPF(np_fm(inc * 4u, bit, x));
            if (S->ph[0] < prev)                     /* a clock: shift the LFSR (Galois, taps 16 14 13 11) */
                S->lfsr = (S->lfsr >> 1) ^ ((0u - (S->lfsr & 1u)) & 0xB400u);
            w = np_verb(S, b, bit / 2, 16000 + y / 3);
            out[i] = (int32_t)(((int64_t)bit * (32767 - y) + (int64_t)w * y) >> 16);
        }
        break;
    }
    case 21: {  /* ATARI: two squares, each moving the other's pulse width and frequency */
        uint32_t i1 = np_mul(inc, np_ratio(x, -1, 2));
        for (i = 0; i < n; i++) {
            int32_t o0 = np_pulse(S->ph[0], 0x80000000u + (uint32_t)(S->last[1] * (y >> 2))),
                    o1 = np_pulse(S->ph[1], 0x80000000u + (uint32_t)(S->last[0] * (y >> 2)));
            S->ph[0] += NPF(np_fm(inc, o1, y >> 1));
            S->ph[1] += NPF(np_fm(i1, o0, y >> 1));
            S->last[0] = o0;
            S->last[1] = o1;
            out[i] = (o0 + o1) / 2;
        }
        break;
    }
    case 22: {  /* WAKING FILOM.: 16 random walkers in a box of X tune pulse oscillators, each FMing the next */
        int32_t box = 2000 + x * 3 / 2;              /* (at most ~0.78: no frequency below 0) */
        uint32_t ci[16];
        for (i = 0; i < n; i++) {
            int32_t s = 0, prev = S->last[0];
            for (j = 0; j < 16u; j++) {
                int32_t o;
                if (!(i & 7u)) {                     /* (the walk, and the increment it sets: every 8 samples) */
                    np_walk(&S->walk[j], &S->rng, 600, box);
                    ci[j] = np_mul(inc, (uint32_t)(65536 + S->walk[j]));
                }
                S->ph[j] += NPF(np_fm(ci[j], prev, y >> 2));
                o = np_pulse(S->ph[j], 0x80000000u);
                s += o;
                prev = o;
            }
            S->last[0] = prev;
            out[i] = s / 6;
        }
        break;
    }
    case 23: {  /* P S H: noise sampled and held at the note's rate (X its multiple); dirty reverb */
        uint32_t ri = np_mul(inc, np_ratio(x, -3, 2));
        for (i = 0; i < n; i++) {
            uint32_t prev = S->ph[0];
            int32_t w;
            S->ph[0] += NPF(ri);
            if (S->ph[0] < prev)
                S->sh = np_noise(&S->rng);
            w = np_verb(S, b, S->sh / 2, 26000);
            out[i] = (int32_t)(((int64_t)S->sh * (32767 - y) + (int64_t)w * y) >> 15);
        }
        break;
    }
    case 24: {  /* ARRAY ROCKS: a sine FMs a 64-step wavetable whose steps are scrambled as it plays */
        uint32_t i1 = np_mul(inc, np_ratio(x, -2, 2)), every = 4096u - (uint32_t)y / 9u;
        for (i = 0; i < n; i++) {
            int32_t m = np_sin(S->mp[0]);
            S->mp[0] += NPF(i1);
            S->ph[0] += NPF(np_fm(inc, m, 16384));
            out[i] = S->wt[S->ph[0] >> 26];
            if ((uint32_t)++S->cnt >= every) {       /* scramble a step */
                S->cnt = 0;
                S->wt[np_rnd(&S->rng) & 63u] = (int16_t)np_noise(&S->rng);
            }
        }
        break;
    }
    case 25:    /* EXIST. PAIN: sample & hold noise through four band-passes swept by four triangle LFOs */
    case 26: {  /* WHO KNOWS: a pulse wave through the same four swept band-passes */
        static const uint32_t BASE[4] = {0x01200000u, 0x02400000u, 0x04800000u, 0x09000000u};   /* ~200 .. 1600 Hz */
        static const uint16_t LR[4] = {300, 470, 710, 1130};   /* hundredths of a hertz */
        uint32_t lr = 1u + (uint32_t)x / 64u, fq[4];
        for (i = 0; i < n; i++) {
            int32_t in, s = 0;
            uint32_t prev = S->ph[0];
            S->ph[0] += NPF(inc);
            if (alg == 25u) {
                if (S->ph[0] < prev)
                    S->sh = np_noise(&S->rng);
                in = S->sh;
            } else
                in = np_pulse(S->ph[0], 0x40000000u);
            for (j = 0; j < 4u; j++) {
                S->mp[j] += LR[j] * 10u * lr;            /* 0.3 .. 1.1 Hz, x 1 .. 512 */
                if (!(i & 3u))                           /* (the sweep's frequency: every 4 samples) */
                    fq[j] = np_mul(BASE[j], (uint32_t)px_exp2((np_tri(S->mp[j]) * (y >> 6)) >> 7));
                s += np_bp(&S->lo[j], &S->bd[j], in, fq[j], 3000);
            }
            out[i] = s / 2;
        }
        break;
    }
    case 27: {  /* SATAN WORKOUT: white noise FMs a sine, then down-sampled and driven into a clip */
        uint32_t every = 1u + (uint32_t)y / 1100u;
        for (i = 0; i < n; i++) {
            S->ph[0] += NPF(np_fm(inc, np_noise(&S->rng), x));
            if ((uint32_t)++S->cnt >= every) {
                S->cnt = 0;
                S->hold = np_clip(np_sin(S->ph[0]) * 4);
            }
            out[i] = S->hold;
        }
        break;
    }
    case 28: {  /* RWALK BITCRUSH: nine walkers in a box tune nine pulses (and their widths); crushed by the first */
        int32_t box = 2000 + x * 3 / 2;
        uint32_t ci[9], bits = 12u;
        for (i = 0; i < n; i++) {
            int32_t s = 0, w;
            for (j = 0; j < 9u; j++) {
                if (!(i & 7u)) {
                    np_walk(&S->walk[j], &S->rng, 500, box);
                    ci[j] = np_mul(inc, (uint32_t)(65536 + S->walk[j]) * (1u + j % 3u) / 2u);
                }
                S->ph[j] += NPF(ci[j]);
                s += np_pulse(S->ph[j], 0x80000000u + (uint32_t)S->walk[j] * 16384u);
            }
            if (!(i & 7u))                           /* (with the walk: 3 .. 12 bits, no divide a sample) */
                bits = 3u + (uint32_t)((S->walk[0] + box) * 9 / (2 * box + 1));
            s /= 4;
            s &= ~((1 << (16u - bits)) - 1);         /* (a mask: shifting a negative left is undefined) */
            w = np_verb(S, b, s / 2, 12000 + y / 2);
            out[i] = (s + w) / 2;
        }
        break;
    }
    case 30: {  /* BIG BANG: the standard waves from the harmonic series, entropy, the bang, the stretch */
        int32_t w16[16], e, e2, st = ksm[voice[k].part & 1u].v[2] * 128 / 127, a;   /* (EXPAND smoothed: synth.c) */
        uint32_t ci[16], seg = (uint32_t)x * 3u, sg = seg >> 15, fr = seg & 32767u, chs = sys.chaos & 127u;
        if (sg > 2u) {
            sg = 2u;
            fr = 32767u;
        }
        for (j = 0; j < 16u; j++)                /* the wave: two of the four standard ones, mixed */
            w16[j] = ((int32_t)NP_BB_W[sg][j] * (32768 - (int32_t)fr) + (int32_t)NP_BB_W[sg + 1u][j] * (int32_t)fr) >> 15;
        if (!chs)
            S->bang = 0;
        else {                                   /* the bang cools: 30 ms .. 4 s (41 .. 5500 blocks) */
            uint32_t tb = (uint32_t)(((uint64_t)41u * px_exp2((int32_t)(chs * 463339u / 127u))) >> 16);
            S->bang -= (int32_t)(((int64_t)S->bang * (int32_t)(65536u / (tb ? tb : 1u))) >> 16) + 1;
            S->bang = S->bang < 0 ? 0 : S->bang;
        }
        e = y + (int32_t)(((int64_t)(32767 - y) * S->bang) >> 15);   /* the entropy now, Q15 */
        e2 = (int32_t)(((int64_t)e * e) >> 15);
        for (j = 0; j < 16u; j++) {              /* each partial: its place in the series (stretched), walking off */
            uint32_t r = px_exp2((int32_t)(((int64_t)(65536 + st) * NP_LOG2[j]) >> 16));   /* n^(1 + s), Q16 */
            uint64_t c;
            np_walk(&S->walk[j], &S->rng, 200 + (e >> 3), 65536);
            c = ((uint64_t)inc * r) >> 16;
            c = (c * (uint32_t)(65536 + (int32_t)(((int64_t)S->walk[j] * e2) >> 16))) >> 16;   /* (+-50 %) */
            ci[j] = c > 0x70000000u || !w16[j] ? 0u : (uint32_t)c;   /* (over the top, or not in this wave: out) */
        }
        a = (int32_t)(inc >> 12);                /* the noise, low-passed near the note (~5 x) */
        a = a > 32767 ? 32767 : a < 64 ? 64 : a;
        for (i = 0; i < n; i++) {
            int32_t s = 0;
            for (j = 0; j < 16u; j++)
                if (ci[j]) {
                    S->ph[j] += NPF(ci[j]);
                    s += (np_sin(S->ph[j]) * w16[j]) >> 15;
                }
            S->lp += (int32_t)(((int64_t)(np_noise(&S->rng) - S->lp) * a) >> 15);
            s = (int32_t)(((int64_t)s * (32768 - (e2 * 3 >> 2)) + (int64_t)S->lp * e2) >> 15);
            out[i] = np_clip(s);
        }
        break;
    }
    case 31:    /* PIANO (above) */
        np_piano(k, S, inc, out, n, fk, fm);
        break;
    default: {  /* RWALK LFREE: four pulses, frequency and width walking in a box; reverb */
        int32_t box = 2000 + x * 3 / 2;
        for (i = 0; i < n; i++) {
            int32_t s = 0, w;
            for (j = 0; j < 4u; j++) {
                if (!(i & 3u))
                    np_walk(&S->walk[j], &S->rng, 900, box);
                S->ph[j] += NPF(np_mul(inc, (uint32_t)(65536 + S->walk[j]) * (j + 2u) / 3u));
                s += np_pulse(S->ph[j], 0x80000000u + (uint32_t)S->walk[(j + 1u) & 3u] * 16384u);
            }
            w = np_verb(S, b, s / 4, 12000 + y / 2);
            out[i] = (s / 2 + w) / 2;
        }
        break;
    }
    }
#undef NPF
    {   /* the level (smoothed above), a ramp through the block */
        int32_t g = g0, dg = n ? (g1 - g0) / (int32_t)n : 0;
        for (i = 0; i < n; i++) {
            g = i + 1u == n ? g1 : g + dg;
            out[i] = np_clip((int32_t)(((int64_t)out[i] * g) >> 12));
        }
    }
}
