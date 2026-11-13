/* SPDX-License-Identifier: GPL-3.0-only */
/* FREE EG (the REC button; after the AN1x's Free EG): a knob's movement recorded and played back onto a parameter.
 *
 *   FREE EG page  KNOB 1 the knob you record (VALUE), KNOB 2 LENGTH: NOTE (each note starts it again, it plays once:
 *                 as long as you recorded, up to ~6 s), 1..8 STEPS (sixteenths), 1..4 BARS (both at the tempo:
 *                 the ARP's BPM, or MIDI clock while it comes in: then START starts it again too), PHRASE
 *                 (KARMA's phrase, the SEQ page: as long as it, from its start with it; a bar without), KNOB 3 DEPTH
 *                 (0: the parameter as it was .. 127: the recording itself), KNOB 4 the TARGET parameter;
 *                 SELECT PLAY on / off; REC (on the page) records: from the start of a cycle, for one length
 *                 (NOTE: until REC again, or full), the target following the knob while it records
 *
 * The recording: 128 points over the length. Playing, the target is driven (base + (curve - base) x DEPTH, base its
 * value when PLAY went on, given back when it goes off or the target changes): its own knob is taken over meanwhile.
 * The targets include KARMA's four realtime controls (KARMA K1..K4): its DENSITY or REPEAT moved over the phrase.
 *
 *   feg_block(n)   keys.c, the audio ISR, after the arpeggiator: once a block */
#define FEG_N 128u
#define FEG_NOTE_STEP 2048u                      /* NOTE: samples a point (128 points ~ 5.9 s) */
#define FEG_NLEN 33
#define FEG_PHRASE 13u                           /* PHRASE: KARMA's phrase (karma.c), each trigger from its start */
#define FEG_LFO0 14u                             /* LFO: the recording a free-running loop, 0.1 .. 32 Hz */
#define FEG_AUD0 23u                             /* AM, FM: a loop at audio rate, x 1/2 .. 4 the note: the voices'
                                                  * level (AM) or pitch (FM) moved by it, every sample (sidebands) */
#define FEG_FM0 28u
static const char *const FEG_LEN_NAME[FEG_NLEN] = {"NOTE", "1 STEP", "2 STEPS", "3 STEPS", "4 STEPS", "5 STEPS",
                                                   "6 STEPS", "7 STEPS", "8 STEPS", "1 BAR", "2 BARS", "3 BARS",
                                                   "4 BARS", "PHRASE", "LFO 0.1 HZ", "LFO 0.25 HZ", "LFO 0.5 HZ",
                                                   "LFO 1 HZ", "LFO 2 HZ", "LFO 4 HZ", "LFO 8 HZ", "LFO 16 HZ",
                                                   "LFO 32 HZ", "AM X1/2", "AM X1", "AM X2", "AM X3", "AM X4",
                                                   "FM X1/2", "FM X1", "FM X2", "FM X3", "FM X4"};
static const uint16_t FEG_LFO_CHZ[9] = {10, 25, 50, 100, 200, 400, 800, 1600, 3200};   /* centihertz */
static const uint8_t FEG_AUD_R4[5] = {8, 16, 32, 48, 64};   /* the note x this / 16 */
static uint32_t feg_ticks(uint32_t len)          /* MIDI clocks a cycle (24 a quarter, a step a sixteenth) */
{
    return len <= 8u ? len * 6u : len <= 12u ? (len - 8u) * 96u : 96u;   /* (PHRASE without KARMA: a bar) */
}

