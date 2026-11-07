/* SPDX-License-Identifier: GPL-3.0-only */
/* The arpeggiator (ARP), in the audio ISR with the keys (keys.c): the notes held (keys and MIDI in) are not played
 * but collected; at each step the pattern picks which of them sounds, in which octave, how loud and how long.
 *
 * The clock: MIDI clock (USB or TRS, 24 a quarter note) while it comes in (START restarts the pattern); without it
 * (none for 0.5 s) the arpeggiator runs on its own at BPM (40 .. 250, the ARP page).
 *
 *   ARP page   KNOB 1 BPM (EXT while MIDI clock comes in: its tempo shown), KNOB 2 SUBDIVIDE (the AN1x's ten:
 *              3/8 .. 1/32, dotted and triplets), KNOB 3 GATE, KNOB 4 SWING (50 .. 83 %, as the AN1x);
 *              ALGORITHM the pattern; SELECT LATCH (as KARMA's)
 *
 * The patterns: the Yamaha AN1x's thirty Arpeggiator Types (1-15 orders, 16-30 step patterns) and two of the DJX's,
 * see ARP_STEPS.
 *
 *   arp_add(note, vel), arp_remove(note)   keys.c: a note held, let go (with ARP on)
 *   arp_block(n)                          keys.c: the clock and the steps, once a block (n samples)
 *   arp_midi_rt(status)                   keys.c: MIDI realtime in: F8 clock, FA start, FB continue, FC stop */
/* the notes it plays (and KARMA's) go here: the synth, and USB MIDI out on the TX channel (if on: the GLO page) */
static uint8_t gen_ch[128], gen_out[128];
static uint8_t gen_q[128];                       /* each note made: the one it sounds as (+1; LOGIC's NOTE QUANT) */
static uint32_t gen_n;                           /* notes made (the tests count them) */
static uint32_t logic_q(uint32_t note);          /* (LOGIC's note quantize, below) */
static void gen_on(uint32_t note, uint32_t vel)
{
    uint32_t q = logic_q(note);
    gen_n++;
    if (gen_q[note] && gen_q[note] - 1u != q)    /* (moved since: the one it sounded as ends) */
        synth_note_off(gen_q[note] - 1u);
    gen_q[note] = (uint8_t)(q + 1u);
    note = q;
    synth_note_on(note, vel);
    if (gen_out[note])                           /* (the same note again: its old one ends first) */
        midi_out_event(0x08u | (0x80u | gen_ch[note]) << 8 | note << 16);
    gen_out[note] = 0;
    if (sys.midi_usb & 2u) {
        gen_ch[note] = (uint8_t)(sys.midi_tx & 15u);
        gen_out[note] = 1;
        midi_out_event(0x09u | (0x90u | gen_ch[note]) << 8 | note << 16 | vel << 24);
    }
}
static void gen_off(uint32_t note)
{
    if (gen_q[note & 127u]) {                    /* (what it sounds as) */
        uint32_t q = gen_q[note & 127u] - 1u;
        gen_q[note & 127u] = 0;
        note = q;
    }
    synth_note_off(note);
    if (gen_out[note]) {
        gen_out[note] = 0;
        midi_out_event(0x08u | (0x80u | gen_ch[note]) << 8 | note << 16);
    }
}

#define ARP_MAXN 16                              /* notes held at most */
#define ARP_EXT_TIMEOUT 22050u                   /* samples without a clock: back on its own tempo (0.5 s) */

/* the note values (the AN1x's ten Arpeggio Subdivide settings): MIDI clocks a step (24 a quarter note) */
#define ARP_NDIV 10
static const uint8_t ARP_TPS[ARP_NDIV] = {36, 24, 18, 16, 12, 9, 8, 6, 4, 3};
static const char *const ARP_DIV_NAME[ARP_NDIV] = {"3/8", "1/4", "3/16", "1/6", "1/8", "3/32", "1/12", "1/16",
                                                   "1/24", "1/32"};

/* the patterns: the AN1x's thirty Arpeggiator Types (its Data List, p. 5: names and one-line comments) and two more
 * of the DJX's (its manual, p. 38). Types 1-15 are orders, worked out (UpDwB: the ends twice, as the DJX's
 * Up & Down B, "slightly different" from UpDwA); 16-30 are step patterns: the comments give their character only, the
 * steps are this firmware's reading of them (TekkEcho's "moving filter" as a velocity sweep: the gate opens wider).
 * A step: sel the held note by place (0 the lowest; past the highest: on into the next octave), or ARP_REST,
 * ARP_TIE (the last goes on), ARP_ALL (the chord), ARP_UPPER (the chord but the lowest: DAHouse's right hand),
 * ARP_PREV (an echo: the last note again); oct octaves up (-1 .. 3); acc 0 normal, 1 accent, 2 soft, 3 a ghost.
 * After the Roland MC-303's Arpeggio Styles (its manual, p. 38-41, 98: names and one-line descriptions; the steps
 * are this firmware's reading of them, as for the AN1x): ARP_TOP (the highest note held: SHAMISEN, 3 FINGER's
 * melody), ARP_GLS (GLISSANDO: semitone by semitone between the lowest and the highest held, up and down),
 * ARP_STRUP / ARP_STRDN (the chord strummed up from the lowest / down from the highest, a few ms a string: the
 * guitar styles), acc AM (muted: short, a little softer: the MC-303's MUTE beat patterns, slap bass, the guitars'
 * chops, reggae's skank) */
