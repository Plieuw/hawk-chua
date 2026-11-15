/* SPDX-License-Identifier: GPL-3.0-only */
/* SHIMMER: a stereo reverb with a pitch shifter in its feedback loop. Every trip round the loop part of the tail is
 * shifted down by the INTERVAL and fed back in, so the tail sinks: an octave, then another, and with SHIMMER high and a
 * long DECAY it keeps falling and building, a deepening undertow, until the high-pass (~40 Hz) takes the bottom.
 *
 *   input   two allpasses diffuse the dry signal
 *   tank    a 4-line feedback delay network (Householder matrix: o - sum / 2), each line with a one-pole low-pass
 *           (TONE) and the gain of its length for the DECAY time; SIZE scales the lengths (they glide when it turns)
 *   shimmer the tank's outputs, high-passed (~40 Hz: no rumble building), through a two-tap granular pitch shifter
 *           (taps sweeping a 2048-sample window, crossfaded by triangles): SHIMMER crossfades part of the tank's
 *           feedback from itself to this shifted copy, keeping its energy (weights a and b, a^2 + b^2 = 1), so the
 *           loop gain stays the DECAY's (bounded). Each trip round (~60 .. 110 ms) that part of the tail moves up an
 *           interval down: at SHIMMER 127 (30 % a trip) about 2..3 octaves a second, the higher octaves fading as the
 *           lower ones build
 *   output  left and right from different lines; dry / wet by MIX (the dry stays full to the middle)
 *
 *   SIZE 0..127      the room: 0.35 .. 1 of the longest lines (~60 .. 110 ms)
 *   DECAY 0..127     RT60 0.4 s .. 30 s
 *   SHIMMER 0..127   how much of the feedback is the shifted tail: how fast it rises (0: a plain reverb)
 *   MIX 0..127       0: off (nothing runs; the tank is cleared at the next use)
 *   INTERVAL         -OCT, -5TH, -OCT-5TH, -2 OCT (down: the sub-octaves under the sound)
 *   TONE 0..127      the tank's damping: dark (1.5 kHz) .. open (16 kHz)
 *   SPACE            the building it rings in (ALGORITHM, with the INTERVAL): HALL (the plain tank), TEMPLE (a stone
 *                    chamber: dense early reflections, the hypogeum's resonance ~110 Hz and its neighbours, dark),
 *                    CATHEDRAL (the nave: echoes off the pillars 23 .. 89 ms, the long low modes 49 / 98 / 147 Hz),
 *                    MOSQUE (the dome: a flutter echo focused back and forth, its ringing modes 165 / 330 / 495 Hz,
 *                    bright); and the instruments' own bodies, the sound through their wood (as a body filter in
 *                    commuted synthesis: eight modes each, rung by the dry sound; the tank, an eighth as loud, the
 *                    room around it): PIANO (the soundboard: its modes 92 Hz .. 1.1 kHz, the lid's reflections),
 *                    GUITAR (the air in the box at 100 Hz, the top's modes 204 / 395 / 550 Hz ...), VIOLIN (A0 275 Hz,
 *                    the B1 modes 460 / 540 Hz, the bridge hill ~2.5 .. 3 kHz), CELLO (the same an octave and more
 *                    down), HARP (a box: wide, soft modes 90 Hz .. 2 kHz)
 *                    DELAYS (the last three SPACEs, in the tempo): PING PONG, PATTERN, BOUNCE; see fx_delay below
 * Fixed point: signals Q15 in int32, the lines int16 (saturating), coefficients Q15 / Q16; the modes Q28.
 *
 *   fx_shimmer(in, l, r, n)   audio ISR, core 0: in[] mono Q15 -> l[], r[] Q15 */
#define SH_NLINE 4
#define SH_MAXLEN 4800u                          /* the longest line (at SIZE 127) */
static const uint16_t SH_LEN[SH_NLINE] = {4799, 3931, 3203, 2687};   /* mutually prime: no shared echoes */
#define SH_AP1 142u
#define SH_AP2 379u
#define SH_PS 4096u                              /* the pitch shifter's buffer (a power of two) */
#define SH_WIN 2048u                             /* its window */
enum { SI_OCT, SI_FIFTH, SI_OCT5, SI_2OCT, SI_COUNT };
static const char *const SH_INTERVAL_NAME[SI_COUNT] = {"-OCT", "-5TH", "-OCT-5TH", "-2 OCT"};
static const uint32_t SH_INC[SI_COUNT] = {1u << 21, 699051u, 1398101u, 3u << 20};   /* (1 - ratio) 2^32 / SH_WIN:
                                                  * 1/2, 2/3, 1/3, 1/4 */
enum { SS_HALL, SS_TEMPLE, SS_CATHEDRAL, SS_MOSQUE, SS_PIANO, SS_GUITAR, SS_VIOLIN, SS_CELLO, SS_HARP, SS_ACCORDION,
       SS_BAGPIPE, SS_ORGAN, SS_LESLIE, SS_PINGPONG, SS_PATTERN, SS_BOUNCE, SS_MOUTH, SS_COUNT };   /* (32 at most: a
                                                  * preset keeps it in 5 bits, ui.c PR_SPACE) */
#define SS_DELAY SS_PINGPONG                     /* (the delays: PING PONG .. BOUNCE) */
#define SS_IS_DELAY(sp) ((sp) >= SS_DELAY && (sp) <= SS_BOUNCE)
#define SS_BODY SS_PIANO                         /* (from here: an instrument's body) */
static const char *const SH_SPACE_NAME[SS_COUNT] = {"HALL", "TEMPLE", "CATHEDRAL", "MOSQUE", "PIANO BODY",
                                                    "GUITAR BODY", "VIOLIN BODY", "CELLO BODY", "HARP BODY",
                                                    "ACCORDION", "BAGPIPE", "PIPE ORGAN", "LESLIE", "PING PONG",
                                                    "PATTERN DLY", "BOUNCE DLY", "MOUTH"};
/* each space: its three modes (two-pole resonators: a1 = 2 r cos w, a2 = r^2, b = (1 - r) 2 sin w x 2 gain: the peak
 * twice the gain; Q28), its early
 * reflections (taps in ms, gains Q15; MOSQUE: one tap fed back, the flutter), its damping (x / 16 of TONE's) */
