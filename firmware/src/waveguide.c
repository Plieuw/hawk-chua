/* SPDX-License-Identifier: GPL-3.0-only */
/* The FX waveguides: three more models for resonator.c, beside Rings' (the same page, the same polyphony: one per
 * note, fed by the voices that play it). Digital waveguides: travelling waves in delay lines, scattered where the
 * tube changes, reflected at its ends. This firmware's own fixed-point code (after the textbook models: Kelly and
 * Lochbaum's vocal tract, Cook's slide flute / STK Flute's jet and bore), signals Q20 like Rings' models.
 *
 *   THROAT   a vocal tract: a tube of up to 12 sections (Kelly-Lochbaum; each 2 .. 10 samples long), the voices its glottal source. ANIMAL sets
 *            the tube's length (its formants: a cat's short throat, a cow's long one, an elephant's longer still)
 *            and how much the mouth opens with the voice's loudness ("meow", "moo"); VOWEL (POSITION) the tongue:
 *            U, O, A, E, I, morphing; BRIGHTNESS the mouth's radiation; DAMPING how long the tube rings (the
 *            glottis' and the lips' reflections)
 *   FLUTE    a jet over the mouth of a bore tuned to the note: the voices' loudness is the breath that blows it,
 *            their sound rides on the breath (as STK's Flute: jet delay, the jet's x (x^2 - 1), the bore's low-pass
 *            reflection). JET (STRUCTURE) the jet's length: long blows the fundamental, short overblows to octaves
 *            and twelfths; BREATH (POSITION) the breath noise; BRIGHTNESS the bore's loss of highs; DAMPING its loss
 *   SYRINX   a bird's two-sided voice box: two small whistles (jets on short bores) two octaves above the note, their
 *            pitch rising with the breath (CHIRP: STRUCTURE), the second a little apart (TWO VOICE: POSITION: beats
 *            .. a fifth), through the beak's resonance (BRIGHTNESS: 1.5 .. 6 kHz); DAMPING the whistles' loss
 *
 * The waveguides run at half the rate (22 kHz: half the CPU; their lines half as long, their one-poles the same in
 * hertz, wg_half): their input the average of two samples, their output interpolated back; the beak's band-pass
 * (SYRINX, HAWK) and the output's DC blocker run at the full rate.
 *
 * The breath: each model follows its input's loudness (an envelope, ~2 ms up, ~80 ms down); the flute and the
 * syrinx blow only while it plays, the throat's mouth moves with it. */

/* ------------------------------------------------------------------ THROAT --- */
#define WG_NSEC 12                               /* sections at most (half the junctions of 24: the CPU) */
#define WG_KMAX 10                               /* samples per section at most (a section: k samples each way) */
enum { WA_CAT, WA_MONKEY, WA_HUMAN, WA_DOG, WA_WOLF, WA_COW, WA_LION, WA_ELEPHANT, WA_COUNT };
static const char *const WG_ANIMAL[WA_COUNT] = {"CAT", "MONKEY", "HUMAN", "DOG", "WOLF", "COW", "LION", "ELEPHANT"};
static const struct {
    uint8_t n, k;                                /* sections, samples each: the tube n k / 44100 s one way (x 350 m/s) */
    uint8_t wah;                                 /* how much the mouth follows the loudness, /128 */
    uint8_t nasal;                               /* the glottis' reflection's floor, /128 (a "closed", nasal tone) */
} WG_ANI[WA_COUNT] = {
    {5, 2, 120, 90},                         /* cat: 8 cm, the mouth opening with the meow */
    {7, 2, 90, 70},                          /* monkey: 11 cm, hoots */
    {11, 2, 40, 60},                         /* human: 17 cm */
    {7, 4, 70, 70},                          /* dog: 22 cm, barks open */
    {10, 4, 60, 80},                         /* wolf: 32 cm, the howl's slow vowel */
    {12, 4, 80, 100},                         /* cow: 38 cm, "moo": nasal, closed then open */
    {10, 6, 50, 70},                         /* lion: 48 cm, a roar */
    {11, 10, 60, 110},                        /* elephant: 87 cm (the trunk), a low rumble */
};
/* the area along the tube, glottis to lips, per vowel (relative, Q4; after Fant's measured shapes, simplified) */
#define WG_NV 5
static const uint8_t WG_SHAPE[WG_NV][16] = {
    {19, 26, 38, 51, 58, 51, 38, 19, 10, 13, 26, 38, 26, 10, 5, 3},       /* U: lips rounded, nearly closed */
    {16, 22, 32, 38, 38, 32, 22, 16, 16, 26, 38, 48, 42, 29, 16, 10},     /* O */
    {16, 13, 10, 8, 8, 10, 14, 22, 35, 48, 58, 64, 67, 64, 58, 51},       /* A: the pharynx narrow, the mouth wide */
    {19, 29, 42, 51, 54, 48, 38, 29, 22, 16, 13, 13, 16, 22, 29, 32},     /* E */
    {22, 42, 58, 67, 70, 64, 48, 32, 16, 8, 5, 4, 5, 10, 19, 26},         /* I: the tongue high, front narrow */
};
static const char *const WG_VOWEL[WG_NV] = {"U", "O", "A", "E", "I"};