enum { ARP_REST = 100, ARP_TIE, ARP_ALL, ARP_UPPER, ARP_PREV, ARP_TOP, ARP_GLS, ARP_STRUP, ARP_STRDN };
enum { AN, AA, AS, AG, AM };                     /* (accents; AM muted) */
typedef struct { uint8_t sel; int8_t oct; uint8_t acc; } arp_step_t;
typedef struct { const char *name; uint8_t n; arp_step_t s[32]; } arp_pat_t;
#define R_ {ARP_REST, 0, AN}
#define C_(a) {ARP_ALL, 0, a}
#define U_(a) {ARP_UPPER, 0, a}
#define E_(a) {ARP_PREV, 0, a}
static const arp_pat_t ARP_STEPS[] = {
    {"TECHNO-A", 16, {{0, -1, AA}, {0, 0, AS}, {1, 0, AN}, {0, 0, AS}, {0, -1, AN}, {0, 0, AS}, {2, 0, AN}, {0, 0, AS},
                      {0, -1, AA}, {0, 0, AS}, {1, 0, AN}, {0, 0, AS}, {0, -1, AN}, {0, 0, AS}, {3, 0, AA}, {2, 0, AS}}},
    {"TECHNO-B", 16, {{0, 0, AA}, R_, {0, 0, AN}, {1, 0, AN}, R_, {2, 0, AA}, {0, 0, AN}, R_,
                      {0, 1, AN}, R_, {1, 0, AN}, {2, 0, AN}, R_, {0, 0, AA}, R_, {2, 0, AS}}},
    {"TECHNO-C", 16, {{0, 0, AA}, {1, 0, AN}, {2, 0, AN}, {3, 0, AN}, {0, 1, AA}, {1, 1, AN}, {2, 1, AN}, {3, 1, AN},
                      {3, 1, AA}, {2, 1, AN}, {1, 1, AN}, {0, 1, AN}, {3, 0, AA}, {2, 0, AN}, {1, 0, AN}, {0, 0, AS}}},
    {"TECHNO-D", 16, {{0, 0, AA}, {0, 0, AS}, {0, 0, AG}, {0, 0, AS}, {1, 0, AA}, {0, 0, AS}, {0, 0, AG}, {0, 0, AS},
                      {0, 0, AA}, {0, 0, AS}, {2, 0, AN}, {0, 0, AS}, {1, 0, AA}, {0, 0, AG}, {0, 0, AS}, {0, 0, AS}}},
    {"DA HOUSE", 16, {{0, -1, AA}, R_, U_(AN), R_, {0, -1, AN}, {0, -1, AS}, U_(AA), R_,
                      {0, -1, AA}, R_, U_(AN), {0, 0, AS}, R_, {0, -1, AN}, U_(AA), R_}},
    {"SYNCOPA A", 16, {{0, 0, AA}, R_, R_, {1, 0, AN}, R_, {2, 0, AN}, R_, {0, 1, AA},
                       R_, R_, {1, 0, AN}, R_, {2, 0, AA}, R_, {1, 0, AS}, R_}},
    {"SYNCOPA B", 16, {{0, -1, AA}, R_, {0, 2, AN}, {1, 0, AN}, R_, {2, 1, AA}, {0, -1, AN}, R_,
                         {1, 2, AN}, R_, {0, 0, AA}, {2, 2, AS}, R_, {1, -1, AN}, {0, 1, AA}, R_}},
    {"SYNCO ECHO", 16, {{0, 0, AA}, R_, E_(AS), {1, 0, AN}, R_, E_(AG), {2, 0, AA}, R_,
                        E_(AS), {0, 1, AN}, R_, E_(AG), {1, 0, AA}, E_(AS), R_, E_(AG)}},
    {"TEKK ECHO A", 16, {{0, 0, AG}, E_(AG), {1, 0, AS}, E_(AG), {2, 0, AS}, E_(AS), {0, 1, AN}, E_(AS),
                         {1, 1, AN}, E_(AS), {2, 1, AA}, E_(AN), {0, 2, AA}, E_(AN), {2, 1, AA}, E_(AS)}},
    {"TEKK ECHO B", 16, {{0, 2, AA}, E_(AN), {2, 1, AA}, E_(AN), {1, 1, AA}, E_(AS), {0, 1, AN}, E_(AS),
                         {2, 0, AN}, E_(AS), {1, 0, AS}, E_(AG), {0, 0, AS}, E_(AG), {1, 0, AG}, E_(AG)}},
    {"PULSE LINE", 32, {{0, 0, AA}, {0, 0, AS}, {0, 2, AN}, {0, 0, AS}, {0, 0, AA}, {0, 3, AG}, {0, 0, AS}, {0, 1, AN},
                   {0, 0, AA}, {0, 0, AS}, {0, 2, AN}, {0, 0, AS}, {0, 0, AA}, {0, 1, AN}, {0, 3, AG}, {0, 0, AS},
                   {1, 0, AA}, {1, 0, AS}, {1, 2, AN}, {1, 0, AS}, {1, 0, AA}, {1, 3, AG}, {1, 0, AS}, {1, 1, AN},
                   {0, 0, AA}, {0, 0, AS}, {0, 2, AN}, {0, 3, AG}, {0, 0, AA}, {0, 1, AN}, {0, 2, AS}, {0, 3, AG}}},
    {"BASSLINE A", 16, {{0, -1, AA}, {0, -1, AS}, R_, {0, 0, AN}, {0, -1, AN}, R_, {1, -1, AA}, {0, -1, AS},
                      {0, -1, AA}, R_, {0, 0, AN}, {0, -1, AS}, R_, {1, -1, AN}, {0, 0, AA}, {0, -1, AS}}},
    {"BASSLINE B", 16, {{0, -1, AA}, {0, -1, AG}, {0, 0, AS}, {0, -1, AA}, R_, {0, -1, AS}, {1, -1, AA}, {0, -1, AG},
                        {0, -1, AA}, {0, 0, AS}, R_, {0, -1, AN}, {1, -1, AA}, {0, -1, AG}, {0, 0, AA}, {0, -1, AS}}},
    {"BASSLINE C", 16, {{0, -1, AA}, R_, {1, -1, AN}, R_, {2, -1, AN}, R_, {1, -1, AN}, R_,
                        {0, -1, AA}, R_, {0, 0, AN}, R_, {2, -1, AN}, R_, {1, -1, AS}, {0, -1, AN}}},
    {"BASSLINE D", 16, {{0, -1, AA}, {0, 0, AS}, R_, {0, -1, AN}, {0, 0, AS}, R_, {0, -1, AA}, {1, 0, AN},
                        {0, -1, AN}, {0, 0, AS}, R_, {0, -1, AA}, {0, 0, AN}, {1, -1, AS}, {0, -1, AA}, {0, 0, AS}}},
    {"ECHO (DJX)", 32, {{0, 0, AA}, E_(AS), R_, E_(AG), {1, 0, AA}, E_(AS), R_, E_(AG), {2, 0, AA}, E_(AS), R_, E_(AG),
                  {3, 0, AA}, E_(AS), R_, E_(AG), {4, 0, AA}, E_(AS), R_, E_(AG), {3, 0, AA}, E_(AS), R_, E_(AG),
                  {2, 0, AA}, E_(AS), R_, E_(AG), {1, 0, AA}, E_(AS), R_, E_(AG)}},
    {"SWEEP (DJX)", 32, {{0, 0, AA}, {1, 0, AN}, {2, 0, AN}, {3, 0, AN}, {4, 0, AA}, {5, 0, AN}, {6, 0, AN}, {7, 0, AN},
                   {8, 0, AA}, {9, 0, AN}, {10, 0, AN}, {11, 0, AN}, {12, 0, AA}, {13, 0, AN}, {14, 0, AN}, {15, 0, AA},
                   {15, 0, AA}, {14, 0, AN}, {13, 0, AN}, {12, 0, AN}, {11, 0, AA}, {10, 0, AN}, {9, 0, AN}, {8, 0, AN},
                   {7, 0, AA}, {6, 0, AN}, {5, 0, AN}, {4, 0, AN}, {3, 0, AA}, {2, 0, AN}, {1, 0, AN}, {0, 0, AS}}},
    /* the MC-303's styles (see above) */
    {"GLISSANDO", 1, {{ARP_GLS, 0, AN}}},
    {"SYN BASS", 16, {{0, -1, AA}, {0, 0, AS}, {0, -1, AM}, {1, -1, AN}, {0, -1, AA}, {0, 0, AM}, {2, -1, AN}, {0, -1, AM},
                      {0, -1, AA}, {0, 0, AS}, {0, -1, AM}, {1, -1, AN}, {0, 0, AA}, {0, -1, AM}, {2, -1, AN}, {1, -1, AS}}},
    {"SLAP BASS", 16, {{0, -1, AA}, {0, -1, AM}, {0, 0, AA}, {0, -1, AM}, R_, {0, -1, AG}, {0, 0, AA}, {0, -1, AM},
                       {0, -1, AA}, R_, {0, 0, AA}, {0, -1, AM}, {0, -1, AG}, {1, -1, AN}, {0, 0, AA}, {0, -1, AM}}},
    {"WALK BASS", 8, {{0, -1, AA}, {1, -1, AN}, {2, -1, AN}, {1, -1, AS}, {0, -1, AA}, {2, -1, AN}, {0, 0, AN},
                      {2, -1, AS}}},
    {"RHYTHM GTR", 16, {C_(AA), C_(AM), R_, C_(AM), C_(AN), R_, C_(AM), C_(AA), R_, C_(AM), C_(AN), C_(AM),
                        R_, C_(AM), C_(AA), C_(AM)}},
    {"STRUM GTR", 8, {{ARP_STRDN, 0, AA}, R_, {ARP_STRDN, 0, AN}, {ARP_STRUP, 0, AS}, R_, {ARP_STRUP, 0, AS},
                      {ARP_STRDN, 0, AN}, {ARP_STRUP, 0, AS}}},
    {"3 FINGER", 8, {{0, -1, AA}, {ARP_TOP, 0, AN}, {2, -1, AN}, {1, 0, AS}, {0, -1, AA}, {ARP_TOP, 0, AN},
                     {2, -1, AN}, {ARP_TOP, 0, AS}}},
    {"PIANO BACK", 16, {{0, -1, AA}, R_, U_(AN), R_, {0, 0, AS}, R_, U_(AN), U_(AM), {0, -1, AA}, R_, U_(AN), R_,
                        {2, -1, AS}, R_, U_(AA), R_}},
    {"CLAVI CHORD", 16, {U_(AA), R_, U_(AM), U_(AN), R_, U_(AM), U_(AA), R_, U_(AM), U_(AN), R_, U_(AA), R_, U_(AM),
                         U_(AN), U_(AM)}},
    {"WALTZ", 6, {{0, -1, AA}, R_, U_(AN), R_, U_(AS), R_}},
    {"SWING WALTZ", 9, {{0, -1, AA}, R_, R_, U_(AN), R_, U_(AG), U_(AS), R_, U_(AG)}},
    {"REGGAE", 16, {{0, -1, AA}, R_, U_(AM), R_, R_, R_, U_(AM), R_, {0, -1, AN}, R_, U_(AM), R_, R_, R_, U_(AM),
                    U_(AG)}},
    {"PERCUSSION", 16, {{0, 0, AA}, {0, 0, AG}, {0, 1, AM}, {0, 0, AS}, {0, 0, AA}, {0, 1, AM}, R_, {0, 0, AS},
                        {0, 0, AA}, {0, 0, AG}, {0, 1, AM}, {0, 1, AN}, R_, {0, 0, AS}, {0, 1, AA}, {0, 0, AG}}},
    {"HARP", 16, {{0, 0, AS}, {1, 0, AS}, {2, 0, AS}, {3, 0, AN}, {4, 0, AN}, {5, 0, AN}, {6, 0, AN}, {7, 0, AA},
                  {8, 0, AA}, {9, 0, AA}, {10, 0, AA}, {11, 0, AA}, R_, R_, R_, R_}},
    {"SHAMISEN", 16, {{ARP_TOP, 0, AA}, R_, {ARP_TOP, 0, AM}, {0, 0, AN}, {1, 0, AN}, {ARP_TOP, 0, AA}, R_,
                      {ARP_TOP, -1, AM}, {ARP_TOP, 0, AA}, {2, 0, AN}, {ARP_TOP, 0, AM}, R_, {1, 0, AN}, {0, 0, AN},
                      {ARP_TOP, 0, AA}, R_}},
    {"BOUND BALL", 32, {{0, 1, AA}, R_, R_, R_, R_, R_, R_, R_, {0, 1, AA}, R_, R_, R_, R_, R_, {0, 1, AN}, R_, R_, R_,
                        R_, {0, 1, AN}, R_, R_, R_, {0, 1, AS}, R_, R_, {0, 1, AS}, R_, {0, 1, AG}, {0, 1, AG},
                        {0, 1, AG}, R_}},
};
#undef R_
#undef C_
#undef U_
#undef E_
#define ARP_NSTEPS (sizeof ARP_STEPS / sizeof ARP_STEPS[0])
/* the order types, the AN1x's 1-15: up, down, up & down A (the ends once), B (the ends twice), random, each over
 * 1, 2, 4 octaves */