#define SH_NMODE 8
static const int32_t SH_MODE[SS_COUNT][SH_NMODE][3] = {
    {{0, 0, 0}},
    {{536777290, 268368535, 667}, {536711510, 268341983, 2930}, {536510598, 268259584, 4611}},
    {{536782875, 268360505, 628}, {536668693, 268285575, 3349}, {536528380, 268210666, 4709}},
    {{536512317, 268225188, 7910}, {535857546, 268015084, 23718}, {534907193, 267805145, 35554}},
    /* the bodies (tools: f, Q, gain -> r = exp(-pi f / (Q FS)), w = 2 pi f / FS) */
    { {536573544, 268184246, 3293}, {536437440, 268105791, 5835}, {536206719, 268000235, 11444}, {535879082, 267889101, 17814}, {535353831, 267754854, 26611}, {534347587, 267498341, 43254}, {532283737, 266986051, 78425}, {527700213, 265818909, 163759} },   /* PIANO: 92 138 205 286 392 540 760 1100 Hz */
    { {536604010, 268223065, 3632}, {536344289, 268135543, 8718}, {535909016, 267900552, 12806}, {535335789, 267749652, 27020}, {534175912, 267385761, 49351}, {531914563, 266783253, 91758}, {528045691, 265582309, 170550}, {497545246, 254772082, 1280393} },   /* GUITAR: 100 204 280 395 550 780 1050 2200 Hz */
    { {535802439, 267778914, 23160}, {535204750, 267662099, 31243}, {534920932, 267636966, 47100}, {534347587, 267498341, 64882}, {532540743, 266767416, 83171}, {528285561, 265267171, 180486}, {481261067, 245565346, 4891466}, {455329254, 239518297, 6547333} },   /* VIOLIN: 275 405 460 540 700 1000 2500 3200 Hz */
    { {536543337, 268162412, 3891}, {536448414, 268127543, 5090}, {536311136, 268072369, 8848}, {536165207, 268005538, 12407}, {535664599, 267719309, 18373}, {534338501, 267005072, 45884}, {522050322, 259042343, 672823}, {510826845, 254469771, 1200621} },   /* CELLO: 100 145 190 225 300 450 1000 1500 Hz */
    { {536540034, 268148767, 3677}, {536264423, 268005538, 8823}, {535744104, 267798786, 21777}, {534814527, 267481018, 42844}, {533025493, 266886218, 86086}, {530030326, 265988037, 156843}, {522823954, 263509293, 364564}, {503131388, 255984922, 1062461} },   /* HARP: 90 180 300 450 650 900 1300 2000 Hz */
    { {535793242, 267481018, 16331}, {534308940, 266833938, 67144}, {532511541, 266371794, 152968}, {528968991, 265424693, 285191}, {521926912, 263321642, 589350}, {508640692, 259504095, 1445123}, {485241603, 252363614, 2954088}, {452635619, 243648574, 4729046} },   /* ACCORDION: 150 420 650 950 1350 1900 2600 3400 Hz */
    { {536685594, 268324089, 1479}, {536352503, 268212768, 5914}, {535760620, 267990265, 11084}, {535095604, 267842032, 23636}, {525158688, 262136280, 539854}, {513140401, 258427951, 1369859}, {485565710, 249977914, 3930517}, {448556555, 242955281, 6244884} },   /* BAGPIPE: 116.5 233 349.5 466 1000 1600 2500 3500 Hz */
    { {536644126, 268225188, 1318}, {536454553, 268085100, 3845}, {535906832, 267735202, 13176}, {535018389, 267176315, 29624}, {533130235, 266055719, 67905}, {528344945, 263384177, 230892}, {520075121, 259411678, 464527}, {497193038, 249977914, 1056203} },   /* ORGAN: 55 110 220 330 500 800 1200 2000 Hz */
    {{0, 0, 0}},                                 /* LESLIE: none (its rotors, fx_shimmer) */
    {{0, 0, 0}}, {{0, 0, 0}}, {{0, 0, 0}},       /* the delays: none */
    {{0, 0, 0}},                                 /* MOUTH: its formants move (sh_mouth_set) */
};
static const uint8_t SH_NMODES[SS_COUNT] = {0, 3, 3, 3, 8, 8, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0};
static const uint8_t SH_ROOM8[SS_COUNT] = {8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 2, 8, 1, 0, 0, 0, 1};   /* the tank's share, / 8 (the
                                                  * bodies: a small room; PIPE ORGAN: its church) */
static const uint8_t SH_ER_MS[SS_COUNT][4] = {{0, 0, 0, 0}, {7, 13, 19, 29}, {23, 41, 67, 89}, {31, 0, 0, 0},
                                              {3, 5, 8, 12}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {4, 9, 0, 0},
                                              {0, 0, 0, 0}, {0, 0, 0, 0}, {29, 53, 0, 0}, {0, 0, 0, 0}, {0}, {0}, {0}, {0}};
static const uint16_t SH_ER_G[SS_COUNT][4] = {{0, 0, 0, 0}, {16384, 13107, 9830, 6554}, {14746, 13107, 11469, 9830},
                                              {18022, 0, 0, 0}, {9830, 8192, 6554, 4915}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                              {0, 0, 0, 0}, {8192, 4915, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0},
                                              {13107, 9830, 0, 0}, {0, 0, 0, 0}, {0}, {0}, {0}, {0}};
static const uint8_t SH_DAMP16[SS_COUNT] = {16, 9, 12, 20, 12, 12, 14, 10, 12, 14, 16, 10, 12, 16, 16, 16, 12};
#define SH_ER 4096u                              /* the early reflections' line (93 ms, a power of two) */

