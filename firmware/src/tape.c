/* SPDX-License-Identifier: GPL-3.0-only */
/* TAPE: a four-track reel-to-reel recorder (the REC button's page). A loop of tape with four tracks side by side, at
 * a quarter of the rate (11 kHz, 4-bit IMA ADPCM: 7.2 s a track in the room 8-bit took for 3.6), recorded from the synth's own output, played under
 * it. The tape runs at the tempo it was recorded at: turn the BPM (or the MIDI clock changes it) and it runs faster
 * or slower, its pitch going with it, as a tape machine's varispeed does; the loop stays the same number of beats.
 *
 *   LENGTH   (the SPEED menu, KNOB 3) the loop in beats: 1, 2, 4 (a bar), 8, 16 (4 bars: the default); as long as the
 *            tape holds at the tape's tempo (4 bars from ~133 BPM, 2 bars below)
 *   TRACK    (ALGORITHM) the track REC records (1..4)
 *   LEVELS   KNOB 1..4: a blank track's knob arms it, a recorded one's is its level (the mixer's faders)
 *   BPM      (PRESETS) the tempo (the ARP's): the tape's speed and pitch follow it
 *   PLAY     the transport: start (from the loop's start) / stop
 *   REC      record the armed track for one pass of the loop (starting the tape if it stands); again: stop
 *            recording. With PLAY held: erase the armed track
 *
 * The first recording on a blank tape sets its tempo (TEMPO: the samples a beat then); an erased tape (all four
 * tracks) is blank again. MIDI START puts the tape back at the loop's start. Recording replaces what was on the
 * track (an erase head before the record head); the track being recorded is heard live, not from the tape. */

#define TAPE_NT 4u
#define TAPE_N 80000u                            /* cells per track: 7.2 s at 11 kHz (two a byte: the pool's room) */
#define TAPE_DIV 4u                              /* the tape's rate: FS / 4 */
#define TAPE_KB 256u                             /* cells a block: each block's coder state kept (a seek, a junction) */
#define TAPE_NK ((TAPE_N + TAPE_KB - 1u) / TAPE_KB)
static uint8_t tape_buf[TAPE_NT][TAPE_N / 2u] __attribute__((section(".pool")));   /* (0: silence) */
static int16_t tape_kp[TAPE_NT][TAPE_NK];        /* each block's start: the predictor */
static uint8_t tape_ki[TAPE_NT][TAPE_NK];        /* and the step's index */
typedef struct { int32_t p, i; } ima_t;
static const int16_t IMA_STEP[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
    118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
    32767};