enum { AT_UP1, AT_UP2, AT_UP4, AT_DN1, AT_DN2, AT_DN4, AT_UD1, AT_UD2, AT_UD4, AT_UDB1, AT_UDB2, AT_UDB4, AT_RND1,
       AT_RND2, AT_RND4, AT_BASIC };
#define AT_UP AT_UP2                             /* (tests: up over two octaves) */
#define AT_UPDOWN AT_UD2
#define AT_UDB AT_UDB1
static const char *const ARP_BASIC_NAME[AT_BASIC] = {"UP OCT1", "UP OCT2", "UP OCT4", "DOWN OCT1", "DOWN OCT2",
                                                     "DOWN OCT4", "UPDW A OCT1", "UPDW A OCT2", "UPDW A OCT4",
                                                     "UPDW B OCT1", "UPDW B OCT2", "UPDW B OCT4", "RANDOM OCT1",
                                                     "RANDOM OCT2", "RANDOM OCT4"};
#define ARP_NTYPE (AT_BASIC + ARP_NSTEPS)
static const char *arp_type_name(uint32_t t)
{
    t %= ARP_NTYPE;
    return t < AT_BASIC ? ARP_BASIC_NAME[t] : ARP_STEPS[t - AT_BASIC].name;
}

static struct {
    uint8_t note[ARP_MAXN], vel[ARP_MAXN];       /* held, in the order played */
    uint8_t n;
    uint8_t keys;                                /* notes held by a key or MIDI now (HOLD: the set is kept at 0) */
    uint8_t on[ARP_MAXN], non;                   /* sounding now (their note offs at the gate) */
    uint32_t step, pos;                          /* steps since the start; place in the pattern */
    uint32_t t, len;                             /* samples since this step, its length (samples) */
    uint32_t gate;                               /* samples into the step: the notes off */
    uint32_t q8;                                 /* own clock: time in the step, Q8 samples */
    uint32_t ext_age, ticks, tick_len;           /* since the last MIDI clock; clocks in this step; their spacing */
    uint32_t clk, starts;                        /* MIDI clocks since START, STARTs seen (feg.c follows them) */
    uint32_t tick_t;
    uint32_t rng;
    uint8_t running;                             /* a step is due at the next block (the first note at once) */
    uint32_t swing_wait;                         /* a swung step: samples into the step it is due at (0: none) */
    uint32_t gls;                                /* GLISSANDO: semitones played */
    uint8_t pend[ARP_MAXN], pvel[ARP_MAXN], npend;   /* a strum's strings to come (one each ARP_STRUM samples) */
    uint32_t pt;                                 /* samples to the next of them */
    uint32_t lt;                                 /* LOGIC: clocks since KARMA's phrase started (its steps on them) */
    uint32_t gpos;                               /* GROOVE: steps since the start (each pattern its own length) */
    int32_t vmul;                                /* GROOVE: this step's velocity, % */
    uint8_t lpass;                               /* LOGIC: the swung step waiting lets its notes through */
} arp = {.rng = 0x2545F491u, .ext_age = 0xFFFFFFFFu, .vmul = 100};