#define SH_DLEN (SH_NLINE * SH_MAXLEN + SH_AP1 + SH_AP2 + SH_PS + SH_ER)   /* the delays' line: the reverb's memory */
static struct {
    union {
        struct {
            int16_t line[SH_NLINE][SH_MAXLEN];
            int16_t ap1[SH_AP1], ap2[SH_AP2];
            int16_t ps[SH_PS];
            int16_t er[SH_ER];
        };
        int16_t dl[SH_DLEN];                     /* the delays (at a quarter rate, 11025 Hz: ~2.5 s) */
    };
    uint32_t dw, dsub;                           /* the delays: the line's write place; samples into the quarter */
    int32_t dacc, dlp[2], dfb[2], dtap[12][2], dout[2], dprev[2], dhp;   /* decimation, feedback filters, the taps'
                                                  * filters, the output's last two (interpolated up) */
    uint8_t dmode;                               /* what the memory holds: 1 the reverb, 2 a delay */
    int32_t my[SH_NMODE][2];                     /* the modes' last two outputs */
    uint32_t ew;
    uint32_t wp[SH_NLINE], a1, a2, pw;
    uint32_t len[SH_NLINE];                      /* the read lengths now (gliding to SIZE's) */
    int32_t lp[SH_NLINE];                        /* the damping filters */
    int32_t hp_x, hp_y;                          /* the shifter's high-pass */
    int32_t dc[2], dcx[2];                       /* the wet output's DC blockers (~23 Hz), Q15 x 256 */
    uint32_t ph;                                 /* the shifter's phase, Q32 */
    uint32_t rh, rd;                             /* LESLIE: the horn's and the drum's turn, Q32 */
    int32_t rlo, rlo1;                           /* LESLIE: the crossover's low part (two poles) */
    uint8_t on;                                  /* the tank holds something: cleared when MIX goes to 0 */
} sh __attribute__((section(".pool")));

/* clearing the memory (~56 KB) all at once in the audio interrupt made a glitch (a preset changing the space): the
 * state is reset at once, the memory cleared 4096 samples a block from the bottom (~20 ms). A delay plays on at once:
 * what is not cleared yet reads as silence (its writes start at the bottom, behind the clearing); a reverb stays dry
 * till it is done */
static uint32_t sh_cpos = SH_DLEN;               /* (the memory clean below this) */
static void sh_reset(void)
{
    memset((char *)&sh + sizeof sh.dl, 0, sizeof sh - sizeof sh.dl);   /* (all but the memory: small) */
    sh_cpos = 0;
}
static int sh_clearing(void)                     /* a block's share of the clearing; 1 while it lasts */
{
    uint32_t c;
    if (sh_cpos >= SH_DLEN)
        return 0;
    c = SH_DLEN - sh_cpos < 4096u ? SH_DLEN - sh_cpos : 4096u;
    memset(&sh.dl[sh_cpos], 0, c * sizeof sh.dl[0]);
    sh_cpos += c;
    return sh_cpos < SH_DLEN;
}
static inline int32_t sh_dread(uint32_t i) { return i < sh_cpos ? sh.dl[i] : 0; }   /* (not cleared yet: silence) */

/* for the screen (ui.c): the wet level per block, a ring the main loop reads */
#define SH_ENV 64u
static struct { uint16_t env[SH_ENV]; volatile uint32_t w; } sh_view;

static inline int16_t sh_sat(int32_t v) { return (int16_t)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }

/* a soft limit for the shifted feedback: linear to 16000, then towards 32000 */
static inline int32_t sh_soft(int32_t v)
{
    int32_t a = v < 0 ? -v : v;
    if (a > 16000) {
        uint32_t e = (uint32_t)(a - 16000) > 131071u ? 131071u : (uint32_t)(a - 16000);   /* (32-bit division: the */
        a = 16000 + (int32_t)(e * 16000u / (16000u + e));                                   /* toolchain has no 64) */
        v = v < 0 ? -a : a;
    }
    return v;
}

/* an instrument's body: its eight modes rung by x (Q15) -> their sum, and the sum with the odd modes turned over */
static void sh_body(uint32_t sp, int32_t x, int32_t *bm, int32_t *bs)
{
    uint32_t k;
    int32_t e = x * 2048, m = 0, d = 0;          /* (/ 2, the states x 4096: the low modes' tiny inputs kept) */
    for (k = 0; k < SH_NMODE; k++) {
        const int32_t *c = SH_MODE[sp][k];
        int32_t z = (int32_t)(((int64_t)c[0] * sh.my[k][0] - (int64_t)c[1] * sh.my[k][1] + (int64_t)c[2] * e +
                               (1 << 27)) >> 28);   /* (rounded: no bias rings up) */
        z = z > 245760000 ? 245760000 : z < -245760000 ? -245760000 : z;
        sh.my[k][1] = sh.my[k][0];
        sh.my[k][0] = z;
        z >>= 12;
        m += z;
        d += k & 1u ? -z : z;
    }
    *bm = m;
    *bs = d;
}

/* --------------------------------------------------------------------- MOUTH --- */
/* MOUTH: the sound through a mouth, as a talk box puts it: the vocal tract's four formants (two-pole resonators, in
 * parallel) shaping what is played into a vowel; SHIMMER (FREE EG's SHIMMER target: talking) moves the mouth along the
 * INTERVAL's path of vowels (U O A E I, WAH: U to A, YAI: A to I, OUI: O U I), SIZE the mouth (big: the formants down,
 * x 0.75; small: up, x 1.4), TONE and DECAY the little room around it */