static const int8_t IMA_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static int32_t ima_dec(ima_t *s, uint32_t n)
{
    int32_t st = IMA_STEP[s->i], d = st >> 3;
    if (n & 4u) d += st;
    if (n & 2u) d += st >> 1;
    if (n & 1u) d += st >> 2;
    s->p += n & 8u ? -d : d;
    s->p = s->p > 32767 ? 32767 : s->p < -32768 ? -32768 : s->p;
    s->i += IMA_IDX[n & 7u];
    s->i = s->i < 0 ? 0 : s->i > 88 ? 88 : s->i;
    return s->p;
}
static uint32_t ima_enc(ima_t *s, int32_t v)
{
    int32_t st = IMA_STEP[s->i], d = v - s->p;
    uint32_t n = 0;
    if (d < 0) { n = 8u; d = -d; }
    if (d >= st) { n |= 4u; d -= st; }
    if (d >= st >> 1) { n |= 2u; d -= st >> 1; }
    if (d >= st >> 2) n |= 1u;
    ima_dec(s, n);
    return n;
}
static uint32_t tape_nib(uint32_t t, uint32_t c) { return (uint32_t)(tape_buf[t][c >> 1] >> ((c & 1u) * 4u)) & 15u; }
static void tape_wnib(uint32_t t, uint32_t c, uint32_t n)
{
    uint8_t *b = &tape_buf[t][c >> 1];
    *b = (uint8_t)(c & 1u ? (*b & 0x0Fu) | n << 4 : (*b & 0xF0u) | n);
}
static void tape_key(uint32_t t, uint32_t c, const ima_t *s)   /* (c a block's start) */
{
    tape_kp[t][c / TAPE_KB] = (int16_t)s->p;
    tape_ki[t][c / TAPE_KB] = (uint8_t)s->i;
}
static void tape_blank(uint32_t t)
{
    memset(tape_buf[t], 0, sizeof tape_buf[t]);
    memset(tape_kp[t], 0, sizeof tape_kp[t]);
    memset(tape_ki[t], 0, sizeof tape_ki[t]);
}
/* playing: each track's decoder at the head (v0 the cell c, v1 the next, the loop's start after its end) */
static struct { ima_t s; uint32_t c; int32_t v0, v1; } tape_dec[TAPE_NT];
static void tape_dec_next(uint32_t t, uint32_t c, uint32_t len)   /* v1: the cell after c (s decoded through c) */
{
    uint32_t c1 = c + 1u;
    if (c1 >= len) {                                             /* (the loop's start: its first cell, apart) */
        ima_t z;
        z.p = tape_kp[t][0]; z.i = tape_ki[t][0];
        tape_dec[t].v1 = ima_dec(&z, tape_nib(t, 0));
        return;
    }
    if (c1 % TAPE_KB == 0u) {
        tape_dec[t].s.p = tape_kp[t][c1 / TAPE_KB];
        tape_dec[t].s.i = tape_ki[t][c1 / TAPE_KB];
    }
    tape_dec[t].v1 = ima_dec(&tape_dec[t].s, tape_nib(t, c1));
}
static void tape_dec_at(uint32_t t, uint32_t c, uint32_t len)
{
    uint32_t k;
    if (c == tape_dec[t].c)
        return;
    if (c == tape_dec[t].c + 1u && c < len) {                    /* (the head on: one cell) */
        tape_dec[t].v0 = tape_dec[t].v1;
    } else {                                                     /* (a jump: from the block's start) */
        tape_dec[t].s.p = tape_kp[t][c / TAPE_KB];
        tape_dec[t].s.i = tape_ki[t][c / TAPE_KB];
        for (k = c - c % TAPE_KB; k <= c; k++)
            tape_dec[t].v0 = ima_dec(&tape_dec[t].s, tape_nib(t, k));
    }
    tape_dec[t].c = c;
    tape_dec_next(t, c, len);
}
static void tape_dec_lost(void)                  /* (the tape written under the decoders: seek again) */
{
    uint32_t t;
    for (t = 0; t < TAPE_NT; t++)
        tape_dec[t].c = 0xFFFFFFF0u;
}
static struct {
    uint32_t pos;                                /* the head, cells Q16 */
    uint32_t len;                                /* the loop, cells */
    uint32_t spb;                                /* the tape's tempo: output samples a beat when recorded (0: blank) */
    uint32_t inc;                                /* the head's step an output sample, Q16 (speed / TAPE_DIV) */
    uint32_t speed;                              /* Q12: 4096 = the tempo it was recorded at */
    uint32_t rec_left, cell, starts;
    uint8_t rec_wait;                            /* REC: waiting for a block's start (the coder starts clean there) */
    ima_t enc, benc;                             /* the recording's coder; the bounce's */
    uint32_t lock_q12, lock_bpm;                 /* a SPEED chosen: exactly this ratio while the BPM stays lock_bpm */
    int32_t acc, accn, lp, lvl[TAPE_NT];
    int32_t meter[TAPE_NT];                      /* each track's level for the screen, 0..32767 */
    uint8_t playing, rec, has[TAPE_NT];
    volatile uint8_t req;                        /* from the UI: 1 play/stop, 2 rec, 3 erase the armed track, 4 bounce
                                                  * all onto track 1 */
    uint32_t bounce;                             /* BOUNCE: the next cell to mix down (0: none going) */
} tape;