/* the parameters it can drive (all 0..127) */
#define FEG_NT 30
static const char *const FEG_T_NAME[FEG_NT] = {
    "CHAOS", "SHAPE", "OUTPUT", "RING", "NOISE X", "NOISE Y", "NOISE LEVEL", "MOD ENV HEIGHT", "FX STRUCTURE",
    "FX BRIGHTNESS", "FX DAMPING", "FX POSITION", "FX MIX", "SHIMMER SIZE", "SHIMMER DECAY", "SHIMMER", "SHIMMER MIX", "SHIMMER TONE", "BASIS", "ORBIT", "ARP GATE", "ARP SWING", "ENV DECAY",
    "KARMA K1", "KARMA K2", "KARMA K3", "KARMA K4", "SCREAM", "VIBRATO", "RING MOD",
};
static volatile uint8_t *feg_field(uint32_t t)
{
    switch (t % FEG_NT) {
    case 0: return &sys.chaos;
    case 1: return &sys.shape;
    case 2: return &sys.output;
    case 3: return &sys.ring;
    case 4: return &sys.np_x;
    case 5: return &sys.np_y;
    case 6: return &sys.np_level;
    case 7: return &sys.menv_height;
    case 8: return &sys.fx_struct;
    case 9: return &sys.fx_bright;
    case 10: return &sys.fx_damp;
    case 11: return &sys.fx_pos;
    case 12: return &sys.fx_mix;
    case 13: return &sys.sh_size;
    case 14: return &sys.sh_decay;
    case 15: return &sys.sh_shimmer;
    case 16: return &sys.sh_mix;
    case 17: return &sys.sh_tone;
    case 18: return &sys.basis;
    case 19: return &sys.orbit;
    case 20: return &sys.arp_gate;
    case 21: return &sys.arp_swing;
    case 23: return &sys.k_m[0];                 /* KARMA's realtime controls (what each does: its GE) */
    case 24: return &sys.k_m[1];
    case 25: return &sys.k_m[2];
    case 26: return &sys.k_m[3];
    case 27: return &sys.scream;
    case 28: return &sys.mw_vib;                 /* MOD FX's three (fx_mod.c): how much of each, on its own */
    case 29: return &sys.mw_ringm;
    default: return &sys.decay;
    }
}

/* TWO of them (the LFO button: FREE EG 1, again FREE EG 2, again HOME): two modulation sources, each its own
 * recording, length, depth and target. The one the page shows is in sys (sys.feg_...) and is `feg`; the other's
 * settings in feg_po (feg_edit_to swaps them, the IRQs off). Both on the same parameter: their moves add up (the
 * second takes the first's base). FREE EG 1 is the presets' (their contours); AM / FM: the first that wants it */
typedef struct {
    uint8_t curve[FEG_N];                        /* the recording (sys.feg_has: there is one) */
    uint32_t t;                                  /* samples into the cycle (its own clock; NOTE: since the note) */
    uint32_t clk0, starts;                       /* MIDI clock: the clock the cycle counts from; STARTs seen */
    uint32_t notes;                              /* note ons seen (NOTE: each starts it again) */
    uint32_t ktrig;                              /* KARMA's phrase starts seen (PHRASE: each starts it again) */
    uint32_t last_idx;                           /* recording: the last point written */
    uint8_t rec;                                 /* recording */
    uint8_t playing, target, base;               /* driving: which parameter, its value before */
    volatile uint8_t pos;                        /* the play head, 0..127 (the screen); 255: none */
    int16_t off;                                 /* (this block: how far it moves its parameter from the base) */
} feg_st_t;
typedef struct { uint8_t val, len, depth, target, on, has, npts; } feg_par_t;
static feg_st_t fegs[2] = {{.pos = 255}, {.pos = 255}};
static volatile uint8_t feg_ed;                  /* the one in sys, on the page: 0 FREE EG 1, 1 FREE EG 2 */
static feg_par_t feg_po = {64, 9, 127, 0, 0, 0, 0};   /* the other's settings */
static volatile uint8_t feg_pause;               /* (ui.c, a preset loading: both give their parameters back) */
#define feg (fegs[feg_ed])

static int feg_owns(volatile uint8_t *f)        /* (karma.c: its envelope leaves this one alone) */
{
    uint32_t i;
    for (i = 0; i < 2u; i++)
        if (fegs[i].playing && feg_field(fegs[i].target) == f)
            return 1;
    return feg.rec && feg_field(sys.feg_target) == f;
}