/* the ARP page's picture (ui.c): the last steps played, their notes (0: a rest) */
static struct { uint8_t note[32]; volatile uint32_t w; } arp_view;

#define ARP_STRUM 529u                           /* samples between a strum's strings (12 ms) */
static void arp_off_all(void)
{
    uint32_t i;
    arp.npend = 0;
    for (i = 0; i < arp.non; i++)
        gen_off(arp.on[i]);
    arp.non = 0;
}

static void arp_reset(void)                      /* everything let go, the pattern from its start */
{
    arp_off_all();
    arp.n = arp.keys = 0;
    arp.step = arp.pos = 0;
    arp.running = 0;
}

static void arp_add(uint32_t note, uint32_t vel)
{
    uint32_t i;
    if (!arp.keys && sys.arp_hold) {             /* HOLD: a new chord after all were let go replaces the old */
        arp.n = 0;
    }
    arp.keys++;
    for (i = 0; i < arp.n; i++)
        if (arp.note[i] == note)
            return;
    if (arp.n >= ARP_MAXN)
        return;
    arp.note[arp.n] = (uint8_t)note;
    arp.vel[arp.n] = (uint8_t)vel;
    if (!arp.n++) {                              /* the first note: the pattern starts with it, now */
        arp.pos = 0;
        arp.running = 1;
        arp.q8 = 0;
    }
}