typedef struct {
    int32_t f[WG_NSEC][WG_KMAX], b[WG_NSEC][WG_KMAX];   /* each section: its forward and backward waves */
    uint32_t p;                                  /* the place in the sections' lines (0 .. k - 1) */
    int32_t r[WG_NSEC];                          /* the junctions' reflections (Q15), this block */
    int32_t lip, rad, glo;                       /* the lips' low-pass, the mouth's softening, the glottis' */
    int32_t env;                                 /* the input's loudness, Q20 */
    int32_t iav, oav, ag;                        /* the level: in, out (Q14 averages), the gain (Q12) */
    int32_t yp;                                  /* the last half-rate output (the interpolation) */
} wg_throat_t;

/* a one-pole's coefficient (Q15) at the full rate -> the same in hertz at half the rate: 1 - (1 - a)^2 */
static inline int32_t wg_half(int32_t a) { return 2 * a - ((a * a) >> 15); }

/* the loudness of n samples of in (Q20) into *env: ~2 ms up, ~80 ms down (per sample) */
static int32_t wg_follow(int32_t *env, const int32_t *in, uint32_t n)
{
    uint32_t i;
    int32_t e = *env;
    for (i = 0; i < n; i++) {
        int32_t a = in[i] < 0 ? -in[i] : in[i];
        e += a > e ? (a - e) / 64 : -((e - a) / 4096) - (e > 0);
    }
    return *env = e < 0 ? 0 : e;
}

static void wg_throat_run(wg_throat_t *T, const int32_t *in, int32_t *y, uint32_t n, uint32_t animal, int32_t vowel,
                          int32_t bright, int32_t damp)
{
    uint32_t an = animal % WA_COUNT, ns = WG_ANI[an].n, k = WG_ANI[an].k, i, j;
    int32_t env = wg_follow(&T->env, in, n), A[WG_NSEC + 1], open, rg, rl, lp, ro;
    {   /* the areas: the vowel (0..127 across U O A E I), resampled to the sections; the mouth (the last quarter)
         * opened by the loudness as much as the animal does it */
        int32_t pos = vowel * 4 * 256 / 127, v = pos >> 8, fr = pos & 255;
        int32_t e = env > PX_ONE / 2 ? 256 : env / (PX_ONE / 512);   /* 0..256: loud at half scale */
        v = v > WG_NV - 2 ? WG_NV - 2 : v;
        if (pos >= (WG_NV - 1) * 256)
            fr = 256;
        open = (128 - WG_ANI[an].wah) * 2 + WG_ANI[an].wah * (40 + e * 3 / 2) / 128;   /* /256 */
        for (j = 0; j < ns; j++) {
            uint32_t s = j * 15u * 16u / (ns - 1u), s0 = s >> 4, s1 = s0 < 15u ? s0 + 1u : 15u, sf = s & 15u;
            int32_t a0 = WG_SHAPE[v][s0] * (16 - (int32_t)sf) + WG_SHAPE[v][s1] * (int32_t)sf;
            int32_t a1 = WG_SHAPE[v + 1][s0] * (16 - (int32_t)sf) + WG_SHAPE[v + 1][s1] * (int32_t)sf;
            int32_t a = (a0 * (256 - fr) + a1 * fr) >> 8;
            if (j * 4u >= ns * 3u)
                a = a * open >> 8;
            A[j] = a < 40 ? 40 : a;              /* (never shut: a closed mouth would trap a whistle) */
        }
        for (j = 0; j + 1u < ns; j++)            /* Kelly-Lochbaum: r = (A_j - A_j+1) / (A_j + A_j+1) */
            T->r[j] = (A[j] - A[j + 1]) * 32767 / (A[j] + A[j + 1]);
    }
    rg = 22000 + WG_ANI[an].nasal * 40 + damp / 11;   /* the glottis: ~0.67 .. 0.99 (Q15) */
    rg = rg > 32440 ? 32440 : rg;
    rl = -(26000 + damp / 10);                   /* the lips (open end: inverted), 0.79 .. 0.99 */
    rl = rl < -32440 ? -32440 : rl;
    lp = wg_half(2500 + bright / 5);             /* the lips' low-pass, Q15: what reflects (the lows), ~0.4 .. 4 kHz */
    ro = wg_half(6800 + bright / 3);             /* the mouth's softening, Q15 */
    k = (k + 1u) / 2u;                           /* (half the rate: half the samples a section) */
    for (i = 0; i < n; i += 2u) {
        int32_t fo[WG_NSEC], bo[WG_NSEC], lo, x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i];
        uint32_t p = T->p;
        for (j = 0; j < ns; j++) {
            fo[j] = T->f[j][p];
            bo[j] = T->b[j][p];
        }
        for (j = 0; j + 1u < ns; j++) {          /* the junctions */
            int32_t w = px_m(T->r[j], fo[j] - bo[j + 1u], 15);
            T->f[j + 1u][p] = fo[j] + w;         /* (lossless junctions between lossy ends: bounded, unclamped) */
            T->b[j][p] = bo[j + 1u] + w;
        }
        T->glo += px_m(22041, bo[0] - T->glo, 15);   /* the glottis: soft (the highs lost there, ~3 kHz; wg_half(14000)) */
        T->f[0][p] = px_clamp((x >> 1) + px_m(rg, T->glo, 15), -16 * PX_ONE, 16 * PX_ONE);
        lo = fo[ns - 1u];                        /* the lips: the lows reflect (inverted), the highs radiate */
        T->lip += px_m(lp, lo - T->lip, 15);
        T->b[ns - 1u][p] = px_m(rl, T->lip, 15);
        /* out at the mouth: the wave arriving there (the radiation's loss of the lows left out: it would leave a
         * voice below its first formant nearly silent), its top softened (BRIGHTNESS: ~1.5 .. 12 kHz) */
        T->rad += px_m(ro, lo - T->rad, 15);
        y[i] = (T->yp + T->rad) >> 1;            /* (back to the full rate: between the last and this one) */
        if (i + 1u < n)
            y[i + 1u] = T->rad;
        T->yp = T->rad;
        T->p = p + 1u >= k ? 0u : p + 1u;
    }
    {   /* the level: the tube's resonances lift some vowels and animals far above others; a slow automatic gain
         * (~0.1 s) holds the mouth at 3.6 x the input's level (the dry's, out of the FX: RZ_GAIN), x 1/8 .. 24 */
        uint32_t si = 0, so = 0;
        int32_t gt;
        for (i = 0; i < n; i++) {
            si += (uint32_t)(in[i] < 0 ? -in[i] : in[i]) >> 6;
            so += (uint32_t)(y[i] < 0 ? -y[i] : y[i]) >> 6;
        }
        T->iav += ((int32_t)(si / n) - T->iav) / 8;
        T->oav += ((int32_t)(so / n) - T->oav) / 8;
        gt = T->oav > 0 ? (int32_t)((uint32_t)T->iav * 14746u / (uint32_t)(T->oav > 0 ? T->oav : 1)) : 4096;   /* Q12 (32 bits; never / 0) */
        gt = gt < 512 ? 512 : gt > 98304 ? 98304 : gt;
        T->ag += (gt - T->ag) / 16;
        for (i = 0; i < n; i++)
            y[i] = px_clamp(px_m(y[i], T->ag, 12), -64 * PX_ONE, 64 * PX_ONE);
    }
}

