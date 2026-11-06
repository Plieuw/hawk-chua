/* SPDX-License-Identifier: GPL-3.0-only */
/* The FX waveguides: more models for resonator.c, beside Rings' (the same page, the same polyphony: one per
 * note, fed by the voices that play it). Digital waveguides: travelling waves in delay lines, scattered where the
 * tube changes, reflected at its ends. This firmware's own fixed-point code (after the textbook models: Kelly and
 * Lochbaum's vocal tract, Cook's slide flute / STK Flute's jet and bore), signals Q20 like Rings' models.
 *
 *   VOICE    a human vocal tract (the choir's): a tube of 8 .. 12 sections (Kelly-Lochbaum; 2 samples each), the
 *            voices its glottal source. SIZE sets the tube's length, its formants: CHILD (12 cm), SOPRANO, ALTO,
 *            TENOR, BASS (18.5 cm); VOWEL (POSITION) the tongue: U, O, A, E, I, morphing; BRIGHTNESS the mouth's
 *            radiation; DAMPING how long the tube rings (the glottis' and the lips' reflections)
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
 * syrinx blow only while it plays, the voice's mouth moves with it. */

/* ------------------------------------------------------------------- VOICE --- */
#define WG_NSEC 12                               /* sections at most (half the junctions of 24: the CPU) */
#define WG_KMAX 2                                /* samples per section (a section: k samples each way) */
enum { WV_CHILD, WV_SOPRANO, WV_ALTO, WV_TENOR, WV_BASS, WV_COUNT };
static const char *const WG_VSIZE[WV_COUNT] = {"CHILD", "SOPRANO", "ALTO", "TENOR", "BASS"};
static const struct {
    uint8_t n, k;                                /* sections, samples each: the tube n k / 44100 s one way (x 350 m/s) */
    uint8_t wah;                                 /* how much the mouth follows the loudness, /128 */
    uint8_t nasal;                               /* the glottis' reflection's floor, /128 (a "closed", nasal tone) */
} WG_VS[WV_COUNT] = {
    {8, 2, 40, 60},                          /* a child: 12 cm */
    {9, 2, 40, 60},                          /* soprano: 14 cm */
    {10, 2, 40, 60},                         /* alto: 15.5 cm */
    {11, 2, 40, 60},                         /* tenor: 17 cm */
    {12, 2, 40, 60},                         /* bass: 18.5 cm */
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
    uint32_t an = animal % WV_COUNT, ns = WG_VS[an].n, k = WG_VS[an].k, i, j;
    int32_t env = wg_follow(&T->env, in, n), A[WG_NSEC + 1], open, rg, rl, lp, ro;
    {   /* the areas: the vowel (0..127 across U O A E I), resampled to the sections; the mouth (the last quarter)
         * opened by the loudness as much as the animal does it */
        int32_t pos = vowel * 4 * 256 / 127, v = pos >> 8, fr = pos & 255;
        int32_t e = env > PX_ONE / 2 ? 256 : env / (PX_ONE / 512);   /* 0..256: loud at half scale */
        v = v > WG_NV - 2 ? WG_NV - 2 : v;
        if (pos >= (WG_NV - 1) * 256)
            fr = 256;
        open = (128 - WG_VS[an].wah) * 2 + WG_VS[an].wah * (40 + e * 3 / 2) / 128;   /* /256 */
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
    rg = 22000 + WG_VS[an].nasal * 40 + damp / 11;   /* the glottis: ~0.67 .. 0.99 (Q15) */
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
    gt = *oav > 0 ? (int32_t)((uint32_t)*iav * 14746u / (uint32_t)(*oav > 0 ? *oav : 1)) : *ag;   /* Q12 (no
                                                  * output yet: as it is, no jump at the onset) */
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
        o = px_clamp(B->bell / 2 + (B->bell - B->prv) * (1 + bright / 8192), -32 * PX_ONE, 32 * PX_ONE);
        B->prv = B->bell;
        {   /* the mute: a band-pass (Q ~4) mixed in as MUTE turns, the open bell out */
            int32_t hi;
            B->mlo += px_m(mf, B->mbd, 15);
            hi = o - B->mlo - px_m(8192, B->mbd, 15);
            B->mbd = px_clamp(B->mbd + px_m(mf, hi, 15), -16 * PX_ONE, 16 * PX_ONE);
            B->mlo = px_clamp(B->mlo, -16 * PX_ONE, 16 * PX_ONE);
            o = px_clamp((int32_t)(((int64_t)o * (127 - mute) + (int64_t)B->mbd * mute * 2) / 127), -32 * PX_ONE,
                         32 * PX_ONE);
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

/* -------------------------------------------------------------------- DRUM --- */
/* A struck membrane (toms, taiko, timpani): eight of its modes, each a two-pole resonator rung by the voices (a short
 * gated note is the stick). STRUCTURE morphs the modes from a free membrane (the circular membrane's Bessel ratios
 * 1, 1.59, 2.14, 2.30, ...: a tom, a taiko) to a kettle drum's (the air under the head pulls the (n,1) modes near
 * 1 : 1.5 : 2 : 2.5: a pitched timpani); POSITION where it is struck: the middle rings the round modes ((0,n): a
 * dull boom), the edge the others (a ringing tone); BRIGHTNESS how long the higher modes ring against the lowest;
 * DAMPING the decay, 0.05 .. 4 s. A hard strike tightens the head: the pitch starts up to a quarter tone .. a
 * fourth high and falls back (the drum's "doom"), as hard as the input is loud. The note tunes the drum. */
#define WG_DM 8
static const uint16_t WG_MEMB[WG_DM] = {4096, 6529, 8749, 9405, 10867, 11952, 12927, 14746};   /* Q12 */
static const uint16_t WG_KETTLE[WG_DM] = {3482, 4096, 6144, 8110, 9994, 11878, 7004, 13640};    /* (0,1) low and short */
static const uint8_t WG_ROUND[WG_DM] = {1, 0, 0, 1, 0, 0, 0, 1};   /* the round modes (0,n): the middle rings them */
typedef struct {
    int32_t y1[WG_DM], y2[WG_DM];
    int32_t env, yp;
} wg_drum_t;

static void wg_drum_run(wg_drum_t *D, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t kettle,
                        int32_t pos, int32_t bright, int32_t damp)
{
    int32_t a1[WG_DM], a2[WG_DM], b[WG_DM], env = wg_follow(&D->env, in, n);
    uint32_t i, m, nm = 0;
    uint32_t ts = (uint32_t)(((uint64_t)1102u * px_exp2(damp * 63 / 10)) >> 16);   /* T60, half-rate samples:
                                                  * 0.05 .. ~4 s */
    int32_t up = env > PX_ONE / 4 ? 65536 : env / 4;   /* the strike's tension: 0..1 (Q16) */
    uint32_t fm = (uint32_t)(((uint64_t)f0 * px_exp2((int32_t)(((int64_t)up * (2730 + kettle / 3)) >> 16))) >> 16);
    for (m = 0; m < WG_DM; m++) {
        int32_t ratio = WG_MEMB[m] + (int32_t)(((int64_t)(WG_KETTLE[m] - WG_MEMB[m]) * kettle) >> 16);   /* Q12 */
        uint64_t fr = ((uint64_t)fm * (uint32_t)ratio) >> 12;   /* cycles Q32 at the full rate */
        uint32_t ph, tm, e;
        int32_t r, cs, sn, amp, d;
        if (fr * 2u >= 0x70000000u)              /* (above ~0.44 of the half rate's Nyquist: left out) */
            break;
        ph = (uint32_t)fr * 2u;                  /* (the half rate's turn) */
        tm = ts / (1u + m * (uint32_t)(131072 - bright) / 65536u);   /* the higher modes shorter, less so bright */
        tm = tm < 16u ? 16u : tm;
        e = 653126u / tm;                        /* log2(1000) / T60, Q16 */
        d = e < 2000u ? (int32_t)(e * 2839u) : (int32_t)((1u << 28) - (px_exp2(-(int32_t)e) << 12));   /* 1 - r, Q28 */
        r = (1 << 28) - d;
        cs = px_sin(ph + 0x40000000u) >> 2;      /* Q28 */
        sn = px_sin(ph) >> 2;
        amp = WG_ROUND[m] ? 65536 - pos * 300 : 26000 + pos * 300;   /* the strike's place (Q16) */
        a1[m] = (int32_t)(((int64_t)r * cs) >> 27);   /* 2 r cos w, Q28 */
        a2[m] = (int32_t)(((int64_t)r * r) >> 28);
        b[m] = (int32_t)(((int64_t)(sn >> 3) * amp) >> 16);   /* sin w: a strike rings as loud at any decay */
        nm = m + 1u;
    }
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i], o = 0;
        for (m = 0; m < nm; m++) {
            int32_t v = (int32_t)(((int64_t)a1[m] * D->y1[m] - (int64_t)a2[m] * D->y2[m] + (int64_t)b[m] * x) >> 28);
            v = px_clamp(v, -32 * PX_ONE, 32 * PX_ONE);
            D->y2[m] = D->y1[m];
            D->y1[m] = v;
            o += v >> 2;
        }
        for (; m < WG_DM; m++)
            D->y1[m] = D->y2[m] = 0;
        y[i] = (D->yp + o) >> 1;
        if (i + 1u < n)
            y[i + 1u] = o;
        D->yp = o;
    }
}

/* ------------------------------------------------------------------- PIANO --- */
/* A piano's note (after the digital waveguide pianos, Smith / Van Duyne, Bank / Välimäki, and commuted synthesis):
 * two strings a note (a unison, tuned a little apart: they beat, and with their losses a little apart the sound
 * falls in two stages, the prompt sound and the aftersound), each a delay loop with a loss filter (the highs die
 * first) and a chain of up to eight all-passes for the steel's stiffness, fitted per note to a piano's inharmonicity
 * B (tools/piano_dispersion.py: ~1.3e-4 at the break .. 1.5e-2 at the top; the partials stretched sharp, the more the
 * higher: what makes a piano a piano and not a harp), their delay at the fundamental taken off the loop so each note
 * is in tune. Struck once, by the hammer (below): its force, through its place on the string (a comb: the partials
 * there, 1/16 .. 1/5 of the length, left out), is all that drives the strings; the voices' sound only colours the
 * strike (the felt's first ~20 ms), so a held key rings out as a piano's does, never bowed. The strike also knocks
 * the case: a short burst through the soundboard's two broad resonances (its warmth, the thump of a hard note).
 * The damper falls when the key is let go (no voice holds the note any more), unless the sustain pedal (CC 64) is
 * down. UNISON (STRUCTURE): in tune .. honky-tonk (0 .. 12 cents apart); HAMMER at (POSITION); BRIGHTNESS the felt's
 * hardness; DAMPING how long it sings (x 1/4 .. 4 of a piano's own: ~20 s in the bass, ~1 s at the top) */
/* PIANO's stiffness per note (tools/piano_dispersion.py): its first-order all-passes, their coefficient (Q15)
 * and their delay at the fundamental (half-rate samples, Q8), fitted to a piano's inharmonicity B */
static const uint8_t WG_PN_M[128] = {8, 8, 8, 8, 8, 8, 7, 8, 8, 7, 8, 8, 8, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 8, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 8, 8, 7, 8, 6, 8, 6, 5, 6, 5, 4, 4, 4, 4, 1, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const int16_t WG_PN_A[128] = {-31130, -31130, -31130, -31130, -31130, -31130, -31130, -30966, -30802, -30802, -30638, -30474, -30310, -30310, -30147, -29983, -29655, -29491, -29327, -29164, -29000, -28836, -28508, -28344, -27689, -27361, -26870, -26706, -26051, -25395, -24904, -24412, -23921, -23429, -22938, -22446, -21791, -21299, -20644, -19988, -19333, -19005, -18514, -18186, -17695, -17367, -16876, -16384, -16056, -15565, -15073, -14746, -14254, -13763, -13435, -12943, -12616, -12124, -11633, -11305, -10813, -10486, -9994, -9667, -9339, -8847, -8520, -8192, -7700, -7373, -7045, -6717, -6390, -6226, -5898, -5571, -5243, -4915, -4588, -4260, -4096, -3932, -3604, -3441, -3113, -2949, -2621, -2458, -2621, -2130, -1966, -1966, -1638, -1966, -1311, -1638, -1802, -1475, -1475, -1638, -1638, -1475, -1475, -4096, -2130, -1966, -3604, -3441, -3277, -2949, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint16_t WG_PN_T[128] = {65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 57864, 60890, 56397, 52503, 45934, 42952, 40320, 41021, 38866, 36918, 35146, 33528, 32046, 29425, 24727, 24353, 22753, 20687, 17564, 17917, 16143, 15007, 14003, 13112, 12316, 11597, 10947, 10171, 9649, 9016, 8448, 7936, 7698, 7363, 7150, 6851, 6661, 6392, 6139, 5978, 5750, 5532, 5394, 5197, 5010, 4890, 4718, 4605, 4449, 4298, 4201, 4060, 3971, 3840, 3756, 3674, 3556, 3482, 3407, 3300, 3231, 3164, 3098, 3034, 3000, 2939, 2880, 2821, 2762, 2708, 2652, 2624, 2598, 2547, 2519, 2470, 2445, 2396, 2371, 2094, 2324, 2301, 2012, 2255, 1723, 2209, 1687, 1418, 1669, 1390, 1121, 1121, 1108, 1108, 317, 571, 566, 305, 302, 297, 292, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#define WG_PL 740u                               /* each string's line (half-rate samples: 30 Hz and up; lower: up) */
#define WG_PH 160u                               /* the hammer's comb (a power of two is not needed: modulo) */
#define WG_PHU 128u                              /* the hammer: the near end's echo (an eighth of the period)'s line */
#define WG_PAP 8u                                /* the stiffness all-passes a string, at most */
typedef struct {
    int32_t s[2][WG_PL];
    int32_t hb[WG_PH];
    uint32_t w, hw;
    int32_t lp[2], apx[2][WG_PAP], apy[2][WG_PAP];   /* the loss filters; the stiffness all-passes */
    int32_t fx[2], fy[2];                        /* the tuning all-passes (the fraction of a sample: lossless) */
    int32_t felt, env, b1l, b1d, b2l, b2d, ox, oy, yp, orad;
    int32_t win, knock, kn;                      /* the voices' window on the strike (Q15); the knock, its noise */
    uint32_t rng;
    int32_t hy, hv, hu, hx;                      /* the hammer: its place (Q24 m), speed (Q16 m/s); the string at
                                                  * it, the waves sent (Q24 m); hx: its last force (Q12 N) */
    int32_t hu_d[WG_PHU];                        /* the waves sent, a sample each: back from the near end */
    uint32_t hw_u;
    uint8_t hon;                                 /* the hammer on its way or against the string */
} wg_piano_t;

static inline int32_t wg_readm(const int32_t *l, uint32_t len, uint32_t w, uint32_t d)   /* d back, Q16; any length */
{
    int32_t t = (int32_t)w - 1 - (int32_t)(d >> 16), a, b;
    while (t < 0)
        t += (int32_t)len;
    a = l[t];
    b = l[t ? t - 1 : (int32_t)len - 1];
    return a + px_m(b - a, (int32_t)(d & 0xFFFFu), 16);
}
/* the hammer (after Chaigne & Askenfelt, Stulov): a felt-covered mass thrown at the string at the key's speed (0.4 ..
 * 5 m/s: the velocity), the felt a stiffening spring, F = K c^2.5 (c: how far it is pressed in). The string gives
 * way as a line would (its impedance: F / 2Z its speed there), and an eighth of a period later the wave comes back
 * from the near end (the agraffe) and throws the hammer off. So the force is a pulse of 1 .. 14 ms, shorter and
 * sharper the harder the key is struck (the felt stiffens as it is pressed: a loud note is brighter, by itself) and
 * the higher the note (lighter hammers, harder felt: 11 g, K 3e8 in the bass .. 5 g, 8e9 at the top);
 * BRIGHTNESS makes the felt harder or softer. Four steps a sample (88 kHz): the contact's stiffness needs them */
static void wg_hammer_strike(wg_piano_t *P, uint32_t vel)
{
    P->win = 32767;                              /* (the voices: into this strike's felt) */
    P->knock = (int32_t)(vel * vel) * 8;         /* (the case's knock: as hard as the strike, Q15 at 127 ~ 4) */
    if (!P->rng)
        P->rng = 0x2545F491u;
    P->hy = 0;
    P->hu = 0;
    P->hv = 26214 + (int32_t)(vel * vel * 1196u >> 6);   /* 0.4 .. 5 m/s, Q16 (velocity squared) */
    P->hon = 1;
    memset(P->hu_d, 0, sizeof P->hu_d);
}
static void wg_piano_run(wg_piano_t *P, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, uint32_t note,
                         int32_t unison, int32_t pos, int32_t bright, int32_t damp, int held, uint32_t strike)
{
    int32_t hkn = 0, ham = 0;
    uint32_t hdl = 1;
    uint32_t i, k, j, per = wg_per(f0, WG_PL - 8u), d[2], hd, nap = WG_PN_M[note & 127u];
    int32_t g[2], la, c = WG_PN_A[note & 127u], fa, cents;
    uint32_t t60;
    (void)wg_follow(&P->env, in, n);
    {   /* the loss filter (a one-pole pass, Q15): dull in the bass, open at the top (a pass at the fundamental loses
         * little: the top still sings); the loop less the all-passes' and the filter's delay at the fundamental */
        int32_t tr = note < 21u ? 0 : note > 108u ? 87 : (int32_t)note - 21;   /* 0 .. 87 */
        la = 26000 + tr * 70 + bright / 24;     /* (C4: ~0.9: its 4th partial ~5 s, its 8th ~1 s, as a piano's) */
        la = la > 32600 ? 32600 : la;
        {
            uint32_t ap = (uint32_t)WG_PN_T[note & 127u] << 8;   /* (Q8 -> Q16) */
            uint32_t lpd = (uint32_t)((32768 - la) * 65536u / (uint32_t)la);   /* (1 - a) / a, Q16 */
            uint32_t comp = ap + lpd;
            cents = unison * unison / 1350;      /* 0 .. 12 cents apart (64: 3) */
            for (k = 0; k < 2u; k++) {
                uint32_t pk = (uint32_t)(((uint64_t)per * px_exp2((k ? cents : -cents) * 65536 / 2400)) >> 16);
                d[k] = pk > comp + (2u << 16) ? pk - comp : 2u << 16;
            }
        }
        /* T60: ~20 s at A0 halving every 20 semitones, x 2^((DAMPING - 64) / 32); the damper: 0.12 s */
        t60 = (uint32_t)(((uint64_t)441000u * px_exp2(-tr * 3277 + (damp / 516 - 64) * 2048)) >> 16);   /* half-rate
                                                  * samples */
        if (!held && !sys.pedal)
            t60 = 2646u;
        t60 = t60 < 600u ? 600u : t60;
        for (k = 0; k < 2u; k++) {               /* the loop gain a period (the second string longer: two stages) */
            uint32_t tk = k ? t60 + t60 / 2u : t60 - t60 / 4u;
            uint32_t e = (uint32_t)(((uint64_t)d[k] * 10u) / tk);   /* periods: log2(1000) x d / T60, Q16 */
            g[k] = (int32_t)(px_exp2(-(int32_t)e) >> 1);   /* Q15 */
        }
    }
    hd = (uint32_t)(((uint64_t)per * (uint32_t)(4096 + pos * 70)) >> 16);   /* the hammer at 1/16 .. 1/5 */
    hd = hd >> 16 < WG_PH - 2u ? hd >> 16 : WG_PH - 2u;
    hd = hd < 1u ? 1u : hd;
    fa = wg_half(1500 + bright / 6 + 4096);      /* the felt: the voices' sound under the hammer */
    {   /* the hammer for this note: K / 2^20 (300 .. 7500, BRIGHTNESS x 1/3 .. 3), h / m x 16 Q16 (h a quarter
         * sample: 1 / 88200 s), the near end's echo an eighth of the period (half-rate samples) */
        int32_t tr = note < 21u ? 0 : note > 108u ? 87 : (int32_t)note - 21;
        hkn = (int32_t)(((uint64_t)1200u * px_exp2(tr * 3500 + (bright / 516 - 64) * 1638)) >> 16);
        hkn = hkn < 16 ? 16 : hkn;
        ham = (int32_t)(((uint64_t)1081u * px_exp2(tr * 853)) >> 16);   /* 11 g .. 5 g */
        hdl = per >> 19;                         /* (per: Q16 half-rate samples; / 8) */
        hdl = hdl < 1u ? 1u : hdl > WG_PHU - 1u ? WG_PHU - 1u : hdl;
        if (strike)
            wg_hammer_strike(P, strike);
    }
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i], ex, e0, o = 0, t, kn;
        P->felt += px_m(fa, x - P->felt, 15);
        {   /* the hammer against the string (its force, the strike) */
            int32_t fsum = 0;
            if (P->hon) {
                int32_t back = P->hu_d[(P->hw_u + WG_PHU - hdl) % WG_PHU], s4;
                for (s4 = 0; s4 < 4; s4++) {
                    int32_t cc0 = P->hy - (P->hu - back), f = 0;
                    if (cc0 > 0) {
                        uint32_t cc = (uint32_t)(cc0 > 50331 ? 50331 : cc0);   /* (3 mm at most) */
                        uint64_t c25 = (uint64_t)cc * cc * px_isqrt64(cc);
                        f = (int32_t)(((uint64_t)hkn * c25) >> 28);   /* Q12 N */
                        f = f > 2000000 ? 2000000 : f;
                    }
                    P->hv -= (int32_t)(((int64_t)f * ham) >> 16);
                    P->hy += (int32_t)(((int64_t)P->hv * 190) >> 16);
                    P->hu += (int32_t)(((int64_t)f * 725) >> 16);
                    fsum += f;
                }
                if (P->hv < 0 && P->hy < P->hu - back - 2000)   /* (thrown off and away: done) */
                    P->hon = 0;
            }
            P->hu_d[P->hw_u] = P->hu;
            P->hw_u = P->hw_u + 1u >= WG_PHU ? 0u : P->hw_u + 1u;
            P->hx = fsum >> 2;
        }
        /* what strikes the string: the hammer's force and, in its window, the voices' sound; through the comb */
        e0 = P->hx * 32 + px_m(P->felt >> 1, P->win, 15);
        P->win -= P->win >> 9;                   /* (~23 ms) */
        P->hb[P->hw] = e0;
        ex = e0 - P->hb[(P->hw + WG_PH - hd) % WG_PH];
        P->hw = P->hw + 1u >= WG_PH ? 0u : P->hw + 1u;
        for (k = 0; k < 2u; k++) {
            int32_t v;
            {   /* the delay: whole samples, then a first-order all-pass for the fraction (0.5 .. 1.5: Thiran),
                 * which loses nothing where linear interpolation would damp the top notes' short loops */
                uint32_t m = (d[k] - 0x8000u) >> 16, fr = d[k] - (m << 16);   /* fr: 0.5 .. 1.5, Q16 */
                int32_t eta = (int32_t)(((65536 - (int32_t)fr) * 32768) / (65536 + (int32_t)fr)), xr;
                int32_t t2 = (int32_t)P->w - (int32_t)m;
                while (t2 < 0)
                    t2 += (int32_t)WG_PL;
                xr = P->s[k][t2];
                v = px_m(eta, xr - P->fy[k], 15) + P->fx[k];
                P->fx[k] = xr;
                P->fy[k] = v;
            }
            P->lp[k] += px_m(la, v - P->lp[k], 15);
            v = px_m(P->lp[k], g[k], 15);
            for (j = 0; j < nap; j++) {          /* the stiffness: the all-passes (a x + x1 - a y1) */
                int32_t a = px_m(c, v - P->apy[k][j], 15) + P->apx[k][j];
                P->apx[k][j] = v;
                P->apy[k][j] = a;
                v = a;
            }
            P->s[k][P->w % WG_PL] = px_clamp(v + (ex >> 1), -8 * PX_ONE, 8 * PX_ONE);
            o += v;
        }
        P->w = P->w + 1u >= WG_PL ? 0u : P->w + 1u;
        kn = 0;
        if (P->knock > 16) {                     /* the knock: noise, as loud as the strike, dying in ~10 ms */
            P->kn += ((int32_t)(px_rand(&P->rng) >> 12) - (1 << 19) - P->kn) >> 2;   /* (a little dull) */
            kn = px_m(P->kn, P->knock, 15);
            P->knock -= P->knock >> 8;
        }
        {   /* the soundboard: ~140 Hz and ~900 Hz, broad; rung by the strings and by the knock */
            int32_t hi, sb = o + kn;
            P->b1l += px_m(1300, P->b1d, 15);
            hi = sb - P->b1l - px_m(26000, P->b1d, 15);
            P->b1d = px_clamp(P->b1d + px_m(1300, hi, 15), -32 * PX_ONE, 32 * PX_ONE);
            P->b1l = px_clamp(P->b1l, -32 * PX_ONE, 32 * PX_ONE);
            P->b2l += px_m(8400, P->b2d, 15);
            hi = sb - P->b2l - px_m(26000, P->b2d, 15);
            P->b2d = px_clamp(P->b2d + px_m(8400, hi, 15), -32 * PX_ONE, 32 * PX_ONE);
            P->b2l = px_clamp(P->b2l, -32 * PX_ONE, 32 * PX_ONE);
            o = o + P->b1d / 3 + P->b2d / 4 + kn / 8;
        }
        {   /* the radiation: a soundboard sends out its motion's change, +6 dB an octave over the bass (a tilt) */
            int32_t r = o + (o - P->orad) * 4;
            P->orad = o;
            o = r;
        }
        t = o - P->ox + px_m(P->oy, 32440, 15);
        P->ox = o;
        P->oy = t;
        y[i] = (P->yp + t) >> 1;
        if (i + 1u < n)
            y[i + 1u] = t;
        P->yp = t;
    }
}

/* --------------------------------------------------------------- ACCORDION --- */
/* Free reeds (the accordion's, the bandoneon's, the harmonium's): each reed a tongue swinging through its slot at
 * its own pitch, the air let through only while it is out of the slot (a valve: the flow a pulse each swing, the
 * harder the bellows push the wider the reed swings and the shorter the pulse: louder and brighter, a reed's buzz),
 * heard as the flow and a little of its change (radiated) through the reed chamber (a resonance, BRIGHTNESS: 500 Hz .. 3 kHz, the
 * cassotto's dark to the open reed block's bright). The registers (POSITION): one reed (8'), two a little apart (8'
 * 8'), three (the musette: one in tune, one flat, one sharp: its beating tremolo), the bassoon (16'), the master (16'
 * 8' 8' 4'); STRUCTURE how far apart the musette's reeds are (0 .. 30 cents: dry .. wet); DAMPING the chamber's
 * resonance (wide .. sharp). The bellows are the voices' loudness (and aftertouch) */
#define WG_AR 4
static const int8_t WG_AREG[5][WG_AR] = {   /* per register its reeds: the octave (-1 16', 0 8', 1 4'), the detune */
    {0, 99, 99, 99}, {0, 4, 99, 99}, {0, -2, 2, 99}, {-1, 99, 99, 99}, {-1, 0, 2, 1}};   /* (99: none; 4: + half) */
typedef struct {
    uint32_t ph[WG_AR];
    int32_t env, prv, c1, c2, ox, oy, iav, oav, ag;
} wg_accord_t;
static void wg_accord_run(wg_accord_t *A, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t musette,
                          int32_t reg, int32_t bright, int32_t damp)
{
    uint32_t i, k, inc[WG_AR], nr = 0, rg = (uint32_t)reg * 5u / 128u;
    int32_t env = wg_follow(&A->env, in, n), pr, w, cf, cq, cb;
    for (k = 0; k < WG_AR; k++) {                /* the reeds of the register, each its pitch */
        int32_t o = WG_AREG[rg][k], c;
        if (o == 99)
            continue;
        c = o == 4 ? 1 : o == 2 ? 1 : o == -2 ? -1 : 0;   /* (the detuned ones: +-) */
        {
            int32_t cents = c * (musette * musette / 540);   /* 0 .. 30 cents */
            uint32_t f = (uint32_t)(((uint64_t)f0 * px_exp2(cents * 65536 / 1200)) >> 16);
            if (o == -1 || (rg == 4u && k == 0u))
                f >>= 1;
            else if (o == 1 && rg == 4u)
                f <<= 1;
            inc[nr++] = f > 0x60000000u ? 0x60000000u : f;
        }
    }
    pr = env * 16 > PX_ONE ? PX_ONE : env * 16;  /* the bellows, 0 .. 1 */
    w = 26000 - (pr >> 6);                       /* the slot: the pulse's width (Q15), shorter as the reed swings wider */
    {   /* the chamber: a two-pole band-pass (the state-variable kind), f 500 Hz .. 3 kHz, Q from DAMPING */
        int32_t oct = (int32_t)(((int64_t)bright * 169000) >> 16);   /* 0 .. 2.6 octaves */
        uint32_t fc = (uint32_t)(((uint64_t)500u * px_exp2(oct)) >> 16);
        cf = (int32_t)(fc * 4669u / 1000u);      /* 2 sin(pi f / FS) ~ 2 pi f / FS, Q15 (x 32768 / 44100 x 2 pi) */
        cq = 32768 - (int32_t)(((int64_t)damp * 29000) >> 16);   /* 1 / Q: wide .. sharp */
        cb = 9000 + damp / 8;
    }
    for (i = 0; i < n; i++) {
        int32_t u = 0, o, d, hp;
        for (k = 0; k < nr; k++) {               /* each reed: out of the slot, the air through */
            int32_t s;
            A->ph[k] += inc[k];
            s = px_sin(A->ph[k]) >> 15;          /* its swing, Q15 */
            s = s - w;
            u += s > 0 ? s : 0;
        }
        u = (int32_t)(((int64_t)u * pr) >> 20) * 4;   /* the flow, on the bellows */
        d = u - A->prv;                          /* radiated: the flow's change */
        A->prv = u;
        hp = u - A->c1 - px_m(A->c2, cq, 15);   /* the chamber (low c1, band c2), rung by the flow */
        A->c2 += px_m(cf, hp, 15);
        A->c1 += px_m(cf, A->c2, 15);
        o = u + d * 2 + px_m(A->c2, cb, 15);     /* the flow, a little of its change (the buzz), the chamber */
        d = o - A->ox + px_m(A->oy, 32440, 15);  /* (DC out) */
        A->ox = o;
        A->oy = d;
        y[i] = px_clamp(d, -32 * PX_ONE, 32 * PX_ONE);   /* (the reeds are loud: the gain below in range) */
    }
    wg_level(&A->iav, &A->oav, &A->ag, in, y, n);
}

/* ----------------------------------------------------------------- BAGPIPE --- */
/* The bagpipe: the chanter a double reed on a narrow cone (the REED waveguide, its reed hard: loud, bright, buzzing;
 * STRUCTURE softens it), and the drones (resonator.c's fx_resonator: one for the whole instrument, not a note): a
 * bass and two tenors, cylinders with single reeds (odd harmonics, a square's), two octaves and an octave under the
 * key POSITION sets (A .. G#: 12 places), the tenors a little apart (they beat); they sound while the bag is full
 * (a note held, then ~0.7 s as it empties); DAMPING their level (0: none) */
static uint32_t bag_ph[3];
static int32_t bag_fill, bag_lp[3];
static inline int32_t bag_blep(uint32_t ph, uint32_t inc)   /* a square's step smoothed (polyBLEP), Q15 */
{
    int32_t s = ph < 0x80000000u ? 32767 : -32767;
    uint32_t t;
    if (ph < inc) {                              /* (the rising edge) */
        t = ph / (inc >> 15 ? inc >> 15 : 1u);  /* (32-bit: the position in the step, Q15) */
        s -= (int32_t)((32768 - (int32_t)t) * (32768 - (int32_t)t) >> 15);
    } else if (ph - 0x80000000u < inc) {         /* (the falling edge) */
        t = (ph - 0x80000000u) / (inc >> 15 ? inc >> 15 : 1u);
        s += (int32_t)((32768 - (int32_t)t) * (32768 - (int32_t)t) >> 15);
    }
    return s;
}
/* the drones this block into y (added, the model's units), the bag filled while held */
static void bag_drones(int32_t *y, uint32_t n, int held, int32_t key, int32_t level)
{
    static const int16_t OFS[3] = {0, 4, -3};   /* the tenors' beating (cents x 4 ... a few cents) */
    uint32_t i, k, inc[3], note = 45u + (uint32_t)key * 12u / 128u;   /* A2 .. G#3 */
    int32_t tgt = held ? 32767 : 0;
    if (!level && !bag_fill)
        return;
    for (k = 0; k < 3u; k++) {
        uint32_t f = note_inc(k ? note : note - 12u);   /* the bass an octave under the tenors */
        inc[k] = (uint32_t)(((uint64_t)f * px_exp2(OFS[k] * 65536 / 4800)) >> 16);
    }
    for (i = 0; i < n; i++) {
        int32_t s = 0;
        bag_fill += held ? (tgt - bag_fill) / 256 : -(bag_fill / 2048) - (bag_fill > 0);   /* (~6 ms up, ~0.7 s down) */
        for (k = 0; k < 3u; k++) {
            int32_t q;
            bag_ph[k] += inc[k];
            q = bag_blep(bag_ph[k], inc[k]);
            bag_lp[k] += (q - bag_lp[k]) / 3;    /* (a reed's softness) */
            s += k ? bag_lp[k] / 2 : bag_lp[k];
        }
        y[i] += (int32_t)(((int64_t)s * bag_fill >> 15) * level >> 7) * 8;   /* Q15 x 8: the model's units */
    }
}

/* ----------------------------------------------------------------- KALIMBA --- */
/* A kalimba (mbira, sanza: a lamellophone), built after its physics (Chapman, "The tones of the kalimba", 2012;
 * Rossing, "Science of Percussion Instruments"): a steel tine pressed onto a bridge, its long end free, flicked by the
 * thumb. What it plays, at the half rate:
 *   the tine     a clamped-free bar: its modes at 1 : 6.27 : 17.5 (far from harmonic: neither a bell nor a string);
 *                the fundamental sings 1.5 .. 9 s (the low tines the longest), the second dies ~7 x sooner, the
 *                third in a moment. A wide swing is a little stiffer: a hard flick starts a few cents sharp and settles
 *                (~60 ms). TINE (STRUCTURE): the bare bar's ratios .. a tip weighted and bent, as a tuned tine's
 *                (1 : 5.3 : 13.5): the overtone nearer, warmer
 *   the tail     the short end behind the bridge rings too, high (~2.6 .. 4.4 kHz) and brief (~0.2 s): the "ting"
 *   the flick    the thumb pushes the tip and lets go: a smooth pulse, shorter (brighter) the harder (2.5 .. 0.6 ms),
 *                and the nail's click on the steel (a 1 ms burst, ~4 kHz; BRIGHTNESS); the voices' sound only colours
 *                that flick (~6 ms), so the tine rings out on its own, held or not, as a real one does
 *   the box      a hollow box with a sound hole: the tines' force on the bridge rings its air (a Helmholtz resonance
 *                ~210 Hz) and its top (~470 Hz, ~1.25 kHz): warmth and the "hollow" of a box kalimba; a steel tine
 *                itself hardly radiates the low end, the box does
 *   the buzzers  (BUZZ, POSITION) the mbira's bottle caps and shells: they rattle each time the tine swings past
 *                their gap, the wider the more, dying with the tine
 *   DAMPING      how long it sings (x 1/2 .. 2 of the tine's own) */
#define WG_KM 4
/* Q28 to an integer, rounded toward zero: a sharp two-pole's state then dies away (rounded down it can ring on for
 * ever at the bottom bit: a limit cycle, a faint tone left on its resonance) */
static inline int32_t wg_q28z(int64_t a) { return (int32_t)((a < 0 ? a + ((1 << 28) - 1) : a) >> 28); }
static const uint32_t WG_TINE0[3] = {4096, 25670, 71885};    /* Q12: 1, 6.267, 17.55 (the bare clamped-free bar) */
static const uint32_t WG_TINE1[3] = {4096, 21709, 55296};    /* 1, 5.3, 13.5 (a weighted, bent tip) */
typedef struct {
    int32_t y1[WG_KM], y2[WG_KM];                /* the tine's three modes and its tail */
    int32_t bx[3][2];                            /* the box: its air, its top's two (state: y1, y2) */
    int32_t bz, env, yp, ck;                     /* the buzzers; the voices; the last out; the nail's click (Q15) */
    int32_t win, pa, gl;                         /* the voices' window (Q15); the flick's height; the glide (Q16 cents) */
    uint32_t pt, pl;                             /* half-rate samples into the flick; its length */
    uint32_t rng;
} wg_kalimba_t;
static void wg_mode(int32_t *a1, int32_t *a2, int32_t *b, uint32_t ph, uint32_t t60, int32_t amp)
{   /* a two-pole at the half rate: ph the half-rate phase a sample, t60 in half-rate samples, amp Q16 */
    uint32_t e = 653126u / (t60 < 16u ? 16u : t60);
    int32_t d = e < 2000u ? (int32_t)(e * 2839u) : (int32_t)((1u << 28) - (px_exp2(-(int32_t)e) << 12));
    int32_t r = (1 << 28) - d, cs = px_sin(ph + 0x40000000u) >> 2, sn = px_sin(ph) >> 2;
    *a1 = (int32_t)(((int64_t)r * cs) >> 27);
    *a2 = (int32_t)(((int64_t)r * r) >> 28);
    *b = (int32_t)(((int64_t)(sn >> 3) * amp) >> 16);
}
static void wg_kalimba_run(wg_kalimba_t *K, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t tine,
                           int32_t buzz, int32_t bright, int32_t damp, uint32_t strike)
{
    int32_t a1[WG_KM], a2[WG_KM], b[WG_KM], ba1[3], ba2[3], bb[3];
    uint32_t i, m, hz = (uint32_t)(((uint64_t)f0 * 44100u) >> 32);
    uint32_t ts;
    (void)wg_follow(&K->env, in, n);
    {   /* the fundamental's T60 (half-rate samples): ~4.5 s at 260 Hz, a sixth longer an octave down (to 9 s), shorter
         * up (1.5 s at 2 kHz), x 2^((DAMPING - 64) / 64) */
        int32_t oc = 0;                          /* octaves above 260 Hz, Q16 (a rough log2 by halving) */
        uint32_t h = hz < 20u ? 20u : hz;
        while (h >= 520u) { h >>= 1; oc += 65536; }
        while (h < 260u) { h <<= 1; oc -= 65536; }
        oc += (int32_t)(h - 260u) * 65536 / 260;
        ts = (uint32_t)(((uint64_t)99225u * px_exp2(-oc * 2 / 5 + (damp / 516 - 64) * 1024)) >> 16);
        ts = ts > 220000u ? 220000u : ts;
    }
    if (strike) {                                /* the thumb: as hard as the note on */
        uint32_t v = strike > 127u ? 127u : strike;
        K->pa = (int32_t)(v * v / 127u + v / 4u) * (PX_ONE / 512);   /* (the thump's height) */
        K->pl = 55u - v * 42u / 127u;            /* (55 .. 13 half-rate samples: 2.5 .. 0.6 ms) */
        K->pt = 0;
        K->win = 32767;
        K->gl = (int32_t)v * 6 * 65536 / 127;    /* (up to 6 cents sharp) */
        K->ck = 1000 + bright / 6 + (int32_t)v * 60;   /* the nail */
        if (!K->rng)
            K->rng = 0x9E3779B9u;
    }
    {   /* the tine's modes, a few cents sharp while the swing is wide */
        uint32_t fg = (uint32_t)(((uint64_t)f0 * px_exp2(K->gl / 1200)) >> 16);
        for (m = 0; m < 3u; m++) {
            int32_t ratio = (int32_t)WG_TINE0[m] + (int32_t)(((int64_t)((int32_t)WG_TINE1[m] - (int32_t)WG_TINE0[m]) * tine) >> 16);
            uint64_t fr = ((uint64_t)fg * (uint32_t)ratio) >> 12;
            if (fr * 2u >= 0x70000000u)          /* (past ~9.6 kHz: not there) */
                break;
            wg_mode(&a1[m], &a2[m], &b[m], (uint32_t)fr * 2u, m == 0u ? ts : m == 1u ? ts / 7u : ts / 22u,
                    m == 0u ? 65536 : m == 1u ? 15000 + bright / 6 : 5000 + bright / 10);
        }
        for (; m < 3u; m++)
            a1[m] = a2[m] = b[m] = 0;
        {   /* the tail behind the bridge: high, brief */
            uint32_t th = 2600u + (hz > 2000u ? 1800u : hz * 9u / 10u);
            wg_mode(&a1[3], &a2[3], &b[3], (uint32_t)(((uint64_t)th << 32) / 22050u), 4410u, 4000 + bright / 12);
        }
        K->gl -= K->gl / 40;                     /* (the glide: ~60 ms, a block at a time) */
        if (strike) {   /* the tip let go: the tine starts from where the thumb held it, each mode a cosine from its
                         * share of that bend (the tip's: the fundamental most; harder and BRIGHTER, more of the upper
                         * ones and of the tail, which the swing sets ringing at the bridge) */
            uint32_t v = strike > 127u ? 127u : strike;
            int32_t A = (int32_t)(v * v / 127u + v / 4u) * (PX_ONE / 48), up = 16384 + (int32_t)v * 160 + bright / 4;
            static const int32_t SHARE[WG_KM] = {32768, 6500, 900, 2100};   /* (Q15 of the bend, at up = 1) */
            for (m = 0; m < WG_KM; m++) {
                int32_t am;
                if (!a1[m])
                    continue;
                am = m ? px_m(px_m(A, SHARE[m], 15), up, 15) : A;
                K->y1[m] += am;                  /* (y[0] = A, y[-1] = A cos w / r ~ A a1 / 2: a1 = 2 r cos w, Q28) */
                K->y2[m] += (int32_t)(((int64_t)am * (a1[m] >> 13)) >> 16);
            }
        }
    }
    /* the box: the air ~210 Hz (T60 0.12 s), its top ~470 Hz (0.09 s) and ~1.25 kHz (0.05 s) */
    wg_mode(&ba1[0], &ba2[0], &bb[0], (uint32_t)((210ull << 32) / 22050u), 2650u, 52000);
    wg_mode(&ba1[1], &ba2[1], &bb[1], (uint32_t)((470ull << 32) / 22050u), 2000u, 30000);
    wg_mode(&ba1[2], &ba2[2], &bb[2], (uint32_t)((1250ull << 32) / 22050u), 1100u, 16000);
    for (i = 0; i < n; i += 2u) {
        int32_t x = i + 1u < n ? (in[i] + in[i + 1u]) >> 1 : in[i], o = 0, br = 0, a, t;
        x = px_m(x, K->win, 15) / 32;            /* (the voices: only in the flick, softly) */
        K->win -= K->win >> 7;                   /* (~6 ms) */
        if (K->pt < K->pl) {                     /* the flick: a raised cosine */
            x += px_m(K->pa, (32768 - (px_sin(K->pt * (0xFFFFFFFFu / K->pl) + 0x40000000u) >> 15)) / 2, 15);
            K->pt++;
        }
        for (m = 0; m < WG_KM; m++) {
            int32_t v;
            if (!b[m] && !K->y1[m] && !K->y2[m])
                continue;
            v = wg_q28z((int64_t)a1[m] * K->y1[m] - (int64_t)a2[m] * K->y2[m] + (int64_t)b[m] * x);
            v = px_clamp(v, -32 * PX_ONE, 32 * PX_ONE);
            K->y2[m] = K->y1[m];
            K->y1[m] = v;
            if (m == 0u)
                br += v;                         /* (the bridge feels the fundamental most) */
            else
                br += v >> 2;
            o += m == 0u ? v / 3 : v;            /* (the steel itself: its highs; its low end it hardly sends) */
        }
        for (m = 0; m < 3u; m++) {               /* the box, rung through the bridge */
            int32_t v = wg_q28z((int64_t)ba1[m] * K->bx[m][0] - (int64_t)ba2[m] * K->bx[m][1] + (int64_t)bb[m] * (br >> 3));
            v = px_clamp(v, -32 * PX_ONE, 32 * PX_ONE);
            K->bx[m][1] = K->bx[m][0];
            K->bx[m][0] = v;
            o += v;
        }
        o += br / 2;                             /* (and the bridge's own sound through the box: the body of it) */
        if (K->ck > 64) {                        /* the nail's click: a brief burst, high */
            t = (int32_t)(px_rand(&K->rng) >> 12) - (int32_t)(px_rand(&K->rng) >> 12);   /* (a difference: high) */
            o += px_m(t, K->ck, 15) >> 1;
            K->ck -= K->ck >> 4;                 /* (~1 ms) */
        }
        if (buzz) {                              /* the buzzers: a rattle as the swing passes their gap */
            a = K->y1[0] < 0 ? -K->y1[0] : K->y1[0];
            a = a > PX_ONE / 6 ? a - PX_ONE / 6 : 0;
            K->bz += px_m(9000, px_m((int32_t)(px_rand(&K->rng) >> 11) - (1 << 20), a, 20) - K->bz, 15);
            o += K->bz * buzz / 12;
        }
        y[i] = (K->yp + o) >> 1;
        if (i + 1u < n)
            y[i + 1u] = o;
        K->yp = o;
    }
}

/* ------------------------------------------------------------- DIDGERIDOO --- */
/* The didgeridoo: the lips buzzing into a long, widening bore (the BRASS waveguide, low: the drone), and what makes
 * it a didgeridoo, the player's mouth and throat in front of it: two formants the vocal tract shapes over the drone
 * (POSITION: the mouth, oo .. aw .. ee: F1 250 .. 700 Hz, F2 600 .. 2400 Hz), moved by the breath (louder: wider
 * open), and a voice hummed into the lips (DAMPING: how much of the formants' growl). STRUCTURE the lips: low the
 * drone, high a toot (the bore's next mode, overblown). The voices' sound is the breath (circular, so held notes
 * drone on); FREE EG on POSITION is the "wa-wa" */
typedef struct {
    wg_brass_t br;
    int32_t a1, b1, a2, b2, env;                 /* the formants (state-variable: low, band), the breath */
} wg_didge_t;
static void wg_didge_run(wg_didge_t *D, const int32_t *in, int32_t *y, uint32_t n, uint32_t f0, int32_t lip,
                         int32_t mouth, int32_t bright, int32_t growl)
{
    uint32_t i;
    int32_t env = wg_follow(&D->env, in, n), op = env * 8 > PX_ONE ? PX_ONE : env * 8, m, f1, f2, c1, c2, q;
    wg_brass_run(&D->br, in, y, n, f0, lip, 0, bright, 58000);
    m = mouth * 516 + (op >> 6) * 4;             /* (the breath opens the mouth a little) */
    m = m > 65535 ? 65535 : m;
    f1 = 250 + (int32_t)(((int64_t)m * 450) >> 16);                    /* Hz */
    f2 = 600 + (int32_t)(((int64_t)m * 1800) >> 16);
    c1 = f1 * 4669 / 1000;                       /* 2 pi f / FS, Q15 */
    c2 = f2 * 4669 / 1000;
    q = 6000 + growl / 32;                       /* 1 / Q: sharp formants (growl: wider, louder) */
    for (i = 0; i < n; i++) {
        int32_t x = y[i], h1, h2;
        h1 = x - D->a1 - px_m(D->b1, q, 15);
        D->b1 += px_m(c1, h1, 15);
        D->a1 += px_m(c1, D->b1, 15);
        h2 = x - D->a2 - px_m(D->b2, q, 15);
        D->b2 += px_m(c2, h2, 15);
        D->a2 += px_m(c2, D->b2, 15);
        y[i] = px_clamp(x / 4 + px_m(D->b1, 22000 + growl / 8, 15) + px_m(D->b2, 16000 + growl / 8, 15),
                        -64 * PX_ONE, 64 * PX_ONE);
    }
}