static void arp_remove(uint32_t note)
{
    uint32_t i, j;
    if (arp.keys)
        arp.keys--;
    if (sys.arp_hold)                            /* (latched: they stay) */
        return;
    for (i = 0; i < arp.n; i++)
        if (arp.note[i] == note) {
            for (j = i; j + 1u < arp.n; j++) {
                arp.note[j] = arp.note[j + 1u];
                arp.vel[j] = arp.vel[j + 1u];
            }
            arp.n--;
            break;
        }
    if (!arp.n)
        arp_off_all();
}

static void arp_hold_released(void)              /* HOLD turned off: the notes no longer held go */
{
    if (!arp.keys) {
        arp.n = 0;
        arp_off_all();
    }
}

/* the held notes low to high (into s[]; their velocities into v[]) */
static uint32_t arp_sorted(uint8_t *s, uint8_t *v)
{
    uint32_t i, j, n = arp.n;
    for (i = 0; i < n; i++) {
        s[i] = arp.note[i];
        v[i] = arp.vel[i];
    }
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && s[j - 1u] > s[j]; j--) {
            uint8_t t = s[j]; s[j] = s[j - 1u]; s[j - 1u] = t;
            t = v[j]; v[j] = v[j - 1u]; v[j - 1u] = t;
        }
    return n;
}

static void arp_sound(uint32_t note, uint32_t vel)
{
    if (note > 127u || arp.non >= ARP_MAXN)
        return;
    if (arp.vmul != 100) {                       /* (GROOVE's VELOCITY) */
        int32_t v = (int32_t)vel * arp.vmul / 100;
        vel = (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v);
    }
    gen_on(note, vel);
    arp.on[arp.non++] = (uint8_t)note;
}

static uint8_t arp_last;                          /* the last note played (ARP_PREV: the echo) */

/* GROOVE (the ARP's second page): two short patterns over the steps, each its own length, so they turn against the
 * pattern and each other: VELOCITY (each step's level, 1 .. 9: x 0.55 .. 1.75 with DEPTH 127) and NOTE OFF (each
 * step's gate, 1 .. 9 = 10 .. 90 % of the step, T tied on to the next, . a rest: no note), DEPTH from GATE's to the
 * pattern's */
typedef struct { const char *name, *s; } arp_groove_t;
static const arp_groove_t ARP_VPAT[] = {
    {"OFF", ""}, {"ACCENT 4", "9555"}, {"ACCENT 3", "955"}, {"OFFBEAT", "59"}, {"RAMP UP", "3579"},
    {"RAMP DOWN", "9753"}, {"TRESILLO", "95595595"}, {"WALTZ", "966"}, {"GHOSTS", "9393"}, {"PUMP", "39"},
    {"SWELL", "234567"}, {"FIVE", "95757"}, {"SEVEN", "9473582"}, {"BACKBEAT", "5595"},
};
static const arp_groove_t ARP_GPAT[] = {
    {"OFF", ""}, {"STACCATO", "2"}, {"LEGATO", "T"}, {"LONG SHORT", "82"}, {"SHORT LONG", "28"},
    {"DOTTED", "T.2"}, {"SKIP", "8."}, {"OFF SKIP", ".8"}, {"3 OF 4", "888."}, {"TRESILLO", "T..T..T."},
    {"STUTTER", "2228"}, {"BREATH", "6666."}, {"FIVE", "8.88."}, {"TIE TWO", "T2"}, {"GALLOP", "8.22"},
};
#define ARP_NVP (sizeof ARP_VPAT / sizeof ARP_VPAT[0])
#define ARP_NGP (sizeof ARP_GPAT / sizeof ARP_GPAT[0])
static char arp_gchar(const char *s, uint32_t pos) { uint32_t n = (uint32_t)str_len(s); return n ? s[pos % n] : 0; }
/* this step's groove (before its notes): 0 a rest (no notes), else 1; arp.vmul and arp.gate set */
static int arp_groove(void)
{
    char vc = arp_gchar(ARP_VPAT[sys.arp_vp % ARP_NVP].s, arp.gpos), gc = arp_gchar(ARP_GPAT[sys.arp_gp % ARP_NGP].s, arp.gpos);
    arp.gpos++;
    arp.vmul = 100;
    if (vc >= '1' && vc <= '9')
        arp.vmul = 100 + ((40 + (vc - '0') * 15) - 100) * (int32_t)(sys.arp_vd & 127u) / 127;
    if (gc == '.')
        return 0;
    if (gc) {
        uint32_t want = gc == 'T' ? arp.len + arp.len / 8u : arp.len * (uint32_t)(gc - '0') / 10u;
        int32_t d = (int32_t)want - (int32_t)arp.gate;
        arp.gate = (uint32_t)((int32_t)arp.gate + d * (int32_t)(sys.arp_gd & 127u) / 127);
    }
    return 1;
}
/* the step patterns stay in the chord's octave: a note above or below (the pattern's octave steps, its places past
 * the highest held) is folded back by octaves into the lowest note held .. an octave above it (or the highest held, if
 * the chord is wider), so changing the pattern keeps the register */