/* ---------------------------------------------------- the jet (FLUTE, SYRINX) --- */
/* (the lines are clipped at +-5, not 4: -4.0 in Q20 is 0xFFC00000, which tools/build.py takes for a ROM address) */
static inline int32_t wg_jet(int32_t x)          /* STK's JetTable: x (x^2 - 1), the output clipped to +-1 (Q20) */
{
    int32_t c = px_clamp(x, -2 * PX_ONE, 2 * PX_ONE);
    return px_clamp(px_m(c, px_m(c, c, 20) - PX_ONE, 20), -PX_ONE, PX_ONE);
}
static inline int32_t wg_tap(const int32_t *l, uint32_t mask, uint32_t w, uint32_t d)   /* d samples back, whole */
{
    return l[(w - 1u - (d >> 16)) & mask];
}
static inline int32_t wg_read(const int32_t *l, uint32_t mask, uint32_t w, uint32_t d)   /* d samples back, Q16 */
{
    uint32_t t = w - (d >> 16);
    int32_t a = l[(t - 1u) & mask], b = l[(t - 2u) & mask];
    return a + px_m(b - a, (int32_t)(d & 0xFFFFu), 16);
}

/* ------------------------------------------------------------------- FLUTE --- */
#define WG_BORE 1024u                            /* the bore's line (1.5 periods): 66 Hz and up (lower: up) */
#define WG_JETL 512u
typedef struct {
    int32_t bore[WG_BORE], jet[WG_JETL];
    uint32_t w, rng;
    int32_t lp, dcx, dcy, env;
    int32_t ox, oy;                              /* the output's DC blocker (the breath's pressure is DC in the bore) */
    int32_t yp;                                  /* the last half-rate output */
} wg_flute_t;

/* f0 the note (cycles Q32); jet 0..127 its length (long .. overblowing); breath the noise; bright, damp Q16 */
static void wg_flute_run(wg_flute_t *F, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t jet,
                         int32_t breath, int32_t bright, int32_t damp)
{
    uint32_t i, per = f0 < 65536u ? 0xFFFFFFFFu : px_period(f0 > 0x40000000u ? 0x40000000u : f0) >> 1, bd, jd;   /* Q16,
                                                  * half-rate samples */
    int32_t env = wg_follow(&F->env, in, n), pr, g, loss, nz;
    while (per > (WG_BORE * 2u / 3u - 8u) << 16)  /* fold up an octave */
        per >>= 1;
    bd = per > (4u << 16) ? per - (2u << 16) : 2u << 16;   /* the bore: a period (less the filters' delay) */
    jd = (uint32_t)(((uint64_t)bd * (uint32_t)(16384 - jet * 90)) >> 15);   /* 0.5 .. 0.15 of the bore: the
                                                  * fundamental (bending sharp), the octave, the twelfth */
    jd = jd > (WG_JETL - 4u) << 16 ? (WG_JETL - 4u) << 16 : jd < (2u << 16) ? 2u << 16 : jd;
    pr = env * 16 > PX_ONE * 13 / 10 ? PX_ONE * 13 / 10 : env * 16;   /* the breath: the loudness (STK blows at ~1.1 ..
                                                  * 1.3: it speaks from ~1/16 of full scale up) */
    g = wg_half(4900 + bright / 5);              /* the bore's one-pole (STK: pole 0.65): 0.85 .. 0.45, Q15 */
    loss = 30500 + (int32_t)(((int64_t)damp * 2260) >> 16);   /* 0.93 .. 1.0 */
    nz = breath / 2;                             /* the noise: 0 .. 0.5 of the pressure (STK: 0.15), Q16 */
    for (i = 0; i < n; i += 2u) {
        int32_t b = wg_read(F->bore, WG_BORE - 1u, F->w, bd), t, pd, j, o;
        int32_t noise = px_m(px_m((int32_t)(px_rand(&F->rng) >> 11) - (1 << 20), pr, 20), nz, 16);
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i];
        F->lp += px_m(g, b - F->lp, 15);         /* the open end's reflection: low-passed, its DC blocked */
        t = F->lp;
        o = t - F->dcx + px_m(F->dcy, 32440, 15);   /* (32604 at the full rate) */
        F->dcx = t;
        F->dcy = o;
        t = px_m(o, loss, 15);
        pd = pr + noise + (x >> 3) - (t >> 1);   /* the breath (the voices on it), less the jet's reflection */
        F->jet[F->w & (WG_JETL - 1u)] = px_clamp(pd, -5 * PX_ONE, 5 * PX_ONE);
        j = wg_jet(wg_tap(F->jet, WG_JETL - 1u, F->w + 1u, jd));   /* (the jet: a whole-sample delay) */
        F->bore[F->w & (WG_BORE - 1u)] = px_clamp(j + (t >> 1), -5 * PX_ONE, 5 * PX_ONE);
        F->w++;
        o = b - F->ox + px_m(F->oy, 32440, 15); /* out: the bore, its DC blocked (the breath is DC in it) */
        F->ox = b;
        F->oy = o;
        y[i] = (F->yp + o) >> 1;                 /* (back to the full rate) */
        if (i + 1u < n)
            y[i + 1u] = o;
        F->yp = o;
    }
}