static void feg_release_(feg_st_t *F)           /* the parameter given back (the other on it too: it keeps it) */
{
    feg_st_t *O = F == &fegs[0] ? &fegs[1] : &fegs[0];
    if (F->playing && !(O->playing && O->target == F->target))
        *feg_field(F->target) = F->base;
    F->playing = 0;
}
static void feg_release(void) { feg_release_(&feg); }

static void feg_record(int on)                   /* REC: start (from the cycle's start) or stop */
{
    if (on) {
        feg_release();
        feg.rec = 1;
        feg.t = 0;
        feg.clk0 = arp.clk;
        feg.last_idx = 0xFFFFFFFFu;
        if (!sys.feg_len)
            sys.feg_npts = 0;
    } else if (feg.rec) {
        feg.rec = 0;
        if (sys.feg_len && feg.last_idx != 0xFFFFFFFFu)   /* (stopped early: the rest holds the last value) */
            for (uint32_t i = feg.last_idx + 1u; i < FEG_N; i++)
                feg.curve[i] = feg.curve[feg.last_idx];
        sys.feg_has = 1;
    }
}

static void feg_par_get(feg_par_t *P)            /* (the one in sys) */
{
    P->val = sys.feg_val;
    P->len = sys.feg_len;
    P->depth = sys.feg_depth;
    P->target = sys.feg_target;
    P->on = sys.feg_on;
    P->has = sys.feg_has;
    P->npts = sys.feg_npts;
}
static void feg_par_set(const feg_par_t *P)
{
    sys.feg_val = P->val;
    sys.feg_len = P->len;
    sys.feg_depth = P->depth;
    sys.feg_target = P->target;
    sys.feg_on = P->on;
    sys.feg_has = P->has;
    sys.feg_npts = P->npts;
}
static volatile uint8_t feg_req;                 /* the main loop's REC: 1 start, 2 stop (done here, in the ISR) */
static uint8_t feg_am_by;                        /* (this block: AM / FM taken, by 1 + which) */
/* one of them over this block: its offset (F->off, F->playing), or AUDIO's feg_am. P: its settings (P->npts, P->has:
 * the recording's, written back) */