/* SPEED (the TAPE page's second menu): the tape's speed in ratios of the harmonic series (5-limit just intonation),
 * each landing on a step of the chromatic scale: -12 .. +12 semitones, 1/2 (half speed) .. 2/1 (double) */
#define TAPE_NSPD 25
static const uint8_t TAPE_SPD[TAPE_NSPD][2] = {
    {1, 2}, {8, 15}, {9, 16}, {3, 5}, {5, 8}, {2, 3}, {45, 64}, {3, 4}, {4, 5}, {5, 6}, {9, 10}, {15, 16},
    {1, 1}, {16, 15}, {9, 8}, {6, 5}, {5, 4}, {4, 3}, {45, 32}, {3, 2}, {8, 5}, {5, 3}, {9, 5}, {15, 8}, {2, 1},
};
/* the pitch of a speed (Q12) in cents: 1200 log2 (log2 of 1 + f ~ f + 0.3466 f (1 - f): within ~5 cents) */
static int32_t tape_cents(uint32_t q12)
{
    int32_t oct = 0, f;
    if (!q12)
        return 0;
    while (q12 >= 8192u) { q12 >>= 1; oct++; }
    while (q12 < 4096u) { q12 <<= 1; oct--; }
    f = (int32_t)q12 - 4096;                                     /* Q12, 0 .. 1 */
    f = f + (f * (4096 - f) >> 12) * 1420 / 4096;
    return oct * 1200 + f * 1200 / 4096;
}
static int32_t tape_beats(void)                  /* the LENGTH in beats */
{
    static const uint8_t B[5] = {1, 2, 4, 8, 16};
    return B[sys.tape_len % 5u];
}
static void tape_init(void)
{
    uint32_t i;
    for (i = 0; i < TAPE_NT; i++)
        tape_blank(i);
    tape_dec_lost();
}
/* the loop in cells at the tape's tempo: LENGTH beats, as many as fit */
static uint32_t tape_cells(uint32_t spb)
{
    uint32_t b = (uint32_t)tape_beats(), c;
    while ((c = b * spb / TAPE_DIV) > TAPE_N && b > 1u)
        b >>= 1;
    return c > TAPE_N ? TAPE_N : c ? c : 1u;
}
/* the LENGTH the loop has (0 .. 4: 1 beat .. 4 bars): the one chosen, or as long as fits */
static uint32_t tape_spb_now(void);
static uint32_t tape_fit(void)
{
    uint32_t spb = tape.spb ? tape.spb : tape_spb_now(), b = (uint32_t)tape_beats(), k = sys.tape_len % 5u;
    while (b * spb / TAPE_DIV > TAPE_N && b > 1u) {
        b >>= 1;
        k--;
    }
    return k;
}
/* the output samples a beat now (the BPM, or the MIDI clock) */
static uint32_t tape_spb_now(void)
{
    uint32_t bpm = sys.arp_bpm < 40u ? 40u : sys.arp_bpm > 250u ? 250u : sys.arp_bpm;
    if (arp_ext() && arp.tick_len)
        return arp.tick_len * 24u;
    return 2646000u / bpm;
}