/* ------------------------------------------------------------------ SYRINX --- */
#define WG_SB 128u                               /* each whistle's bore (345 Hz and up) */
#define WG_SJ 64u
typedef struct {
    int32_t bore[2][WG_SB], jet[2][WG_SJ];
    uint32_t w, rng;
    int32_t lp[2], dcx[2], dcy[2], env;
    int32_t blo, bbd;                            /* the beak: a resonant band-pass (Chamberlin) */
    int32_t ox, oy;                              /* the output's DC blocker */
    uint32_t t, quiet;                           /* HAWK: samples since the scream began; the input silent */
    int32_t am, amt;                             /* HAWK: the rasp's level now, its next target */
    int32_t yp;                                  /* the whistles' last half-rate output */
    uint32_t rc;                                 /* HAWK: samples to the rasp's next target */
} wg_syrinx_t;

/* the two whistles at half the rate (SYRINX, HAWK): n samples of input -> their sum at the full rate (interpolated) */
static void wg_whistles(wg_syrinx_t *S, const int32_t *in, int32_t *o, uint32_t n, const uint32_t *bd, const uint32_t *jd,
                        int32_t pr, int32_t loss, int32_t nshift)
{
    uint32_t i, s;
    for (i = 0; i < n; i += 2u) {
        int32_t sum = 0, x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i];
        int32_t noise = px_m((int32_t)(px_rand(&S->rng) >> nshift) - (int32_t)(1u << (31 - nshift)), pr, 22);
        for (s = 0; s < 2u; s++) {
            int32_t b = wg_read(S->bore[s], WG_SB - 1u, S->w, bd[s]), t, pd, j;
            S->lp[s] += px_m(27793, b - S->lp[s], 15);   /* (wg_half(20000)) */
            t = S->lp[s] - S->dcx[s] + px_m(S->dcy[s], 32440, 15);
            S->dcx[s] = S->lp[s];
            S->dcy[s] = t;
            t = px_m(t, loss, 15);
            pd = pr + noise + (x >> 4) - (t >> 1);
            S->jet[s][S->w & (WG_SJ - 1u)] = px_clamp(pd, -5 * PX_ONE, 5 * PX_ONE);
            j = wg_jet(wg_tap(S->jet[s], WG_SJ - 1u, S->w + 1u, jd[s]));
            S->bore[s][S->w & (WG_SB - 1u)] = px_clamp(j + (t >> 1), -5 * PX_ONE, 5 * PX_ONE);
            sum += b;
        }
        S->w++;
        o[i] = (S->yp + sum) >> 1;
        if (i + 1u < n)
            o[i + 1u] = sum;
        S->yp = sum;
    }
}