#ifndef SH_MOUTH_G
#define SH_MOUTH_G 5                             /* (the mouth about as loud as the dry) */
#endif
static const char *const SH_MPATH_NAME[SI_COUNT] = {"U O A E I", "WAH", "YAI", "OUI"};
static const uint16_t SH_VOWEL[5][4] = {         /* F1 .. F4, Hz (a man's: Peterson and Barney) */
    {300, 870, 2240, 3300},                      /* U */
    {450, 800, 2830, 3500},                      /* O */
    {730, 1090, 2440, 3400},                     /* A */
    {530, 1840, 2480, 3500},                     /* E */
    {270, 2290, 3010, 3700},                     /* I */
};
static const uint8_t SH_MPATH[SI_COUNT][4] = {{5, 0, 1, 2}, {2, 0, 2, 0}, {2, 2, 4, 0}, {3, 1, 0, 4}};   /* (count, vowels) */
static const uint8_t SH_MPATH5[5] = {0, 1, 2, 3, 4};
static const uint16_t SH_FBW[4] = {70, 90, 140, 180};          /* their bandwidths, Hz */
static const uint16_t SH_FG[4] = {32768, 16000, 6000, 3000};    /* their levels, Q15 */
static int32_t sh_mc[4][3];                      /* the formants now: a1, a2, b (Q28) */
static void sh_mouth_set(void)                   /* (once a block) */
{
    uint32_t iv = sys.sh_interval % SI_COUNT, nv = SH_MPATH[iv][0], k;
    const uint8_t *path = iv ? &SH_MPATH[iv][1] : SH_MPATH5;
    uint32_t pos = (sys.sh_shimmer & 127u) * (nv - 1u) * 256u / 127u, seg = pos >> 8, fr = pos & 255u;
    uint32_t scale = 49152u + (sys.sh_size & 127u) * 42598u / 127u;   /* Q16, 0.75 .. 1.4 */
    if (seg >= nv - 1u) { seg = nv - 2u; fr = 256u; }
    for (k = 0; k < 4u; k++) {
        uint32_t f0 = SH_VOWEL[path[seg]][k], f1 = SH_VOWEL[path[seg + 1u]][k];
        uint32_t f = (f0 * (256u - fr) + f1 * fr) >> 8, ph;
        int32_t r16, cw, sw;
        f = (uint32_t)(((uint64_t)f * scale) >> 16);
        f = f > 9000u ? 9000u : f;
        ph = f * 97391u;                         /* (a turn, Q32: f / 44100) */
        r16 = (int32_t)px_exp2(-(int32_t)((uint32_t)SH_FBW[k] * scale / 65536u * 6735u / 1000u));   /* e^(-pi bw / FS), Q16 */
        cw = px_sin(ph + 0x40000000u);           /* Q30 */
        sw = px_sin(ph);
        sh_mc[k][0] = (int32_t)(((int64_t)r16 * cw) >> 17);                 /* 2 r cos w, Q28 */
        sh_mc[k][1] = (int32_t)(((int64_t)r16 * r16) >> 4);                 /* r^2 */
        sh_mc[k][2] = (int32_t)(((((((int64_t)(65536 - r16) * sw) >> 17) * SH_FG[k]) >> 15) * (int32_t)f) / 600);   /* (1 - r)
                                                  * 2 sin w g, x f / 600 Hz: as loud on any vowel (what is played falls
                                                  * ~6 dB an octave) */
    }
}
static void sh_mouth(int32_t x, int32_t *bm, int32_t *bs)   /* (as sh_body: the sum, the sum with every other turned) */
{
    uint32_t k;
    int32_t e = x * 2048, m = 0, d = 0;
    for (k = 0; k < 4u; k++) {
        int32_t z = (int32_t)(((int64_t)sh_mc[k][0] * sh.my[k][0] - (int64_t)sh_mc[k][1] * sh.my[k][1] +
                               (int64_t)sh_mc[k][2] * e + (1 << 27)) >> 28);
        z = z > 245760000 ? 245760000 : z < -245760000 ? -245760000 : z;
        sh.my[k][1] = sh.my[k][0];
        sh.my[k][0] = z;
        z >>= 12;
        m += z;
        d += k & 1u ? -z : z;
    }
    *bm = m * SH_MOUTH_G;
    *bs = d * SH_MOUTH_G;
}

/* ------------------------------------------------------------------ the DELAYS --- */
/* Three delays in the tempo (the BPM, or MIDI clock's), in the reverb's memory (a quarter rate, 11025 Hz: up to ~2.5 s,
 * dark and soft as a tape echo), each a pattern of taps on one line: a tap a time after the note (in a step's 24ths),
 * a place left .. right, a level and its own filter (a resonant low-pass, the pattern's cutoff: the echoes darker,
 * brighter, in turn); the line's loop (the pattern's length) fed back, so the pattern comes round again, softer.
 *   PING PONG   STRAIGHT (left a step, right two), DOTTED (1.5 steps), TRIPLET (2/3), FILTER (left dark, right bright)
 *   PATTERN     TRESILLO (3 + 3 + 2), DUB (dotted, then the step after, dark), GALLOP, CLAVE (the son's 3-2, a bar)
 *   BOUNCE      BALL (a ball dropped: each bounce sooner and softer), BALL PONG (bouncing left, right), RISE (the
 *               ball backwards: slowing, swelling, sweeping across), SCATTER (taps strewn, each its own filter)
 *   SIZE    TIME: the step, 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/8. (dotted), 1/4 (halved while a pattern would not
 *           fit the line)
 *   DECAY   FEEDBACK: how much of the pattern comes round again (0 .. 95 %)
 *   SHIMMER FILTER: the taps' cutoff, 150 Hz .. 2.6 kHz (each tap's offset from it), 127 open (no filter)
 *   TONE    the filters' resonance (none .. a whistle)
 *   MIX     as the reverb's */
enum { SH_DTIMES = 8 };
static const uint8_t SH_DCLK[SH_DTIMES] = {3, 4, 6, 8, 12, 16, 18, 24};   /* MIDI clocks a step */
static const char *const SH_DTIME_NAME[SH_DTIMES] = {"1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/8.", "1/4"};
typedef struct { uint16_t t; int8_t pan, cut; uint8_t lvl; } sh_tap_t;   /* 24ths of a step; -127 .. 127; quarter
                                                  * octaves; / 255 */