static int32_t arp_fold(int32_t x, const uint8_t *s, uint32_t n)
{
    int32_t lo = s[0], hi = s[n - 1u] > lo + 11 ? s[n - 1u] : lo + 11;
    while (x > hi)
        x -= 12;
    while (x < lo)
        x += 12;
    return x;
}
static uint32_t arp_vel(uint32_t v, uint32_t acc)
{
    return acc == AA ? v + (127u - v) / 2u : acc == AS ? v * 2u / 3u : acc == AG ? v / 3u + 1u :
           acc == AM ? v * 3u / 4u + 1u : v;
}

/* one step: the notes of the last off (unless tied), the pattern's next on */
static void arp_do_step(void)
{
    uint8_t s[ARP_MAXN], v[ARP_MAXN];
    uint32_t n = arp_sorted(s, v), type = sys.arp_type % ARP_NTYPE, i, nt, shown = 0;
    if (!n)
        return;
    if (!arp_groove()) {                         /* (GROOVE's NOTE OFF: a rest; the pattern goes on) */
        arp_off_all();
        arp.npend = 0;
        arp.pos++;
        arp_view.note[arp_view.w++ & 31u] = 0;
        return;
    }
    if (type < AT_BASIC) {
        static const uint8_t OCT[3] = {1, 2, 4};
        uint32_t total = n * OCT[type % 3u], p = arp.pos, idx, fam = type / 3u;
        arp_off_all();
        if (fam == 0u)
            idx = p % total;
        else if (fam == 1u)
            idx = total - 1u - p % total;
        else if (fam == 2u) {                    /* A: the ends once (0 1 2 3 2 1 ...) */
            uint32_t c = total > 1u ? 2u * total - 2u : 1u, q = p % c;
            idx = q < total ? q : c - q;
        } else if (fam == 3u) {                  /* B: the ends twice (0 1 2 3 3 2 1 0 ...) */
            uint32_t q = p % (2u * total);
            idx = q < total ? q : 2u * total - 1u - q;
        } else {
            arp.rng = arp.rng * 1664525u + 1013904223u;
            idx = (arp.rng >> 16) % total;
        }
        nt = s[idx % n] + 12u * (idx / n);
        arp_sound(nt, v[idx % n]);
        arp_last = (uint8_t)nt;
        arp_view.note[arp_view.w++ & 31u] = (uint8_t)nt;
        arp.pos++;
        return;
    }
    {   /* a step pattern */
        const arp_pat_t *P = &ARP_STEPS[type - AT_BASIC];
        const arp_step_t *st = &P->s[arp.pos % P->n];
        arp.pos++;
        if (st->sel == ARP_TIE) {                /* (the notes go on) */
            arp_view.note[arp_view.w++ & 31u] = arp.non ? arp.on[0] : 0u;
            return;
        }
        arp_off_all();
        if (st->sel == ARP_REST) {
            arp_view.note[arp_view.w++ & 31u] = 0;
            return;
        }
        if (st->acc == AM && arp.gate > arp.len / 4u)   /* (muted: a quarter of the step at most) */
            arp.gate = arp.len / 4u;
        if (st->sel == ARP_PREV) {               /* an echo: the last note again, softer */
            if (arp_last)
                arp_sound(arp_last, arp_vel(v[0], st->acc));
            arp_view.note[arp_view.w++ & 31u] = arp_last;
            return;
        }
        if (st->sel == ARP_GLS) {                /* semitone by semitone, lowest to highest and back */
            uint32_t lo = s[0], r = s[n - 1u] > lo ? s[n - 1u] - lo : 12u, c = arp.gls++ % (2u * r);
            nt = lo + (c <= r ? c : 2u * r - c);
            arp_sound(nt, arp_vel(v[0], st->acc));
            arp_last = (uint8_t)nt;
            arp_view.note[arp_view.w++ & 31u] = (uint8_t)nt;
            return;
        }
        if (st->sel == ARP_STRUP || st->sel == ARP_STRDN) {   /* strummed: the first string now, the others after */
            for (i = 0; i < n; i++) {
                uint32_t k = st->sel == ARP_STRUP ? i : n - 1u - i;
                int32_t x = arp_fold((int32_t)s[k] + 12 * st->oct, s, n);
                if (!i)
                    arp_sound((uint32_t)x, arp_vel(v[k], st->acc));
                else if (arp.npend < ARP_MAXN) {
                    arp.pend[arp.npend] = (uint8_t)x;
                    arp.pvel[arp.npend++] = (uint8_t)arp_vel(v[k], st->acc) * 7u / 8u + 1u;
                }
            }
            arp.pt = ARP_STRUM;
            arp_last = s[st->sel == ARP_STRUP ? 0u : n - 1u];
            arp_view.note[arp_view.w++ & 31u] = arp_last;
            return;
        }
        for (i = st->sel == ARP_UPPER && n > 1u ? 1u : 0u; i < n; i++) {
            uint32_t k = st->sel == ARP_ALL || st->sel == ARP_UPPER ? i : st->sel == ARP_TOP ? n - 1u : st->sel;
            int32_t o = (int32_t)(k / n) + st->oct, x = arp_fold((int32_t)s[k % n] + 12 * o, s, n);
            arp_sound((uint32_t)x, arp_vel(v[k % n], st->acc));
            if (!shown++) {
                arp_last = (uint8_t)x;
                arp_view.note[arp_view.w++ & 31u] = (uint8_t)x;
            }
            if (st->sel != ARP_ALL && st->sel != ARP_UPPER)
                break;
        }
    }
}