/* every 32 samples (keys.c, the audio interrupt): the UI's requests, the speed, MIDI START */
static void tape_block(void)
{
    uint32_t now = tape_spb_now(), t, a = sys.tape_arm % TAPE_NT;
    switch (tape.req) {
    case 1:                                      /* PLAY: start from the loop's start / stop */
        tape.playing = (uint8_t)!tape.playing;
        tape.pos = 0;
        tape.rec = 0;
        break;
    case 2:                                      /* REC: one pass of the armed track / stop recording */
        if (tape.rec)
            tape.rec = 0;
        else {
            if (!tape.spb)                       /* (a blank tape: its tempo now) */
                tape.spb = now;
            tape.len = tape_cells(tape.spb);
            if (!tape.playing) {
                tape.playing = 1;
                tape.pos = 0;
            }
            tape.rec = 1;
            tape.rec_left = tape.len;
            tape.cell = tape.pos >> 16;
            tape.acc = tape.accn = 0;
            tape.has[a] = 1;
            tape.rec_wait = (uint8_t)(tape.cell % TAPE_KB != 0u);   /* (from the next block's start: one whole pass) */
            tape.enc.p = tape.enc.i = 0;
            if (!tape.rec_wait)
                tape_key(a, tape.cell, &tape.enc);
        }
        break;
    case 3:                                      /* erase the armed track (all erased: the tape blank) */
        tape_blank(a);
        tape_dec_lost();
        tape.has[a] = 0;
        tape.rec = 0;
        if (!(tape.has[0] | tape.has[1] | tape.has[2] | tape.has[3]))
            tape.spb = 0;
        break;
    case 4:                                      /* BOUNCE: the four tracks at their faders onto track 1, a slice a
                                                  * block (below), 2 .. 4 blank for new takes */
        if (!tape.bounce && (tape.has[0] | tape.has[1] | tape.has[2] | tape.has[3])) {
            tape.rec = 0;
            tape.bounce = 1;
        }
        break;
    default:
        break;
    }
    tape.req = 0;
    if (tape.bounce) {                           /* (a block of 256 cells a call: the 80000 in ~230 ms) */
        static int32_t mixb[TAPE_KB];
        uint32_t c0 = tape.bounce - 1u, e = c0 + TAPE_KB, c, k;
        int32_t g[TAPE_NT];
        for (k = 0; k < TAPE_NT; k++)            /* (each fader, Q16 of 100: no divide a cell) */
            g[k] = tape.has[k] ? (int32_t)(((sys.tape_lv[k] & 127u) << 16) / 100u) : 0;
        e = e > TAPE_N ? TAPE_N : e;
        memset(mixb, 0, sizeof mixb);
        for (k = 0; k < TAPE_NT; k++)            /* (each track decoded from the block's start) */
            if (g[k]) {
                ima_t d;
                d.p = tape_kp[k][c0 / TAPE_KB]; d.i = tape_ki[k][c0 / TAPE_KB];
                for (c = c0; c < e; c++)
                    mixb[c - c0] += (ima_dec(&d, tape_nib(k, c)) * g[k]) >> 16;
            }
        if (!c0)
            tape.benc.p = tape.benc.i = 0;
        tape_key(0, c0, &tape.benc);
        for (c = c0; c < e; c++) {
            int32_t m = mixb[c - c0];
            tape_wnib(0, c, ima_enc(&tape.benc, m > 32767 ? 32767 : m < -32768 ? -32768 : m));
        }
        for (k = 1; k < TAPE_NT; k++) {
            memset(&tape_buf[k][c0 >> 1], 0, (e - c0 + 1u) >> 1);
            tape_kp[k][c0 / TAPE_KB] = 0;
            tape_ki[k][c0 / TAPE_KB] = 0;
        }
        tape.bounce = e >= TAPE_N ? 0u : e + 1u;
        if (!tape.bounce) {                      /* (done: track 1 holds all, at 100; the others blank, armed 2) */
            tape.has[0] = 1;
            tape.has[1] = tape.has[2] = tape.has[3] = 0;
            sys.tape_lv[0] = 100;
            sys.tape_arm = 1;
            tape_dec_lost();
        }
    }
    if (arp.starts != tape.starts) {             /* MIDI START: the loop's start */
        tape.starts = arp.starts;
        tape.pos = 0;
    }
    if (tape.spb) {
        tape.len = tape_cells(tape.spb);
        if (tape.lock_q12 && !arp_ext() && sys.arp_bpm == tape.lock_bpm)
            tape.speed = tape.lock_q12;          /* (a SPEED's ratio, exact: in tune) */
        else {
            tape.lock_q12 = 0;                   /* (the BPM turned: free varispeed again) */
            tape.speed = tape.spb * 4096u / (now ? now : 1u);   /* (tape.spb < 2^17: no overflow) */
        }
        tape.speed = tape.speed < 1024u ? 1024u : tape.speed > 16384u ? 16384u : tape.speed;
    } else {
        tape.len = tape_cells(now);
        tape.speed = 4096;
    }
    tape.inc = tape.speed * 16u / TAPE_DIV;      /* (Q12 -> Q16, a quarter a sample) */
    if ((tape.pos >> 16) >= tape.len)
        tape.pos = 0;
    for (t = 0; t < TAPE_NT; t++) {
        tape.lvl[t] = (int32_t)(sys.tape_lv[t] & 127u) * 258;   /* Q15 */
        if (!tape.playing)
            tape.meter[t] -= tape.meter[t] >> 4;
    }
}