static void feg_run(feg_st_t *F, feg_par_t *P, uint32_t i, uint32_t n, uint32_t note_ons)
{
    uint32_t len = P->len % FEG_NLEN, L, p, idx, f8;
    feg_st_t *O = &fegs[i ^ 1u];
    F->off = 0;
    if (note_ons != F->notes) {                  /* a note: NOTE starts again */
        F->notes = note_ons;
        if (!len && !F->rec)
            F->t = 0;
    }
    if (arp.starts != F->starts) {               /* MIDI START: the cycle from its start */
        F->starts = arp.starts;
        F->clk0 = arp.clk;
        F->t = 0;
    }
    if (kar.triggers != F->ktrig) {              /* KARMA's phrase from its start: PHRASE too */
        F->ktrig = kar.triggers;
        if (len == FEG_PHRASE) {
            if (F->rec && F->last_idx != 0xFFFFFFFFu && F->last_idx + 8u >= FEG_N)
                feg_record(0);                   /* (a whole phrase recorded: only the one in sys records) */
            else if (F->rec)
                F->last_idx = 0xFFFFFFFFu;       /* (recording: from the phrase's start, in step with it) */
            F->t = 0;
        }
    }
    if (len == FEG_PHRASE) {                     /* PHRASE: KARMA's (while it plays; else a bar at the tempo) */
        uint32_t bpm = sys.arp_bpm < 40u ? 40u : sys.arp_bpm > 250u ? 250u : sys.arp_bpm;
        L = kar.running && kar.phrase_len ? kar.phrase_len : 2646000u / bpm / 24u * 96u;
        if (F->t >= L)
            F->t = F->rec ? L - 1u : F->t % L;
        p = F->t;
        if (F->rec && p + n >= L) {
            F->t = L - 1u;
        } else
            F->t = F->t + n < L || F->rec ? F->t + n : (F->t + n) % L;
    } else if (!len) {                           /* NOTE: points every FEG_NOTE_STEP samples */
        uint32_t pts = F->rec ? FEG_N : P->npts;
        L = (pts ? pts : 1u) * FEG_NOTE_STEP;
        p = F->t < L ? F->t : L - 1u;
        F->t = F->t < 0x7FFFFFFFu ? F->t + n : F->t;
    } else if (len >= FEG_LFO0) {                /* LFO (and AUDIO, recorded at 1 Hz): its own free-running loop */
        L = len >= FEG_AUD0 ? FS : FS * 100u / FEG_LFO_CHZ[len - FEG_LFO0];
        if (F->rec && F->t + n >= L) {
            F->t = L - 1u;
            p = F->t;
        } else
            p = F->t % L;
        F->t = (F->t + n) % L;
    } else if (arp_ext()) {                      /* MIDI clock: the clocks since the cycle's start */
        uint32_t tk = feg_ticks(len), c = (arp.clk - F->clk0) % tk;
        uint32_t tl = arp.tick_len ? arp.tick_len : 919u;
        L = tk * tl;
        p = c * tl + (arp.tick_t < tl ? arp.tick_t : tl - 1u);
        if (F->rec && arp.clk - F->clk0 >= tk)   /* one cycle recorded */
            feg_record(0);
    } else {                                     /* its own: the ARP's BPM */
        uint32_t bpm = sys.arp_bpm < 40u ? 40u : sys.arp_bpm > 250u ? 250u : sys.arp_bpm;
        uint32_t spt = 2646000u / bpm / 24u;     /* samples a MIDI clock */
        L = spt * feg_ticks(len);
        if (F->rec && F->t + n >= L) {
            F->t = L - 1u;
            p = F->t;
        } else
            p = F->t % L;
        F->t = (F->t + n) % L;
    }
    if (!len) {                                  /* NOTE: a point every FEG_NOTE_STEP samples */
        idx = p / FEG_NOTE_STEP;
        f8 = (p % FEG_NOTE_STEP) * 256u / FEG_NOTE_STEP;
    } else {                                     /* the cycle: 128 points over its length */
        idx = p * FEG_N / L;                     /* (32 bits: p < L < 2^25, x 128 < 2^32) */
        f8 = (p * FEG_N % L) * 256u / L;
    }
    idx = idx >= FEG_N ? FEG_N - 1u : idx;
    if (F->rec) {                                /* the knob into the points passed since the last block */
        uint32_t k = F->last_idx == 0xFFFFFFFFu ? 0u : F->last_idx + 1u;
        uint8_t v = (uint8_t)(P->val & 127u);
        for (; k <= idx && k < FEG_N; k++)
            F->curve[k] = v;
        F->last_idx = idx;
        *feg_field(P->target) = v;               /* (heard while recorded) */
        F->pos = (uint8_t)idx;
        if (!len) {
            P->npts = (uint8_t)(idx + 1u > 127u ? 127u : idx + 1u);
            sys.feg_npts = P->npts;
            if (idx + 1u >= FEG_N)               /* full */
                feg_record(0);
        } else if ((len == FEG_PHRASE || len >= FEG_LFO0 || !arp_ext()) && p + n >= L)
            feg_record(0);
        return;
    }
    if (len >= FEG_AUD0 && P->on && P->has && !feg_am_by) {   /* AUDIO: the voices take it, every sample */
        feg_release_(F);
        feg_am.r16 = FEG_AUD_R4[(len - FEG_AUD0) % 5u];
        feg_am.fm = (uint8_t)(len >= FEG_FM0);
        feg_am.depth = (uint8_t)(P->depth & 127u);
        feg_am.curve = F->curve;
        {   /* (its mean: FM moves the pitch about the note, whatever the drawing's middle) */
            uint32_t q, sm = 0;
            for (q = 0; q < FEG_N; q++)
                sm += F->curve[q];
            feg_am.mid = (uint8_t)((sm + FEG_N / 2u) / FEG_N);
        }
        feg_am.on = 1;
        feg_am_by = (uint8_t)(i + 1u);
        F->pos = (uint8_t)idx;
        return;
    }
    if (!P->on || !P->has || len >= FEG_AUD0) {  /* (AUDIO taken by the other: still) */
        feg_release_(F);
        F->pos = 255;
        return;
    }
    if (!F->playing || F->target != P->target % FEG_NT) {   /* (on, or another target: take it over) */
        feg_release_(F);
        F->target = (uint8_t)(P->target % FEG_NT);
        F->base = O->playing && O->target == F->target ? O->base : *feg_field(F->target);   /* (the other on it:
                                                                                               * its base) */
        F->playing = 1;
    }
    {   /* between two points (Q8), then DEPTH around the base */
        int32_t a = F->curve[idx], b = F->curve[idx + 1u < FEG_N ? idx + 1u : idx], v;
        if (!len && idx + 1u >= P->npts)
            b = a;                               /* (NOTE: the last point holds) */
        v = a + (((b - a) * (int32_t)f8) >> 8);
        {   /* (the mod wheel on the same parameter: it sets how deep, 0 still .. DEPTH x the EDIT page's DEPTH) */
            int32_t d = (int32_t)(P->depth & 127u), md = (int32_t)(sys.mw_depth & 127u);
            if (md && sys.mw_seen && sys.mw_target % FEG_NT == F->target)
                d = d * (int32_t)(sys.mw & 127u) * md / (127 * 127);
            F->off = (int16_t)((v - F->base) * d / 127);
        }
        F->pos = (uint8_t)idx;
    }
}