static void arp_midi_rt(uint32_t status)
{
    if (status == 0xF8u) {
        if (arp.ext_age != 0xFFFFFFFFu && arp.ext_age < ARP_EXT_TIMEOUT)
            arp.tick_len = arp.tick_len ? (arp.tick_len * 3u + arp.tick_t) / 4u : arp.tick_t;   /* (smoothed) */
        arp.ext_age = 0;
        arp.tick_t = 0;
        arp.ticks++;
        arp.clk++;
    } else if (status == 0xFAu) {                /* START: the pattern from its first step, at the next clock */
        arp.clk = 0;
        arp.starts++;
        arp.pos = 0;
        arp.ticks = 24u;
        arp.running = 0;
    }
}

static int arp_ext(void) { return arp.ext_age < ARP_EXT_TIMEOUT; }

/* LOGIC (the ARP and KARMA on at once, the LOGIC page): both play the notes held, on one clock (KARMA's: its phrase
 * started, its clocks counted), and at each clock their hits meet: OR both sound, AND each only where the other hits
 * too, XOR each only where the other does not. The meeting is on the straight grid (the clock both count), SWING
 * after it, each its own; a step let through plays as it would, one held back is a rest (the pattern goes on) */
static int logic_on(void) { return sys.k_logic && sys.arp_on && sys.k_on; }
/* LOGIC's NOTE QUANT: what the two make (KARMA's echoes transposed, the ARP's glissando, its octaves folded) put
 * in a key: CHORD the notes held only; AUTO the scale the chord implies from ROOT (major, or minor with a minor third
 * and no major one; the chord's own notes always in); or a scale of its own from ROOT. ROOT (KNOB 3): AUTO the lowest
 * note held, or C .. B. A note outside to the nearest inside (the lower of two as near) */
enum { LQ_OFF, LQ_CHORD, LQ_AUTO, LQ_SCALES };
static const struct { const char *name; uint16_t mask; } LQ_SCALE[] = {   /* (bit n: n semitones over the root) */
    {"MAJOR", 0xAB5}, {"MINOR", 0x5AD}, {"DORIAN", 0x6AD}, {"PHRYGIAN", 0x5AB}, {"LYDIAN", 0xAD5},
    {"MIXOLYDIAN", 0x6B5}, {"LOCRIAN", 0x56B}, {"HARM MINOR", 0x9AD}, {"MEL MINOR", 0xAAD}, {"PENTA MAJOR", 0x295},
    {"PENTA MINOR", 0x4A9}, {"BLUES", 0x4E9}, {"WHOLE TONE", 0x555}, {"HIRAJOSHI", 0x18D}, {"DOUBLE HARM", 0x9B3},
};
#define LQ_N (LQ_SCALES + sizeof LQ_SCALE / sizeof LQ_SCALE[0])
static const char *logic_q_name(uint32_t q)
{
    return q == LQ_OFF ? "OFF" : q == LQ_CHORD ? "CHORD" : q == LQ_AUTO ? "AUTO" : LQ_SCALE[(q - LQ_SCALES) % (LQ_N - LQ_SCALES)].name;
}
static uint32_t logic_q(uint32_t note)
{
    uint32_t i, lo = 127u, root, held = 0, mask, pc, d, q = sys.k_lq % LQ_N;
    if (!logic_on() || q == LQ_OFF || !arp.n || note > 127u)
        return note;
    for (i = 0; i < arp.n; i++) {
        lo = arp.note[i] < lo ? arp.note[i] : lo;
        held |= 1u << (arp.note[i] % 12u);       /* (the chord's pitch classes, from C) */
    }
    root = sys.k_root ? (sys.k_root - 1u) % 12u : lo % 12u;
    held = ((held >> root) | (held << (12u - root))) & 0xFFFu;   /* (from the root) */
    if (q == LQ_CHORD)
        mask = held;
    else if (q == LQ_AUTO)
        mask = held | ((held >> 3 & 1u) && !(held >> 4 & 1u) ? 0x5ADu : 0xAB5u);
    else
        mask = LQ_SCALE[q - LQ_SCALES].mask;
    if (!mask)
        return note;
    pc = (note + 120u - root) % 12u;
    for (d = 0; d < 7u; d++) {
        if (mask >> ((pc + 12u - d) % 12u) & 1u)
            return note >= d ? note - d : note + 12u - d;
        if (mask >> ((pc + d) % 12u) & 1u)
            return note + d <= 127u ? note + d : note + d - 12u;
    }
    return note;
}
static void arp_lsync(void)                      /* (KARMA's phrase from its start: the pattern too) */
{
    arp.lt = 0;
    arp.pos = 0;
    arp.gls = 0;
    arp.gpos = 0;
}
static int arp_ltick(void)                       /* (a clock: a step due?) */
{
    return ++arp.lt % ARP_TPS[sys.arp_div % ARP_NDIV] == 0u;
}
static int arp_would_hit(void)                   /* (the next step plays notes: not a rest, not a tie) */
{
    const arp_step_t *st;
    if (!arp.n)
        return 0;
    if (sys.arp_type % ARP_NTYPE < AT_BASIC)
        return 1;
    st = &ARP_STEPS[sys.arp_type % ARP_NTYPE - AT_BASIC].s[arp.pos % ARP_STEPS[sys.arp_type % ARP_NTYPE - AT_BASIC].n];
    return st->sel != ARP_REST && st->sel != ARP_TIE && (st->sel != ARP_PREV || arp_last);
}
static void arp_skip(void)                       /* (a step held back: a rest, the pattern on) */
{
    arp.pos++;
    arp_off_all();
    arp.npend = 0;
    arp_view.note[arp_view.w++ & 31u] = 0;
}
static void arp_lstep(int pass)                  /* (a step due at this clock: through or held back) */
{
    uint32_t gate = 8u + (sys.arp_gate & 127u) * 92u / 127u, sw = 50u + (sys.arp_swing & 127u) * 33u / 127u;
    arp.t = 0;
    arp.lpass = (uint8_t)pass;
    if (arp.pos & 1u && sw > 50u) {
        arp.swing_wait = 1u + arp.len * 2u * (sw - 50u) / 100u;
    } else {
        arp.swing_wait = 0;
        arp.gate = arp.len * gate / 100u;
        if (pass)
            arp_do_step();
        else
            arp_skip();
        arp.step++;
    }
}