static void wg_syrinx_run(wg_syrinx_t *S, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t chirp,
                          int32_t two, int32_t bright, int32_t damp)
{
    uint32_t i, s, bd[2], jd[2];
    int32_t env = wg_follow(&S->env, in, n), pr, loss, bf, ow[CTL];
    int32_t e = env > PX_ONE / 12 ? 65536 : env * 12 / 16;   /* 0..1 Q16: the breath, full at 1/12 scale */
    pr = env * 16 > PX_ONE * 13 / 10 ? PX_ONE * 13 / 10 : env * 16;
    for (s = 0; s < 2u; s++) {   /* the pitch: two octaves up, rising with the breath; the second apart */
        int32_t oct = 2 * 65536 + (int32_t)(((int64_t)chirp * 1032 * e) >> 16)     /* up to +2 octaves */
                      + (s ? two * 301 : 0);                                      /* +0 .. 7 semitones */
        uint64_t f = ((uint64_t)f0 * px_exp2(oct)) >> 16;
        uint32_t per = f < 65536u ? 0xFFFFFFFFu : f >= 0x40000000u ? 4u << 16 : px_period((uint32_t)f) >> 1;
        while (per > (WG_SB - 8u) << 16)         /* (half-rate samples) */
            per >>= 1;
        bd[s] = per > (4u << 16) ? per - (2u << 16) : 2u << 16;
        jd[s] = bd[s] / 3u < (2u << 16) ? 2u << 16 : bd[s] / 3u;
        jd[s] = jd[s] > (WG_SJ - 4u) << 16 ? (WG_SJ - 4u) << 16 : jd[s];
    }
    loss = 29000 + (int32_t)(((int64_t)damp * 3400) >> 16);
    bf = 14000 + bright / 6;                     /* the beak: 2 sin(pi f / FS), ~1.5 .. 6 kHz (Q15) */
    wg_whistles(S, in, ow, n, bd, jd, pr, loss, 12);
    for (i = 0; i < n; i++) {
        int32_t o = ow[i], hi;
        S->blo += px_m(bf, S->bbd, 15);          /* the beak (Q 2.5) */
        hi = o - S->blo - px_m(13107, S->bbd, 15);
        S->bbd = px_clamp(S->bbd + px_m(bf, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
        S->blo = px_clamp(S->blo, -16 * PX_ONE, 16 * PX_ONE);
        S->oy = (o >> 2) - S->ox + px_m(S->oy, 32604, 15);
        S->ox = o >> 2;
        y[i] = S->bbd + S->oy;
    }
}

/* -------------------------------------------------------------------- HAWK --- */
/* A raptor's scream (the red-tailed hawk's "kee-eeeee-arr"), from the SYRINX: its two whistles a quarter tone apart
 * (they beat: the roughness), ~2.5 octaves above the note, and a pitch that rises a little at the start and then
 * falls half an octave over the scream (SCREAM: 0.1 .. 2.7 s, from when the input starts after a silence); the
 * breath noisier, the output torn by a fast random tremolo (RASP), the beak's band-pass higher and wider */
static void wg_hawk_run(wg_syrinx_t *S, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t scream,
                        int32_t rasp, int32_t bright, int32_t damp)
{
    uint32_t i, s, bd[2], jd[2], len = 4410u + (uint32_t)scream * 882u, ph;
    int32_t env = wg_follow(&S->env, in, n), pr, loss, bf, g, ow[CTL];
    int32_t e = env > PX_ONE / 12 ? 65536 : env * 12 / 16;
    if (env < PX_ONE / 400)                      /* silent: the next breath is a new scream */
        S->quiet = 1;
    else if (S->quiet) {
        S->quiet = 0;
        S->t = 0;
    }
    ph = S->t < len ? S->t / (len / 256u + 1u) : 256u;   /* 0 .. 256 over the scream */
    ph = ph > 256u ? 256u : ph;
    g = ph < 40u ? (int32_t)ph * 16384 / 40 :   /* +1/4 octave in the first sixth, then down to -1/4 */
        16384 - ((int32_t)ph - 40) * 32768 / 216;
    pr = env * 16 > PX_ONE * 13 / 10 ? PX_ONE * 13 / 10 : env * 16;
    for (s = 0; s < 2u; s++) {
        int32_t oct = 5 * 32768 + g + (int32_t)(((int64_t)13107 * e) >> 16)   /* 2.5 octaves, +0.2 with the breath */
                      + (s ? 2731 : 0);                                       /* (a quarter tone apart) */
        uint64_t f = ((uint64_t)f0 * px_exp2(oct)) >> 16;
        uint32_t per = f < 65536u ? 0xFFFFFFFFu : f >= 0x40000000u ? 4u << 16 : px_period((uint32_t)f) >> 1;
        while (per > (WG_SB - 8u) << 16)         /* (half-rate samples) */
            per >>= 1;
        bd[s] = per > (4u << 16) ? per - (2u << 16) : 2u << 16;
        jd[s] = bd[s] / 3u < (2u << 16) ? 2u << 16 : bd[s] / 3u;
        jd[s] = jd[s] > (WG_SJ - 4u) << 16 ? (WG_SJ - 4u) << 16 : jd[s];
    }
    loss = 29000 + (int32_t)(((int64_t)damp * 3400) >> 16);
    bf = 20000 + bright / 5;                     /* the beak: ~2.3 .. 7 kHz */
    wg_whistles(S, in, ow, n, bd, jd, pr, loss, 11);   /* (the breath's noise twice the SYRINX's) */
    for (i = 0; i < n; i++) {
        int32_t o = ow[i], hi;
        if (!S->rc--) {                          /* the rasp: a new random depth every ~1.5 ms (a ~700 Hz roughness) */
            S->rc = 63u;
            S->amt = (int32_t)(px_rand(&S->rng) >> 17);   /* 0 .. 32767 */
        }
        S->am += (S->amt - S->am) >> 3;
        o = px_m(o, 32767 - (S->am * rasp >> 7), 15);
        S->blo += px_m(bf, S->bbd, 15);          /* the beak (Q ~1.6: wider) */
        hi = o - S->blo - px_m(20000, S->bbd, 15);
        S->bbd = px_clamp(S->bbd + px_m(bf, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
        S->blo = px_clamp(S->blo, -16 * PX_ONE, 16 * PX_ONE);
        S->oy = (o >> 2) - S->ox + px_m(S->oy, 32604, 15);
        S->ox = o >> 2;
        y[i] = S->bbd + S->oy;
    }
    S->t = S->t < 0x7FFFFFFFu ? S->t + n : S->t;
}

/* ------------------------------------------------------- the level (AGC) --- */
/* a slow automatic gain (~0.1 s), as the THROAT's: the output held at 3.6 x the input's level (the dry's, out of the
 * FX: RZ_GAIN), x 1/8 .. 24 (a self-oscillating model's level hangs on its breath and register, not on its input) */
static void wg_level(int32_t *iav, int32_t *oav, int32_t *ag, const int32_t *in, int32_t *y, uint32_t n)
{
    uint32_t si = 0, so = 0, i;
    int32_t gt;
    for (i = 0; i < n; i++) {
        si += (uint32_t)(in[i] < 0 ? -in[i] : in[i]) >> 6;
        so += (uint32_t)(y[i] < 0 ? -y[i] : y[i]) >> 6;
    }
    *iav += ((int32_t)(si / n) - *iav) / 8;
    *oav += ((int32_t)(so / n) - *oav) / 8;
    gt = *oav > 0 ? (int32_t)((uint32_t)*iav * 14746u / (uint32_t)(*oav > 0 ? *oav : 1)) : 4096;   /* Q12 */
    gt = gt < 512 ? 512 : gt > 98304 ? 98304 : gt;
    *ag += (gt - *ag) / 16;
    for (i = 0; i < n; i++)
        y[i] = px_clamp(px_m(y[i], *ag, 12), -64 * PX_ONE, 64 * PX_ONE);
}

/* SCREAM (the VL1's): the voices' own sound on the breath, from a touch (1/8) to driving it (x 2): with the CHUA circuit
 * chaotic, chaos blows the instrument */
static inline int32_t wg_scream(int32_t x, int32_t sh)
{
    return (int32_t)(((int64_t)x * (16 + (int32_t)(sys.scream & 127u) * 2)) >> (7 + sh));
}

/* the half-rate period of the note (Q16 samples), folded up by octaves until it fits in max samples */
static __attribute__((noinline)) uint32_t wg_per(uint32_t f0, uint32_t max)   /* (one division, not three) */
{
    uint32_t per = f0 < 65536u ? 0xFFFFFFFFu : px_period(f0 > 0x40000000u ? 0x40000000u : f0) >> 1;
    while (per > max << 16)
        per >>= 1;
    return per;
}

/* ------------------------------------------------------------------- BRASS --- */
/* Lips buzzing into a bore. The lips (after the measured lip-reed waveforms: a short pulse of air a period, the
 * shorter the harder they are blown, so a loud note is a bright note: brass's own sound) are a pulse at the note,
 * its width 0.5 of a period at a breath and down to ~0.1 at full breath x LIP (STRUCTURE: loose, mellow, a horn
 * .. tight, brassy, a trumpet); the bore a tube a period long that the pulse rings (a feedback comb: its modes the
 * harmonics; DAMPING its loss, BRIGHTNESS its walls' loss of the highs and the bell's radiation); MUTE
 * (POSITION): open bell .. a harmon mute (a narrow band-pass, the nasal "wah"). The voices' loudness is the
 * breath, their sound on it. (A hybrid: STK's lip-reed loop, tried first, drifted in pitch and jumped registers
 * at this rate; the pulse keeps every note in tune and the bore and the bell keep the brass.) */
#define WG_BR 1024u
typedef struct {
    int32_t bore[WG_BR];
    uint32_t w, ph, rng;
    int32_t lp, env, jit;
    int32_t bell, prv, mlo, mbd;                 /* the bell's low-pass; the mute's band-pass (Chamberlin) */
    int32_t ox, oy, iav, oav, ag, yp;
} wg_brass_t;

static void wg_brass_run(wg_brass_t *B, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t lip,
                         int32_t mute, int32_t bright, int32_t damp)
{
    uint32_t i, per = wg_per(f0, WG_BR - 8u), bd = per > (3u << 16) ? per - (1u << 15) : 2u << 16;
    uint32_t inc = f0 > 0x20000000u ? 0x40000000u : f0 * 2u, wq;   /* (the half rate's increment) */
    int32_t env = wg_follow(&B->env, in, n), pr, br, R, g, gb, mf;
    pr = env * 16 > PX_ONE ? PX_ONE : env * 16;  /* the breath, 0..1 */
    br = pr >> 5;                                /* 0..32768 */
    wq = 0x80000000u - (uint32_t)((((int64_t)br * (9830 + lip * 128)) >> 15) << 16);   /* the pulse's width: a turn
                                                  * x 0.5 .. ~0.1 */
    R = 16000 + (int32_t)(((int64_t)damp * 13000) >> 16);   /* the bore's feedback: 0.49 .. 0.89 */
    g = wg_half(9000 + bright / 4);              /* the walls' low-pass in the loop */
    gb = wg_half(7000 + bright / 4);             /* the bell */
    mf = 9000;                                   /* the mute: 2 sin(pi f / FS_half), ~1 kHz */
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i], lips, b, o, t;
        uint32_t ph = B->ph;
        B->ph += inc + (uint32_t)(B->jit >> 4);  /* (a little jitter: the lips are not a clock) */
        if (B->ph < ph)                          /* (a new period: the jitter's next value) */
            B->jit = (int32_t)(px_rand(&B->rng) >> 20) - 2048;
        lips = 0;
        if (ph < wq) {                           /* the puff: a raised cosine over wq */
            uint32_t q = (ph / (wq >> 16)) << 16;   /* (32 bits: ph < wq) */
            lips = (PX_ONE - (px_sin(q + 0x40000000u) >> 10)) >> 1;
        }
        lips = px_m(lips, pr, 20) + wg_scream(x, 0);
        b = wg_read(B->bore, WG_BR - 1u, B->w, bd);
        B->lp += px_m(g, b - B->lp, 15);
        o = lips + px_m(B->lp, R, 15);
        B->bore[B->w & (WG_BR - 1u)] = px_clamp(o, -8 * PX_ONE, 8 * PX_ONE);
        B->w++;
        B->bell += px_m(gb, o - B->bell, 15);    /* out: the bell, radiating the change more the brighter */
        o = B->bell / 2 + (B->bell - B->prv) * (1 + bright / 8192);
        B->prv = B->bell;
        {   /* the mute: a band-pass (Q ~4) mixed in as MUTE turns, the open bell out */
            int32_t hi;
            B->mlo += px_m(mf, B->mbd, 15);
            hi = o - B->mlo - px_m(8192, B->mbd, 15);
            B->mbd = px_clamp(B->mbd + px_m(mf, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
            B->mlo = px_clamp(B->mlo, -16 * PX_ONE, 16 * PX_ONE);
            o = (o * (127 - mute) + B->mbd * mute * 2) / 127;
        }
        t = o - B->ox + px_m(B->oy, 32440, 15);
        B->ox = o;
        B->oy = t;
        y[i] = (B->yp + t) >> 1;
        if (i + 1u < n)
            y[i + 1u] = t;
        B->yp = t;
    }
    wg_level(&B->iav, &B->oav, &B->ag, in, y, n);
}

/* --------------------------------------------------------------------- BOW --- */
/* A bowed string (after STK's Bowed, Smith): the string two delay lines, the bow between them (POSITION: near the
 * bridge, sul ponticello .. over the fingerboard, sul tasto), the nut's and the bridge's reflections (the bridge's
 * low-passed: BRIGHTNESS; their loss: DAMPING); at the bow the friction of the hair on the string, the bow table
 * (x slope + 0.75)^-4 (PRESSURE, STRUCTURE: light and airy .. pressed, scratching): stick, slip. The voices'
 * loudness is the bow's speed, their sound the rosin's grain on it; the bridge's wave through a body: a low
 * resonance an octave and a half over the note (the air and the top plate, between 140 and 560 Hz) and the
 * bridge hill (~2.5 kHz) */
#define WG_BW 512u
typedef struct {
    int32_t neck[WG_BW], brid[WG_BW];
    uint32_t w;
    int32_t sf, env;                             /* the bridge's filter */
    int32_t l1, d1, l2, d2;                      /* the body's two resonances (Chamberlin) */
    int32_t ox, oy, iav, oav, ag, yp;
} wg_bow_t;

static void wg_bow_run(wg_bow_t *W, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t press,
                       int32_t pos, int32_t bright, int32_t damp)
{
    uint32_t i, per = wg_per(f0, WG_BW - 8u), base = per > (4u << 16) ? per - (294u << 9) : 2u << 16, bdl, ndl;   /* (less the filters' lag:
                                                  * 2.3 samples, measured) */
    int32_t env = wg_follow(&W->env, in, n), vb, slope, g, loss, f1, f2;
    bdl = (uint32_t)(((uint64_t)base * (uint32_t)(2000 + pos * 128)) >> 16);   /* the bow: 0.03 .. 0.28 of the string */
    bdl = bdl < (1u << 16) ? 1u << 16 : bdl;
    ndl = base - bdl > (1u << 16) ? base - bdl : 1u << 16;
    vb = env * 3 > PX_ONE / 4 ? PX_ONE / 4 : env * 3;   /* the bow's speed (STK: up to ~0.23) */
    slope = 5 * 65536 - press * 2064;            /* STK: 5 - 4 pressure, Q16 */
    g = wg_half(9000 + bright / 6);              /* the bridge's one-pole (STK: 1 - pole 0.55 at this rate) */
    loss = 29500 + (int32_t)(((int64_t)damp * 3100) >> 16);   /* 0.90 .. 0.995 */
    {   /* the body: f1 the note x 2.8 between 140 and 560 Hz; f2 the bridge hill (2 sin(pi f / FS_half), Q15) */
        uint32_t fb = (uint32_t)(((uint64_t)f0 * 183501u) >> 16);   /* x 2.8 (cycles Q32, the full rate) */
        fb = fb < 13636u * 1000u ? 13636u * 1000u : fb > 54545u * 1000u ? 54545u * 1000u : fb;   /* (140 .. 560 Hz) */
        f1 = (int32_t)(fb >> 15) * 25 / 8;       /* 2 sin(pi f / FS_half) ~ 4 pi f / FS, Q15 */
        f2 = 20000 + bright / 12;                /* ~2.2 .. 2.5 kHz */
    }
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i];
        int32_t bo = wg_read(W->brid, WG_BW - 1u, W->w, bdl), no = wg_read(W->neck, WG_BW - 1u, W->w, ndl);
        int32_t bref, nref, vd, s, t, t4, f, nv, o, hi;
        W->sf += px_m(g, bo - W->sf, 15);
        bref = -px_m(W->sf, loss, 15);
        nref = -no;
        vd = vb + wg_scream(x, 2) - (bref + nref);   /* the bow against the string (the rosin's grain on it) */
        s = px_m(vd, slope, 16);                 /* the bow table */
        t = (s < 0 ? -s : s) + PX_ONE * 3 / 4;
        t = t > 6 * PX_ONE ? 6 * PX_ONE : t;
        t4 = px_m(t, t, 20);
        t4 = px_m(t4, t4, 20);
        f = t4 > PX_ONE ? (int32_t)((1u << 30) / ((uint32_t)t4 >> 10)) : PX_ONE;
        nv = px_m(vd, f, 20);
        W->neck[W->w & (WG_BW - 1u)] = px_clamp(bref + nv, -5 * PX_ONE, 5 * PX_ONE);
        W->brid[W->w & (WG_BW - 1u)] = px_clamp(nref + nv, -5 * PX_ONE, 5 * PX_ONE);
        W->w++;
        W->l1 += px_m(f1, W->d1, 15);            /* the body */
        hi = bo - W->l1 - px_m(6554, W->d1, 15);
        W->d1 = px_clamp(W->d1 + px_m(f1, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
        W->l1 = px_clamp(W->l1, -16 * PX_ONE, 16 * PX_ONE);
        W->l2 += px_m(f2, W->d2, 15);
        hi = bo - W->l2 - px_m(16384, W->d2, 15);
        W->d2 = px_clamp(W->d2 + px_m(f2, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
        W->l2 = px_clamp(W->l2, -16 * PX_ONE, 16 * PX_ONE);
        o = bo / 2 + W->d1 / 4 + W->d2 / 2;
        t = o - W->ox + px_m(W->oy, 32440, 15);
        W->ox = o;
        W->oy = t;
        y[i] = (W->yp + t) >> 1;
        if (i + 1u < n)
            y[i + 1u] = t;
        W->yp = t;
    }
    wg_level(&W->iav, &W->oav, &W->ag, in, y, n);
}

/* -------------------------------------------------------------------- REED --- */
/* A single or double reed on a bore (after STK's Clarinet and Saxofony, Cook / Scavone): the reed a valve, its
 * opening offset + slope x the pressure across it (REED, STRUCTURE: soft .. hard: dark .. bright, buzzing). BORE
 * (POSITION): below 16 a cylinder half a period long (the clarinet: closed at the reed, odd harmonics); above, a
 * cone a period long (sax, oboe, bassoon: all the harmonics), the reed blowing into it at 0.06 .. 0.45 of its
 * length (the conical bore's shape: low, a sax; high, the narrow double reeds). The voices' loudness is the
 * breath, their sound on it; BRIGHTNESS the bell; DAMPING the bore's loss */
#define WG_RD 512u
typedef struct {
    int32_t d0[WG_RD], d1[WG_RD];
    uint32_t w, rng;
    int32_t lp, env, bell, prv;
    int32_t ox, oy, iav, oav, ag, yp;
} wg_reed_t;

static void wg_reed_run(wg_reed_t *D, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t reed,
                        int32_t bore, int32_t bright, int32_t damp)
{
    uint32_t i, cyl = bore < 16, per = wg_per(f0, WG_RD - 8u), base, l0, l1;
    int32_t env = wg_follow(&D->env, in, n), pr, R, g, gb, slope;
    base = cyl ? per / 2u : per;
    base = base > (4u << 16) ? base - (cyl ? 1u << 16 : 269u << 9) : 2u << 16;   /* (less the filters' lag:
                                                  * measured) */
    l1 = cyl ? 0u : (uint32_t)(((uint64_t)base * (uint32_t)(4000 + (bore - 16) * 233)) >> 16);   /* 0.06 .. 0.45 */
    l1 = !cyl && l1 < (1u << 16) ? 1u << 16 : l1;
    l0 = base - l1;
    pr = env * 16 > PX_ONE ? PX_ONE : env * 16;
    pr = pr * (cyl ? 12 : 10) / 16;             /* (STK blows them at 0.55 .. 0.85) */
    slope = (cyl ? -1 : 1) * (int32_t)(PX_ONE * 22 / 100 + (uint32_t)reed * (PX_ONE / 700));   /* 0.22 .. 0.4 */
    R = 30000 + (int32_t)(((int64_t)damp * 2600) >> 16);   /* 0.92 .. 0.995 (STK: 0.95) */
    g = wg_half(22000 + bright / 10);             /* the reflection's low-pass */
    gb = wg_half(9000 + bright / 4);             /* the bell */
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i];
        int32_t noise = px_m((int32_t)(px_rand(&D->rng) >> 11) - (1 << 20), pr, 24);   /* (the breath: 1/16) */
        int32_t p = pr + noise + wg_scream(x, 0), b = wg_read(D->d0, WG_RD - 1u, D->w, l0), o, pd, rt, t;
        D->lp += px_m(g, b - D->lp, 15);
        if (cyl) {                               /* STK Clarinet */
            pd = -px_m(D->lp, R, 15) - p;
            rt = px_clamp(PX_ONE * 7 / 10 + px_m(slope, pd, 20), -PX_ONE, PX_ONE);
            D->d0[D->w & (WG_RD - 1u)] = px_clamp(p + px_m(pd, rt, 20), -5 * PX_ONE, 5 * PX_ONE);
            o = b;
        } else {                                 /* STK Saxofony */
            int32_t tmp = -px_m(D->lp, R, 15);
            o = tmp - wg_read(D->d1, WG_RD - 1u, D->w, l1);
            pd = p - o;
            rt = px_clamp(PX_ONE * 7 / 10 + px_m(slope, pd, 20), -PX_ONE, PX_ONE);
            D->d1[D->w & (WG_RD - 1u)] = px_clamp(tmp, -5 * PX_ONE, 5 * PX_ONE);
            D->d0[D->w & (WG_RD - 1u)] = px_clamp(p - px_m(pd, rt, 20) - tmp, -5 * PX_ONE, 5 * PX_ONE);
        }
        D->w++;
        t = o - D->prv;                          /* the bell radiates the pressure's change (a high-pass: the
                                                  * reed's square wave's harmonics heard) */
        D->prv = o;
        o = t * 4;
        D->bell += px_m(gb, o - D->bell, 15);
        o = D->bell;
        t = o - D->ox + px_m(D->oy, 32440, 15);
        D->ox = o;
        D->oy = t;
        y[i] = (D->yp + t) >> 1;
        if (i + 1u < n)
            y[i + 1u] = t;
        D->yp = t;
    }
    wg_level(&D->iav, &D->oav, &D->ag, in, y, n);
}