/* the output (16-bit, l and r): the armed track recorded from it, the tape played under it */
static void tape_run(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i, t, a = sys.tape_arm % TAPE_NT, len = tape.len ? tape.len : 1u;
    int32_t pk[TAPE_NT] = {0, 0, 0, 0};
    if (!tape.playing)
        return;
    for (i = 0; i < n; i++) {
        uint32_t c = tape.pos >> 16, f = (tape.pos >> 4) & 4095u;
        int32_t mix = 0;
        for (t = 0; t < TAPE_NT; t++) {
            int32_t s0, s1;
            if (!tape.lvl[t] || (tape.rec && t == a) || tape.bounce) {   /* (BOUNCE: quiet meanwhile) */
                if (tape.rec && t == a)
                    tape_dec[t].c = 0xFFFFFFF0u;                 /* (written: seek after) */
                continue;
            }
            tape_dec_at(t, c, len);
            s0 = tape_dec[t].v0;
            s1 = tape_dec[t].v1;
            s0 = (s0 + (((s1 - s0) * (int32_t)f) >> 12)) * tape.lvl[t] >> 15;
            mix += s0;
            s0 = s0 < 0 ? -s0 : s0;
            pk[t] = s0 > pk[t] ? s0 : pk[t];
        }
        tape.lp += (mix - tape.lp) * 15 >> 5;    /* (the tape's highs rolling off: ~5 kHz) */
        if (tape.rec) {                          /* the input averaged over the cell, written when it is passed */
            int32_t in = (l[i] + r[i]) / 2;
            tape.acc += in;
            in = in < 0 ? -in : in;
            pk[a] = in > pk[a] ? in : pk[a];
            tape.accn++;
            if (c != tape.cell && tape.rec_wait) {   /* (waiting: a block's start reached, the pass begins) */
                tape.acc = tape.accn = 0;
                tape.cell = c;
                if (c % TAPE_KB == 0u) {
                    tape.rec_wait = 0;
                    tape_key(a, c, &tape.enc);
                }
            } else if (c != tape.cell) {
                tape_wnib(a, tape.cell, ima_enc(&tape.enc, tape.accn ? tape.acc / tape.accn : 0));
                tape.acc = tape.accn = 0;
                tape.cell = c;
                if (!--tape.rec_left)
                    tape.rec = 0;
                else if (c % TAPE_KB == 0u)
                    tape_key(a, c, &tape.enc);   /* (the coder's state where the block starts) */
            }
        }
        l[i] += tape.lp;
        r[i] += tape.lp;
        tape.pos += tape.inc;
        if ((tape.pos >> 16) >= len)
            tape.pos -= len << 16;
    }
    for (t = 0; t < TAPE_NT; t++)
        tape.meter[t] = pk[t] > tape.meter[t] ? pk[t] : tape.meter[t] - (tape.meter[t] >> 5);
}