typedef struct { const char *name; uint16_t loop; uint8_t n; sh_tap_t tap[10]; } sh_dpat_t;
static const sh_dpat_t SH_DPAT[3][SI_COUNT] = {
    {   /* PING PONG */
        {"STRAIGHT", 48, 2, {{24, -110, 0, 255}, {48, 110, 0, 235}}},
        {"DOTTED", 72, 2, {{36, -110, 0, 255}, {72, 110, 0, 235}}},
        {"TRIPLET", 32, 2, {{16, -110, 0, 255}, {32, 110, 0, 235}}},
        {"FILTER", 48, 2, {{24, -120, -5, 255}, {48, 120, 5, 235}}},
    },
    {   /* PATTERN */
        {"TRESILLO", 192, 3, {{72, -100, 0, 255}, {144, 100, 2, 225}, {192, 0, -2, 200}}},
        {"DUB", 96, 2, {{72, -90, -3, 255}, {96, 90, -1, 220}}},
        {"GALLOP", 96, 4, {{24, -60, 2, 170}, {36, 60, 1, 200}, {72, -100, 0, 255}, {96, 100, -2, 210}}},
        {"CLAVE", 384, 5, {{72, -100, -2, 255}, {144, 100, 0, 235}, {240, -60, 2, 215}, {288, 60, 4, 200},
                           {384, 0, 0, 190}}},
    },
    {   /* BOUNCE */
        {"BALL", 192, 10, {{48, -24, 0, 255}, {84, -12, 0, 209}, {111, -2, -1, 171}, {131, 6, -1, 140},
                           {146, 12, -2, 115}, {157, 17, -2, 94}, {165, 21, -3, 77}, {171, 24, -3, 63},
                           {176, 26, -4, 52}, {180, 28, -4, 43}}},   /* (rolling a little to the right) */
        {"BALL PONG", 192, 10, {{48, -110, 0, 255}, {84, 110, 0, 209}, {111, -100, -1, 171}, {131, 100, -1, 140},
                                {146, -90, -2, 115}, {157, 90, -2, 94}, {165, -80, -3, 77}, {171, 80, -3, 63},
                                {176, -70, -4, 52}, {180, 70, -4, 43}}},
        {"RISE", 192, 10, {{4, -120, -4, 43}, {9, -95, -4, 52}, {15, -70, -3, 63}, {23, -45, -3, 77}, {34, -20, -2, 94},
                           {49, 5, -2, 115}, {69, 30, -1, 140}, {96, 55, -1, 171}, {132, 80, 0, 209},
                           {180, 110, 0, 255}}},
        {"SCATTER", 192, 8, {{20, -80, 3, 200}, {44, 90, -4, 120}, {58, -30, 0, 180}, {90, 60, 5, 150},
                             {101, -95, -2, 90}, {130, 20, 2, 160}, {155, -60, -5, 110}, {178, 100, 1, 140}}},
    },
};
static uint32_t arp_bpm_now(void);               /* (arp.c: the tempo, its own or MIDI clock's) */
static const char *sh_iv_name(uint32_t sp, uint32_t iv)   /* the INTERVAL's name, a delay's pattern */
{
    return SS_IS_DELAY(sp) ? SH_DPAT[sp - SS_DELAY][iv % SI_COUNT].name : sp == SS_MOUTH ? SH_MPATH_NAME[iv % SI_COUNT] :
           SH_INTERVAL_NAME[iv % SI_COUNT];
}
static uint32_t sh_dtime(void) { return (sys.sh_size & 127u) * SH_DTIMES / 128u; }

static void fx_delay(const int32_t *in, int32_t *l, int32_t *r, uint32_t n, uint32_t sp, int32_t dry, int32_t wet)
{
    const sh_dpat_t *P = &SH_DPAT[sp - SS_DELAY][sys.sh_interval % SI_COUNT];
    static uint32_t key = 0xFFFFFFFFu, loop, tap[10];   /* (the taps and their filters: worked out again only when
                                                  * the tempo, the TIME, the pattern or the FILTER changed, not
                                                  * every block: no 64-bit divisions in the interrupt then) */
    static int32_t ff[10];
    static uint32_t held = 120u;                 /* (the tempo the taps follow: MIDI clock's wavers by a BPM or so,
                                                  * block to block; they move only for 2 BPM or more, or a held 1 BPM
                                                  * after a second; no clock (0): as they were) */
    static uint32_t off_n;
    uint32_t i, k, bpm = arp_bpm_now(), spt, peak = 0, kn;
    int32_t fb = (int32_t)(sys.sh_decay & 127u) * 245, q, open = (sys.sh_shimmer & 127u) >= 127u;
    if (bpm) {
        uint32_t d = bpm > held ? bpm - held : held - bpm;
        off_n = d ? off_n + n : 0u;
        if (d >= 2u || off_n > 44100u) {
            held = bpm;
            off_n = 0;
        }
    }
    bpm = held < 20u ? 20u : held > 300u ? 300u : held;
    kn = bpm | sh_dtime() << 9 | (sp - SS_DELAY) << 12 | (sys.sh_interval % SI_COUNT) << 16 | (sys.sh_shimmer & 127u) << 24;
    if (kn != key) {
        key = kn;
        /* samples (11025 Hz) a tick, Q16: 60 / BPM s a quarter, clocks / 24 of it a step, a 24th of that a tick */
        spt = (uint32_t)(((uint64_t)11025u * 60u * 65536u / bpm) * SH_DCLK[sh_dtime()] / 576u);
        while ((uint32_t)(((uint64_t)P->loop * spt) >> 16) > SH_DLEN - 4u)   /* (too long for the line: halved) */
            spt >>= 1;
        loop = (uint32_t)(((uint64_t)P->loop * spt) >> 16);
        for (k = 0; k < P->n; k++) {
            tap[k] = (uint32_t)(((uint64_t)P->tap[k].t * spt) >> 16);
            tap[k] = tap[k] < 1u ? 1u : tap[k];
            {   /* its filter: 150 Hz x 2^(FILTER 0 .. 4.1 oct + its offset), f = 2 sin(pi fc / 11025), Q15 */
                int32_t oc = (int32_t)(sys.sh_shimmer & 127u) * 268697 / 127 + P->tap[k].cut * 16384;   /* Q16 octaves */
                uint32_t fc = (uint32_t)(((uint64_t)150u * px_exp2(oc < 0 ? 0 : oc)) >> 16);
                fc = fc > 2600u ? 2600u : fc < 60u ? 60u : fc;
                ff[k] = px_sin((uint32_t)(((uint64_t)fc << 32) / 22050u)) >> 14;   /* (Q30 -> 2 x Q15) */
            }
        }
    }
    q = 32768 - (int32_t)(sys.sh_tone & 127u) * 210;   /* 1 / Q: 1 .. ~0.19 */
    for (i = 0; i < n; i++) {
        sh.dacc += in[i];
        if (++sh.dsub >= 4u) {                   /* a quarter-rate sample: in, the feedback, the taps */
            int32_t x = sh.dacc / 8, f, yl = 0, yr = 0;   /* (the four's mean, / 2: headroom) */
            uint32_t w = sh.dw;
            sh.dsub = 0;
            sh.dacc = 0;
            f = sh_dread(w >= loop ? w - loop : w + SH_DLEN - loop);   /* the loop's end, back in: darker, no rumble */
            sh.dfb[0] += ((f - sh.dfb[0]) * 13000) >> 15;    /* (~2.6 kHz) */
            sh.dhp += ((sh.dfb[0] - sh.dhp) * 2200) >> 15;   /* (~120 Hz) */
            f = (int32_t)(((int64_t)(sh.dfb[0] - sh.dhp) * fb) >> 15);
            sh.dl[w] = sh_sat(x + f);
            for (k = 0; k < P->n; k++) {
                int32_t v = sh_dread(w >= tap[k] ? w - tap[k] : w + SH_DLEN - tap[k]), gl, gr, p = P->tap[k].pan;
                if (!open) {                     /* the tap's filter: a state-variable low-pass */
                    int32_t *z = sh.dtap[k], hi;
                    z[0] += (int32_t)(((int64_t)ff[k] * z[1]) >> 15);
                    hi = v - z[0] - (int32_t)(((int64_t)q * z[1]) >> 15);
                    z[1] += (int32_t)(((int64_t)ff[k] * hi) >> 15);
                    z[1] = z[1] > 98304 ? 98304 : z[1] < -98304 ? -98304 : z[1];
                    z[0] = z[0] > 98304 ? 98304 : z[0] < -98304 ? -98304 : z[0];
                    v = z[0];
                }
                v = v * P->tap[k].lvl >> 8;
                gl = p > 0 ? 256 - 2 * p : 256;  /* (a balance: the middle full on both) */
                gr = p < 0 ? 256 + 2 * p : 256;
                yl += v * gl >> 8;
                yr += v * gr >> 8;
            }
            sh.dw = w + 1u >= SH_DLEN ? 0u : w + 1u;
            sh.dprev[0] = sh.dout[0];
            sh.dprev[1] = sh.dout[1];
            sh.dout[0] = yl * 2;                 /* (x 2: the headroom back) */
            sh.dout[1] = yr * 2;
        }
        {   /* up to the full rate: between the last two, linearly */
            int32_t t = (int32_t)sh.dsub, v, y;
            v = sh.dprev[0] + (sh.dout[0] - sh.dprev[0]) * (t + 1) / 4;
            y = sh.dprev[1] + (sh.dout[1] - sh.dprev[1]) * (t + 1) / 4;
            sh.dlp[0] += (v - sh.dlp[0]) / 2;    /* (a little smoothing: the steps' edges) */
            sh.dlp[1] += (y - sh.dlp[1]) / 2;
            l[i] = (int32_t)(((int64_t)in[i] * dry + (int64_t)sh.dlp[0] * wet) >> 15);
            r[i] = (int32_t)(((int64_t)in[i] * dry + (int64_t)sh.dlp[1] * wet) >> 15);
            {
                uint32_t a2 = (uint32_t)(sh.dlp[0] < 0 ? -sh.dlp[0] : sh.dlp[0]) + (uint32_t)(sh.dlp[1] < 0 ? -sh.dlp[1] : sh.dlp[1]);
                peak = a2 > peak ? a2 : peak;
            }
        }
    }
    sh_view.env[sh_view.w++ & (SH_ENV - 1u)] = (uint16_t)(peak > 65535u ? 65535u : peak);
}