/* the clock, once a block: steps at the note value (swung: every second step later by SWING, the AN1x's 50 % none
 * .. 83 % most: its pair split 50:50 .. 83:17), the notes off at the gate */
static void arp_block(uint32_t n)
{
    uint32_t tps = ARP_TPS[sys.arp_div % ARP_NDIV], gate = 8u + (sys.arp_gate & 127u) * 92u / 127u;   /* 8..100 % */
    uint32_t sw = 50u + (sys.arp_swing & 127u) * 33u / 127u;                                            /* 50..83 % */
    int due = 0;
    if (arp.ext_age < 0xFFFF0000u)
        arp.ext_age += n;
    arp.tick_t += n;
    if (!sys.arp_on || !arp.n) {
        if (arp.non)
            arp_off_all();
        arp.swing_wait = 0;
        return;
    }
    if (arp_ext()) {                             /* MIDI clock: 24 a quarter, tps a step */
        if (arp.ticks >= tps) {
            arp.ticks = 0;
            due = 1;
        }
        arp.len = arp.tick_len ? arp.tick_len * tps : 5512u;
    } else {                                     /* its own: samples a step, Q8 (FS 60 s / bpm x tps / 24) */
        uint32_t bpm = sys.arp_bpm < 40u ? 40u : sys.arp_bpm > 250u ? 250u : sys.arp_bpm;
        uint32_t len8 = 2646000u * 256u / bpm / 24u * tps;   /* (< 2^32: 2.7e9 at 40 BPM, 3/8) */
        arp.len = len8 >> 8;
        arp.q8 += n * 256u;
        if (arp.q8 >= len8) {
            arp.q8 -= len8;
            due = 1;
        }
    }
    if (logic_on()) {                            /* (LOGIC: the steps on KARMA's clock, karma.c) */
        due = 0;
        arp.running = 0;
    } else if (arp.running) {                    /* the first note: at once */
        arp.running = 0;
        due = 1;
        arp.q8 = 0;
        arp.ticks = 0;
        arp.pos = 0;
        arp.gls = 0;
        arp.gpos = 0;
    }
    if (due) {
        arp.t = 0;
        if (arp.pos & 1u && sw > 50u) {          /* the second of a pair: later */
            arp.swing_wait = 1u + arp.len * 2u * (sw - 50u) / 100u;
        } else {
            arp.swing_wait = 0;
            arp.gate = arp.len * gate / 100u;
            arp_do_step();
            arp.step++;
        }
        return;
    }
    arp.t += n;
    if (arp.npend) {                             /* a strum: its next strings */
        if (arp.pt > n)
            arp.pt -= n;
        else {
            uint32_t j;
            arp_sound(arp.pend[0], arp.pvel[0]);
            for (j = 1; j < arp.npend; j++) {
                arp.pend[j - 1u] = arp.pend[j];
                arp.pvel[j - 1u] = arp.pvel[j];
            }
            arp.npend--;
            arp.pt = ARP_STRUM;
        }
    }
    if (arp.swing_wait && arp.t >= arp.swing_wait) {   /* the swung step, now (shorter by as much) */
        uint32_t left = arp.len > arp.swing_wait ? arp.len - arp.swing_wait : 1u;
        arp.swing_wait = 0;
        arp.t = 0;
        arp.gate = left * gate / 100u;
        if (!logic_on() || arp.lpass)
            arp_do_step();
        else
            arp_skip();
        arp.step++;
        return;
    }
    if (!arp.swing_wait && arp.t >= arp.gate && arp.non && (gate < 100u || arp.gate < arp.len)) {
        const arp_pat_t *P = sys.arp_type % ARP_NTYPE >= AT_BASIC ? &ARP_STEPS[sys.arp_type % ARP_NTYPE - AT_BASIC] : 0;
        if (!P || P->s[arp.pos % P->n].sel != ARP_TIE)   /* (a tie next: they go on) */
            arp_off_all();
    }
}

/* the tempo now, for the screen: BPM, or the MIDI clock's (0: not known yet) */
static uint32_t arp_bpm_now(void)
{
    if (!arp_ext())
        return sys.arp_bpm;
    return arp.tick_len ? (2646000u / 24u + arp.tick_len / 2u) / arp.tick_len : 0u;
}