static void feg_block(uint32_t n, uint32_t note_ons)
{
    uint32_t i;
    if (feg_req) {
        feg_record(feg_req == 1u);
        feg_req = 0;
    }
    if (feg_pause) {                             /* (a preset loading: given back, taken again after it) */
        feg_release_(&fegs[0]);
        feg_release_(&fegs[1]);
        return;
    }
    feg_am_by = 0;
    for (i = 0; i < 2u; i++) {
        feg_par_t P;
        uint8_t np, hs;
        if (i == feg_ed)
            feg_par_get(&P);
        else
            P = feg_po;
        np = P.npts;
        hs = P.has;
        feg_run(&fegs[i], &P, i, n, note_ons);
        if (i != feg_ed) {                       /* (its recording's length: written back, if it changed) */
            if (P.npts != np)
                feg_po.npts = P.npts;
            if (P.has != hs)
                feg_po.has = P.has;
        }
    }
    if (!feg_am_by)
        feg_am.on = 0;
    for (i = 0; i < 2u; i++) {                   /* the parameters: base + the moves (both on one: added) */
        feg_st_t *F = &fegs[i], *O = &fegs[i ^ 1u];
        int32_t v;
        if (!F->playing || (i && O->playing && O->target == F->target))
            continue;
        v = F->base + F->off + (O->playing && O->target == F->target ? O->off : 0);
        *feg_field(F->target) = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    }
}

/* the mod wheel (MIDI CC 1, the EDIT page): it moves a parameter of FREE EG's list up from where it is, DEPTH how far
 * (127: the wheel up takes it to 127); the parameter given back when the wheel is down. FREE EG playing the same one: the wheel sets FREE EG's depth instead (feg_block) */
static struct { uint8_t on, target, base; } mwd;
static void mw_block(void)
{
    uint32_t m = sys.mw & 127u, d = sys.mw_depth & 127u, t = sys.mw_target % FEG_NT;
    volatile uint8_t *f;
    if (mwd.on && (mwd.target != t || !m || !d)) {   /* (down, off, or another target: given back) */
        *feg_field(mwd.target) = mwd.base;
        mwd.on = 0;
    }
    if (!m || !d)
        return;
    f = feg_field(t);
    if ((fegs[0].playing && fegs[0].target == t) || (fegs[1].playing && fegs[1].target == t))
        return;
    if (!mwd.on) {
        mwd.on = 1;
        mwd.target = (uint8_t)t;
        mwd.base = *f;
    }
    *f = (uint8_t)(mwd.base + (127u - mwd.base) * m * d / (127u * 127u));
}