static int32_t sh_dry = 32768;                   /* this block's dry level, Q15 (synth.c pans the dry with it) */
static void fx_shimmer(const int32_t *in, int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i, k, size = sys.sh_size & 127u, iv = sys.sh_interval % SI_COUNT, sp = sys.sh_space % SS_COUNT;
    uint32_t etap[4], rinc;
    int32_t mix = sys.sh_mix > 127u ? 127 : (int32_t)sys.sh_mix, g[SH_NLINE], damp, fa, fb_, dry, wet;
    uint32_t inc = SH_INC[iv], peak = 0;
    sh_dry = 32768;
    if (!mix) {
        if (sh.on)                               /* (off: the next use starts from silence) */
            sh_reset();
    } else if (sh.dmode != (SS_IS_DELAY(sp) ? 2u : 1u)) {   /* (the memory changing hands: from silence) */
        sh_reset();
        sh.dmode = SS_IS_DELAY(sp) ? 2u : 1u;
    }
    {
        int clearing = sh_clearing();
        if (!mix || (clearing && !SS_IS_DELAY(sp))) {   /* (off, or a reverb's memory clearing: dry) */
            for (i = 0; i < n; i++)
                l[i] = r[i] = in[i];
            return;
        }
    }
    if (SS_IS_DELAY(sp)) {                       /* a delay: in the tempo (above) */
        int32_t dr = mix < 64 ? 32768 : (127 - mix) * 32768 / 63, wt = mix > 64 ? 32768 : mix * 32768 / 64;
        sh_dry = dr;
        sh.on = 1;
        fx_delay(in, l, r, n, sp, dr, wt);
        return;
    }
    if (!sh.on) {
        for (k = 0; k < SH_NLINE; k++)
            sh.len[k] = SH_LEN[k];
        sh.on = 1;
    }
    {   /* per block: the lengths glide to SIZE's, the gains of the DECAY time, TONE, SHIMMER */
        uint32_t scale = 22938u + size * 330u;   /* 0.35 .. 1, Q16 */
        uint32_t rt = (uint32_t)(((uint64_t)17640u * px_exp2((int32_t)((sys.sh_decay & 127u) * 408220u / 127u))) >> 16);
        int32_t fc_oct = (int32_t)((sys.sh_tone & 127u) * 222822u / 127u);   /* 1.5 kHz x 2^(0 .. 3.4) */
        uint32_t fc = (uint32_t)(((uint64_t)1500u * px_exp2(fc_oct)) >> 16) * SH_DAMP16[sp] / 16u;
        /* LESLIE: the rotors' speed from SIZE, 0.8 Hz (chorale) .. 7 Hz (tremolo), a turn in Q32 per sample */
        rinc = (uint32_t)((((uint64_t)52429u * px_exp2((int32_t)(size * 205128u / 127u)) >> 16) * 97391u) >> 16);
        for (k = 0; k < 4u; k++)                 /* the early reflections' taps, samples */
            etap[k] = (uint32_t)SH_ER_MS[sp][k] * 441u / 10u;
        for (k = 0; k < SH_NLINE; k++) {
            uint32_t t = (uint32_t)(((uint64_t)SH_LEN[k] * scale) >> 16);
            int32_t d = (int32_t)t - (int32_t)sh.len[k];
            sh.len[k] = (uint32_t)((int32_t)sh.len[k] + (d > 16 ? 16 : d < -16 ? -16 : d));
            /* gain 2^(-9.966 len / rt) (60 dB in rt samples) */
            g[k] = (int32_t)(px_exp2(-(int32_t)((653126u * sh.len[k]) / rt)) >> 1);   /* Q15 (< 2^32: 32-bit division) */
        }
        /* the damping one-pole: a = 1 - exp(-2 pi fc / FS) = 1 - 2^(-2 pi fc / (FS ln 2)) */
        damp = 32768 - (int32_t)(px_exp2(-(int32_t)(((uint64_t)fc * 13794u) >> 10)) >> 1);
        {   /* the crossfade: c = 0 .. 0.3 of the amplitude; a = 1 - c, b = sqrt(1 - a^2) x 1.22 (the shifter's two
             * triangle-weighted taps keep ~2/3 of the energy of uncorrelated input) */
            int32_t c = sp == SS_MOUTH ? 0 : (int32_t)(sys.sh_shimmer & 127u) * 77;   /* Q15, 0 .. 0.3 (MOUTH: the vowel) */
            uint32_t a2;
            fa = 32768 - c;
            a2 = (uint32_t)(((int64_t)fa * fa) >> 15);                   /* a^2, Q15 */
            fb_ = (int32_t)px_isqrt64((uint64_t)(32768u - a2) << 15);    /* Q15 */
            fb_ = (int32_t)(((int64_t)fb_ * 40132) >> 15);                /* x 1.22 */
        }
        if (sp == SS_MOUTH)
            sh_mouth_set();
        dry = mix < 64 ? 32768 : (127 - mix) * 32768 / 63;
        wet = mix > 64 ? 32768 : mix * 32768 / 64;
        sh_dry = dry;
    }
    for (i = 0; i < n; i++) {
        int32_t x = in[i] >> 1, o[SH_NLINE], s, ps, v, y, bm = 0, bs = 0;
        /* input diffusion: two allpasses (gain 0.6) */
        {
            int32_t d1 = sh.ap1[sh.a1], d2;
            v = x + (d1 * 19661 >> 15);
            sh.ap1[sh.a1] = sh_sat(v);
            x = d1 - (v * 19661 >> 15);
            sh.a1 = sh.a1 + 1u == SH_AP1 ? 0u : sh.a1 + 1u;
            d2 = sh.ap2[sh.a2];
            v = x + (d2 * 19661 >> 15);
            sh.ap2[sh.a2] = sh_sat(v);
            x = d2 - (v * 19661 >> 15);
            sh.a2 = sh.a2 + 1u == SH_AP2 ? 0u : sh.a2 + 1u;
        }
        if (sp == SS_LESLIE) {                   /* the rotary speaker: the highs from a turning horn (its Doppler:
                                                  * a delay of 1 ms +- 0.4; louder facing the microphone, a quarter
                                                  * turn later), the lows from a turning drum (louder and softer);
                                                  * two microphones half a turn apart */
            int32_t xi = in[i], hi, sh_, ch, sd, a, b;
            uint32_t d, p;
            sh.rlo1 += (int32_t)(((int64_t)(xi - sh.rlo1) * 3542 + 16384) >> 15);   /* (~800 Hz, two poles: little */
            sh.rlo += (int32_t)(((int64_t)(sh.rlo1 - sh.rlo) * 3542 + 16384) >> 15);  /* of the horn's in the drum) */
            hi = xi - sh.rlo;
            sh.er[sh.ew & (SH_ER - 1u)] = sh_sat(hi);
            sh_ = px_sin(sh.rh) >> 15;
            ch = px_sin(sh.rh + 0x40000000u) >> 15;
            sd = px_sin(sh.rd) >> 15;
            d = (uint32_t)((44 << 8) + 18 * sh_ / 128);      /* Q8 samples */
            p = sh.ew - (d >> 8);
            a = sh.er[p & (SH_ER - 1u)]; b = sh.er[(p - 1u) & (SH_ER - 1u)];
            bm = a + (((b - a) * (int32_t)(d & 255u)) >> 8);
            bm = (int32_t)(((int64_t)bm * (32768 + ch / 3)) >> 15) + (int32_t)(((int64_t)sh.rlo * (32768 + sd / 6)) >> 15);
            d = (uint32_t)((44 << 8) - 18 * sh_ / 128);      /* (the other microphone: the horn half a turn on) */
            p = sh.ew - (d >> 8);
            a = sh.er[p & (SH_ER - 1u)]; b = sh.er[(p - 1u) & (SH_ER - 1u)];
            bs = a + (((b - a) * (int32_t)(d & 255u)) >> 8);
            bs = (int32_t)(((int64_t)bs * (32768 - ch / 3)) >> 15) + (int32_t)(((int64_t)sh.rlo * (32768 - sd / 6)) >> 15);
            sh.ew++;
            sh.rh += rinc;
            sh.rd += rinc - (rinc >> 3);         /* (the drum a little slower) */
            x += (bm + bs) / 8;
        } else if (sp >= SS_BODY) {              /* the instrument's body: its modes rung by the dry sound (left and
                                                  * right: the odd modes turned over, a little width) */
            if (sp == SS_MOUTH)
                sh_mouth(in[i], &bm, &bs);
            else
                sh_body(sp, in[i], &bm, &bs);
            x += bm / 4;                         /* (the room hears the body) */
        }
        /* the building: early reflections (MOSQUE: the flutter, fed back) into the tank's input */
        if (sp != SS_HALL && sp != SS_LESLIE) {
            int32_t e = 0, w = in[i] >> 1;
            for (k = 0; k < 4u; k++)
                if (etap[k])
                    e += sh.er[(sh.ew - etap[k]) & (SH_ER - 1u)] * (int32_t)SH_ER_G[sp][k] >> 15;
            if (sp == SS_MOSQUE)
                w += e;
            sh.er[sh.ew & (SH_ER - 1u)] = sh_sat(w);
            sh.ew++;
            x += e / 2;
        }
        /* the tank's outputs, damped */
        for (k = 0; k < SH_NLINE; k++) {
            uint32_t rd = sh.wp[k] + SH_MAXLEN - sh.len[k];
            int32_t t = sh.line[k][rd >= SH_MAXLEN ? rd - SH_MAXLEN : rd];
            sh.lp[k] += (int32_t)(((int64_t)(t - sh.lp[k]) * damp + 16384) >> 15);   /* (rounded: no DC builds) */
            o[k] = sh.lp[k];
        }
        /* the shifter: its input the tail, high-passed (one pole, ~120 Hz) */
        {
            int32_t hx = (o[0] + o[1] - o[2] - o[3]) / 2, a, b;   /* (the signs the shifted tail goes in with) */
            uint32_t ph2, d1, d2, w1, w2;
            sh.hp_y = hx - sh.hp_x + (int32_t)(((int64_t)sh.hp_y * 32581 + 16384) >> 15);
            sh.hp_x = hx;
            sh.ps[sh.pw & (SH_PS - 1u)] = sh_sat(sh.hp_y);
            /* two taps, half a window apart: delay = SH_WIN phase, growing: the pitch falls */
            ph2 = sh.ph + 0x80000000u;
            d1 = ((sh.ph >> 16) + 16u) * SH_WIN;    /* Q16 samples (at least a sample: the interpolation) */
            d2 = ((ph2 >> 16) + 16u) * SH_WIN;
            {
                uint32_t p1 = sh.pw - 1u - (d1 >> 16), p2 = sh.pw - 1u - (d2 >> 16);
                int32_t f1 = (int32_t)(d1 & 0xFFFFu), f2 = (int32_t)(d2 & 0xFFFFu);
                int32_t s0 = sh.ps[p1 & (SH_PS - 1u)], s1 = sh.ps[(p1 - 1u) & (SH_PS - 1u)];
                int32_t t0 = sh.ps[p2 & (SH_PS - 1u)], t1 = sh.ps[(p2 - 1u) & (SH_PS - 1u)];
                a = s0 + (int32_t)(((int64_t)(s1 - s0) * f1 + 32768) >> 16);   /* (rounded throughout: a floor */
                b = t0 + (int32_t)(((int64_t)(t1 - t0) * f2 + 32768) >> 16);   /* bias would build DC in the loop) */
            }
            w1 = sh.ph >> 16;                    /* triangle windows: 1 - |2 p - 1|, Q16; the two sum to 1 */
            w1 = w1 < 32768u ? w1 * 2u : (65535u - w1) * 2u;
            w2 = 65536u - w1;
            ps = sh_soft((int32_t)(((int64_t)a * w1 + (int64_t)b * w2 + 32768) >> 16));
            sh.ph += inc;
            sh.pw++;
        }
        /* the feedback: the Householder mix of the lines and the shifted tail, crossfaded by SHIMMER, at the gain of
         * the DECAY; then the input (signs alternate: decorrelated) */
        s = (o[0] + o[1] + o[2] + o[3]) / 2;
        for (k = 0; k < SH_NLINE; k++) {
            int32_t h = o[k] - s, q = k & 2u ? -ps : ps;
            int32_t fb = (int32_t)(((int64_t)h * fa + (int64_t)q * fb_ + 16384) >> 15);
            int32_t f = (int32_t)(((int64_t)fb * g[k] + 16384) >> 15) + (k & 1u ? -x : x);
            sh.line[k][sh.wp[k]] = sh_sat(f);
            sh.wp[k] = sh.wp[k] + 1u == SH_MAXLEN ? 0u : sh.wp[k] + 1u;
        }
        /* out: left and right from different lines */
        v = o[0] - o[2] + o[1] / 2;
        y = o[1] - o[3] + o[0] / 2;
        if (sp == SS_LESLIE) {                   /* the rotors, a small room */
            v = v / 8 + bm;
            y = y / 8 + bs;
        } else if (sp >= SS_BODY) {              /* the body (x 2), the room (bodies: an eighth) */
            v = v * SH_ROOM8[sp] / 8 + bm * 3;
            y = y * SH_ROOM8[sp] / 8 + (bm + bs) * 3 / 2;
        } else if (sp != SS_HALL) {              /* the building's modes, rung by the input and the tail */
            int32_t e = x + s / 4, m = 0;
            for (k = 0; k < SH_NMODES[sp]; k++) {
                const int32_t *c = SH_MODE[sp][k];
                int32_t z = (int32_t)(((int64_t)c[0] * sh.my[k][0] - (int64_t)c[1] * sh.my[k][1] +
                                       (int64_t)c[2] * e) >> 28);
                z = z > 60000 ? 60000 : z < -60000 ? -60000 : z;
                sh.my[k][1] = sh.my[k][0];
                sh.my[k][0] = z;
                m += z;
            }
            v += m / 2;
            y += m / 2;
        }
        {   /* DC blockers on the wet, y = x - x' + R y' (R ~ 1 - 2 pi 23 Hz / FS), states x 256 for precision */
            int32_t xv = v * 256, xy = y * 256;
            sh.dc[0] = xv - sh.dcx[0] + (int32_t)(((int64_t)sh.dc[0] * 32622 + 16384) >> 15);
            sh.dcx[0] = xv;
            sh.dc[1] = xy - sh.dcx[1] + (int32_t)(((int64_t)sh.dc[1] * 32622 + 16384) >> 15);
            sh.dcx[1] = xy;
            v = sh.dc[0] / 420;                  /* (/ 256, x 0.6: three lines summed come out ~1.6x the dry) */
            y = sh.dc[1] / 420;
        }
        l[i] = (int32_t)(((int64_t)in[i] * dry + (int64_t)v * wet) >> 15);
        r[i] = (int32_t)(((int64_t)in[i] * dry + (int64_t)y * wet) >> 15);
        {
            uint32_t a2 = (uint32_t)(v < 0 ? -v : v) + (uint32_t)(y < 0 ? -y : y);
            peak = a2 > peak ? a2 : peak;
        }
    }
    sh_view.env[sh_view.w++ & (SH_ENV - 1u)] = (uint16_t)(peak > 65535u ? 65535u : peak);
}
