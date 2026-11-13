/* SPDX-License-Identifier: GPL-3.0-only */
/* CHUA UI (main loop only): the attractor drawn live, the knobs, presets, LEDs, the settings in flash.
 *
 * Four pages: HOME the circuit (the oscillator), ENV its page (again: back HOME), FX the resonator, FX again the
 * shimmer, FX again back HOME; the LED of the page's button is lit:
 *   HOME     ALGORITHM the oscillator: CHUA (the circuit) or one of the 30 noise programs (noise.c: A0..C9)
 *   CIRCUIT  KNOB 1 CHAOS   the regime: cycle, period 2, spiral, double scroll, outer cycle
 *            KNOB 2 SHAPE   beta: slow round spirals .. tight fast ones
 *            KNOB 3 OUTPUT  which voltage you hear: X, X+Y, Y, Y+Z, Z
 *            KNOB 4 RING    the low pass gate's resonance
 *   NOISE    KNOB 1 X, KNOB 2 Y (the program's two controls), KNOB 3 LEVEL, KNOB 4 RING; drawn: the output against
 *            itself a moment ago (its "attractor")
 *   MOD ENV  (ENV again) an AD envelope straight on the noise program's pitch (no oscillator, no VCA), started by
 *            each note: KNOB 1 LENGTH, KNOB 2 SKEW (where the peak falls), KNOB 3 HEIGHT (the sweep at the peak, up to
 *            36 semitones), KNOB 4 BEND (its curve, both ways); drawn: its shape, the sweep now
 *   ENV      KNOB 1..4      ATTACK, DECAY, SUSTAIN, RELEASE of the ADSR that drives the low pass gate; the envelope drawn
 *   SCL      the PAN page (again: back HOME): KNOB 1 BASIS: the stereo spread, each note the next place of a cycle
 *            (left, right, half left, ...); KNOB 2 ORBIT: how much each voice's phase moves with its picture (all-passes: the circuit's
 *            scroll left / right, a drift); the voices drawn where they are
 *   FX       KNOB 1..4      STRUCTURE, BRIGHTNESS, DAMPING, MIX of the resonators (resonator.c: Rings' models, 4 voices;
 *            and the waveguides of waveguide.c: VOICE (KNOB 1 the SIZE, SELECT the VOWEL), FLUTE (JET, BREATH),
 *            SYRINX (CHIRP, TWO VOICE), HAWK (SCREAM, RASP), BRASS (LIP, MUTE), BOW (PRESSURE, BOW AT),
 *            REED (REED, BORE), DRUM (KETTLE, STRIKE), PIANO (UNISON, HAMMER));
 *            ALGORITHM the model (MODAL, SYMP STRING, STRING), SELECT the POSITION; its resonances and output drawn
 *   FX (2nd) KNOB 1..4      SIZE, DECAY, SHIMMER, MIX of the shimmer reverb (shimmer.c); ALGORITHM the INTERVAL,
 *            SELECT the TONE; the tail's level over the last seconds drawn (the undertow building)
 *   SELECT   HOME: the view (3D turning, X-Y, X-Z, Y-Z, TIME)
 * Each parameter lives in one place: RING on HOME (KNOB 4), the oscillator on HOME (ALGORITHM).
 *   PRESETS the presets
 *   PLAY     starts / stops the ARP or KARMA (on their pages that one; elsewhere the last of them opened; lit while
 *            one runs)
 *   ARP      (the ARP button; again: back HOME; its LED lit while on) the arpeggiator (arp.c): KNOB 1 BPM (EXT under
 *            MIDI clock), KNOB 2 SUBDIVIDE, KNOB 3 GATE, KNOB 4 SWING; ALGORITHM the pattern, SELECT LATCH
 *   LFO      FREE EG 1, again FREE EG 2 (feg.c): KNOB 1 the knob recorded, KNOB 2 LENGTH (NOTE, 1..8 STEPS, 1..4 BARS),
 *            KNOB 3 DEPTH, KNOB 4 the TARGET; SELECT PLAY; REC again there: record (NOTE: REC once more to stop)
 *   SEQ      the KARMA page (karma.c; again: back HOME; PLAY starts it, its LED lit while on): ALGORITHM the GE (the phrase
 *            generator), KNOB 1..4 its four realtime controls (named per GE: DENSITY, RANGE, REPEAT ...), SELECT LATCH,
 *            PRESETS the tempo (the ARP page's BPM: one for all)
 *   EDIT     the EDIT page (again: back HOME): KNOB 1 the MOD WHEEL's parameter (MIDI CC 1; FREE EG's list), KNOB 2
 *            its DEPTH (FREE EG playing the same one: the wheel sets FREE EG's depth), KNOB 3 VELOCITY on the sound (OFF, BRIGHT, SWEEP, BOTH), KNOB 4 how much; MIDI program change
 *            loads the preset (program 1 the first)
 *   PRESETS  on the FX page: the FX presets (a resonator / waveguide with its knobs), on SHIMMER: the shimmer
 *            presets; elsewhere the sound presets (on KARMA: the tempo)
 *   REC      the TAPE page (tape.c; on FREE EG's page: record there): a four-track reel-to-reel. REC there records
 *            one pass on the armed track (PLAY held: erase it), PLAY the transport, SELECT its two menus: TAPE (KNOB
 *            1..4: a blank track's knob arms it, a recorded one's is its level; ALGORITHM arms a recorded one) and
 *            SPEED (KNOB 1 the global BPM, KNOB 2 the SPEED in the harmonic series' ratios, 1/2 .. 2/1, each a
 *            chromatic step, the BPM following; KNOB 3 the LENGTH, 1 .. 16 beats; KNOB 4 the pitch); PRESETS the BPM
 *   LFO      the FREE EG page (again: back HOME); REC records there
 *   OCT- / OCT+  octave -3..+3               SAVE       the settings into flash (restored at power-on)
 *   GLO      the MIDI page (again: back HOME): KNOB 1 TRS IN on / off, KNOB 2 USB (OFF, IN, OUT, IN+OUT), KNOB 3 the
 *            RX channel (OMNI, 1..16), KNOB 4 the TX channel; SELECT the CLOCK: right EXTERNAL (MIDI), left INTERNAL
 *            (the 2ND CORE always on: ALL)
 *   MASTER pot   volume
 *
 * The picture: synth.c keeps a ring of the circuit's state (chua_trace: the newest voice, or the ghost circuit when
 * nothing plays); each frame draws its last TRACE_DRAW points as a line, the oldest faint, the newest bright, scaled
 * to fit (each axis follows its own range). Drawing: the canvas (gfx.c cv_*) is 240 x 124 at most, blitted by DMA
 * while the next frame is prepared. */
#define UI_VERSION FELUCCA_VERSION
#define TRACE_DRAW 640u                  /* points drawn per frame (of chua_trace's TRACE_N) */
#define PLOT_Y 24u                       /* the plot: 240 x 124 from here */
#define PLOT_H 124
enum { US_INITIAL, US_PSYCHE, US_COUNT };   /* the UI STYLEs (ui_style.c) */
#define UI_PALETTE 2u                    /* AMBER (build/gen/ui_palettes.h): an oscilloscope's colours */

static const char *const NOTE_NAME[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
enum { V_3D, V_XY, V_XZ, V_YZ, V_TIME, V_COUNT };
static const char *const VIEW_NAME[V_COUNT] = {"3D", "X-Y", "X-Z", "Y-Z", "TIME"};

/* MOD FX's presets (PRESETS on its page; a sound preset's .mfx = MFP_..., its index + 1): a mode, KNOB 1's note value
 * (VIBRATO, TREMOLO: an index into MF_DIV) or ratio (RING: into MF_RATIO), the slew, the rectifier (0 .. 3), the mix */
typedef struct { const char *name; uint8_t mode, k1, slew, rect, mix; } mfx_preset_t;
enum { MFP_NONE, MFP_SLOW_WOBBLE, MFP_TAPE_WARBLE, MFP_SEASICK, MFP_CHAOS_VIB, MFP_TRIPLET_SHIMMY, MFP_OCTAVE_RING,
       MFP_FIFTH_BELL, MFP_SUB_RING, MFP_METAL_BELL, MFP_ROBOT, MFP_GONG, MFP_DIRTY_RING, MFP_HELICOPTER,
       MFP_PULSE_GATE, MFP_SLOW_SWELL, MFP_TRIPLET_CHOP, MFP_EP_TREMOLO, MFP_VIBES_MOTOR, MFP_ORGAN_VIB, MFP_STRING_VIB,
       MFP_BREATH_VIB, MFP_LEAD_VIB, MFP_BELL_RING, MFP_CHAOS_RING, MFP_SLOW_PULSE, MFP_WIDE_SHIFT, MFP_OCTAVE_SPLIT,
       MFP_SUB_SPLIT, MFP_BELL_SPLIT, MFP_CHAOS_SPLIT, MFP_COUNT };
static const mfx_preset_t MFX_PRE[MFP_COUNT - 1] = {
    {"SLOW WOBBLE", MF_VIBRATO, 2, 60, 0, 55},   {"TAPE WARBLE", MF_VIBRATO, 5, 95, 0, 40},
    {"SEASICK", MF_VIBRATO, 1, 30, 0, 90},       {"CHAOS VIBRATO", MF_VIBRATO, 8, 0, 0, 70},
    {"TRIPLET SHIMMY", MF_VIBRATO, 9, 40, 1, 45}, {"OCTAVE RING", MF_RING, 9, 0, 0, 60},
    {"FIFTH BELL", MF_RING, 7, 0, 0, 55},        {"SUB RING", MF_RING, 2, 0, 0, 65},
    {"METAL BELL", MF_RING, 10, 0, 0, 75},       {"ROBOT VOICE", MF_RING, 5, 0, 1, 85},
    {"GONG", MF_RING, 6, 25, 0, 70},             {"DIRTY RING", MF_RING, 11, 0, 2, 80},
    {"HELICOPTER", MF_TREMOLO, 11, 0, 0, 100},   {"PULSE GATE", MF_TREMOLO, 8, 0, 2, 100},
    {"SLOW SWELL", MF_TREMOLO, 2, 80, 0, 85},    {"TRIPLET CHOP", MF_TREMOLO, 9, 10, 0, 90},
    {"EP TREMOLO", MF_TREMOLO, 8, 70, 0, 50},    {"VIBES MOTOR", MF_TREMOLO, 9, 90, 0, 55},
    {"ORGAN VIBRATO", MF_VIBRATO, 12, 80, 0, 35}, {"STRING VIBRATO", MF_VIBRATO, 8, 110, 0, 22},
    {"BREATH VIBRATO", MF_VIBRATO, 9, 100, 0, 26}, {"LEAD VIBRATO", MF_VIBRATO, 8, 60, 0, 35},
    {"BELL RING", MF_RING, 12, 0, 0, 30},        {"CHAOS RING", MF_RING, 5, 0, 0, 55},
    {"SLOW PULSE", MF_TREMOLO, 5, 50, 0, 50},
    {"WIDE SHIFT", MF_SHIFT, 5, 0, 0, 60},       {"OCTAVE SPLIT", MF_SHIFT, 9, 0, 0, 80},
    {"SUB SPLIT", MF_SHIFT, 2, 0, 0, 70},        {"BELL SPLIT", MF_SHIFT, 10, 0, 0, 75},
    {"CHAOS SPLIT", MF_SHIFT, 5, 20, 1, 90},
};
#define MFX_NPRE (sizeof MFX_PRE / sizeof MFX_PRE[0])
static void mfx_set(uint32_t k)                  /* MOD FX as preset k (0: off; else MFX_PRE[k - 1]) */
{
    const mfx_preset_t *P;
    if (!k || k > MFX_NPRE) {
        sys.mfx_mode = MF_OFF;
        return;
    }
    P = &MFX_PRE[k - 1u];
    sys.mfx_mode = (uint8_t)(P->mode % MF_COUNT);
    sys.mfx_rate = (uint8_t)(MF_KEYED(P->mode) ? P->k1 * 128u / MF_NRATIO + 4u : P->k1 * 128u / MF_NDIV + 4u);
    sys.mfx_slew = P->slew;
    sys.mfx_rect = mf_rect_val(P->rect);
    sys.mfx_mix = P->mix;
}

static int mfx_edited(uint32_t k)                /* MOD FX not as preset k sets it */
{
    const mfx_preset_t *P;
    if (!k || k > MFX_NPRE)
        return sys.mfx_mode % MF_COUNT != MF_OFF;
    P = &MFX_PRE[k - 1u];
    return sys.mfx_mode != P->mode || sys.mfx_slew != P->slew || sys.mfx_mix != P->mix ||
           mf_rect_of(sys.mfx_rect) != P->rect ||
           (MF_KEYED(P->mode) ? (sys.mfx_rate & 127u) * MF_NRATIO / 128u : (sys.mfx_rate & 127u) * MF_NDIV / 128u) != P->k1;
}

/* ------------------------------------------------------------- presets --- */
/* ADSR times: 0 = 0.5 ms .. 127 = 10 s (exponential: 40 ~11 ms, 64 ~74 ms, 80 ~250 ms, 100 ~1.2 s); RING: 0 none .. 127 Q 6 */
/* FX (resonator.c): model, STRUCTURE, BRIGHTNESS, DAMPING, POSITION, MIX; MIX 0 (left out): no resonator */
typedef struct {
    const char *name;
    uint8_t chaos, shape, output, attack, decay, sustain, release, ring;
    uint8_t fx_model, fx_struct, fx_bright, fx_damp, fx_pos, fx_mix;
    uint8_t sh_size, sh_decay, sh_shimmer, sh_mix, sh_interval, sh_tone;   /* SHIMMER; MIX 0 (left out): off */
    uint8_t osc, np_x, np_y;                     /* the oscillator: 0 (left out) the circuit, 1..30 a noise program */
    uint8_t menv_len, menv_skew, menv_height;    /* its AD envelope (HEIGHT 0, left out: none) */
    uint8_t scream;                              /* SCREAM: the voices on the waveguides' breath (0, left out) */
    uint8_t sh_space, np_level, menv_bend;       /* the SPACE (0, left out: HALL), NOISE LEVEL and BEND (0: 64) */
    uint8_t eg;                                  /* a FREE EG contour each note plays (PRE_EG; .eg = EG_..., 0: none) */
    int8_t oct;                                  /* the sound's octave: its instrument's register (.oct = -1 ...) */
    uint8_t mfx;                                 /* its MOD FX (.mfx = MFP_..., 0: off) */
} preset_t;
/* the presets' FREE EG contours: each note moves a parameter around the preset's value (the offsets, at points of
 * ~46 ms: FREE EG's NOTE length), as a player would: a flute's overtones coming in as the jet shortens, a burst of air
 * at the lips (the chiff) settling to a steady breath, brass blooming as the lips tighten, a bow swelling, a sax
 * growling late in a long note, a choir's vowel opening, a pad's shimmer rising */
enum { EG_NONE, EG_OVERTONE, EG_CHIFF, EG_BLOOM, EG_SWELL, EG_REEDSWELL, EG_GROWL, EG_VOWEL, EG_SHIMRISE, EG_OPEN, EG_TALK,
       EG_TINE, EG_CLICK, EG_PERC, EG_PICK, EG_TONE, EG_STRIKE, EG_PUNCH, EG_SLAP, EG_HOOVER, EG_COUNT };
typedef struct { uint8_t target, n; int8_t p[5][2]; uint8_t len; } pre_eg_t;   /* FREE EG's target; points: (point,
                                                  * offset); its LENGTH (0, left out: NOTE) */
static const pre_eg_t PRE_EG[EG_COUNT] = {
    {0, 0, {{0, 0}}},
    {8, 4, {{0, -10}, {4, 0}, {26, 25}, {60, 10}}},              /* OVERTONE: the jet (FX STRUCTURE) shortening */
    {11, 4, {{0, 60}, {3, -10}, {20, 6}, {60, 0}}},              /* CHIFF: the breath (FX POSITION): a burst, then */
    {8, 3, {{0, -30}, {6, 0}, {30, 8}}},                         /* BLOOM: the lips (FX STRUCTURE) tightening */
    {9, 3, {{0, -20}, {14, 12}, {60, 4}}},                       /* SWELL: the bow's brightness swelling */
    {8, 3, {{0, -15}, {12, 6}, {60, 0}}},                        /* REEDSWELL: the reed (FX STRUCTURE) */
    {27, 4, {{0, 0}, {16, 0}, {44, 45}, {90, 30}}},              /* GROWL: SCREAM late in the note */
    {11, 3, {{0, -40}, {24, 0}, {70, 30}}},                      /* VOWEL: U .. A .. E (FX POSITION) */
    {15, 2, {{0, -40}, {80, 30}}},                               /* SHIMRISE: SHIMMER rising */
    {3, 2, {{0, -40}, {60, 40}}},                                /* OPEN: RING opening the gate's resonance */
    {15, 4, {{0, -50}, {10, 10}, {40, 40}, {90, 0}}},             /* TALK: MOUTH's vowel (SHIMMER): U, opening to A .. E, back */
    /* the attack transients (a point ~46 ms; the noise programs glide their X, Y and LEVEL back, ~46 ms, so each
     * a short burst decaying into the sound): */
    {5, 3, {{0, 45}, {1, 15}, {4, 0}}},                          /* TINE: the e-piano's bark (NOISE Y: its FM) */
    {6, 2, {{0, 24}, {1, 0}}},                                   /* CLICK: the organ's key click (NOISE LEVEL) */
    {6, 3, {{0, 36}, {3, 12}, {8, 0}}},                          /* PERC: the percussion, dying away (NOISE LEVEL) */
    {5, 3, {{0, -40}, {1, -10}, {3, 0}}},                        /* PICK: the plectrum (NOISE Y: a narrow pulse) */
    {5, 3, {{0, 45}, {1, 10}, {3, 0}}},                          /* TONE: the finger, the bow plucked (NOISE Y: TONE) */
    {5, 2, {{0, 50}, {1, 0}}},                                   /* STRIKE: the mallet, the stick (NOISE Y: FOLD, FM) */
    {0, 3, {{0, -40}, {1, -10}, {3, 0}}},                        /* PUNCH: the synth bass's thump (CHAOS: the cycle) */
    {0, 3, {{0, 30}, {1, 8}, {3, 0}}},                           /* SLAP: the thumb's pop (CHAOS) */
    {5, 3, {{0, -22}, {64, 30}, {127, -22}}, 18},               /* HOOVER: the pulses' width swept (NOISE Y: PWM), a
                                                                 * triangle at 2 Hz (LFO 2 HZ), as the Alpha Juno's */
};
#define NZ(a, b) (1 + (a) * 10 + (b))            /* noise program A0 = NZ(0, 0) .. C9 = NZ(2, 9) */
#define NOFX 0, 0, 0, 0, 0, 0
#define NOSH 0, 0, 0, 0, 0, 0
#define NOOSC 0, 0, 0, 0, 0, 0                   /* (the circuit, no MOD ENV: to reach SCREAM) */
/* 128 in all, in the order of General MIDI (program change 0..127 = GM program 1..128): each GM sound as near as
 * the circuit, the noise programs, the resonators and the waveguides get (brass, bowed strings and reeds:
 * the BRASS, BOW and REED waveguides). */
/* the exciters, from the oscillators' measured spectra (a held A3 through the open gate): the circuit's periodic
 * regimes are near sines (the harmonics falling 12 .. 26 dB an octave; OUTER LOOP the odd ones only: a clarinet's),
 * its chaos rumbles below the note; the bright harmonic sources are the clusters (PW CLUSTER: centroid 6 x the
 * note, -5 dB/oct; PARTIAL CLUST x 0: a saw Y darkens), the inharmonic clicks CROSS MOD RING and ARRAY ROCKS
 * (centroid 11 .. 18 x), the noise around the note PRIME C NOISE (x high: breath, rosin, a felt mallet). Plucks and
 * strikes: a short burst of the brightness the instrument's hammer, plectrum or mallet has, at full level (the
 * pianos: their own hammer, the burst only the action's knock); bows, reeds, flutes, accordions and bagpipes: the
 * noise around the note on the breath (PRIME C NOISE), the rosin, the air; brass: the clean cycle (the lips buzz
 * in the model); the choir: detuned saws */
#define CYC 6, 64, 64                            /* the pure cycle, Y: near a sine (a soft mallet, a clean breath) */
#define NZK 85, 76, 0                            /* (a noise program: the circuit's knobs kept for going back) */
#define ROOM 85, 72, 0, 30, SI_OCT, 70           /* the shimmer as a plain room (SHIMMER 0) */
#define HALL 115, 96, 0, 40, SI_OCT, 80          /* a big plain hall */
#define AIR 110, 100, 60, 45, SI_OCT, 90         /* a hall that sinks a little */
#define SH_DFILT 85u                             /* a delay's FILTER by default: ~1 kHz (150 Hz x 2^(85/127 x 4.1)) */
#define PNOB 40, 30, 0, 72, SI_OCT, 90           /* an instrument's body (with .sh_space = SS_PIANO ...): */
#define GTRB 30, 20, 0, 72, SI_OCT, 90           /* its wood and a small room around it */
#define VLNB 30, 20, 0, 72, SI_OCT, 100
#define CELB 40, 30, 0, 72, SI_OCT, 90
#define HRPB 50, 40, 0, 72, SI_OCT, 90
#define ACCB 40, 30, 0, 72, SI_OCT, 90           /* the accordion's reed box; the bagpipe (its drones ring) */
#define BAGB 70, 60, 0, 72, SI_OCT, 90
#define ORGB 120, 110, 0, 64, SI_OCT, 80         /* the pipe organ: its case and its church */
#define LESS 20, 20, 0, 90, SI_OCT, 90           /* the rotary speaker: slow (chorale), fast (tremolo) */
#define LESF 120, 20, 0, 90, SI_OCT, 90
static const preset_t PRESETS[] = {
    /* PIANO 1-8 */
    {"GRAND PIANO", 70, 76, 51, 0, 127, 127, 24, 0, RZ_PIANO, 0, 0, 0, 0, 0, 44, 34, 0, 46, SI_OCT, 96, 32, 70, 60, 0, 0, 0, 0, SS_PIANO, 64, 0},   /* (PIANO: a concert grand: its board) */
    {"BRIGHT PIANO", 60, 76, 58, 0, 127, 127, 22, 0, RZ_PIANO, 0, 0, 0, 0, 0, 36, 26, 0, 40, SI_OCT, 110, 32, 108, 54, 0, 0, 0, 0, SS_PIANO, 64, 0},   /* (PIANO: a hard hammer, open) */
    {"ELECTRIC GRAND", 40, 76, 40, 24, 127, 127, 20, 0, RZ_PIANO, 0, 0, 0, 0, 0, 24, 20, 0, 30, SI_OCT, 90, 32, 100, 92, 0, 0, 0, 0, SS_HALL, 64, 0},   /* (PIANO: a CP-70: short strings, no board, a touch of chorus) */
    {"HONKY-TONK", 56, 76, 51, 0, 127, 127, 22, 0, RZ_PIANO, 0, 0, 0, 0, 0, 20, 18, 0, 30, SI_OCT, 80, 32, 86, 124, 0, 0, 0, 0, SS_HALL, 64, 0, .mfx = MFP_TAPE_WARBLE},   /* (PIANO: the unisons far apart, a small room) */
    {"E.PIANO 1", NZK, 0, 92, 0, 72, 20, NOFX, 64, 28, SH_DFILT, 18, 0, 20, NZ(1, 3), 2, 18, .sh_space = SS_PINGPONG, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"E.PIANO 2", NZK, 0, 88, 10, 70, 22, NOFX, 64, 28, SH_DFILT, 18, 0, 20, NZ(1, 3), 8, 45, .sh_space = SS_PINGPONG, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"HARPSICHORD", 85, 100, 96, 0, 87, 52, 62, 6, 2, 40, 90, 45, 50, 90, 100, 78, 40, 0, 0, 90, 18, 0, 35, 6, 0, 55, 0, 0, 95, 64},   /* (yours: saved on the FM-1) */
    {"CLAVINET", 38, 100, 32, 0, 54, 66, 74, 110, 0, 52, 119, 76, 30, 0, 100, 78, 40, 0, 0, 90, 0, 39, 0, 6, 0, 55, 0, 0, 68, 64, .oct = 1},   /* (yours: saved on the FM-1) */
    /* CHROMATIC PERCUSSION 9-16 */
    {"CELESTA", 54, 90, 64, 0, 25, 0, 76, 43, 0, 52, 119, 70, 30, 110, 112, 55, 95, 26, 0, 30, 9, 39, 0, 6, 0, 55, 0, SS_PINGPONG, 68, 64},   /* (yours: saved on the FM-1) */
    {"GLOCKENSPIEL", 85, 76, 0, 0, 36, 0, 36, 0, RZ_MODAL, 104, 127, 74, 28, 110, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .oct = 1, .eg = EG_STRIKE},
    {"MUSIC BOX", 6, 64, 64, 0, 42, 0, 70, 9, 0, 52, 127, 35, 40, 99, 60, 48, 20, 39, 0, 70, 7, 65, 0, 6, 0, 55, 0, 0, 52, 64, .mfx = MFP_TAPE_WARBLE},   /* (yours: saved on the FM-1) */
    {"VIBRAPHONE", 85, 76, 0, 0, 58, 0, 58, 0, RZ_MODAL, 92, 90, 84, 24, 110, 112, 55, 95, 26, 0, 30, 5, 40, 0, 0, 0, 0, 0, SS_PINGPONG, 100, 0, .mfx = MFP_VIBES_MOTOR, .eg = EG_STRIKE},
    {"MARIMBA", CYC, 0, 52, 0, 52, 0, RZ_MODAL, 90, 70, 54, 24, 48, .sh_size = 112, .sh_decay = 55, .sh_shimmer = 95, .sh_mix = 26, .sh_interval = 0, .sh_tone = 30, .sh_space = SS_PINGPONG},
    {"XYLOPHONE", 85, 76, 0, 0, 40, 0, 40, 0, RZ_MODAL, 116, 110, 48, 20, 100, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .oct = 1, .eg = EG_STRIKE},
    {"TUBULAR BELLS", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 104, 100, 78, 16, 115, 0, 0, 0, 0, SI_OCT, 0, 5, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .mfx = MFP_BELL_RING, .eg = EG_STRIKE},
    {"DULCIMER", 85, 76, 0, 0, 42, 0, 42, 0, RZ_SYMP, 4, 100, 82, 60, 90, 50, 40, 0, 72, SI_OCT, 90, 12, 64, 45, 0, 0, 0, 0, SS_HARP, 127, 0, .eg = EG_PICK},
    /* ORGAN 17-24 */
    {"DRAWBAR ORGAN", NZK, 24, 64, 127, 50, 0, NOFX, LESS, NZ(1, 9), 0, 12, .sh_space = SS_LESLIE, .mfx = MFP_ORGAN_VIB, .eg = EG_CLICK},
    {"PERC. ORGAN", NZK, 0, 60, 100, 50, 40, NOFX, LESS, NZ(1, 9), 2, 20, .sh_space = SS_LESLIE, .mfx = MFP_ORGAN_VIB, .eg = EG_PERC},
    {"ROCK ORGAN", NZK, 2, 64, 127, 50, 20, NOFX, LESF, NZ(1, 9), 6, 70, .sh_space = SS_LESLIE, .mfx = MFP_ORGAN_VIB, .eg = EG_CLICK},
    {"CHURCH ORGAN", NZK, 24, 64, 127, 80, 0, NOFX, ORGB, NZ(1, 9), 2, 8, .sh_space = SS_ORGAN},
    {"REED ORGAN", NZK, 20, 64, 120, 60, 10, RZ_ACCORD, 15, 50, 40, 30, 120, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_REEDSWELL, .mfx = MFP_ORGAN_VIB},
    {"ACCORDION", NZK, 24, 64, 120, 56, 10, RZ_ACCORD, 70, 80, 60, 64, 127, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_REEDSWELL},
    {"HARMONICA", NZK, 24, 64, 115, 56, 30, RZ_ACCORD, 0, 110, 80, 0, 127, NOSH, NZ(1, 6), 126, 63, 0, 0, 0, 15,
     .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"TANGO ACCORD.", NZK, 16, 64, 120, 56, 20, RZ_ACCORD, 20, 60, 70, 30, 127, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_REEDSWELL},
    /* GUITAR 25-32 */
    {"NYLON GUITAR", 85, 76, 0, 0, 58, 0, 58, 0, RZ_STRING, 45, 110, 67, 40, 127, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 45, 0, 0, 0, 0, SS_GUITAR, 127, 0, .oct = -1, .eg = EG_PICK},
    {"STEEL GUITAR", 85, 76, 0, 0, 56, 0, 56, 0, RZ_STRING, 40, 125, 75, 30, 127, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .oct = -1, .eg = EG_PICK},
    {"JAZZ GUITAR", 85, 76, 0, 0, 46, 0, 46, 0, RZ_STRING, 44, 65, 63, 50, 127, 30, 20, 0, 40, SI_OCT, 90, 19, 0, 0, 0, 0, 0, 0, SS_GUITAR, 110, 0, .oct = -1, .eg = EG_TONE},
    {"CLEAN GUITAR", 85, 76, 0, 0, 58, 0, 58, 0, RZ_STRING, 45, 115, 72, 40, 100, 64, 55, SH_DFILT, 28, 0, 20, 12, 64, 25, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .oct = -1, .mfx = MFP_EP_TREMOLO, .eg = EG_PICK},
    {"MUTED GUITAR", 85, 76, 0, 0, 40, 0, 40, 0, RZ_STRING, 40, 95, 22, 40, 100, 30, 20, 0, 40, SI_OCT, 90, 19, 0, 0, 0, 0, 0, 0, SS_GUITAR, 72, 0, .oct = -1, .eg = EG_TONE},
    {"OVERDRIVE GTR", 110, 64, 96, 24, 80, 100, 64, 60, RZ_STRING, 45, 115, 72, 40, 50, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -1},
    {"DISTORTION GTR", 124, 64, 96, 2, 64, 110, 64, 50, NOFX, ROOM, .oct = -1},
    {"GTR HARMONICS", 54, 90, 64, 0, 70, 0, 74, 60, RZ_MODAL, 8, 120, 75, 50, 120},
    /* BASS 33-40 */
    {"ACOUSTIC BASS", 85, 76, 0, 0, 50, 0, 50, 0, RZ_STRING, 40, 30, 54, 30, 100, 40, 30, 0, 72, SI_OCT, 90, 19, 0, 0, 0, 0, 0, 0, SS_CELLO, 72, 0, .oct = -2, .eg = EG_TONE},
    {"FINGER BASS", 85, 76, 0, 0, 50, 0, 50, 0, RZ_STRING, 40, 50, 60, 30, 90, 0, 0, 0, 0, SI_OCT, 0, 19, 0, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = -2, .eg = EG_TONE},
    {"PICK BASS", 85, 76, 0, 0, 46, 0, 46, 0, RZ_STRING, 40, 90, 60, 20, 90, 0, 0, 0, 0, SI_OCT, 0, 12, 0, 63, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = -2, .eg = EG_PICK},
    {"FRETLESS BASS", 6, 30, 64, 0, 70, 90, 56, 40, RZ_STRING, 50, 40, 67, 40, 50, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"SLAP BASS 1", 54, 50, 64, 0, 62, 40, 52, 100, .oct = -2, .eg = EG_SLAP},
    {"SLAP BASS 2", 38, 70, 32, 0, 62, 40, 52, 120, 8, 110, 90, 88, 60, 0, 30, 20, 0, 0, 0, 100, 9, 63, 63, 80, 10, 0, 0, 6, 64, 64, .oct = -2},   /* (yours: saved on the FM-1) */
    {"SYNTH BASS 1", 85, 20, 0, 0, 60, 100, 50, 30, .oct = -2, .eg = EG_PUNCH},
    {"SYNTH BASS 2", 54, 70, 16, 0, 60, 40, 54, 120, .oct = -2},
    /* STRINGS 41-48: the BOW waveguide */
    {"VIOLIN", 85, 76, 0, 62, 80, 118, 70, 20, RZ_BOW, 110, 90, 88, 60, 127, 30, 20, 0, 72, SI_OCT, 100, 17, 63, 63, 0, 0, 0, 0, SS_VIOLIN, 0, 0, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"VIOLA", 85, 76, 0, 64, 80, 118, 70, 20, RZ_BOW, 110, 75, 88, 64, 127, 30, 20, 0, 72, SI_OCT, 100, 17, 63, 63, 0, 0, 0, 0, SS_VIOLIN, 0, 0, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"CELLO", 85, 76, 0, 66, 80, 118, 72, 20, RZ_BOW, 115, 60, 93, 70, 127, 40, 30, 0, 72, SI_OCT, 90, 17, 63, 63, 0, 0, 0, 0, SS_CELLO, 0, 0, .eg = EG_SWELL, .oct = -1, .mfx = MFP_STRING_VIB},
    {"CONTRABASS", 85, 76, 0, 68, 80, 118, 72, 20, RZ_BOW, 120, 45, 93, 74, 127, 40, 30, 0, 72, SI_OCT, 90, 17, 63, 63, 0, 0, 0, 0, SS_CELLO, 0, 0, .eg = EG_SWELL, .oct = -2},
    {"TREMOLO STR.", NZK, 40, 80, 110, 80, 30, RZ_BOW, 105, 85, 88, 60, 127, HALL, NZ(1, 6), 63, 63, .eg = EG_SWELL},
    {"PIZZICATO", 85, 76, 0, 0, 42, 0, 42, 0, RZ_STRING, 40, 70, 37, 40, 100, 30, 20, 0, 72, SI_OCT, 100, 19, 0, 63, 0, 0, 0, 0, SS_VIOLIN, 72, 0, .eg = EG_TONE},
    {"HARP", 85, 76, 0, 0, 46, 0, 46, 0, RZ_STRING, 40, 90, 45, 50, 90, 50, 40, 0, 72, SI_OCT, 90, 12, 64, 45, 0, 0, 0, 0, SS_HARP, 127, 0, .eg = EG_PICK},
    {"TIMPANI", NZK, 0, 40, 0, 40, 50, RZ_DRUM, 120, 90, 95, 30, 127, ROOM, NZ(1, 6), 126, 0, .oct = -1},
    /* ENSEMBLE 49-56 */
    {"STRINGS", NZK, 80, 96, 115, 96, 20, RZ_BOW, 110, 80, 93, 64, 110, HALL, NZ(1, 0), 40, 30, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"SLOW STRINGS", NZK, 96, 100, 115, 100, 20, RZ_BOW, 110, 70, 93, 70, 110, HALL, NZ(1, 0), 40, 30, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"SYNTH STRINGS 1", NZK, 60, 90, 110, 90, 40, NOFX, AIR, NZ(1, 0), 20, 100},
    {"SYNTH STRINGS 2", NZK, 70, 90, 110, 94, 30, NOFX, AIR, NZ(1, 9), 6, 40},
    {"CHOIR AAHS", NZK, 80, 96, 115, 100, 20, RZ_VOICE, 64, 80, 48, 60, 110, HALL, NZ(1, 0), 8, 40, .eg = EG_VOWEL},
    {"VOICE OOHS", NZK, 80, 96, 115, 100, 20, RZ_VOICE, 64, 60, 48, 4, 110, HALL, NZ(1, 0), 8, 40, .eg = EG_VOWEL, .mfx = MFP_BREATH_VIB},
    {"SYNTH VOICE", NZK, 70, 90, 110, 96, 30, RZ_VOICE, 40, 90, 48, 90, 100, 60, 30, 64, 90, 0, 60, NZ(1, 1), 10, 80, .eg = EG_VOWEL, .sh_space = SS_MOUTH},
    {"ORCHESTRA HIT", CYC, 0, 70, 0, 74, 40, RZ_BRASS, 110, 110, 68, 0, 110, HALL},
    /* BRASS 57-64: the BRASS waveguide (the synth brass: saws) */
    {"TRUMPET", CYC, 26, 80, 112, 60, 20, RZ_BRASS, 100, 100, 68, 0, 127, ROOM, NOOSC, 10, .eg = EG_BLOOM},
    {"TROMBONE", CYC, 34, 80, 112, 64, 20, RZ_BRASS, 70, 80, 73, 0, 127, ROOM, .eg = EG_BLOOM, .oct = -1},
    {"TUBA", CYC, 36, 80, 112, 64, 20, RZ_BRASS, 30, 55, 78, 0, 127, .eg = EG_BLOOM, .oct = -2},
    {"MUTED TRUMPET", CYC, 24, 76, 110, 60, 20, RZ_BRASS, 90, 100, 68, 110, 127, ROOM, .eg = EG_BLOOM},
    {"FRENCH HORN", CYC, 44, 84, 112, 76, 20, RZ_BRASS, 20, 50, 83, 0, 127, HALL, .eg = EG_BLOOM, .oct = -1},
    {"BRASS SECTION", CYC, 30, 80, 112, 64, 30, RZ_BRASS, 90, 95, 68, 0, 127, ROOM, NOOSC, 10, .eg = EG_BLOOM},
    {"SYNTH BRASS 1", NZK, 24, 70, 90, 60, 80, RZ_BRASS, 110, 120, 63, 0, 90, NOSH, NZ(1, 0), 10, 40, .menv_len = 62, .menv_skew = 0, .menv_height = 160, .menv_bend = 96},
    {"SYNTH BRASS 2", NZK, 40, 80, 100, 70, 60, RZ_BRASS, 80, 100, 68, 40, 90, ROOM, NZ(1, 0), 16, 80, .eg = EG_BLOOM, .menv_len = 62, .menv_skew = 0, .menv_height = 160, .menv_bend = 96},
    /* REED 65-72: the REED waveguide (BORE: a cone, the clarinet a cylinder) */
    {"SOPRANO SAX", NZK, 20, 76, 115, 60, 20, RZ_REED, 70, 100, 78, 40, 127, ROOM, NZ(1, 6), 126, 63, 0, 0, 0, 10,
     .eg = EG_GROWL, .mfx = MFP_BREATH_VIB},
    {"ALTO SAX", NZK, 20, 76, 115, 60, 20, RZ_REED, 60, 90, 78, 60, 127, ROOM, NZ(1, 6), 126, 63, 0, 0, 0, 15,
     .eg = EG_GROWL, .mfx = MFP_BREATH_VIB},
    {"TENOR SAX", NZK, 24, 76, 115, 62, 20, RZ_REED, 55, 80, 78, 80, 127, ROOM, NZ(1, 6), 126, 63, 0, 0, 0, 25,
     .eg = EG_GROWL, .oct = -1, .mfx = MFP_BREATH_VIB},
    {"BARITONE SAX", NZK, 24, 76, 115, 62, 20, RZ_REED, 50, 65, 78, 100, 127, NOSH, NZ(1, 6), 126, 63, 0, 0, 0, 30,
     .eg = EG_GROWL, .oct = -1},
    {"OBOE", NZK, 20, 76, 115, 60, 20, RZ_REED, 110, 100, 78, 120, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"ENGLISH HORN", NZK, 24, 76, 115, 62, 20, RZ_REED, 100, 80, 78, 110, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"BASSOON", NZK, 24, 76, 115, 62, 20, RZ_REED, 90, 60, 83, 105, 127, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .eg = EG_REEDSWELL, .oct = -1},
    {"CLARINET", 124, 64, 64, 30, 76, 118, 60, 20, RZ_REED, 40, 80, 83, 0, 127, ROOM, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    /* PIPE 73-80: the FLUTE waveguide */
    {"PICCOLO", NZK, 30, 76, 110, 64, 20, RZ_FLUTE, 40, 90, 78, 40, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .eg = EG_OVERTONE, .oct = 1, .mfx = MFP_BREATH_VIB},
    {"FLUTE", NZK, 34, 78, 110, 70, 20, RZ_FLUTE, 30, 70, 78, 50, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    {"RECORDER", NZK, 30, 76, 110, 64, 20, RZ_FLUTE, 20, 60, 78, 20, 127, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    {"PAN FLUTE", NZK, 40, 80, 110, 76, 20, RZ_FLUTE, 30, 60, 78, 100, 127, HALL, NZ(1, 6), 126, 63, .eg = EG_CHIFF, .mfx = MFP_BREATH_VIB},
    {"BLOWN BOTTLE", NZK, 50, 80, 110, 74, 20, RZ_FLUTE, 0, 30, 78, 110, 127, NOSH, NZ(1, 6), 126, 63, .eg = EG_CHIFF},
    {"SHAKUHACHI", 85, 76, 0, 50, 84, 105, 80, 20, RZ_SHAKU, 70, 70, 78, 0, 127, 115, 96, 0, 40, SI_OCT, 80, 17, 126, 63, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_CHIFF, .mfx = MFP_BREATH_VIB},
    {"WHISTLE", 6, 64, 64, 20, 70, 115, 70, 20, RZ_FLUTE, 30, 90, 78, 20, 120, .eg = EG_OVERTONE, .oct = 1, .mfx = MFP_BREATH_VIB},
    {"OCARINA", 6, 64, 127, 30, 76, 110, 66, 20, RZ_FLUTE, 10, 30, 78, 10, 127, ROOM, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    /* SYNTH LEAD 81-88 */
    {"SQUARE LEAD", NZK, 24, 64, 115, 60, 40, NOFX, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 9), 0, 0, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"SAW LEAD", NZK, 24, 64, 115, 60, 50, NOFX, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 0), 4, 0, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"CALLIOPE", NZK, 24, 70, 110, 60, 20, RZ_FLUTE, 60, 100, 78, 80, 100, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 9), 0, 4, .eg = EG_CHIFF, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"CHIFF LEAD", CYC, 0, 70, 110, 60, 40, RZ_FLUTE, 40, 100, 88, 90, 110, .eg = EG_CHIFF, .sh_size = 64, .sh_decay = 60, .sh_shimmer = SH_DFILT, .sh_mix = 30, .sh_interval = 1, .sh_tone = 20, .sh_space = SS_PINGPONG, .mfx = MFP_BREATH_VIB},
    {"CHARANG", 120, 50, 96, 2, 66, 110, 60, 60, .sh_size = 64, .sh_decay = 60, .sh_shimmer = SH_DFILT, .sh_mix = 30, .sh_interval = 1, .sh_tone = 20, .sh_space = SS_PINGPONG},
    {"VOICE LEAD", NZK, 24, 70, 110, 64, 40, RZ_VOICE, 40, 90, 48, 64, 110, 64, 20, 64, 100, 0, 60,
     NZ(1, 0), 4, 20, .eg = EG_VOWEL, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .sh_space = SS_MOUTH, .mfx = MFP_BREATH_VIB},
    {"FIFTHS LEAD", NZK, 24, 70, 110, 64, 50, RZ_SYMP, 20, 90, 75, 110, 80, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 0), 4, 0, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"BASS + LEAD", 85, 76, 0, 6, 80, 110, 70, 50},
    /* SYNTH PAD 89-96 */
    {"NEW AGE PAD", 6, 64, 64, 90, 100, 110, 104, 30, RZ_MODAL, 100, 127, 90, 40, 60, 110, 98, 100, 62, SI_OCT, 100,
     .eg = EG_SHIMRISE},
    {"WARM PAD", 6, 30, 64, 92, 100, 115, 104, 20, NOFX, ROOM, .eg = EG_SHIMRISE, .mfx = MFP_SLOW_WOBBLE},
    {"POLYSYNTH", NZK, 20, 90, 80, 80, 60, NOFX, ROOM, NZ(1, 0), 8, 40, .eg = EG_SHIMRISE},
    {"CHOIR PAD", NZK, 96, 100, 115, 104, 30, RZ_VOICE, 64, 80, 48, 64, 90, 110, 98, 60, 52, SI_OCT, 90,
     NZ(1, 0), 8, 30, .eg = EG_VOWEL},
    {"BOWED PAD", CYC, 96, 100, 115, 104, 20, RZ_BOW, 90, 70, 98, 100, 110, AIR, .eg = EG_SHIMRISE},
    {"METALLIC PAD", NZK, 90, 100, 110, 104, 40, NOFX, 127, 113, 80, 58, SI_OCT5, 90, NZ(1, 5), 30, 20, .eg = EG_SHIMRISE, .mfx = MFP_METAL_BELL},
    {"HALO PAD", NZK, 96, 100, 115, 104, 30, RZ_VOICE, 40, 60, 48, 20, 90, 127, 115, 90, 58, SI_OCT, 90,
     NZ(1, 0), 6, 20, .eg = EG_VOWEL, .mfx = MFP_SLOW_SWELL},
    {"SWEEP PAD", 85, 76, 64, 100, 104, 115, 104, 60, NOFX, 110, 98, 40, 45, SI_OCT, 80, .eg = EG_OPEN, .mfx = MFP_SLOW_WOBBLE},
    /* SYNTH EFFECTS 97-104 */
    {"RAIN", NZK, 0, 70, 60, 80, 30, NOFX, 110, 98, 100, 62, SI_OCT, 100, NZ(0, 7), 90, 50},
    {"SOUNDTRACK", 85, 100, 127, 110, 110, 120, 115, 60, NOFX, 127, 115, 60, 65, SI_OCT, 70},
    {"CRYSTAL", 6, 100, 64, 0, 84, 0, 86, 20, RZ_MODAL, 100, 127, 90, 40, 80, 90, 98, 120, 65, SI_FIFTH, 110, .mfx = MFP_FIFTH_BELL},
    {"ATMOSPHERE", CYC, 0, 80, 0, 90, 40, RZ_STRING, 40, 90, 75, 40, 90, 127, 108, 50, 52, SI_OCT, 80, .mfx = MFP_SLOW_WOBBLE},
    {"BRIGHTNESS", NZK, 40, 90, 110, 96, 40, NOFX, 120, 103, 90, 52, SI_2OCT, 100, NZ(1, 9), 8, 60},
    {"GOBLINS", 6, 64, 64, 40, 90, 100, 90, 30, RZ_HAWK, 70, 90, 58, 70, 127, 110, 88, 40, 22, SI_OCT, 90, .mfx = MFP_DIRTY_RING},
    {"ECHOES", 54, 90, 64, 0, 70, 0, 80, 40, RZ_MODAL, 70, 100, 82, 12, 80, 64, 70, SH_DFILT, 32, 3, 60, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG},
    {"SCI-FI", NZK, 24, 80, 100, 90, 30, NOFX, AIR, NZ(2, 1), 50, 30, 100, 127, 77, .mfx = MFP_ROBOT},
    /* ETHNIC 105-112 */
    {"SITAR", 85, 76, 0, 0, 46, 0, 46, 10, RZ_SYMP, 16, 105, 78, 70, 90, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"BANJO", 85, 76, 0, 0, 52, 0, 52, 0, RZ_STRING, 20, 127, 45, 20, 100, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"SHAMISEN", 85, 76, 0, 0, 50, 0, 50, 0, RZ_STRING, 10, 127, 37, 30, 127, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"KOTO", 85, 76, 0, 0, 46, 0, 46, 0, RZ_STRING, 60, 95, 72, 20, 85, 50, 40, 0, 72, SI_OCT, 90, 12, 64, 45, 0, 0, 0, 0, SS_HARP, 127, 0, .eg = EG_PICK},
    {"KALIMBA", 85, 76, 0, 0, 42, 0, 42, 0, RZ_KALIMBA, 20, 80, 64, 0, 127, 50, 40, 0, 72, SI_OCT, 90, 19, 0, 0, 0, 0, 0, 0, SS_HARP, 72, 0, .eg = EG_TONE},
    {"BAGPIPE", NZK, 10, 70, 127, 60, 20, RZ_BAGPIPE, 20, 100, 80, 0, 127, BAGB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_BAGPIPE},
    {"FIDDLE", NZK, 36, 76, 115, 66, 30, RZ_BOW, 100, 110, 83, 45, 127, VLNB, NZ(1, 6), 63, 63, 0, 0, 0, 12, .sh_space = SS_VIOLIN, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"SHANAI", CYC, 24, 76, 115, 60, 20, RZ_REED, 120, 120, 73, 125, 127, ROOM, NOOSC, 20, .eg = EG_GROWL, .mfx = MFP_BREATH_VIB},
    /* PERCUSSIVE 113-120 */
    {"TINKLE BELL", 85, 76, 0, 0, 34, 0, 34, 0, RZ_MODAL, 70, 120, 74, 12, 105, 85, 72, 0, 30, SI_OCT, 70, 25, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = 1},
    {"AGOGO", 85, 76, 0, 0, 38, 0, 38, 0, RZ_MODAL, 116, 100, 60, 40, 110, 0, 0, 0, 0, SI_OCT, 0, 25, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0},
    {"STEEL DRUMS", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 90, 110, 62, 40, 110, 0, 0, 0, 0, SI_OCT, 0, 19, 0, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .eg = EG_TONE},
    {"WOODBLOCK", 85, 76, 0, 0, 34, 0, 34, 0, RZ_MODAL, 30, 70, 37, 20, 90, 0, 0, 0, 0, SI_OCT, 0, 5, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .eg = EG_STRIKE},
    {"TAIKO", 85, 76, 0, 0, 42, 0, 42, 0, RZ_DRUM, 0, 60, 80, 20, 127, 115, 96, 0, 40, SI_OCT, 80, 3, 126, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = -1, .eg = EG_STRIKE},
    {"MELODIC TOM", 85, 76, 0, 0, 40, 0, 40, 0, RZ_DRUM, 10, 80, 70, 40, 127, 0, 0, 0, 0, SI_OCT, 0, 3, 126, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .eg = EG_STRIKE},
    {"SYNTH DRUM", NZK, 0, 52, 0, 52, 30, NOFX, NOSH, NZ(0, 3), 30, 30, 44, 0, 122},
    {"REVERSE CYMBAL", NZK, 100, 30, 0, 30, 20, NOFX, NOSH, NZ(1, 6), 110, 90},
    /* SOUND EFFECTS 121-128 */
    {"GTR FRET NOISE", NZK, 0, 40, 0, 44, 60, RZ_STRING, 40, 90, 45, 40, 60, GTRB, NZ(0, 6), 40, 40, 30, 0, 68, .sh_space = SS_GUITAR},
    {"BREATH NOISE", NZK, 40, 70, 60, 70, 10, RZ_FLUTE, 30, 60, 78, 127, 70, NOSH, NZ(1, 6), 127, 127, .eg = EG_CHIFF},
    {"SEASHORE", NZK, 110, 110, 100, 115, 10, NOFX, HALL, NZ(1, 6), 127, 127},
    {"BIRD TWEET", 6, 64, 64, 0, 90, 0, 60, 30, RZ_SYRINX, 90, 70, 48, 15, 127, 100, 78, 30, 26, SI_OCT, 100},
    {"TELEPHONE", NZK, 24, 64, 127, 40, 30, NOFX, NOSH, NZ(0, 2), 30, 20, .mfx = MFP_HELICOPTER},
    {"HELICOPTER", NZK, 24, 80, 120, 80, 30, NOFX, NOSH, NZ(0, 9), 20, 40, .mfx = MFP_HELICOPTER},
    {"APPLAUSE", NZK, 40, 80, 110, 90, 10, NOFX, HALL, NZ(0, 8), 100, 80},
    {"GUNSHOT", NZK, 0, 60, 0, 60, 30, NOFX, HALL, NZ(2, 0), 80, 20},
    /* ---- BANK 2 (bank select LSB / MSB 1): an alternative take on each GM program, in the same order ---- */
    /* PIANO 1-8 */
    {"MELLOW GRAND", 82, 76, 48, 0, 127, 127, 30, 0, RZ_PIANO, 0, 0, 0, 0, 0, 52, 40, 0, 56, SI_OCT, 70, 32, 26, 60, 0, 0, 0, 0, SS_PIANO, 64, 0},   /* (PIANO: soft felt, the board warm) */
    {"ROCK PIANO", 46, 76, 60, 0, 127, 127, 20, 0, RZ_PIANO, 0, 0, 0, 0, 0, 22, 20, 0, 28, SI_OCT, 110, 32, 116, 72, 0, 0, 0, 0, SS_HALL, 64, 0},   /* (PIANO: hard, short, upfront) */
    {"DETUNED CP", 40, 76, 40, 24, 127, 127, 20, 0, RZ_PIANO, 0, 0, 0, 0, 0, 30, 24, 0, 34, SI_OCT, 90, 32, 104, 127, 0, 0, 0, 0, SS_HALL, 64, 0, .mfx = MFP_TAPE_WARBLE},   /* (PIANO: a CP-70 out of tune) */
    {"SALOON PIANO", 50, 76, 51, 0, 127, 127, 22, 0, RZ_PIANO, 0, 0, 0, 0, 0, 0, 10, SH_DFILT, 36, 0, 0, 32, 94, 118, 0, 0, 0, 0, SS_PINGPONG, 64, 0, .mfx = MFP_TAPE_WARBLE},   /* (PIANO: an old upright, a slapback) */
    {"DETUNED EP 1", NZK, 0, 92, 0, 84, 20, RZ_MODAL, 10, 60, 62, 20, 40, 64, 28, SH_DFILT, 18, 0, 20, NZ(1, 3), 2, 18, .sh_space = SS_PINGPONG, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"FM BELL EP", NZK, 0, 84, 0, 80, 22, RZ_MODAL, 60, 85, 60, 16, 38, 64, 28, SH_DFILT, 18, 0, 20, NZ(1, 3), 8,
     70, .sh_space = SS_PINGPONG, .eg = EG_TINE},
    {"COUPLED HARPSI", 85, 76, 0, 0, 44, 0, 44, 0, RZ_SYMP, 40, 110, 50, 60, 90, 40, 30, 0, 72, SI_OCT, 90, 12, 0, 63, 0, 0, 0, 0, SS_PIANO, 72, 0, .eg = EG_PICK},
    {"PULSE CLAV", 38, 120, 48, 0, 62, 40, 50, 120, RZ_STRING, 50, 110, 30, 10, 60},
    /* CHROMATIC PERCUSSION 9-16 */
    {"SOFT CELESTA", 85, 76, 0, 0, 40, 0, 40, 0, RZ_MODAL, 100, 90, 74, 30, 110, 112, 55, 95, 26, 0, 30, 5, 30, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .oct = 1, .eg = EG_STRIKE},
    {"GLOCK HALL", 85, 76, 0, 0, 36, 0, 36, 0, RZ_MODAL, 104, 120, 84, 28, 127, 115, 96, 0, 40, SI_OCT, 80, 5, 63, 0, 0, 0, 0, 0, SS_CATHEDRAL, 72, 0, .oct = 1, .eg = EG_STRIKE},
    {"MUSIC BOX 2", 85, 76, 0, 0, 34, 0, 34, 0, RZ_MODAL, 96, 120, 92, 50, 80, 110, 100, 60, 45, SI_OCT, 90, 25, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = 1, .mfx = MFP_TAPE_WARBLE},
    {"WET VIBES", CYC, 0, 60, 0, 62, 0, RZ_MODAL, 92, 80, 72, 24, 95, 110, 100, 30, 50, SI_OCT, 80, .mfx = MFP_VIBES_MOTOR},
    {"BALAFON", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 80, 80, 52, 30, 80, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .eg = EG_STRIKE},
    {"HARD XYLO", 85, 76, 0, 0, 36, 0, 36, 0, RZ_MODAL, 120, 127, 44, 16, 110, 112, 55, 95, 26, 0, 30, 25, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .oct = 1},
    {"CHURCH BELL", 85, 76, 0, 0, 50, 0, 50, 0, RZ_MODAL, 110, 90, 90, 10, 127, 115, 96, 0, 40, SI_OCT, 80, 5, 63, 0, 0, 0, 0, 0, SS_CATHEDRAL, 72, 0, .oct = -1, .mfx = MFP_BELL_RING, .eg = EG_STRIKE},
    {"SANTUR", 85, 76, 0, 0, 40, 0, 40, 0, RZ_SYMP, 10, 120, 80, 40, 90, 50, 40, 0, 72, SI_OCT, 90, 12, 0, 63, 0, 0, 0, 0, SS_HARP, 72, 0, .eg = EG_PICK},
    /* ORGAN 17-24 */
    {"70S ORGAN", NZK, 24, 64, 127, 50, 10, NOFX, LESS, NZ(1, 9), 4, 30, .sh_space = SS_LESLIE, .mfx = MFP_ORGAN_VIB, .eg = EG_CLICK},
    {"DETUNED PERC", NZK, 24, 58, 90, 50, 50, NOFX, LESS, NZ(1, 9), 8, 24, .sh_space = SS_LESLIE, .mfx = MFP_ORGAN_VIB},
    {"ROTARY ORGAN", NZK, 2, 64, 127, 54, 30, RZ_SYMP, 60, 90, 70, 60, 60, LESF, NZ(1, 9), 6, 90, .sh_space = SS_LESLIE, .eg = EG_CLICK, .mfx = MFP_ORGAN_VIB},
    {"CHURCH OCT MIX", NZK, 30, 64, 127, 90, 0, NOFX, ORGB, NZ(1, 9), 4, 4, .sh_space = SS_ORGAN},
    {"PUFF ORGAN", NZK, 30, 64, 120, 66, 10, RZ_ACCORD, 0, 40, 30, 0, 120, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_CHIFF, .mfx = MFP_ORGAN_VIB},
    {"ITALIAN ACCORD", NZK, 24, 64, 120, 56, 10, RZ_ACCORD, 110, 90, 60, 64, 127, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_REEDSWELL},
    {"BLUES HARP", NZK, 24, 64, 115, 56, 40, RZ_ACCORD, 0, 120, 90, 0, 127, NOSH, NZ(1, 6), 126, 63, 0, 0, 0, 30,
     .eg = EG_GROWL, .mfx = MFP_BREATH_VIB},
    {"BANDONEON", NZK, 24, 64, 120, 60, 20, RZ_ACCORD, 15, 40, 80, 30, 127, ACCB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_ACCORDION, .eg = EG_REEDSWELL},
    /* GUITAR 25-32 */
    {"UKULELE", 85, 76, 0, 0, 40, 0, 40, 0, RZ_STRING, 40, 105, 66, 34, 127, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"12-STRING GTR", 85, 76, 0, 0, 46, 0, 46, 0, RZ_SYMP, 40, 127, 75, 30, 90, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .oct = -1, .eg = EG_PICK},
    {"PEDAL STEEL", 85, 76, 0, 10, 60, 60, 60, 0, RZ_STRING, 50, 115, 80, 40, 100, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_SWELL, .oct = -1, .mfx = MFP_SLOW_WOBBLE},
    {"CHORUS GUITAR", 85, 76, 0, 0, 50, 0, 50, 0, RZ_STRING, 45, 125, 75, 40, 100, 110, 100, 60, 45, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_HALL, 127, 0, .oct = -1, .mfx = MFP_TAPE_WARBLE, .eg = EG_PICK},
    {"FUNK GUITAR", 85, 76, 0, 0, 36, 0, 36, 0, RZ_STRING, 40, 127, 30, 20, 100, 0, 0, 0, 0, SI_OCT, 0, 12, 0, 63, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = -1, .eg = EG_PICK},
    {"GUITAR PINCH", 110, 80, 96, 0, 76, 90, 64, 70, RZ_STRING, 50, 127, 72, 10, 60, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -1},
    {"FEEDBACK GTR", 124, 64, 96, 10, 80, 120, 70, 70, RZ_STRING, 45, 127, 90, 40, 40, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OPEN, .oct = -1},
    {"BELL HARMONICS", 54, 90, 64, 0, 76, 0, 80, 60, RZ_MODAL, 20, 127, 82, 30, 120, ROOM},
    /* BASS 33-40 */
    {"UPRIGHT JAZZ", 85, 76, 0, 0, 54, 0, 54, 0, RZ_STRING, 40, 20, 58, 40, 100, 40, 30, 0, 72, SI_OCT, 90, 19, 0, 0, 0, 0, 0, 0, SS_CELLO, 72, 0, .oct = -2, .eg = EG_TONE},
    {"FINGER SLAP", 85, 76, 0, 0, 56, 0, 56, 0, RZ_STRING, 40, 80, 64, 16, 127, 0, 0, 0, 0, SI_OCT, 0, 12, 0, 63, 0, 0, 0, 0, SS_HALL, 110, 0, .oct = -2},
    {"MUTED PICK BS", 85, 76, 0, 0, 40, 0, 40, 0, RZ_STRING, 40, 70, 24, 20, 90, 0, 0, 0, 0, SI_OCT, 0, 12, 0, 63, 0, 0, 0, 0, SS_HALL, 110, 0, .oct = -2},
    {"FRETLESS 2", 6, 40, 64, 0, 76, 100, 60, 50, RZ_STRING, 60, 30, 72, 50, 60, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"POP BASS", 54, 60, 64, 0, 62, 40, 50, 110, RZ_STRING, 40, 100, 50, 10, 50, .oct = -2, .eg = EG_SLAP},
    {"THUMB BASS", 38, 50, 32, 0, 58, 30, 54, 90, .oct = -2, .eg = EG_SLAP},
    /* the ACIDs: the TB-303: one saw (CLUSTER SAW, 2 voices, no spread) through the gate held near ringing (RING high:
     * the squelch), its envelope a quick fall to little (the cutoff sweep, DECAY ~0.15 s), a short release; a dotted
     * eighth ping-pong (the classic) */
    {"ACID BASS", NZK, 0, 72, 24, 40, 118, NOFX, 64, 70, 95, 30, 1, 30, NZ(1, 0), 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .oct = -2},
    {"RUBBER BASS", 6, 40, 64, 0, 64, 90, 56, 60, .oct = -2},
    /* STRINGS 41-48 */
    {"SLOW VIOLIN", 85, 76, 0, 98, 90, 118, 80, 20, RZ_BOW, 110, 80, 88, 60, 127, 30, 20, 0, 72, SI_OCT, 100, 17, 63, 63, 0, 0, 0, 0, SS_VIOLIN, 0, 0, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"SUL PONT VIOLA", 85, 76, 0, 64, 80, 118, 70, 20, RZ_BOW, 100, 110, 83, 10, 127, 30, 20, 0, 72, SI_OCT, 100, 17, 63, 63, 0, 0, 0, 0, SS_VIOLIN, 0, 0, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"SLOW CELLO", 85, 76, 0, 102, 90, 118, 82, 20, RZ_BOW, 115, 55, 93, 70, 127, 40, 30, 0, 72, SI_OCT, 90, 17, 63, 63, 0, 0, 0, 0, SS_CELLO, 0, 0, .eg = EG_SWELL, .oct = -1, .mfx = MFP_STRING_VIB},
    {"BOWED BASS", 85, 76, 0, 58, 76, 118, 66, 30, RZ_BOW, 120, 60, 88, 60, 127, 40, 30, 0, 72, SI_OCT, 90, 17, 63, 63, 0, 0, 0, 0, SS_CELLO, 0, 0, .eg = EG_SWELL, .oct = -2},
    {"SLOW TREMOLO", NZK, 80, 90, 110, 90, 30, RZ_BOW, 105, 75, 93, 60, 127, AIR, NZ(1, 6), 63, 63,
     .eg = EG_SWELL},
    {"HALL PIZZ", 85, 76, 0, 0, 46, 0, 46, 0, RZ_STRING, 40, 60, 40, 40, 100, 115, 96, 0, 40, SI_OCT, 80, 19, 0, 63, 0, 0, 0, 0, SS_HALL, 72, 0, .eg = EG_TONE},
    {"YANG QIN", 85, 76, 0, 0, 44, 0, 44, 0, RZ_SYMP, 20, 110, 72, 30, 90, 50, 40, 0, 72, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_HARP, 127, 0, .eg = EG_PICK},
    {"TIMPANI HALL", NZK, 0, 46, 0, 46, 50, RZ_DRUM, 120, 80, 98, 30, 127, HALL, NZ(1, 6), 126, 0,
     .sh_space = SS_CATHEDRAL, .oct = -1},
    /* ENSEMBLE 49-56 */
    {"ORCHESTRA", NZK, 70, 96, 115, 96, 20, RZ_BOW, 110, 70, 93, 64, 110, 120, 110, 20, 45, SI_OCT, 70, NZ(1, 0),
     40, 30, .sh_space = SS_CATHEDRAL, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"LEGATO STRINGS", NZK, 90, 100, 120, 100, 20, RZ_BOW, 105, 70, 98, 70, 110, AIR, NZ(1, 0), 40, 30,
     .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"RESO STRINGS", NZK, 60, 90, 110, 90, 70, NOFX, AIR, NZ(1, 0), 20, 100, .eg = EG_OPEN},
    {"SOLINA", NZK, 60, 90, 115, 90, 20, RZ_SYMP, 60, 60, 80, 40, 60, AIR, NZ(1, 0), 10, 60, .mfx = MFP_TAPE_WARBLE},
    {"CHORUS AAHS", NZK, 90, 100, 115, 104, 20, RZ_VOICE, 40, 70, 48, 50, 110, 120, 110, 20, 45, SI_OCT, 70,
     NZ(1, 0), 8, 40, .sh_space = SS_CATHEDRAL, .eg = EG_VOWEL},
    {"HUMMING", NZK, 80, 96, 115, 100, 10, RZ_VOICE, 90, 40, 52, 0, 110, HALL, NZ(1, 0), 8, 20, .mfx = MFP_BREATH_VIB},
    {"ANALOG VOICE", NZK, 60, 90, 110, 96, 50, RZ_VOICE, 64, 100, 48, 30, 100, 60, 30, 64, 90, 0, 60, NZ(1, 9), 10, 60,
     .eg = EG_VOWEL, .sh_space = SS_MOUTH},
    {"BASS HIT", CYC, 0, 74, 0, 78, 40, RZ_BRASS, 60, 80, 73, 0, 110, HALL, .oct = -1},
    /* BRASS 57-64 */
    {"DARK TRUMPET", CYC, 30, 80, 112, 62, 20, RZ_BRASS, 80, 70, 73, 0, 127, ROOM, .eg = EG_BLOOM},
    {"BRIGHT BONE", CYC, 30, 78, 112, 62, 20, RZ_BRASS, 80, 110, 68, 0, 127, ROOM, NOOSC, 10, .eg = EG_BLOOM,
     .oct = -1},
    {"SOFT TUBA", CYC, 44, 84, 112, 66, 20, RZ_BRASS, 20, 40, 83, 0, 127, ROOM, .eg = EG_BLOOM, .oct = -2},
    {"WAH TRUMPET", CYC, 24, 76, 110, 60, 20, RZ_BRASS, 90, 100, 68, 90, 127, ROOM, .eg = EG_OPEN},
    {"HORN SECTION", CYC, 50, 84, 112, 80, 20, RZ_BRASS, 30, 60, 83, 0, 127, HALL, .sh_space = SS_TEMPLE,
     .eg = EG_BLOOM, .oct = -1},
    {"BRIGHT BRASS", CYC, 24, 76, 112, 60, 30, RZ_BRASS, 100, 120, 63, 0, 127, ROOM, NOOSC, 20, .eg = EG_BLOOM},
    {"JUMP BRASS", NZK, 24, 66, 90, 56, 90, RZ_BRASS, 110, 127, 63, 0, 90, ROOM, NZ(1, 0), 4, 60, .menv_len = 62, .menv_skew = 0, .menv_height = 160, .menv_bend = 96},
    {"ANALOG BRASS", 85, 76, 0, 40, 80, 100, 70, 70, NOFX, ROOM, .eg = EG_OPEN},
    /* REED 65-72 */
    {"SOFT SOPRANO", NZK, 30, 80, 115, 64, 20, RZ_REED, 80, 70, 83, 30, 127, HALL, NZ(1, 6), 126, 63,
     .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"BREATHY ALTO", NZK, 24, 78, 115, 62, 20, RZ_REED, 60, 70, 78, 60, 127, ROOM, NZ(1, 6), 126, 90,
     .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"GROWL TENOR", NZK, 20, 76, 115, 60, 20, RZ_REED, 55, 100, 73, 80, 127, ROOM, NZ(1, 6), 126, 63, 0, 0, 0, 50,
     .eg = EG_GROWL, .oct = -1},
    {"BARI ROCK", NZK, 20, 72, 115, 60, 30, RZ_REED, 45, 90, 73, 100, 127, NOSH, NZ(1, 6), 126, 63, 0, 0, 0, 60,
     .eg = EG_GROWL, .oct = -1},
    {"BAROQUE OBOE", NZK, 24, 76, 115, 62, 20, RZ_REED, 100, 80, 83, 124, 127, HALL, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_TEMPLE, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"DARK ENG HORN", NZK, 30, 78, 115, 64, 20, RZ_REED, 90, 60, 83, 110, 127, HALL, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"CONTRABASSOON", NZK, 30, 78, 115, 64, 20, RZ_REED, 80, 50, 88, 100, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .eg = EG_REEDSWELL, .oct = -2},
    {"BASS CLARINET", 124, 64, 64, 30, 76, 118, 62, 20, RZ_REED, 30, 60, 88, 0, 127, ROOM, .eg = EG_REEDSWELL,
     .oct = -1},
    /* PIPE 73-80 */
    {"FIFE", NZK, 20, 72, 110, 60, 20, RZ_FLUTE, 60, 110, 73, 30, 127, NOSH, NZ(1, 6), 126, 63, .eg = EG_CHIFF,
     .oct = 1, .mfx = MFP_BREATH_VIB},
    {"BREATHY FLUTE", NZK, 40, 80, 110, 72, 20, RZ_FLUTE, 30, 60, 78, 60, 127, HALL, NZ(1, 6), 126, 90,
     .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    {"ALTO RECORDER", NZK, 36, 78, 110, 66, 20, RZ_FLUTE, 10, 50, 83, 20, 127, ROOM, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    {"KENA", NZK, 36, 80, 110, 74, 20, RZ_FLUTE, 50, 80, 78, 80, 127, HALL, NZ(1, 6), 126, 63, .eg = EG_CHIFF, .mfx = MFP_BREATH_VIB},
    {"JUG", NZK, 40, 76, 110, 70, 20, RZ_FLUTE, 0, 20, 83, 120, 127, NOSH, NZ(1, 6), 126, 30, .eg = EG_CHIFF,
     .oct = -1},
    {"DIZI", NZK, 30, 76, 110, 66, 20, RZ_FLUTE, 70, 110, 73, 60, 127, ROOM, NZ(1, 6), 126, 63, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    {"TIN WHISTLE", NZK, 20, 70, 115, 62, 20, RZ_FLUTE, 40, 100, 78, 30, 120, ROOM, NZ(1, 6), 126, 63,
     .eg = EG_CHIFF, .oct = 1, .mfx = MFP_BREATH_VIB},
    {"XUN", 6, 64, 127, 40, 80, 110, 70, 20, RZ_FLUTE, 0, 20, 83, 30, 127, HALL, .sh_space = SS_TEMPLE,
     .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},
    /* SYNTH LEAD 81-88 */
    {"SINE LEAD", CYC, 2, 64, 115, 60, 20, NOFX, 64, 60, SH_DFILT, 30, 1, 20, .sh_space = SS_PINGPONG, .mfx = MFP_LEAD_VIB},
    {"DOUBLE SAW", NZK, 24, 64, 115, 60, 60, NOFX, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 0), 10, 40, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"VENT SYNTH", NZK, 20, 72, 110, 64, 30, RZ_FLUTE, 80, 110, 73, 90, 100, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 9), 4, 60, .eg = EG_CHIFF, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_BREATH_VIB},
    {"RUBBY", CYC, 0, 72, 110, 62, 60, RZ_FLUTE, 60, 90, 83, 40, 100, 64, 60, SH_DFILT, 30, 1, 20, .eg = EG_OPEN, .sh_space = SS_PINGPONG, .mfx = MFP_BREATH_VIB},
    {"DIST LEAD", 124, 50, 96, 2, 70, 115, 64, 80, NOFX, 64, 60, SH_DFILT, 30, 1, 20, .sh_space = SS_PINGPONG, .mfx = MFP_LEAD_VIB},
    {"VOX LEAD", NZK, 10, 70, 110, 64, 40, RZ_VOICE, 90, 70, 48, 20, 110, 64, 20, 64, 100, 0, 60, NZ(1, 0), 4, 20, .eg = EG_VOWEL, .sh_space = SS_MOUTH, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_BREATH_VIB},
    {"FOURTHS LEAD", NZK, 24, 70, 110, 64, 50, RZ_SYMP, 50, 90, 75, 80, 80, 64, 60, SH_DFILT, 30, 1, 20, NZ(1, 0), 4, 0, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"FAT LEAD", 85, 60, 0, 4, 80, 115, 70, 70, NOFX, ROOM, .mfx = MFP_LEAD_VIB},
    /* SYNTH PAD 89-96 */
    {"ICE PAD", 6, 100, 64, 90, 100, 110, 104, 30, RZ_MODAL, 110, 127, 92, 30, 60, 110, 98, 100, 62, SI_2OCT, 110,
     .eg = EG_SHIMRISE},
    {"SINE PAD", CYC, 96, 100, 115, 104, 10, NOFX, AIR, .eg = EG_SHIMRISE},
    {"POLY KING", NZK, 30, 90, 90, 84, 70, NOFX, AIR, NZ(1, 0), 12, 60, .eg = EG_OPEN},
    {"ITOPIA", NZK, 96, 100, 115, 104, 30, RZ_VOICE, 30, 90, 48, 90, 90, 127, 115, 100, 58, SI_OCT5, 90,
     NZ(1, 0), 8, 30, .sh_space = SS_CATHEDRAL, .eg = EG_VOWEL},
    {"GLASS BOW", CYC, 96, 100, 115, 104, 20, RZ_BOW, 70, 110, 98, 20, 110, 127, 113, 90, 58, SI_OCT, 100,
     .eg = EG_SHIMRISE},
    {"TINE PAD", NZK, 90, 100, 110, 104, 30, RZ_MODAL, 60, 110, 88, 20, 80, AIR, NZ(1, 3), 8, 40, .eg = EG_SHIMRISE},
    {"VOICE HALO", NZK, 96, 100, 115, 104, 30, RZ_VOICE, 90, 60, 52, 50, 90, 127, 115, 90, 58, SI_FIFTH, 90,
     NZ(1, 0), 6, 20, .sh_space = SS_MOSQUE, .eg = EG_VOWEL, .mfx = MFP_SLOW_SWELL},
    {"POLAR SWEEP", 85, 100, 64, 100, 104, 115, 104, 80, NOFX, 120, 108, 60, 52, SI_OCT, 90, .eg = EG_OPEN},
    /* SYNTH EFFECTS 97-104 */
    {"STORM", NZK, 20, 90, 80, 96, 30, NOFX, 127, 115, 60, 62, SI_OCT, 70, NZ(0, 7), 110, 30},
    {"DARK SCORE", 85, 60, 127, 110, 110, 120, 115, 50, NOFX, 127, 120, 40, 65, SI_FIFTH, 50,
     .sh_space = SS_CATHEDRAL, .mfx = MFP_SUB_RING},
    {"SYN MALLET", 85, 76, 0, 0, 50, 0, 54, 30, RZ_MODAL, 90, 110, 72, 24, 95, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 72, 0, .eg = EG_STRIKE},
    {"WARM ATMOS", CYC, 20, 90, 60, 96, 20, RZ_STRING, 40, 60, 80, 40, 90, 127, 108, 30, 52, SI_OCT, 60, .mfx = MFP_SLOW_WOBBLE},
    {"SHIMMER BELL", 6, 100, 64, 0, 84, 0, 90, 20, RZ_MODAL, 104, 127, 88, 20, 90, 127, 115, 110, 60, SI_2OCT, 110},
    {"GOBLIN SYNTH", 54, 90, 64, 40, 90, 100, 90, 60, RZ_HAWK, 40, 110, 58, 90, 100, 110, 88, 40, 22, SI_FIFTH, 90, .mfx = MFP_ROBOT},
    {"ECHO BELL", 85, 76, 0, 0, 70, 0, 80, 20, RZ_MODAL, 104, 110, 86, 12, 100, 127, 115, 0, 58, SI_OCT, 70, 5, 63, 0, 0, 0, 0, 0, SS_TEMPLE, 72, 0, .eg = EG_STRIKE},
    {"STARTHEME", NZK, 30, 90, 110, 96, 30, RZ_MODAL, 100, 120, 90, 30, 60, AIR, NZ(1, 9), 8, 80, .mfx = MFP_CHAOS_RING},
    /* ETHNIC 105-112 */
    {"SITAR 2", 85, 76, 0, 0, 50, 0, 50, 10, RZ_SYMP, 30, 125, 82, 40, 100, 30, 20, 0, 40, SI_OCT, 90, 12, 0, 63, 0, 0, 0, 0, SS_GUITAR, 72, 0, .eg = EG_PICK},
    {"MUTED BANJO", 85, 76, 0, 0, 40, 0, 40, 0, RZ_STRING, 20, 127, 40, 20, 127, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"TSUGARU", 85, 76, 0, 0, 42, 0, 42, 0, RZ_STRING, 10, 127, 45, 16, 100, 30, 20, 0, 40, SI_OCT, 90, 12, 64, 25, 0, 0, 0, 0, SS_GUITAR, 127, 0, .eg = EG_PICK},
    {"TAISHO KOTO", 85, 76, 0, 0, 46, 0, 46, 0, RZ_SYMP, 60, 115, 72, 30, 85, 50, 40, 0, 72, SI_OCT, 90, 12, 64, 45, 0, 0, 0, 0, SS_HARP, 127, 0, .eg = EG_PICK},
    {"MBIRA", 85, 76, 0, 0, 46, 0, 46, 0, RZ_KALIMBA, 60, 90, 72, 60, 127, 50, 40, 0, 72, SI_OCT, 90, 19, 0, 30, 0, 0, 0, 0, SS_HARP, 72, 0},
    {"DRONE PIPES", NZK, 10, 70, 127, 64, 20, RZ_BAGPIPE, 40, 90, 110, 0, 127, BAGB, .osc = NZ(1, 6), .np_x = 126, .np_y = 63, .np_level = 30, .sh_space = SS_BAGPIPE},
    {"FOLK FIDDLE", NZK, 30, 72, 115, 62, 30, RZ_BOW, 100, 120, 78, 30, 127, VLNB, NZ(1, 6), 63, 63, .sh_space = SS_VIOLIN, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"ZURNA", CYC, 16, 72, 115, 58, 30, RZ_REED, 127, 127, 68, 127, 127, ROOM, NOOSC, 30, .eg = EG_GROWL, .mfx = MFP_BREATH_VIB},
    /* PERCUSSIVE 113-120 */
    {"WIND CHIMES", 85, 76, 0, 0, 40, 0, 44, 0, RZ_MODAL, 80, 127, 88, 20, 120, 110, 100, 60, 45, SI_OCT, 90, 25, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = 1},
    {"COWBELL", 85, 76, 0, 0, 34, 0, 34, 0, RZ_MODAL, 120, 90, 50, 30, 110, 0, 0, 0, 0, SI_OCT, 0, 25, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0},
    {"GLASS DRUM", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 96, 127, 84, 30, 127, 85, 72, 0, 30, SI_OCT, 70, 19, 0, 30, 0, 0, 0, 0, SS_HALL, 72, 0, .eg = EG_TONE},
    {"CASTANETS", 85, 76, 0, 0, 30, 0, 30, 0, RZ_MODAL, 40, 110, 30, 10, 90, 0, 0, 0, 0, SI_OCT, 0, 5, 63, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = 1, .eg = EG_STRIKE},
    {"CONCERT BD", 85, 76, 0, 0, 46, 0, 46, 0, RZ_DRUM, 0, 40, 90, 20, 127, 115, 96, 0, 40, SI_OCT, 80, 3, 126, 0, 0, 0, 0, 0, SS_CATHEDRAL, 72, 0, .oct = -2, .eg = EG_STRIKE},
    {"DEEP TOM", 85, 76, 0, 0, 44, 0, 44, 0, RZ_DRUM, 20, 60, 80, 40, 127, 85, 72, 0, 30, SI_OCT, 70, 3, 126, 0, 0, 0, 0, 0, SS_HALL, 72, 0, .oct = -1, .eg = EG_STRIKE},
    {"808 TOM", CYC, 0, 56, 0, 56, 20, NOFX, NOSH, NZ(0, 3), 30, 30, 50, 0, 108},
    {"REVERSE SNARE", NZK, 90, 30, 0, 30, 10, NOFX, ROOM, NZ(0, 3), 60, 90},
    /* SOUND EFFECTS 121-128 */
    {"STRING SLAP", NZK, 0, 34, 0, 36, 60, RZ_STRING, 40, 100, 30, 20, 60, GTRB, NZ(0, 6), 40, 40, 30, 0, 68, .sh_space = SS_GUITAR},
    {"FLUTE KEY CLK", 85, 76, 0, 0, 30, 0, 30, 20, RZ_FLUTE, 30, 60, 30, 127, 90, 0, 0, 0, 0, SI_OCT, 0, 5, 63, 0, 0, 0, 0, 0, SS_HALL, 120, 0, .eg = EG_STRIKE},
    {"WIND", NZK, 110, 110, 100, 115, 40, NOFX, HALL, NZ(1, 6), 110, 40, .eg = EG_OPEN},
    {"TWO BIRDS", 6, 64, 64, 0, 90, 0, 60, 30, RZ_SYRINX, 60, 90, 58, 110, 127, 100, 78, 30, 26, SI_OCT, 100},
    {"DIAL TONE", CYC, 0, 64, 127, 40, 0, NOFX, NOSH},
    {"ENGINE", NZK, 20, 80, 120, 80, 40, NOFX, NOSH, NZ(0, 9), 60, 20, .oct = -1},
    {"CROWD", NZK, 60, 90, 110, 96, 10, NOFX, HALL, NZ(0, 8), 60, 100},
    /* DOMINATOR (Human Resource's): the Alpha Juno's "What the" hoover: six pulses detuned wide (PW CLUSTER), their
     * width swept by a 2 Hz triangle (FREE EG on NOISE Y: the PWM that makes it churn), each note swooping in from
     * under the pitch and up (MOD ENV down ~0.8 octave, curved: it hangs low a moment, then rises), the gate a little
     * resonant (the Juno's filter half open), held full, an octave down, a short dark room */
    {"DOMINATOR", NZK, 2, 80, 120, 46, 34, NOFX, 70, 60, 0, 28, SI_OCT, 60, NZ(1, 1), 96, 40, 74, 0, 210, 0, SS_HALL, 0, 20,
     .oct = -1, .eg = EG_HOOVER},
    /* ---- BANK 3: SYNTH (playable synthesizer sounds: leads, basses, pads, keys, plucks, brass and strings,
     * sequence sounds, bells; 16 of each) ---- */
    {"MONO SAW", 85, 76, 0, 24, 64, 115, 60, 50, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 11, 4, 0, 66, 0, 172, 0, SS_PINGPONG, 0, 96, .mfx = MFP_LEAD_VIB},
    {"WIDE SAW LEAD", 85, 76, 0, 24, 64, 115, 60, 40, 0, 100, 127, 90, 40, 0, 64, 60, SH_DFILT, 30, 1, 20, 11, 30, 60, 66, 0, 172, 0, SS_PINGPONG, 64, 96, .mfx = MFP_LEAD_VIB},   /* (yours: saved on the FM-1) */
    {"PWM LEAD", 85, 76, 0, 24, 64, 115, 60, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 12, 6, 40, 66, 0, 172, 0, SS_PINGPONG, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"SYNC LEAD", 85, 76, 0, 24, 64, 115, 60, 60, RZ_MODAL, 0, 0, 0, 0, 0, 64, 50, SH_DFILT, 28, 0, 10, 3, 70, 50, 66, 0, 172, 0, SS_PINGPONG, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"FM LEAD", 85, 76, 0, 2, 64, 115, 60, 56, 0, 100, 127, 90, 40, 0, 64, 60, SH_DFILT, 30, 1, 20, 11, 10, 0, 66, 0, 172, 0, SS_PINGPONG, 57, 96, .mfx = MFP_LEAD_VIB},   /* (yours: saved on the FM-1) */
    {"BANG SQUARE", 20, 64, 0, 24, 64, 115, 60, 30, RZ_MODAL, 0, 0, 0, 0, 0, 32, 55, SH_DFILT, 26, 0, 10, NZ(3, 0), 127, 0, 66, 0, 172, 0, SS_PINGPONG, .menv_bend = 96},
    {"CHUA LEAD", 38, 100, 32, 2, 64, 115, 60, 70, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_LEAD_VIB},
    {"SPIRAL LEAD", 54, 90, 64, 2, 64, 115, 60, 60, 9, 61, 90, 58, 110, 47, 64, 60, SH_DFILT, 30, 1, 20, 26, 10, 0, 66, 0, 172, 0, SS_PINGPONG, 57, 96, .mfx = MFP_LEAD_VIB},   /* (yours: saved on the FM-1) */
    {"SCREAMER", 110, 64, 96, 2, 64, 110, 60, 70, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"BANG SAW LEAD", 30, 64, 0, 24, 64, 115, 60, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, NZ(3, 0), 85, 0, .sh_space = SS_PINGPONG, .menv_len = 66, .menv_skew = 0, .menv_height = 172, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"SOFT FLUTE LEAD", 113, 0, 57, 0, 76, 127, 64, 30, 4, 40, 60, 6, 40, 92, 64, 60, SH_DFILT, 30, 1, 20, 4, 12, 20, 66, 0, 172, 0, SS_PINGPONG, 64, 96, .eg = EG_OVERTONE, .mfx = MFP_BREATH_VIB},   /* (yours: saved on the FM-1) */
    {"ACID LEAD", NZK, 24, 76, 40, 44, 122, NOFX, 64, 80, 105, 40, 1, 40, NZ(1, 0), 6, 0, 66, 0, 172, 0, SS_PINGPONG, 0, 96},
    {"BRASSY LEAD", 85, 76, 0, 6, 70, 110, 60, 50, RZ_BRASS, 110, 120, 63, 0, 80, 64, 60, SH_DFILT, 30, 1, 20, 11, 6, 20, 66, 0, 172, 0, SS_PINGPONG, 0, 96, .eg = EG_BLOOM, .mfx = MFP_LEAD_VIB},
    {"ATARI LEAD", 85, 76, 0, 2, 64, 115, 60, 77, 7, 110, 120, 63, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 22, 43, 8, 66, 0, 172, 0, SS_PINGPONG, 64, 96},   /* (yours: saved on the FM-1) */
    {"PHASE LEAD", 85, 76, 0, 2, 64, 115, 60, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 55, SH_DFILT, 28, 2, 10, 20, 10, 60, 66, 0, 172, 0, SS_PINGPONG, .menv_bend = 96, .mfx = MFP_LEAD_VIB},
    {"SEVENTH LEAD", 85, 76, 0, 24, 64, 115, 60, 50, RZ_SYMP, 70, 90, 75, 90, 70, 64, 60, SH_DFILT, 30, 1, 20, 11, 8, 0, 66, 0, 172, 0, SS_PINGPONG, 0, 96, .mfx = MFP_LEAD_VIB},
    {"SUB BASS", 6, 64, 64, 0, 70, 110, 40, 10, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"SAW BASS", 85, 76, 0, 24, 60, 100, 50, 60, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 11, 2, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"SQUARE BASS", 85, 76, 0, 24, 60, 100, 50, 50, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 12, 2, 64, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"CHUA BASS", 38, 64, 32, 0, 60, 100, 50, 80, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"ACID BASS 2", NZK, 0, 66, 16, 36, 124, NOFX, 64, 60, 90, 26, 1, 40, NZ(1, 0), 3, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .oct = -2},
    {"RESO BASS", 85, 76, 0, 0, 60, 80, 50, 100, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 11, 4, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"FM BASS", 85, 76, 0, 0, 60, 100, 50, 40, 1, 70, 90, 75, 90, 0, 85, 72, 0, 0, 0, 70, 16, 2, 80, 64, 0, 117, 0, 0, 64, 127, .oct = -2},   /* (yours: saved on the FM-1) */
    {"PLUCK BASS", 85, 76, 0, 0, 62, 40, 50, 60, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 19, 2, 30, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2, .eg = EG_TONE},
    {"ZAP BASS", 85, 76, 0, 0, 60, 100, 50, 60, 1, 70, 90, 75, 90, 0, 85, 72, 0, 0, 0, 70, 11, 2, 0, 67, 0, 90, 0, 0, 64, 127, .oct = -2},   /* (yours: saved on the FM-1) */
    {"DEEP BASS", 6, 30, 64, 0, 80, 110, 50, 30, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"WOBBLE BASS", 85, 76, 0, 24, 60, 100, 50, 62, 8, 120, 45, 93, 74, 0, 110, 100, 60, 0, 0, 90, 12, 4, 90, 80, 10, 0, 0, 2, 64, 64, .eg = EG_OPEN, .oct = -2, .mfx = MFP_PULSE_GATE},   /* (yours: saved on the FM-1) */
    {"GROWL BASS", 110, 40, 96, 0, 60, 100, 50, 70, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"STRING BASS SYN", 85, 76, 0, 0, 50, 0, 50, 30, RZ_STRING, 40, 50, 60, 30, 90, 0, 0, 0, 0, SI_OCT, 0, 19, 2, 0, 0, 0, 0, 0, SS_HALL, 127, 0, .oct = -2, .eg = EG_TONE},
    {"BANG BASS", 25, 64, 0, 24, 60, 100, 50, 60, RZ_MODAL, 0, 0, 0, 0, 0, NOSH, NZ(3, 0), 100, 0, .oct = -2},
    {"DRONE BASS", 6, 64, 64, 20, 90, 120, 80, 40, RZ_BOW, 120, 45, 93, 74, 100, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -2},
    {"BOING BASS", 6, 127, 106, 0, 60, 70, 50, 67, 8, 120, 45, 93, 74, 0, 85, 72, 0, 0, 0, 70, 8, 0, 12, 80, 10, 0, 0, 0, 97, 64, .oct = -2},   /* (yours: saved on the FM-1) */
    {"SAW PAD", 85, 76, 0, 96, 100, 115, 104, 30, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 11, 20, 60, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"STRING PAD SYN", 85, 76, 0, 102, 100, 115, 82, 20, 8, 120, 45, 93, 74, 0, 115, 96, 0, 40, 0, 80, 2, 30, 90, 80, 10, 0, 0, 0, 64, 64, .eg = EG_SHIMRISE},   /* (yours: saved on the FM-1) */
    {"PWM PAD", 85, 76, 0, 96, 100, 115, 104, 30, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 12, 20, 50, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"GLASS PAD", 85, 76, 0, 96, 100, 115, 104, 30, RZ_MODAL, 100, 127, 90, 40, 50, 110, 100, 60, 45, SI_OCT, 90, 14, 10, 40, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"CHOIR SYN PAD", 85, 76, 0, 96, 100, 115, 104, 30, 3, 94, 80, 54, 60, 99, 115, 96, 0, 40, 0, 80, 11, 8, 40, 80, 10, 0, 0, 0, 64, 64, .eg = EG_VOWEL},   /* (yours: saved on the FM-1) */
    {"WARM PAD 2", 6, 30, 64, 96, 100, 115, 104, 20, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"CHUA PAD", 54, 90, 64, 96, 100, 115, 104, 80, 3, 94, 80, 54, 60, 0, 110, 100, 60, 45, 0, 90, 0, 8, 40, 80, 10, 0, 0, 0, 64, 64, .eg = EG_SHIMRISE},   /* (yours: saved on the FM-1) */
    {"DARK PAD", 85, 40, 0, 96, 100, 115, 104, 50, RZ_MODAL, 0, 0, 0, 0, 0, 127, 115, 30, 50, SI_OCT, 50, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"BRIGHT PAD", 85, 76, 0, 96, 100, 115, 104, 30, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 12, 10, 80, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"SWEEP PAD 2", 85, 76, 0, 96, 100, 115, 104, 60, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 11, 10, 60, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"SHIMMER PAD", 6, 64, 64, 96, 100, 115, 104, 20, RZ_MODAL, 0, 0, 0, 0, 0, 120, 110, 100, 60, SI_OCT, 100, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"FIFTH PAD", 85, 76, 0, 96, 100, 115, 104, 30, 3, 94, 80, 54, 60, 0, 110, 100, 80, 50, 1, 90, 11, 6, 30, 80, 10, 0, 0, 0, 64, 64, .eg = EG_SHIMRISE},   /* (yours: saved on the FM-1) */
    {"BOWED SYN PAD", 6, 64, 64, 96, 100, 115, 104, 20, RZ_BOW, 90, 70, 98, 100, 100, 110, 100, 60, 45, SI_OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SWELL},
    {"COOLING PAD", 110, 64, 0, 96, 100, 115, 104, 30, RZ_MODAL, 0, 0, 0, 0, 0, AIR, NZ(3, 0), 64, 10, .eg = EG_SHIMRISE, .mfx = MFP_SLOW_PULSE},
    {"SPACE PAD", 85, 76, 0, 62, 100, 86, 104, 30, 8, 90, 70, 98, 100, 0, 127, 50, 49, 76, 3, 90, 16, 30, 20, 80, 10, 0, 0, 2, 64, 64, .eg = EG_SHIMRISE, .mfx = MFP_SLOW_WOBBLE},   /* (yours: saved on the FM-1) */
    {"SLOW STR PAD", 85, 76, 0, 110, 104, 115, 110, 20, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 11, 40, 80, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE, .mfx = MFP_SLOW_WOBBLE},
    {"SYNTH PIANO", 85, 76, 0, 0, 60, 30, 56, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 12, 4, 30, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_EP_TREMOLO, .eg = EG_PICK},
    {"FM E.PIANO", 85, 76, 0, 0, 90, 0, 72, 20, RZ_MODAL, 0, 0, 0, 0, 0, 64, 30, SH_DFILT, 18, 1, 20, 14, 2, 18, 0, 0, 0, 0, SS_PINGPONG, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"BELL E.PIANO", 85, 76, 0, 0, 88, 10, 80, 22, RZ_MODAL, 60, 85, 60, 16, 35, 64, 28, SH_DFILT, 18, 0, 20, 14, 8, 45, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"SYNTH ORGAN", 85, 76, 0, 24, 64, 127, 50, 0, 0, 60, 100, 76, 16, 0, 20, 20, 0, 90, 0, 90, 29, 0, 12, 80, 10, 0, 0, 12, 64, 64, .mfx = MFP_ORGAN_VIB, .eg = EG_CLICK},   /* (yours: saved on the FM-1) */
    {"PERC SYN ORGAN", 85, 76, 0, 24, 60, 100, 50, 40, 0, 60, 100, 76, 16, 0, 20, 20, 0, 90, 0, 90, 30, 1, 20, 80, 10, 0, 0, 12, 64, 64, .mfx = MFP_ORGAN_VIB, .eg = EG_PERC},   /* (yours: saved on the FM-1) */
    {"CHUA ORGAN", 42, 127, 91, 24, 47, 109, 43, 64, 12, 78, 127, 28, 16, 24, 120, 48, 1, 90, 0, 90, 0, 2, 64, 31, 13, 44, 0, 12, 64, 127, .oct = -1, .mfx = MFP_ORGAN_VIB},   /* (yours: saved on the FM-1) */
    {"SQUARE KEYS", 85, 76, 0, 24, 70, 60, 60, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 12, 2, 64, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"SAW KEYS", 85, 76, 0, 24, 70, 60, 60, 50, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 11, 4, 10, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"CLAV SYNTH", 38, 120, 48, 0, 62, 40, 50, 110, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"PLUCKED KEYS", 85, 76, 0, 0, 62, 40, 56, 50, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 19, 4, 30, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .eg = EG_TONE},
    {"TINE KEYS", 85, 76, 0, 0, 84, 0, 80, 20, RZ_MODAL, 10, 60, 62, 20, 40, 64, 28, SH_DFILT, 18, 0, 20, 14, 4, 30, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_EP_TREMOLO, .eg = EG_TINE},
    {"HARPSI SYNTH", 85, 76, 0, 0, 50, 0, 50, 30, RZ_STRING, 40, 100, 60, 50, 127, 64, 45, SH_DFILT, 24, 0, 20, 12, 0, 63, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .eg = EG_PICK},
    {"REED KEYS", 6, 64, 64, 24, 60, 110, 56, 20, RZ_REED, 100, 100, 88, 115, 100, 64, 45, SH_DFILT, 24, 0, 20, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .eg = EG_REEDSWELL},
    {"VOX KEYS", 85, 76, 0, 24, 70, 90, 64, 40, RZ_VOICE, 40, 90, 48, 40, 90, 64, 20, 64, 100, 0, 60, 11, 4, 20, 0, 0, 0, 0, SS_MOUTH, 0, 0, .eg = EG_TALK, .mfx = MFP_ROBOT},
    {"CHIME KEYS", 85, 76, 0, 0, 70, 0, 74, 28, 0, 90, 110, 74, 24, 85, 64, 30, SH_DFILT, 18, 0, 20, 5, 63, 0, 80, 10, 0, 0, SS_PINGPONG, 26, 64, .eg = EG_STRIKE},   /* (yours: saved on the FM-1) */
    {"SOFT POLY", 85, 76, 0, 24, 80, 80, 80, 30, RZ_MODAL, 0, 0, 0, 0, 0, 64, 45, SH_DFILT, 24, 0, 20, 11, 10, 40, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"SAW PLUCK", 85, 76, 0, 0, 62, 40, 52, 60, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 11, 4, 0, 0, 0, 0, 0, SS_PINGPONG},
    {"SQUARE PLUCK", 85, 76, 0, 0, 62, 40, 50, 50, RZ_MODAL, 0, 0, 0, 0, 0, 32, 55, SH_DFILT, 30, 0, 30, 12, 4, 64, 0, 0, 0, 0, SS_PATTERN, .eg = EG_PICK},
    {"CHUA PLUCK", 54, 90, 64, 0, 62, 40, 52, 80, RZ_MODAL, 0, 0, 0, 0, 0, 64, 55, SH_DFILT, 28, 0, 20, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"RESO PLUCK", 85, 76, 0, 0, 62, 40, 48, 110, RZ_MODAL, 0, 0, 0, 0, 0, 64, 70, 70, 32, 3, 60, 11, 4, 0, 0, 0, 0, 0, SS_PINGPONG},
    {"KARPLUS PLUCK", 85, 76, 0, 0, 46, 0, 46, 10, RZ_STRING, 40, 90, 60, 40, 100, 64, 30, SH_DFILT, 34, 1, 20, 19, 0, 63, 0, 0, 0, 0, SS_BOUNCE, 127, .eg = EG_TONE},
    {"HARP SYN", 85, 76, 0, 0, 50, 0, 50, 10, RZ_STRING, 40, 80, 72, 50, 90, 64, 55, SH_DFILT, 28, 0, 20, 19, 0, 63, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .eg = EG_TONE},
    {"KALIMBA SYN", 85, 76, 0, 0, 42, 0, 42, 0, 14, 50, 80, 84, 0, 105, 64, 55, SH_DFILT, 28, 0, 20, 5, 64, 0, 80, 10, 0, 0, SS_PINGPONG, 127, 64, .eg = EG_STRIKE},   /* (yours: saved on the FM-1) */
    {"MARIMBA SYN", 85, 76, 0, 0, 50, 0, 50, 0, RZ_MODAL, 90, 70, 54, 24, 64, 64, 55, SH_DFILT, 28, 0, 20, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 45, 0, .eg = EG_STRIKE},
    {"BELL PLUCK", 85, 76, 0, 0, 40, 0, 44, 0, RZ_MODAL, 104, 120, 82, 16, 110, 64, 20, SH_DFILT, 34, 0, 10, 5, 63, 0, 0, 0, 0, 0, SS_BOUNCE, 127, .eg = EG_STRIKE},
    {"BLIP", 6, 64, 64, 0, 40, 0, 40, 30, RZ_MODAL, 0, 0, 0, 0, 0, 64, 50, SH_DFILT, 30, 3, 60, 0, 0, 0, 20, 5, 68, 0, SS_BOUNCE},
    {"FM PLUCK", 85, 76, 0, 0, 62, 40, 50, 58, 0, 104, 120, 82, 16, 0, 85, 99, 67, 59, 1, 70, 16, 4, 80, 80, 10, 0, 0, 5, 64, 64, .oct = 1},   /* (yours: saved on the FM-1) */
    {"SITAR SYN", 85, 76, 0, 0, 50, 0, 50, 10, 1, 16, 80, 78, 70, 90, 64, 55, SH_DFILT, 28, 0, 20, 12, 0, 63, 80, 10, 0, 0, SS_PINGPONG, 127, 64, .eg = EG_PICK},   /* (yours: saved on the FM-1) */
    {"DRUM PLUCK", 85, 76, 0, 0, 42, 0, 42, 0, RZ_DRUM, 10, 80, 70, 40, 110, 64, 55, SH_DFILT, 28, 0, 20, 3, 126, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0},
    {"GLASS PLUCK", 96, 76, 127, 0, 38, 0, 50, 30, 0, 84, 127, 90, 40, 80, 64, 55, SH_DFILT, 28, 0, 20, 31, 52, 14, 80, 10, 0, 0, SS_PINGPONG, 64, 64, .oct = 1, .mfx = MFP_FIFTH_BELL},   /* (yours: saved on the FM-1) */
    {"PING", 6, 64, 64, 0, 36, 0, 40, 61, 0, 84, 127, 90, 40, 0, 64, 55, SH_DFILT, 28, 0, 20, 0, 52, 14, 80, 10, 0, 0, SS_PINGPONG, 64, 64, .oct = 3},   /* (yours: saved on the FM-1) */
    {"ECHO PLUCK", 85, 76, 0, 0, 62, 40, 60, 60, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 11, 4, 10, 0, 0, 0, 0, SS_PINGPONG},
    {"SYN BRASS 3", 85, 76, 0, 30, 80, 110, 64, 70, 0, 84, 127, 90, 40, 0, 58, 36, 0, 11, 0, 70, 11, 6, 40, 62, 0, 160, 0, 0, 64, 96, .eg = EG_BLOOM},   /* (yours: saved on the FM-1) */
    {"POLY BRASS", 85, 76, 0, 20, 80, 100, 70, 60, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 11, 10, 60, 62, 0, 160, 0, SS_HALL, 0, 96},
    {"CHUA BRASS", 38, 80, 64, 30, 80, 110, 64, 70, RZ_BRASS, 90, 100, 68, 0, 80, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_BLOOM},
    {"BRASS STAB", 85, 76, 0, 24, 60, 60, 56, 70, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 11, 6, 20, 62, 0, 160, 0, SS_HALL, 0, 96},
    {"FAT BRASS", 85, 60, 0, 30, 80, 110, 64, 60, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"SOFT HORNS", 6, 64, 64, 40, 84, 112, 76, 20, RZ_BRASS, 30, 60, 83, 0, 110, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_BLOOM, .oct = -1},
    {"SYN STRINGS 3", 85, 76, 0, 60, 90, 110, 90, 30, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 11, 30, 80, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"SOLINA 2", 85, 76, 0, 50, 90, 115, 90, 49, 1, 60, 60, 80, 40, 50, 110, 100, 60, 45, 0, 90, 11, 22, 5, 80, 10, 0, 0, 0, 42, 64, .mfx = MFP_TAPE_WARBLE},   /* (yours: saved on the FM-1) */
    {"BOW STRINGS SYN", 85, 76, 0, 40, 80, 118, 70, 20, RZ_BOW, 110, 80, 93, 64, 110, 115, 96, 0, 40, SI_OCT, 80, 17, 63, 63, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SWELL, .mfx = MFP_STRING_VIB},
    {"PIZZ SYNTH", 85, 76, 0, 0, 42, 0, 42, 10, 2, 40, 70, 40, 40, 90, 85, 72, 0, 30, 0, 70, 19, 0, 63, 2, 26, 89, 0, 0, 127, 127, .eg = EG_TONE},   /* (yours: saved on the FM-1) */
    {"CHUA STRINGS", 54, 80, 32, 60, 90, 110, 90, 40, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"ENSEMBLE SYN", 85, 76, 0, 40, 90, 115, 94, 30, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 11, 20, 70, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"STAB STRINGS", 85, 76, 0, 24, 60, 60, 60, 30, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 11, 20, 70, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"CELLO SYN", 85, 76, 0, 30, 80, 118, 70, 30, RZ_BOW, 115, 60, 93, 70, 110, 85, 72, 0, 30, SI_OCT, 70, 17, 63, 63, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SWELL, .oct = -1, .mfx = MFP_STRING_VIB},
    {"BRASS STRINGS", 85, 76, 0, 30, 84, 110, 80, 40, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 11, 10, 60, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"REED SYN", 85, 76, 0, 20, 76, 115, 60, 20, RZ_REED, 60, 90, 78, 60, 110, 85, 72, 0, 30, SI_OCT, 70, 17, 126, 63, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_REEDSWELL, .mfx = MFP_BREATH_VIB},
    {"SEQ SAW", 85, 76, 0, 0, 62, 50, 44, 70, RZ_MODAL, 0, 0, 0, 0, 0, 64, 50, SH_DFILT, 28, 0, 10, 11, 4, 0, 0, 0, 0, 0, SS_PINGPONG, .mfx = MFP_PULSE_GATE},
    {"SEQ SQUARE", 85, 76, 0, 0, 44, 66, 22, 0, 9, 60, 90, 78, 60, 0, 64, 65, SH_DFILT, 30, 1, 20, 12, 2, 0, 80, 86, 42, 0, SS_PINGPONG, 64, 117, .mfx = MFP_PULSE_GATE},   /* (yours: saved on the FM-1) */
    {"SEQ CHUA", 40, 104, 32, 0, 127, 50, 44, 90, 11, 56, 90, 78, 81, 48, 64, 65, SH_DFILT, 30, 1, 20, 0, 2, 0, 80, 86, 65, 0, SS_PINGPONG, 64, 117, .oct = 2},   /* (yours: saved on the FM-1) */
    {"SEQ ACID", NZK, 0, 68, 10, 36, 120, NOFX, 64, 75, 100, 34, 1, 30, NZ(1, 0), 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"SEQ FM", 85, 76, 0, 0, 62, 50, 44, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 55, SH_DFILT, 28, 2, 10, 14, 4, 80, 0, 0, 0, 0, SS_PINGPONG},
    {"SEQ BELL", 85, 76, 0, 0, 38, 0, 40, 0, RZ_MODAL, 100, 120, 80, 30, 90, 32, 50, SH_DFILT, 28, 2, 20, 5, 63, 0, 0, 0, 0, 0, SS_PATTERN, 127, .eg = EG_STRIKE},
    {"SEQ PLUCK", 85, 76, 0, 0, 40, 0, 40, 30, RZ_STRING, 40, 100, 50, 40, 90, 32, 55, SH_DFILT, 30, 0, 30, 19, 0, 63, 0, 0, 0, 0, SS_PATTERN, 127, .eg = EG_TONE},
    {"SEQ BLIP", 6, 64, 64, 0, 36, 0, 36, 40, RZ_MODAL, 0, 0, 0, 0, 0, 64, 30, SH_DFILT, 34, 1, 20, 0, 0, 0, 16, 5, 59, 0, SS_BOUNCE},
    {"SEQ ATARI", 85, 76, 0, 0, 62, 50, 44, 30, RZ_MODAL, 0, 0, 0, 0, 0, 64, 65, SH_DFILT, 30, 1, 20, 22, 40, 20, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    {"SEQ RESO", 85, 76, 0, 0, 62, 50, 44, 120, RZ_MODAL, 0, 0, 0, 0, 0, 32, 85, 55, 34, 1, 80, 11, 4, 0, 0, 0, 0, 0, SS_PATTERN},
    {"SEQ ZAP", 85, 76, 0, 0, 62, 50, 44, 60, RZ_MODAL, 0, 0, 0, 0, 0, 64, 65, SH_DFILT, 30, 1, 20, 12, 4, 64, 20, 5, 77, 0, SS_PINGPONG, 0, 0},
    {"SEQ DRUM", 85, 76, 0, 0, 40, 0, 40, 0, RZ_DRUM, 10, 80, 70, 40, 110, 64, 65, SH_DFILT, 30, 1, 20, 3, 126, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0},
    {"SEQ BASS", 85, 76, 0, 0, 48, 40, 46, 70, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 11, 2, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .oct = -1},
    {"SEQ ECHO", 85, 76, 0, 0, 62, 50, 50, 60, RZ_MODAL, 0, 0, 0, 0, 0, 64, 60, SH_DFILT, 30, 1, 20, 12, 4, 64, 0, 0, 0, 0, SS_PINGPONG, .mfx = MFP_TRIPLET_CHOP},
    {"SEQ GLASS", 85, 76, 0, 0, 40, 0, 42, 30, RZ_MODAL, 100, 127, 90, 40, 70, 32, 40, SH_DFILT, 30, 3, 40, 14, 10, 40, 0, 0, 0, 0, SS_PATTERN, .eg = EG_TINE},
    {"SEQ METAL", 85, 76, 0, 0, 40, 0, 42, 40, 8, 100, 110, 83, 8, 127, 64, 65, SH_DFILT, 30, 1, 20, 16, 20, 40, 80, 10, 0, 0, SS_PINGPONG, 64, 64, .mfx = MFP_METAL_BELL},   /* (yours: saved on the FM-1) */
    {"FM BELL", 85, 76, 0, 0, 80, 0, 86, 20, RZ_MODAL, 0, 0, 0, 0, 0, 112, 40, 95, 22, 0, 30, 14, 20, 70, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_BELL_RING, .eg = EG_TINE},
    {"TUBE BELL SYN", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 104, 100, 78, 16, 115, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .mfx = MFP_BELL_RING, .eg = EG_STRIKE},
    {"GLASS BELL", 6, 64, 64, 0, 84, 0, 86, 20, RZ_MODAL, 100, 127, 90, 40, 80, 64, 20, SH_DFILT, 34, 0, 10, 0, 0, 0, 0, 0, 0, 0, SS_BOUNCE, .mfx = MFP_FIFTH_BELL},
    {"CRYSTAL 2", 6, 64, 64, 0, 84, 0, 88, 20, RZ_MODAL, 100, 127, 90, 40, 80, 90, 98, 120, 60, SI_FIFTH, 110, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .mfx = MFP_FIFTH_BELL},
    {"CHUA BELL", 54, 90, 64, 0, 70, 0, 80, 40, RZ_MODAL, 70, 100, 82, 12, 100, 112, 55, 95, 26, 0, 30, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_BELL_RING},
    {"TEMPLE BELL", 85, 76, 0, 0, 50, 0, 50, 34, 0, 110, 90, 90, 10, 127, 115, 96, 0, 40, 0, 80, 3, 30, 17, 80, 10, 0, 0, 1, 58, 64, .mfx = MFP_BELL_RING},   /* (yours: saved on the FM-1) */
    {"MUSIC BOX SYN", 85, 76, 0, 0, 36, 0, 36, 0, RZ_MODAL, 100, 127, 90, 40, 80, 64, 25, SH_DFILT, 30, 2, 30, 25, 63, 0, 0, 0, 0, 0, SS_BOUNCE, 127, .oct = 1},
    {"GAMELAN", 85, 76, 0, 0, 44, 0, 44, 0, RZ_MODAL, 116, 100, 80, 30, 110, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .mfx = MFP_GONG, .eg = EG_STRIKE},
    {"SHIMMER BELL 2", 6, 64, 64, 0, 84, 0, 90, 20, RZ_MODAL, 104, 127, 88, 20, 90, 127, 115, 110, 60, SI_2OCT, 110, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"STEEL PAN SYN", 85, 76, 0, 0, 46, 0, 46, 0, RZ_MODAL, 90, 110, 72, 40, 110, 112, 55, 95, 26, 0, 30, 19, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .eg = EG_TONE},
    {"VIBES SYN", 85, 76, 0, 0, 58, 0, 60, 0, RZ_MODAL, 92, 90, 76, 24, 95, 112, 55, 95, 26, 0, 30, 5, 30, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .mfx = MFP_VIBES_MOTOR, .eg = EG_STRIKE},
    {"BOWL", 6, 64, 64, 20, 100, 0, 100, 0, RZ_MODAL, 110, 80, 76, 20, 100, 115, 72, 0, 26, SI_OCT, 50, 0, 0, 0, 0, 0, 0, 0, SS_MOSQUE, 0, 0, .mfx = MFP_GONG},
    {"CELESTA SYN", 85, 76, 0, 0, 36, 0, 36, 0, RZ_MODAL, 100, 120, 74, 30, 110, 112, 55, 95, 26, 0, 30, 5, 63, 0, 0, 0, 0, 0, SS_PINGPONG, 127, 0, .oct = 1, .eg = EG_STRIKE},
    {"ICE BELL", 6, 64, 64, 0, 90, 0, 96, 20, RZ_MODAL, 110, 127, 92, 30, 60, 110, 98, 100, 62, SI_2OCT, 110, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .mfx = MFP_FIFTH_BELL},
    {"METAL BELL", 85, 76, 0, 0, 70, 0, 80, 30, RZ_MODAL, 0, 0, 0, 0, 0, 112, 55, 95, 26, 0, 30, 16, 30, 20, 0, 0, 0, 0, SS_PINGPONG, 0, 0, .mfx = MFP_METAL_BELL},
    {"CHUA CHIMES", 6, 100, 64, 0, 84, 0, 86, 20, RZ_MODAL, 100, 127, 90, 40, 80, 112, 55, 95, 26, 0, 30, 0, 0, 0, 0, 0, 0, 0, SS_PINGPONG, 0, 0},
    /* ---- BANK 4: LAB (experimental sound design: the circuit's chaos, the noise programs at their edges, the
     * waveguides driven by SCREAM, FREE EG contours, the SPACEs; 16 per group) ---- */
    {"DOUBLE SCROLL", 100, 64, 64, 60, 100, 115, 100, 30, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"OUTER ORBIT", 124, 64, 96, 80, 100, 115, 100, 34, RZ_MODAL, 70, 100, 82, 12, 60, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_TEMPLE, 0, 0},
    {"STRANGE ATTRAC", 90, 100, 32, 100, 100, 115, 100, 38, RZ_SYMP, 60, 60, 86, 40, 60, 127, 118, 60, 55, SI_2OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .mfx = MFP_CHAOS_RING},
    {"SPIRAL SINK", 56, 40, 64, 60, 100, 115, 100, 42, RZ_BOW, 120, 45, 93, 74, 90, 127, 120, 30, 50, SI_FIFTH, 60, 0, 0, 0, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"PERIOD DOUBLING", 44, 110, 64, 13, 110, 115, 100, 59, 8, 120, 45, 93, 74, 0, 110, 100, 60, 45, 0, 90, 2, 100, 120, 28, 5, 99, 0, 0, 127, 64},   /* (yours: saved on the FM-1) */
    {"CHAOS ORGAN", 100, 120, 127, 100, 100, 115, 100, 50, 0, 70, 100, 82, 12, 60, 115, 96, 0, 40, 0, 80, 0, 10, 0, 80, 10, 0, 0, 1, 57, 64, .oct = 2},   /* (yours: saved on the FM-1) */
    {"BIFURCATION", 48, 90, 96, 60, 100, 115, 100, 54, RZ_SYMP, 60, 60, 86, 40, 60, 127, 118, 60, 55, SI_2OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"SCROLL DRONE", 104, 30, 0, 80, 100, 115, 100, 58, RZ_BOW, 120, 45, 93, 74, 90, 127, 120, 30, 50, SI_FIFTH, 60, 0, 0, 0, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"RUMBLE FIELD", 110, 20, 0, 100, 100, 115, 100, 62, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"EDGE OF CHAOS", 112, 80, 32, 60, 100, 115, 100, 66, RZ_MODAL, 70, 100, 82, 12, 60, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_TEMPLE, 0, 0, .mfx = MFP_CHAOS_RING},
    {"LIMIT CYCLE", 30, 64, 96, 80, 100, 115, 100, 70, RZ_SYMP, 60, 60, 86, 40, 60, 127, 118, 60, 55, SI_2OCT, 90, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"HOMOCLINIC", 60, 64, 32, 100, 100, 115, 100, 74, RZ_BOW, 120, 45, 93, 74, 90, 127, 120, 30, 50, SI_FIFTH, 60, 0, 0, 0, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"BIG BANG", 70, 64, 0, 20, 100, 115, 100, 30, RZ_MODAL, 0, 0, 0, 0, 0, 127, 118, 60, 55, SI_OCT, 90, NZ(3, 0), 85, 0, .sh_space = SS_CATHEDRAL},
    {"EXPANDING BELL", 40, 64, 127, 0, 90, 0, 96, 20, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, NZ(3, 0), 0, 0},
    {"HEAT DEATH", 127, 64, 40, 60, 110, 120, 110, 30, RZ_MODAL, 0, 0, 0, 0, 0, 127, 120, 30, 55, SI_OCT, 60, NZ(3, 0), 127, 110, .sh_space = SS_MOSQUE},
    {"PRIMORDIAL SOUP", 100, 64, 20, 40, 100, 115, 100, 50, RZ_MODAL, 0, 0, 0, 0, 0, AIR, NZ(3, 0), 30, 70, .eg = EG_OPEN},
    {"RADIO STATIC", 85, 76, 0, 24, 80, 100, 80, 40, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 1, 100, 90, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"FLANGE WALK", 85, 76, 0, 20, 90, 110, 90, 49, 8, 120, 45, 93, 74, 0, 115, 96, 0, 40, 0, 80, 2, 9, 78, 80, 10, 0, 0, 3, 64, 64, .oct = 1},   /* (yours: saved on the FM-1) */
    {"RING SQUARES", 85, 76, 0, 0, 60, 60, 70, 58, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 3, 20, 120, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"RING SINES", 85, 76, 0, 0, 80, 100, 80, 67, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 4, 110, 100, 0, 0, 0, 0, SS_TEMPLE, 0, 0},
    {"CROSS RING", 85, 76, 0, 24, 90, 110, 90, 76, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 5, 64, 127, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"RESONOISE", 85, 76, 0, 0, 60, 60, 70, 85, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 6, 100, 110, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"GRAIN CLOUD", 85, 76, 0, 0, 80, 100, 80, 94, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 7, 110, 40, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"GRAIN GLITCH", 85, 76, 0, 20, 90, 110, 90, 43, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 8, 30, 100, 0, 0, 0, 0, SS_TEMPLE, 0, 0},
    {"GRAIN DUST", 85, 76, 0, 0, 60, 60, 70, 52, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 9, 120, 90, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"BASURILLA", 85, 76, 0, 24, 80, 100, 80, 61, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 10, 90, 30, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"BASURA TOTAL", 85, 76, 0, 20, 90, 110, 90, 70, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 21, 100, 120, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"ATARI CHAOS", 85, 76, 0, 0, 60, 60, 70, 79, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 22, 120, 110, 0, 0, 0, 0, SS_TEMPLE, 0, 0},
    {"WAKING FILOM", 85, 76, 0, 24, 80, 100, 80, 88, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 23, 64, 110, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"P S H", 85, 76, 0, 24, 90, 110, 90, 97, RZ_MODAL, 0, 0, 0, 0, 0, 115, 96, 0, 40, SI_OCT, 80, 24, 100, 90, 0, 0, 0, 0, SS_MOSQUE, 0, 0},
    {"ARRAY ROCKS", 85, 76, 0, 0, 60, 60, 70, 46, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 25, 90, 110, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"EXIST. PAIN", 85, 76, 0, 0, 80, 100, 80, 55, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 26, 110, 110, 0, 0, 0, 0, SS_TEMPLE, 0, 0},
    {"SCREAMING HORN", 110, 64, 64, 20, 80, 115, 70, 30, RZ_BRASS, 110, 120, 63, 0, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 60, SS_HALL, 0, 0},
    {"FERAL FLUTE", 90, 64, 64, 20, 80, 115, 70, 30, 4, 115, 90, 88, 20, 120, 115, 96, 0, 40, 0, 80, 0, 110, 110, 80, 10, 0, 64, 0, 64, 64, .eg = EG_GROWL},   /* (yours: saved on the FM-1) */
    {"REED HOWL", 70, 64, 64, 20, 80, 115, 70, 30, 9, 60, 90, 78, 60, 127, 0, 103, 12, 38, 1, 100, 8, 110, 110, 80, 10, 0, 14, 3, 64, 64},   /* (yours: saved on the FM-1) */
    {"BOW SCRAPE", 110, 64, 64, 20, 80, 115, 70, 30, RZ_BOW, 100, 127, 78, 8, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 72, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"THROAT SINGER", 90, 64, 64, 20, 80, 115, 70, 30, 3, 119, 50, 52, 26, 120, 115, 96, 0, 40, 0, 80, 2, 110, 110, 80, 10, 0, 76, 0, 64, 64, .oct = 3},   /* (yours: saved on the FM-1) */
    {"DIDGERIDOO", 70, 64, 64, 24, 80, 115, 70, 30, RZ_DIDGE, 10, 60, 60, 40, 127, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 80, SS_HALL, 0, 0, .eg = EG_GROWL, .oct = -2},
    {"HAWK CRY", 110, 64, 64, 20, 80, 115, 70, 30, RZ_HAWK, 70, 90, 58, 70, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 84, SS_HALL, 0, 0},
    {"BIRD SWARM", 90, 64, 64, 24, 80, 115, 70, 30, RZ_SYRINX, 60, 90, 58, 110, 120, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 88, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"WOLF TONE", 70, 64, 64, 20, 80, 115, 70, 30, RZ_BOW, 120, 60, 93, 70, 120, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 92, SS_HALL, 0, 0},
    {"CONCH", 110, 64, 64, 20, 80, 115, 70, 30, RZ_BRASS, 20, 40, 83, 0, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 96, SS_HALL, 0, 0, .eg = EG_GROWL},
    {"BAGPIPE DRONE", 90, 64, 64, 24, 80, 115, 70, 30, RZ_REED, 110, 120, 88, 122, 120, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 100, SS_HALL, 0, 0},
    {"OVERBLOWN", 70, 64, 64, 20, 80, 115, 70, 30, RZ_FLUTE, 127, 110, 83, 40, 120, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 104, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"HARMON SCREAM", 110, 64, 64, 20, 80, 115, 70, 30, RZ_BRASS, 90, 100, 68, 110, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 108, SS_HALL, 0, 0},
    {"CHANT", 90, 64, 64, 20, 80, 115, 70, 30, RZ_VOICE, 64, 70, 48, 60, 120, 64, 20, 64, 100, 0, 60, 0, 0, 0, 0, 0, 0, 112, SS_MOUTH, 0, 0, .eg = EG_GROWL},
    {"KAZOO", 70, 64, 64, 20, 80, 115, 70, 30, RZ_REED, 127, 127, 68, 127, 120, 64, 20, 64, 100, 0, 60, 0, 0, 0, 0, 0, 0, 116, SS_MOUTH, 0, 0, .eg = EG_TALK},
    {"HAWK DRONE", 110, 64, 64, 20, 80, 115, 70, 30, RZ_HAWK, 40, 110, 58, 90, 120, 85, 72, 0, 30, SI_OCT, 70, 0, 0, 0, 0, 0, 0, 120, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"SHEET METAL", 85, 76, 0, 0, 50, 0, 60, 20, RZ_MODAL, 0, 90, 70, 0, 110, 85, 72, 0, 30, SI_OCT, 70, 25, 90, 110, 0, 0, 0, 0, SS_HALL, 127, 0, .mfx = MFP_METAL_BELL},
    {"SPRING TANK", 85, 76, 0, 0, 57, 0, 65, 20, RZ_SYMP, 37, 101, 83, 29, 110, 115, 96, 0, 40, SI_OCT, 80, 5, 100, 60, 0, 0, 0, 0, SS_MOSQUE, 127, 0, .eg = EG_STRIKE},
    {"GONG CHAOS", 124, 64, 96, 0, 70, 60, 80, 20, RZ_DRUM, 74, 112, 80, 58, 127, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .mfx = MFP_GONG},
    {"BRAKE DRUM", 85, 76, 0, 0, 71, 0, 75, 20, RZ_STRING, 111, 123, 84, 87, 110, 110, 100, 60, 45, SI_OCT, 90, 3, 126, 40, 0, 0, 0, 0, SS_TEMPLE, 127, 0},
    {"WIND GLASS", 85, 76, 0, 0, 78, 0, 80, 20, RZ_MODAL, 20, 96, 72, 116, 110, 85, 72, 0, 30, SI_OCT, 70, 25, 90, 110, 0, 0, 0, 0, SS_HALL, 127, 0},
    {"PIPE KNOCK", 85, 76, 0, 0, 85, 0, 85, 20, RZ_SYMP, 57, 107, 85, 17, 110, 115, 96, 0, 40, SI_OCT, 80, 5, 100, 60, 0, 0, 0, 0, SS_MOSQUE, 127, 0, .eg = EG_STRIKE},
    {"RAIL HIT", 85, 76, 0, 0, 52, 60, 90, 20, RZ_DRUM, 94, 118, 73, 46, 110, 0, 0, 0, 0, SI_OCT, 0, 3, 126, 40, 0, 0, 0, 0, SS_CATHEDRAL, 127, 0, .eg = EG_STRIKE},
    {"CAN CRUSH", 85, 76, 0, 0, 59, 0, 95, 20, RZ_STRING, 3, 91, 86, 75, 110, 110, 100, 60, 45, SI_OCT, 90, 3, 126, 40, 0, 0, 0, 0, SS_TEMPLE, 127, 0},
    {"TINE CLUSTER", 85, 76, 0, 0, 66, 0, 60, 20, RZ_MODAL, 40, 102, 66, 104, 90, 85, 72, 0, 30, SI_OCT, 70, 25, 90, 110, 0, 0, 0, 0, SS_HALL, 127, 0},
    {"IRON BELL", 85, 76, 0, 0, 73, 0, 65, 20, RZ_SYMP, 77, 113, 87, 5, 110, 115, 96, 0, 40, SI_OCT, 80, 5, 100, 60, 0, 0, 0, 0, SS_MOSQUE, 127, 0, .mfx = MFP_BELL_RING, .eg = EG_STRIKE},
    {"WIRE HARP", 124, 64, 96, 0, 80, 60, 70, 20, RZ_DRUM, 114, 124, 75, 34, 110, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"PLATE REVERB", 85, 76, 0, 0, 87, 0, 75, 20, RZ_STRING, 23, 97, 88, 63, 110, 110, 100, 60, 45, SI_OCT, 90, 3, 126, 40, 0, 0, 0, 0, SS_TEMPLE, 127, 0},
    {"AXE BLADE", 85, 76, 0, 0, 54, 0, 80, 20, RZ_MODAL, 60, 108, 76, 92, 110, 85, 72, 0, 30, SI_OCT, 70, 25, 90, 110, 0, 0, 0, 0, SS_HALL, 127, 0},
    {"CHAIN LINK", 85, 76, 0, 0, 61, 0, 85, 20, RZ_SYMP, 97, 119, 89, 121, 110, 115, 96, 0, 40, SI_OCT, 80, 5, 100, 60, 0, 0, 0, 0, SS_MOSQUE, 127, 0, .eg = EG_STRIKE},
    {"MOBILE", 124, 64, 96, 0, 68, 0, 90, 20, RZ_DRUM, 6, 92, 77, 22, 110, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0},
    {"SAW BLADE", 85, 76, 0, 0, 75, 0, 95, 20, RZ_STRING, 43, 103, 90, 51, 110, 110, 100, 60, 45, SI_OCT, 90, 3, 126, 40, 0, 0, 0, 0, SS_TEMPLE, 127, 0},
    {"VOWEL MORPH", 85, 76, 0, 40, 90, 110, 90, 40, RZ_VOICE, 64, 80, 48, 60, 110, 110, 100, 60, 45, SI_OCT, 90, 11, 10, 40, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_VOWEL},
    {"SHIM RISER", 6, 64, 64, 40, 90, 110, 90, 40, RZ_MODAL, 100, 127, 90, 40, 60, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"RING OPENER", 85, 76, 0, 40, 90, 110, 90, 40, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 12, 6, 60, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"JET SWEEP", 54, 90, 64, 40, 90, 110, 90, 40, RZ_FLUTE, 40, 90, 78, 40, 110, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OVERTONE},
    {"LIP BLOOM", 85, 76, 0, 40, 90, 110, 90, 40, RZ_BRASS, 90, 95, 68, 0, 100, 110, 100, 60, 45, SI_OCT, 90, 11, 10, 40, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_BLOOM},
    {"BOW SWELL", 6, 64, 64, 40, 90, 110, 90, 40, RZ_BOW, 110, 80, 93, 64, 110, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SWELL},
    {"GROWL PAD", 85, 76, 0, 40, 90, 110, 90, 40, RZ_REED, 55, 90, 73, 80, 110, 110, 100, 60, 45, SI_OCT, 90, 12, 6, 60, 0, 0, 0, 40, SS_HALL, 0, 0, .eg = EG_GROWL},
    {"CHIFF MACHINE", 54, 90, 64, 40, 90, 110, 90, 40, RZ_FLUTE, 40, 100, 88, 90, 100, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_CHIFF},
    {"TALKING SAW", 85, 76, 0, 24, 70, 60, 70, 40, RZ_VOICE, 64, 80, 48, 60, 110, 64, 20, 64, 100, 0, 60, 11, 10, 40, 0, 0, 0, 0, SS_MOUTH, 0, 0, .eg = EG_VOWEL},
    {"RISING SPACE", 6, 64, 64, 0, 70, 60, 70, 40, RZ_MODAL, 100, 127, 90, 40, 60, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SHIMRISE},
    {"OPENING DOOR", 85, 76, 0, 24, 70, 60, 70, 40, RZ_MODAL, 0, 0, 0, 0, 0, 110, 100, 60, 45, SI_OCT, 90, 12, 6, 60, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OPEN},
    {"BREATH ENGINE", 54, 90, 64, 0, 70, 60, 70, 40, RZ_FLUTE, 40, 90, 78, 40, 110, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_OVERTONE},
    {"BRASS BLOOM PAD", 85, 76, 0, 0, 70, 60, 70, 40, RZ_BRASS, 90, 95, 68, 0, 100, 110, 100, 60, 45, SI_OCT, 90, 11, 10, 40, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_BLOOM},
    {"SWELL DRONE", 6, 64, 64, 0, 70, 60, 70, 40, RZ_BOW, 110, 80, 93, 64, 110, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_SWELL},
    {"GROWL KEYS", 85, 76, 0, 24, 70, 60, 70, 40, RZ_REED, 55, 90, 73, 80, 110, 110, 100, 60, 45, SI_OCT, 90, 12, 6, 60, 0, 0, 0, 40, SS_HALL, 0, 0, .eg = EG_GROWL},
    {"BLOOM BELL", 54, 90, 64, 0, 70, 60, 70, 40, RZ_FLUTE, 40, 100, 88, 90, 100, 115, 96, 0, 40, SI_OCT, 80, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .eg = EG_CHIFF, .mfx = MFP_BELL_RING},
    {"BITCRUSH WALK", 85, 76, 0, 0, 50, 0, 50, 30, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 29, 90, 110, 20, 5, 0, 0, SS_HALL, 0, 0},
    {"LFREE WALK", 85, 76, 0, 24, 70, 100, 60, 37, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 30, 70, 100, 25, 5, 28, 0, SS_HALL, 0, 0},
    {"WHO KNOWS", 85, 76, 0, 0, 50, 0, 50, 44, 7, 90, 100, 68, 0, 0, 85, 72, 0, 0, 0, 70, 27, 90, 90, 30, 5, 44, 0, 0, 64, 64, .oct = 2},   /* (yours: saved on the FM-1) */
    {"SATAN WORKOUT", 85, 76, 0, 24, 70, 100, 60, 0, 7, 90, 100, 68, 0, 0, 85, 72, 0, 30, 0, 70, 29, 27, 103, 35, 5, 58, 0, 0, 67, 64},   /* (yours: saved on the FM-1) */
    {"DATA BURST", 85, 76, 0, 0, 50, 0, 50, 58, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 7, 127, 127, 40, 5, 70, 0, SS_HALL, 0, 0},
    {"MODEM", 85, 76, 0, 24, 70, 100, 60, 65, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 3, 127, 60, 45, 5, 81, 0, SS_HALL, 0, 0},
    {"DIAL UP", 85, 76, 0, 0, 50, 0, 50, 72, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 4, 40, 127, 50, 5, 20, 0, SS_HALL, 0, 0},
    {"FAX", 85, 76, 0, 24, 70, 100, 60, 79, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 22, 127, 127, 55, 5, 38, 0, SS_HALL, 0, 0},
    {"CIRCUIT BENT", 85, 76, 0, 0, 50, 0, 50, 61, 12, 15, 40, 80, 30, 127, 85, 72, 0, 0, 0, 70, 23, 60, 60, 60, 5, 53, 0, 0, 64, 64, .mfx = MFP_DIRTY_RING},   /* (yours: saved on the FM-1) */
    {"GAMEBOY DEATH", 85, 76, 0, 0, 70, 100, 60, 67, 12, 15, 40, 80, 30, 0, 85, 72, 0, 0, 0, 70, 22, 20, 127, 65, 5, 65, 0, 0, 64, 64, .mfx = MFP_DIRTY_RING},   /* (yours: saved on the FM-1) */
    {"STUTTER", 85, 76, 0, 0, 50, 0, 50, 50, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 8, 127, 20, 70, 5, 77, 0, SS_HALL, 0, 0, .mfx = MFP_TRIPLET_CHOP},
    {"BIT RAIN", 85, 76, 0, 0, 70, 100, 60, 57, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 29, 127, 64, 75, 5, 10, 0, SS_HALL, 0, 0},
    {"SAMPLE HOLD", 85, 76, 0, 0, 50, 0, 50, 64, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 24, 127, 20, 80, 5, 32, 0, SS_HALL, 0, 0},
    {"TAPE EATER", 85, 76, 0, 0, 70, 100, 60, 71, RZ_MODAL, 0, 0, 0, 0, 0, 85, 72, 0, 30, SI_OCT, 70, 2, 127, 127, 85, 5, 47, 0, SS_HALL, 0, 0, .mfx = MFP_SEASICK},
    {"NUMBERS STATION", 85, 76, 0, 0, 50, 0, 50, 78, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 1, 20, 20, 90, 5, 61, 0, SS_HALL, 0, 0, .mfx = MFP_ROBOT},
    {"ERROR TONE", 85, 76, 0, 24, 70, 100, 60, 35, 6, 0, 40, 80, 30, 0, 85, 72, 0, 30, 0, 70, 3, 10, 10, 95, 5, 73, 0, 0, 84, 64, .mfx = MFP_DIRTY_RING},   /* (yours: saved on the FM-1) */
    {"UNDERWORLD", 6, 64, 64, 100, 110, 120, 115, 20, RZ_MODAL, 0, 0, 0, 0, 0, 127, 118, 0, 60, SI_OCT, 60, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0, .mfx = MFP_SUB_RING},
    {"CATHEDRAL DUST", 85, 76, 0, 100, 110, 120, 115, 25, RZ_VOICE, 40, 60, 48, 20, 80, 127, 121, 23, 60, SI_FIFTH, 77, 17, 110, 90, 0, 0, 0, 0, SS_TEMPLE, 0, 0, .eg = EG_SHIMRISE, .oct = -1},
    {"MOSQUE MIRAGE", 85, 60, 0, 100, 110, 120, 115, 30, RZ_MODAL, 100, 127, 90, 40, 50, 127, 124, 46, 60, SI_OCT5, 94, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .oct = -2},
    {"TEMPLE DRONE", 85, 76, 0, 100, 110, 120, 115, 35, RZ_MODAL, 0, 0, 0, 0, 0, 127, 118, 69, 60, SI_2OCT, 111, 11, 30, 90, 0, 0, 0, 0, SS_MOSQUE, 0, 0, .eg = EG_SHIMRISE},
    {"SUB HAZE 2", 6, 64, 64, 100, 110, 120, 115, 40, RZ_MODAL, 0, 0, 0, 0, 0, 127, 121, 92, 60, SI_OCT, 68, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"GLACIER", 85, 76, 0, 100, 110, 120, 115, 45, RZ_VOICE, 40, 60, 48, 20, 80, 127, 124, 115, 60, SI_FIFTH, 85, 17, 110, 90, 0, 0, 0, 0, SS_TEMPLE, 0, 0, .eg = EG_SHIMRISE, .oct = -1},
    {"DEEP SPACE", 85, 60, 0, 100, 110, 120, 115, 50, RZ_MODAL, 100, 127, 90, 40, 50, 127, 118, 10, 60, SI_OCT5, 102, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .oct = -2, .mfx = MFP_SLOW_PULSE},
    {"NEBULA", 85, 76, 0, 100, 110, 120, 115, 55, 0, 100, 127, 90, 40, 0, 127, 121, 33, 60, 3, 119, 11, 30, 90, 80, 10, 0, 0, 3, 64, 64, .eg = EG_SHIMRISE},   /* (yours: saved on the FM-1) */
    {"CAVERN DRIP", 6, 64, 64, 100, 110, 120, 115, 20, RZ_MODAL, 0, 0, 0, 0, 0, 127, 124, 56, 60, SI_OCT, 76, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"AURORA", 85, 76, 0, 100, 110, 120, 115, 25, RZ_VOICE, 40, 60, 48, 20, 80, 127, 118, 79, 60, SI_FIFTH, 93, 17, 110, 90, 0, 0, 0, 0, SS_TEMPLE, 0, 0, .eg = EG_SHIMRISE, .oct = -1},
    {"FOG HORN", 85, 60, 0, 100, 110, 120, 115, 30, RZ_MODAL, 100, 127, 90, 40, 50, 127, 121, 102, 60, SI_OCT5, 110, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .oct = -2},
    {"OCEAN FLOOR", 85, 76, 0, 100, 110, 120, 115, 35, RZ_MODAL, 0, 0, 0, 0, 0, 127, 124, 125, 60, SI_2OCT, 67, 11, 30, 90, 0, 0, 0, 0, SS_MOSQUE, 0, 0, .eg = EG_SHIMRISE},
    {"DESERT WIND", 6, 64, 64, 100, 110, 120, 115, 40, RZ_MODAL, 0, 0, 0, 0, 0, 127, 118, 20, 60, SI_OCT, 84, 0, 0, 0, 0, 0, 0, 0, SS_HALL, 0, 0},
    {"STAR FIELD", 85, 76, 0, 100, 110, 120, 115, 45, 11, 40, 60, 48, 20, 80, 127, 121, 73, 60, 1, 101, 23, 110, 90, 80, 10, 0, 0, 1, 64, 64, .eg = EG_SHIMRISE, .oct = -1},   /* (yours: saved on the FM-1) */
    {"GHOST CHOIR", 85, 60, 0, 100, 110, 120, 115, 50, RZ_MODAL, 100, 127, 90, 40, 50, 127, 124, 66, 60, SI_OCT5, 118, 0, 0, 0, 0, 0, 0, 0, SS_CATHEDRAL, 0, 0, .oct = -2, .mfx = MFP_SEASICK},
    {"EVENT HORIZON", 85, 76, 0, 100, 110, 120, 115, 55, RZ_MODAL, 0, 0, 0, 0, 0, 127, 118, 89, 60, SI_2OCT, 75, 11, 30, 90, 0, 0, 0, 0, SS_MOSQUE, 0, 0, .eg = EG_SHIMRISE, .mfx = MFP_SUB_RING},
    {"CHAOS KICK", 110, 64, 96, 0, 30, 0, 30, 20, RZ_DRUM, 0, 60, 80, 20, 110, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 10, 5, 49, 0, SS_HALL, 0, 0, .oct = -2},
    {"NOISE SNARE", 85, 76, 0, 0, 33, 0, 33, 31, 10, 0, 60, 80, 20, 0, 127, 118, 89, 0, 3, 75, 4, 60, 90, 17, 5, 65, 0, 3, 127, 64, .eg = EG_STRIKE},   /* (yours: saved on the FM-1) */
    {"METAL HAT", 85, 76, 0, 0, 36, 0, 36, 42, RZ_MODAL, 116, 100, 60, 40, 90, 0, 0, 0, 0, SI_OCT, 0, 5, 100, 120, 24, 5, 80, 0, SS_HALL, 127, 0, .oct = 1, .eg = EG_STRIKE},
    {"ZAP TOM", 6, 64, 64, 0, 39, 0, 39, 53, RZ_MODAL, 0, 0, 0, 0, 0, 64, 40, 100, 34, 0, 20, 0, 0, 0, 31, 5, 94, 0, SS_BOUNCE, 0, 0, .oct = -1},
    {"CLICK", 110, 64, 96, 0, 42, 0, 42, 64, RZ_DRUM, 0, 60, 80, 20, 110, 0, 0, 0, 0, SI_OCT, 0, 0, 0, 0, 38, 5, 107, 0, SS_HALL, 0, 0, .oct = -2},
    {"CLAP CLOUD", 85, 76, 0, 0, 45, 0, 45, 75, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 4, 60, 90, 45, 5, 119, 0, SS_HALL, 127, 0},
    {"RIM SHOT LAB", 85, 76, 0, 0, 48, 0, 48, 86, RZ_MODAL, 116, 100, 60, 40, 90, 0, 0, 0, 0, SI_OCT, 0, 5, 100, 120, 12, 5, 61, 0, SS_HALL, 127, 0, .oct = 1, .eg = EG_STRIKE},
    {"BOING", 6, 64, 64, 0, 31, 0, 31, 97, RZ_MODAL, 0, 0, 0, 0, 0, 64, 40, 100, 34, 0, 20, 0, 0, 0, 19, 5, 76, 0, SS_BOUNCE, 0, 0, .oct = -1},
    {"LASER", 110, 64, 96, 0, 34, 0, 34, 108, RZ_DRUM, 0, 60, 80, 20, 110, 64, 40, 100, 34, 0, 20, 0, 0, 0, 26, 5, 90, 0, SS_BOUNCE, 0, 0, .oct = -2, .mfx = MFP_CHAOS_RING},
    {"WATER DROP", 85, 76, 0, 0, 37, 0, 37, 29, RZ_MODAL, 0, 0, 0, 0, 0, 64, 40, 100, 34, 0, 20, 4, 60, 90, 33, 5, 103, 0, SS_BOUNCE, 127, 0},
    {"COIN", 85, 76, 0, 0, 40, 0, 40, 40, RZ_MODAL, 116, 100, 60, 40, 90, 64, 40, 100, 34, 0, 20, 5, 100, 120, 40, 5, 115, 0, SS_BOUNCE, 127, 0, .oct = 1},
    {"SPRING BOING", 6, 64, 64, 0, 43, 0, 43, 51, 10, 0, 60, 80, 20, 0, 64, 40, 100, 34, 0, 20, 0, 100, 120, 28, 5, 99, 0, SS_BOUNCE, 127, 64},   /* (yours: saved on the FM-1) */
    {"THUD", 110, 64, 96, 0, 46, 0, 46, 62, 10, 0, 60, 80, 20, 110, 110, 100, 60, 0, 0, 90, 0, 100, 120, 28, 5, 99, 0, 0, 127, 64, .oct = 1},   /* (yours: saved on the FM-1) */
    {"SNAP", 85, 76, 0, 0, 49, 0, 49, 73, RZ_MODAL, 0, 0, 0, 0, 0, 0, 0, 0, 0, SI_OCT, 0, 4, 60, 90, 21, 5, 86, 0, SS_HALL, 127, 0},
    {"PEW", 85, 76, 0, 0, 32, 0, 32, 84, RZ_MODAL, 116, 100, 60, 40, 90, 64, 40, 100, 34, 0, 20, 5, 100, 120, 28, 5, 99, 0, SS_BOUNCE, 127, 0, .oct = 1, .eg = EG_STRIKE},
    {"WOOD KNOCK", 85, 76, 0, 0, 35, 0, 35, 40, RZ_MODAL, 30, 70, 37, 20, 110, 0, 0, 0, 0, SI_OCT, 0, 5, 63, 0, 0, 0, 0, 0, SS_HALL, 127, 0, .oct = -1, .eg = EG_STRIKE},
};
#define NPRESETS (sizeof PRESETS / sizeof PRESETS[0])
#define NBANKS 4u                                /* four banks of 128 (bank select; HOME held + PRESETS) */
typedef char presets_are_512[NPRESETS == 128u * NBANKS ? 1 : -1];   /* (one for each MIDI program, per bank) */

/* the FX page's own presets (PRESETS turned there): the resonator / waveguide alone, the sound left as it is */
typedef struct { const char *name; uint8_t model, st, br, dm, pos, mix; } fx_preset_t;
static const fx_preset_t FX_PRESETS[] = {
    {"FX OFF", 0, 70, 90, 100, 24, 0},
    {"BELL", RZ_MODAL, 70, 100, 82, 12, 100},     {"TUNED BAR", RZ_MODAL, 116, 40, 60, 24, 70},
    {"GLASS", RZ_MODAL, 100, 127, 90, 40, 80},    {"SITAR", RZ_SYMP, 16, 80, 78, 70, 90},
    {"DRONE STRINGS", RZ_SYMP, 60, 60, 86, 40, 80}, {"PIANO STRING", RZ_STRING, 100, 70, 72, 20, 85},
    {"PLUCKED STRING", RZ_STRING, 40, 90, 45, 50, 90}, {"SOPRANO AAH", RZ_VOICE, 30, 80, 48, 60, 120},
    {"TENOR OOH", RZ_VOICE, 90, 60, 48, 4, 120},   {"BASS OH", RZ_VOICE, 115, 50, 52, 26, 120},
    {"TOM", RZ_DRUM, 10, 80, 70, 40, 127},         {"TIMPANI", RZ_DRUM, 120, 90, 95, 30, 127},
    {"TAIKO", RZ_DRUM, 0, 60, 80, 20, 127},       {"GRAND", RZ_PIANO, 40, 80, 70, 40, 127},
    {"HONKY-TONK", RZ_PIANO, 115, 90, 70, 40, 127},
    {"NIGHT FLUTE", RZ_FLUTE, 30, 70, 78, 50, 127}, {"OVERBLOWN", RZ_FLUTE, 115, 90, 88, 20, 120},
    {"BIRD", RZ_SYRINX, 90, 70, 48, 15, 127},      {"TWO BIRDS", RZ_SYRINX, 60, 90, 58, 110, 127},
    {"HAWK", RZ_HAWK, 70, 90, 58, 70, 127},
    {"TRUMPET", RZ_BRASS, 100, 100, 68, 0, 127},   {"HORN", RZ_BRASS, 20, 50, 83, 0, 127},
    {"HARMON MUTE", RZ_BRASS, 90, 100, 68, 110, 127}, {"VIOLIN", RZ_BOW, 110, 90, 88, 60, 127},
    {"CELLO", RZ_BOW, 115, 60, 93, 70, 127},      {"SUL PONTICELLO", RZ_BOW, 100, 110, 83, 8, 127},
    {"CLARINET", RZ_REED, 40, 80, 83, 0, 127},     {"SAX", RZ_REED, 60, 90, 78, 60, 127},
    {"OBOE", RZ_REED, 110, 100, 78, 120, 127},
    {"MUSETTE", RZ_ACCORD, 70, 80, 60, 64, 127}, {"BANDONEON", RZ_ACCORD, 15, 40, 80, 30, 127},
    {"HIGHLAND PIPES", RZ_BAGPIPE, 20, 100, 80, 0, 127},
    {"KALIMBA", RZ_KALIMBA, 20, 80, 64, 0, 127},
    {"DIDGERIDOO", RZ_DIDGE, 10, 60, 60, 40, 127}, {"SHAKUHACHI", RZ_SHAKU, 60, 70, 80, 0, 127},
};
#define NFX_PRESETS (sizeof FX_PRESETS / sizeof FX_PRESETS[0])
/* the SHIMMER page's own presets */
typedef struct { const char *name; uint8_t size, decay, shimmer, mix, interval, tone, space; } sh_preset_t;
static const sh_preset_t SH_PRESETS[] = {
    {"SHIMMER OFF", 100, 90, 70, 0, SI_OCT, 80, SS_HALL},
    {"UNDERWORLD", 110, 110, 100, 76, SI_OCT, 100, SS_HALL},
    {"STONE TEMPLE", 70, 95, 15, 56, SI_OCT, 50, SS_TEMPLE},
    {"ORACLE CHAMBER", 90, 110, 40, 68, SI_FIFTH, 40, SS_TEMPLE},
    {"CATHEDRAL", 127, 122, 30, 64, SI_OCT, 70, SS_CATHEDRAL},
    {"GOTHIC CHOIR", 120, 118, 90, 72, SI_OCT5, 80, SS_CATHEDRAL},
    {"MOSQUE DOME", 110, 112, 25, 60, SI_FIFTH, 100, SS_MOSQUE},
    {"DOME DEPTHS", 115, 120, 100, 72, SI_2OCT, 110, SS_MOSQUE},
    {"SUB HAZE", 127, 120, 50, 64, SI_OCT, 80, SS_HALL},
    {"SMALL ROOM", 60, 60, 20, 48, SI_OCT, 70, SS_HALL},
    {"PIANO BODY", 40, 30, 0, 72, SI_OCT, 90, SS_PIANO},
    {"GUITAR BODY", 30, 20, 0, 72, SI_OCT, 90, SS_GUITAR},
    {"VIOLIN BODY", 30, 20, 0, 72, SI_OCT, 100, SS_VIOLIN},
    {"CELLO BODY", 40, 30, 0, 72, SI_OCT, 90, SS_CELLO},
    {"HARP BODY", 50, 40, 0, 72, SI_OCT, 90, SS_HARP},
    {"ACCORDION BOX", 40, 30, 0, 72, SI_OCT, 90, SS_ACCORDION},
    {"BAGPIPE", 70, 60, 0, 72, SI_OCT, 90, SS_BAGPIPE},
    {"PIPE ORGAN", 120, 110, 0, 64, SI_OCT, 80, SS_ORGAN},
    {"LESLIE SLOW", 20, 20, 0, 90, SI_OCT, 90, SS_LESLIE},
    {"LESLIE FAST", 120, 20, 0, 90, SI_OCT, 90, SS_LESLIE},
    /* the delays: SIZE the step (0 1/32, 16 1/16T, 32 1/16, 48 1/8T, 64 1/8, 80 1/4T, 96 1/8., 112 1/4), DECAY the
     * feedback, SHIMMER the filter (127 open), TONE its resonance; the INTERVAL the pattern */
    {"PING PONG 1/8", 64, 70, SH_DFILT, 40, 0, 0, SS_PINGPONG},
    {"DOTTED PONG", 64, 80, SH_DFILT, 40, 1, 20, SS_PINGPONG},
    {"TRIPLET PONG", 64, 75, SH_DFILT, 40, 2, 10, SS_PINGPONG},
    {"FILTER PONG", 64, 85, 70, 44, 3, 70, SS_PINGPONG},
    {"SLAPBACK", 0, 10, SH_DFILT, 36, 0, 0, SS_PINGPONG},
    {"TRESILLO ECHO", 32, 60, SH_DFILT, 44, 0, 30, SS_PATTERN},
    {"DUB SIREN", 32, 110, 55, 50, 1, 90, SS_PATTERN},
    {"GALLOP", 32, 55, SH_DFILT, 40, 2, 20, SS_PATTERN},
    {"CLAVE ECHO", 32, 40, SH_DFILT, 42, 3, 40, SS_PATTERN},
    {"BOUNCING BALL", 64, 20, SH_DFILT, 50, 0, 10, SS_BOUNCE},
    {"BALL PONG", 64, 30, SH_DFILT, 50, 1, 20, SS_BOUNCE},
    {"RISING BALL", 64, 25, SH_DFILT, 50, 2, 30, SS_BOUNCE},
    {"SCATTER DELAY", 64, 50, SH_DFILT, 46, 3, 60, SS_BOUNCE},
    /* MOUTH: SHIMMER the vowel (FREE EG on SHIMMER: talking), the INTERVAL its path, SIZE the mouth */
    {"TALK BOX", 64, 20, 64, 110, 0, 60, SS_MOUTH},
    {"WAH MOUTH", 70, 20, 20, 100, 1, 60, SS_MOUTH},
    {"YAI VOICE", 50, 30, 90, 100, 2, 70, SS_MOUTH},
    {"GIANT MOUTH", 0, 40, 50, 100, 0, 50, SS_MOUTH},
};
#define NSH_PRESETS (sizeof SH_PRESETS / sizeof SH_PRESETS[0])
static uint8_t fx_pre, sh_pre;                   /* the last chosen of each (not saved: the knobs are) */

/* the user's presets: SAVE writes the sound as it is over the preset playing (its name kept), in the flash (two
 * objects of 64: a bit each saying which are overwritten, then the 30 values of each); PRESETS and program change
 * load it instead of the built-in one; the remote's PRESETS sends all 128 as C (remote.c), for ui.c's table */
#define PR_N 30                                  /* the values: preset_t's order, then SH SPACE, NOISE LEVEL, BEND */
typedef struct { uint8_t has[8]; uint8_t v[64][PR_N]; } upre_t;
/* value 27: the SPACE (bits 0, 1 and 5, 6) with the sound's octave (bits 2 .. 4, two's complement) */
#define PR_SPACE(b) ((((uint32_t)(b) & 3u) | ((uint32_t)(b) >> 3 & 28u)) % SS_COUNT)   /* (bits 0-1, 5-7) */
#define PR_PACK27(sp, oct) ((uint8_t)(((uint32_t)(sp) & 3u) | ((uint32_t)(oct) & 7u) << 2 | ((uint32_t)(sp) & 28u) << 3))
#define PR_MH 25u                                /* value 25: MOD ENV HEIGHT, its bit 7 the way (down) */
/* a HEIGHT saved before it went both ways: then 36 x (h / 127)^2 semitones, now 36 x (h / 127)^3: the same sweep */
static uint8_t menv_old(uint8_t h)
{
    uint32_t a = h & 127u, n = 0;
    while (n < 127u && (n + 1u) * (n + 1u) * (n + 1u) * 127u <= a * a * 16129u + 8064u * 127u)   /* (n^3 <= a^2 127) */
        n++;
    return (uint8_t)n;
}
/* (in RAM: which are saved, and one flash object at a time, the one last used: 512 x 30 values stay in the flash) */
static uint8_t upre_bits[NPRESETS / 8u];
static upre_t upre_buf __attribute__((section(".pool")));
static uint8_t upre_cur = 0xFF;                  /* the object in upre_buf (0xFF: none) */
static volatile uint8_t *const PR_SYS[PR_N] = {
    &sys.chaos, &sys.shape, &sys.output, &sys.attack, &sys.decay, &sys.sustain, &sys.release, &sys.ring,
    &sys.fx_model, &sys.fx_struct, &sys.fx_bright, &sys.fx_damp, &sys.fx_pos, &sys.fx_mix,
    &sys.sh_size, &sys.sh_decay, &sys.sh_shimmer, &sys.sh_mix, &sys.sh_interval, &sys.sh_tone,
    &sys.osc, &sys.np_x, &sys.np_y, &sys.menv_len, &sys.menv_skew, &sys.menv_height, &sys.scream,
    &sys.sh_space, &sys.np_level, &sys.menv_bend,
};
/* MOD FX in a saved preset: its 27 bits (mode 2, SYNC / RATIO 7, SLEW 7, RECTIFY 2, MIX 7, RECTIFY's third, the mode's
 * third) in bit 7 of values 0 .. 24, 26 and 28, free there (HEIGHT's and the SPACE's, 25 and 27, use theirs): a preset saved before has
 * them 0, MOD FX off */
static uint8_t pr_v(const uint8_t *v, uint32_t k) { return k == PR_MH || k == 27u ? v[k] : (uint8_t)(v[k] & 127u); }
static uint32_t pr_mfx_bits(void)                /* MOD FX now, as the 25 bits */
{
    return (sys.mfx_mode % MF_COUNT & 3u) | (uint32_t)(sys.mfx_rate & 127u) << 2 | (uint32_t)(sys.mfx_slew & 127u) << 9 |
           (mf_rect_of(sys.mfx_rect) & 3u) << 16 | (uint32_t)(sys.mfx_mix & 127u) << 18 |
           (mf_rect_of(sys.mfx_rect) >> 2) << 25 | (uint32_t)((sys.mfx_mode % MF_COUNT) >> 2) << 26;
}
static uint32_t pr_mfx_of(const uint8_t *v)      /* a saved preset's */
{
    uint32_t b = 0, k;
    for (k = 0; k < 25u; k++)
        b |= (uint32_t)(v[k] >> 7) << k;
    return b | (uint32_t)(v[26] >> 7) << 25 | (uint32_t)(v[28] >> 7) << 26;
}
static void pr_mfx_set(uint32_t b)               /* MOD FX from the 25 bits */
{
    sys.mfx_mode = (uint8_t)(((b & 3u) | (b >> 26 & 1u) << 2) % MF_COUNT);
    sys.mfx_rate = (uint8_t)(b >> 2 & 127u);
    sys.mfx_slew = (uint8_t)(b >> 9 & 127u);
    sys.mfx_rect = mf_rect_val((b >> 16 & 3u) | (b >> 25 & 1u) << 2);
    sys.mfx_mix = (uint8_t)(b >> 18 & 127u);
}
static upre_t *upre_obj(uint32_t h)              /* object h (64 presets) in upre_buf */
{
    if (upre_cur != h) {
        if (st_load(OBJ_USER0 + h, &upre_buf, sizeof upre_buf) != (int)sizeof upre_buf)
            memset(&upre_buf, 0, sizeof upre_buf);
        upre_cur = (uint8_t)h;
    }
    return &upre_buf;
}
static int upre_has(uint32_t i) { i %= NPRESETS; return upre_bits[i >> 3] >> (i & 7u) & 1; }
static const uint8_t *upre_v(uint32_t i)        /* (valid until the next upre_ call on another object) */
{
    i %= NPRESETS;
    return upre_obj(i >> 6)->v[i & 63u];
}
static void upre_boot(void)                      /* (settings_boot: the flash there) */
{
    uint32_t h, b;
    for (h = 0; h < NPRESETS / 64u; h++) {
        upre_cur = 0xFF;
        upre_obj(h);
        for (b = 0; b < 8u; b++)
            upre_bits[h * 8u + b] = upre_buf.has[b];
    }
}
static uint32_t feg_hold(void);
static void feg_unhold(uint32_t irq);
static int upre_save(uint32_t i)                 /* the sound now over preset i: 0 saved */
{
    uint32_t k, h = (i % NPRESETS) >> 6, j = i & 63u, irq;
    upre_t *u = upre_obj(h);
    irq = feg_hold();
    for (k = 0; k < PR_N; k++)
        u->v[j][k] = (uint8_t)(*PR_SYS[k] & (k == PR_MH ? 255u : 127u));   /* (HEIGHT: bit 7 down) */
    feg_unhold(irq);
    u->v[j][27] = PR_PACK27(sys.sh_space % SS_COUNT, sys.transpose);   /* (+ the octave) */
    {
        uint32_t b = pr_mfx_bits();
        for (k = 0; k < 25u; k++)                /* (+ MOD FX, in the free bits) */
            u->v[j][k] = (uint8_t)((u->v[j][k] & 127u) | (b >> k & 1u) << 7);
        u->v[j][26] = (uint8_t)((u->v[j][26] & 127u) | (b >> 25 & 1u) << 7);
        u->v[j][28] = (uint8_t)((u->v[j][28] & 127u) | (b >> 26 & 1u) << 7);
    }
    u->has[j >> 3] |= (uint8_t)(1u << (j & 7u));
    upre_bits[(i % NPRESETS) >> 3] = u->has[j >> 3];
    return st_save(OBJ_USER0 + h, u, sizeof *u);
}
static int upre_clear(void)                      /* the built-in presets back (the remote's RESET PRESETS) */
{
    uint32_t h;
    int e = 0;
    memset(&upre_buf, 0, sizeof upre_buf);
    memset(upre_bits, 0, sizeof upre_bits);
    for (h = 0; h < NPRESETS / 64u; h++)
        e |= st_save(OBJ_USER0 + h, &upre_buf, sizeof upre_buf);
    upre_cur = 0xFF;
    return e;
}

/* the duck around a sudden change (synth.c): fade out and wait for it (a few ms; not where the audio does not run on
 * its own: the tests, the browser), then the change, then duck_end fades back in */
static void duck_begin(void)
{
    uint32_t t0 = fm1_ms, spins = 0;
    duck_req = 1;
    while (!duck_low && fm1_ms - t0 < 20u && ++spins < 1000000u)   /* (the duck down: ~8 ms) */
        ;
}
static void duck_end(void) { duck_req = 0; duck_low = 0; }
static void preset_apply(uint32_t i)
{
    const preset_t *p = &PRESETS[i % NPRESETS];
    if (upre_has(i)) {                           /* (overwritten by SAVE: every value as it was saved) */
        const uint8_t *v = upre_v(i);
        uint32_t k;
        mwd.on = 0;
        sys.preset = (uint16_t)(i % NPRESETS);
        for (k = 0; k < PR_N; k++)
            *PR_SYS[k] = pr_v(v, k);
        pr_mfx_set(pr_mfx_of(v));
        sys.fx_model = (uint8_t)(sys.fx_model % RZ_COUNT);
        sys.sh_interval = (uint8_t)(sys.sh_interval % SI_COUNT);
        {   /* (the octave rides in the SPACE's value: 3 bits two's complement above its 2) */
            int32_t t = (int32_t)(v[27] >> 2 & 7u);
            sys.transpose = (int8_t)(t > 3 ? t - 8 : t);
        }
        sys.sh_space = (uint8_t)PR_SPACE(v[27]);
        sys.osc = (uint8_t)(sys.osc % (NP_COUNT + 1u));
        return;
    }
    mwd.on = 0;                                         /* (the mod wheel takes the new value as its base) */
    sys.preset = (uint16_t)(i % NPRESETS);
    sys.chaos = p->chaos;
    sys.shape = p->shape;
    sys.output = p->output;
    sys.attack = p->attack;
    sys.decay = p->decay;
    sys.sustain = p->sustain;
    sys.release = p->release;
    sys.ring = p->ring;
    if (p->fx_mix) {                             /* (a preset without FX leaves the resonator's knobs, MIX off) */
        sys.fx_model = p->fx_model;
        sys.fx_struct = p->fx_struct;
        sys.fx_bright = p->fx_bright;
        sys.fx_damp = p->fx_damp;
        sys.fx_pos = p->fx_pos;
    }
    sys.fx_mix = p->fx_mix;
    if (p->sh_mix) {                             /* (as the resonator: a preset without the shimmer turns it off) */
        sys.sh_size = p->sh_size;
        sys.sh_decay = p->sh_decay;
        sys.sh_shimmer = p->sh_shimmer;
        sys.sh_interval = p->sh_interval;
        sys.sh_tone = p->sh_tone;
        sys.sh_space = (uint8_t)(p->sh_space % SS_COUNT);   /* (0: HALL, the plain tank) */
    }
    sys.sh_mix = p->sh_mix;
    sys.osc = (uint8_t)(p->osc % (NP_COUNT + 1u));
    if (p->osc) {                                /* (a circuit preset leaves the noise knobs) */
        sys.np_x = p->np_x;
        sys.np_y = p->np_y;
        sys.np_level = p->np_level ? p->np_level : 64u;
        sys.menv_len = p->menv_height ? p->menv_len : 80;
        sys.menv_skew = p->menv_height ? p->menv_skew : 10;
        sys.menv_height = p->menv_height;
        sys.menv_target = 0;                     /* (a preset's MOD ENV is on the pitch; the last push forgotten:
                                                  * the preset's own values are its base) */
        menv_p.target = 0;
        menv_p.moved = 0;
        sys.menv_bend = p->menv_bend ? p->menv_bend : 64u;
    }
    sys.scream = p->scream;
    sys.transpose = (int8_t)(p->oct < -3 ? -3 : p->oct > 3 ? 3 : p->oct);
    mfx_set(p->mfx);                             /* its MOD FX (none: off) */
}
static uint8_t eg_from_preset;                   /* FREE EG holds a preset's contour (not a recording) */
static void preset_eg(uint32_t e, uint8_t was_on)  /* after the values: the preset's contour, or none */
{
    const pre_eg_t *g = &PRE_EG[e % EG_COUNT];
    uint32_t k, j;
    int32_t base;
    if (!e || e >= EG_COUNT) {
        if (eg_from_preset) {                    /* (the last preset's contour off: not this one's) */
            sys.feg_has = 0;
            eg_from_preset = 0;
            was_on = 0;
        }
        sys.feg_on = was_on;
        return;
    }
    base = *feg_field(g->target);
    for (k = 0; k < FEG_N; k++) {                /* the points, the offsets between them straight */
        int32_t v = g->p[g->n - 1u][1];
        for (j = 0; j + 1u < g->n; j++)
            if (k < (uint32_t)g->p[j + 1u][0]) {
                int32_t t0 = g->p[j][0], t1 = g->p[j + 1u][0];
                v = g->p[j][1] + (g->p[j + 1u][1] - g->p[j][1]) * ((int32_t)k - t0) / (t1 - t0);
                break;
            }
        v += base;
        feg.curve[k] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    }
    sys.feg_npts = (uint8_t)(g->p[g->n - 1u][0] + 1 > 127 ? 127 : g->p[g->n - 1u][0] + 1);
    sys.feg_target = g->target;
    sys.feg_depth = 127;
    sys.feg_len = (uint8_t)(g->len % FEG_NLEN);  /* (NOTE: each note plays it from its start; else a cycle) */
    sys.feg_has = 1;
    eg_from_preset = 1;
    sys.feg_on = 1;
}
/* MULTI: preset i as part B's sound (its values; A's put back, the audio held off a moment) */
#include "preset_gain.h"                       /* LEVEL MATCH: each built-in preset's gain (tests/level_match.sh) */
static void part_edit_set(uint32_t p)           /* the knobs on part p (B: only in MULTI), the audio held off */
{
    uint32_t irq = irq_save();
    part_edit_to(p);
    irq_restore(irq);
}
static void lvl_set(void)                        /* (each part at its preset's measured level, or as it is) */
{
    synth_pgain[0] = sys.lvl_match ? PRESET_GAIN[part_preset(0) % NPRESETS] : 4096;
    synth_pgain[1] = sys.lvl_match ? PRESET_GAIN[part_preset(1) % NPRESETS] : 4096;
}
static void part_b_load_(uint32_t i);
static void part_b_load(uint32_t i)              /* (B's preset: into B, edited or not) */
{
    uint32_t pe = part_edit;
    part_edit_set(0);
    part_b_load_(i);
    part_edit_set(pe);
    lvl_set();
}
static void part_b_load_(uint32_t i)
{
    uint8_t sa[PR_N], sm[5], mw;
    int8_t tr;
    uint16_t pre;
    uint32_t k, irq = irq_save();
    for (k = 0; k < PR_N; k++)
        sa[k] = *PR_SYS[k];
    for (k = 0; k < 5u; k++)
        sm[k] = *PART_MFX[k];
    tr = sys.transpose;
    pre = sys.preset;
    mw = mwd.on;
    preset_apply(i);
    for (k = 0; k < PR_N; k++) {
        part_b[k] = *PR_SYS[k];
        *PR_SYS[k] = sa[k];
    }
    for (k = 0; k < 5u; k++) {                   /* (B's MOD FX: its preset's) */
        part_b_mfx[k] = *PART_MFX[k];
        *PART_MFX[k] = sm[k];
    }
    part_b_tr = sys.transpose;
    part_b_preset = (uint16_t)(i % NPRESETS);
    sys.transpose = tr;
    sys.preset = pre;
    mwd.on = mw;
    irq_restore(irq);
    lvl_set();
}
static void preset_load_(uint32_t i);
static void preset_load(uint32_t i)                  /* (in the duck: no plop) */
{
    duck_begin();
    preset_load_(i);
    duck_end();
}
static void feg_edit_to(uint32_t e)              /* the page on FREE EG e (main loop, the IRQs off meanwhile: feg.c's two) */
{
    uint32_t irq;
    feg_par_t t;
    e &= 1u;
    if (e == feg_ed)
        return;
    irq = irq_save();
    feg_record(0);                               /* (a recording stops where it is) */
    feg_par_get(&t);
    feg_par_set(&feg_po);
    feg_po = t;
    feg_ed = (uint8_t)e;
    irq_restore(irq);
}

static void preset_load_(uint32_t i)
{
    uint32_t ed = feg_ed, q;
    uint8_t on;
    feg_edit_to(0);                              /* (the presets' contours: FREE EG 1's) */
    on = sys.feg_on;
    feg_pause = 1;                               /* (both FREE EGs give their parameters back; they take the new */
    q = irq_save();                              /* values as their bases when they play again) */
    feg_release_(&fegs[0]);
    feg_release_(&fegs[1]);
    irq_restore(q);
    sys.feg_on = 0;
    preset_apply(i);
    preset_eg(PRESETS[i % NPRESETS].eg, on);
    feg_pause = 0;
    feg_edit_to(ed);
    lvl_set();
}
/* FREE EG's target held at its base for a moment (what it moves around, not where it is now): the comparisons and
 * SAVE see the sound's settings, not the contour's place (the audio held off meanwhile: a few microseconds) */
static volatile uint8_t *feg_held[2];
static uint8_t feg_was[2];
static uint32_t feg_hold(void)                   /* (both FREE EGs: their targets at their bases; the IRQs off) */
{
    uint32_t i, irq = irq_save();
    for (i = 0; i < 2u; i++) {
        feg_held[i] = fegs[i].playing ? feg_field(fegs[i].target) : 0;
        if (feg_held[i]) {
            feg_was[i] = *feg_held[i];
            *feg_held[i] = fegs[i].base;
        }
    }
    return irq;
}
static void feg_unhold(uint32_t irq)             /* (given back as they were: the second first) */
{
    uint32_t i;
    for (i = 2u; i-- > 0u;)
        if (feg_held[i])
            *feg_held[i] = feg_was[i];
    irq_restore(irq);
}
static int preset_edited_(void);
static int preset_edited(void)
{
    uint32_t irq = feg_hold();
    int r = preset_edited_();
    feg_unhold(irq);
    return r;
}
static int preset_edited_(void)
{
    const preset_t *p = &PRESETS[sys.preset % NPRESETS];
    if (upre_has(sys.preset)) {
        const uint8_t *v = upre_v(sys.preset);
        uint32_t k;
        for (k = 0; k < PR_N; k++)
            if (k != 27u && (*PR_SYS[k] & (k == PR_MH ? 255u : 127u)) != pr_v(v, k))
                return 1;
        return (uint32_t)sys.sh_space != PR_SPACE(v[27]) || ((uint32_t)sys.transpose & 7u) != (v[27] >> 2 & 7u) ||
               pr_mfx_of(v) != pr_mfx_bits();
    }
    return sys.chaos != p->chaos || sys.shape != p->shape || sys.output != p->output || sys.attack != p->attack ||
           sys.decay != p->decay || sys.sustain != p->sustain || sys.release != p->release || sys.ring != p->ring ||
           sys.fx_mix != p->fx_mix || (p->fx_mix && (sys.fx_model != p->fx_model || sys.fx_struct != p->fx_struct ||
           sys.fx_bright != p->fx_bright || sys.fx_damp != p->fx_damp || sys.fx_pos != p->fx_pos)) ||
           sys.sh_mix != p->sh_mix || (p->sh_mix && (sys.sh_size != p->sh_size || sys.sh_decay != p->sh_decay ||
           sys.sh_shimmer != p->sh_shimmer || sys.sh_interval != p->sh_interval || sys.sh_tone != p->sh_tone ||
           sys.sh_space != p->sh_space % SS_COUNT)) ||
           sys.osc != p->osc || (p->osc && (sys.np_x != p->np_x || sys.np_y != p->np_y ||
           sys.np_level != (p->np_level ? p->np_level : 64u) ||
           sys.menv_height != p->menv_height || (p->menv_height && (sys.menv_len != p->menv_len ||
           sys.menv_skew != p->menv_skew || sys.menv_bend != (p->menv_bend ? p->menv_bend : 64u))))) || sys.scream != p->scream ||
           sys.transpose != p->oct || mfx_edited(p->mfx);
}

/* ------------------------------------------------------------ settings --- */
#define SET_MAGIC 0x43485547u      /* "CHUA": bump when the layout changes (older records are then ignored) */
typedef struct {
    uint32_t magic;
    int8_t octave;
    uint8_t chaos, shape, output, attack, release, view, preset, cpu2, decay, sustain, ring;
    uint8_t fx_model, fx_struct, fx_bright, fx_damp, fx_pos, fx_mix;
    uint8_t sh_size, sh_decay, sh_shimmer, sh_mix, sh_interval, sh_tone;
    uint8_t osc, np_x, np_y, np_level;
    uint8_t menv_len, menv_skew, menv_height, basis, orbit, menv_bend;
    uint8_t arp_on, arp_type, arp_bpm, arp_div, arp_gate, arp_swing, arp_hold;
    uint8_t midi_trs, midi_usb, midi_rx, midi_tx;
    uint8_t feg_len, feg_depth, feg_target, feg_on, feg_has, feg_npts, feg_curve[FEG_N];
    uint8_t k_on, k_ge, k_latch, k_m[4];
    uint8_t mw_target, mw_depth, vel_to, vel_amt;
    uint8_t tape_lv[4], tape_len, tape_arm, sh_space, tape_spd;
    int8_t transpose;
    uint8_t bank;                                /* (the preset's bank; settings saved before: bank 1) */
    uint8_t multi, ch_b;                         /* MULTI (saved before: off, B on channel 2) */
    uint16_t preset_b;                           /* part B's preset */
    uint8_t menv3;                               /* MOD ENV HEIGHT as now (both ways, cubic; saved before: squared,
                                                  * up only: the saved presets' turned once, settings_boot) */
    uint8_t tape4;                               /* (saved before the tape held 7 s: LENGTH 4 BARS, once) */
    uint8_t snd;                                 /* (saved before the SOUND page: SPEAKER BASS 4, LEVEL MATCH on; 1: 1.9.2's 64) */
    uint8_t spk_bass, lvl_match;
    uint8_t bend_range;                          /* (3: saved; before: 12) */
    uint8_t k_logic;                             /* (4: saved; before: OFF) */
    uint8_t k_inv;                               /* (11: saved; before: off) */
    uint8_t arp_vp, arp_gp, arp_vd, arp_gd;      /* (8: saved; before: off, 100; 13: gp the GATE SEQ, gd its length) */
    uint8_t arp_slen;                            /* (10: saved; 12: the VELOCITY SEQ's length, 0 its own) */
    uint8_t k_root;                              /* (9: saved; NOTE QUANT's new list, CHORD once) */
    uint8_t k_lq;                                /* (4: saved; before: SCALE; 5: SUBDIVIDE's 1/8 turned to once; 6: LOGIC OR, SCALE once; 7: the ARP's LATCH on once) */
    uint8_t menv_target;                         /* (16: saved; before: the pitch) */
    uint8_t ui_style, ui_motion, ui_backdrop, ui_react;   /* (17: saved; before: INITIAL, 64, 70, 80) */
    uint8_t clk_int;                             /* (21: saved; before: EXTERNAL) */
    uint8_t spare22;                             /* (22 .. 24: CLOCK PPQ, gone: MIDI clock is 24 a quarter) */
    uint8_t autoplay;                            /* (24: saved; before: on, once after this install) */
    uint8_t mfx[5];                              /* (25: MOD FX: mode, rate, slew, rectify, mix; before: off) */
    uint8_t tune432;                             /* (26: saved; before: A 440) */
    uint8_t mono;                                /* (27: saved, bit 0 A MONO, bit 1 B; before: POLY) */
    uint8_t feg2_len, feg2_depth, feg2_target, feg2_on, feg2_has, feg2_npts, feg2_curve[FEG_N];   /* (28: FREE EG 2) */
    uint8_t analog;                              /* (29: saved; before: INITIAL) */
} settings_t;

#define BOOT_PRESET 299u                        /* after an install: bank 3 (SYNTH), FIFTH PAD; then the last chosen */
static uint8_t menv3_save;                       /* (settings_boot turned the saved presets' HEIGHT: save the mark) */
static int settings_save(void);
static void settings_boot(void)    /* power-on, before the audio */
{
    settings_t s;
    uint32_t f = irq_save();
    sys.tape_len = 3;                            /* (the tape's LENGTH: 2 bars, unless saved) */
    flash_ok = fl_id_ok(FL_FAR(fl_jedec_ram)());   /* (either chip) */
    if (fl_emulated()) {                         /* (a browser's emulated FM-1: lighter, so it plays in time there; a
                                                  * real FM-1 never: see fl_emulated) */
        fm1_emu = 1;
        synth_light_min = 1;
        synth_light = 1;
    }
    irq_restore(f);
    if (!flash_ok) {
        preset_load(BOOT_PRESET);
        part_b_load(BOOT_PRESET);
        return;
    }
    upre_boot();
    memset(&s, 0, sizeof s);                     /* (saved before the bank: its byte 0, bank 1) */
    if (st_load(OBJ_SETTINGS, &s, sizeof s) < (int)__builtin_offsetof(settings_t, bank) || s.magic != SET_MAGIC)
    {
        preset_load(BOOT_PRESET);                /* (a fresh install, nothing saved: bank 3, SYNTH, FIFTH PAD) */
        part_b_load(BOOT_PRESET);
    } else {
        if (!s.menv3) {                          /* (saved before HEIGHT went both ways: the saved presets' turned to
                                                  * the same sweep, once, and the settings marked) */
            uint32_t h, j;
            for (h = 0; h < NPRESETS / 64u; h++) {
                upre_t *u = upre_obj(h);
                uint32_t any = 0;
                for (j = 0; j < 64u; j++)
                    if (u->has[j >> 3] >> (j & 7u) & 1u && (u->v[j][PR_MH] & 127u)) {
                        u->v[j][PR_MH] = menv_old(u->v[j][PR_MH]);
                        any = 1;
                    }
                if (any)
                    st_save(OBJ_USER0 + h, u, sizeof *u);
            }
            menv3_save = 1;
        }
        sys.multi = (uint8_t)(s.multi != 0);
        sys.ch_b = (uint8_t)(s.ch_b <= 16u ? s.ch_b : 2u);   /* (0: OMNI, B layered on A) */
        part_b_load(s.preset_b % NPRESETS);
        sys.octave = (int8_t)clamp(s.octave, OCT_MIN, OCT_MAX);
        sys.chaos = (uint8_t)(s.chaos & 127u);
        sys.shape = (uint8_t)(s.shape & 127u);
        sys.output = (uint8_t)(s.output & 127u);
        sys.attack = (uint8_t)(s.attack & 127u);
        sys.release = (uint8_t)(s.release & 127u);
        sys.decay = (uint8_t)(s.decay & 127u);
        sys.sustain = (uint8_t)(s.sustain & 127u);
        sys.ring = (uint8_t)(s.ring & 127u);
        sys.view = V_3D;                         /* (the turning 3D picture: SELECT on HOME is the octave) */
        sys.preset = (uint16_t)((s.preset & 127u) + 128u * (s.bank % NBANKS));
        sys.cpu2 = 1u;                           /* (2ND CORE: always ALL, not to be changed; the saved one ignored) */
        sys.fx_model = (uint8_t)(s.fx_model % RZ_COUNT);
        sys.fx_struct = (uint8_t)(s.fx_struct & 127u);
        sys.fx_bright = (uint8_t)(s.fx_bright & 127u);
        sys.fx_damp = (uint8_t)(s.fx_damp & 127u);
        sys.fx_pos = (uint8_t)(s.fx_pos & 127u);
        sys.fx_mix = (uint8_t)(s.fx_mix & 127u);
        sys.sh_size = (uint8_t)(s.sh_size & 127u);
        sys.sh_decay = (uint8_t)(s.sh_decay & 127u);
        sys.sh_shimmer = (uint8_t)(s.sh_shimmer & 127u);
        sys.sh_mix = (uint8_t)(s.sh_mix & 127u);
        sys.sh_interval = (uint8_t)(s.sh_interval % SI_COUNT);
        sys.sh_space = (uint8_t)(s.sh_space % SS_COUNT);
        sys.sh_tone = (uint8_t)(s.sh_tone & 127u);
        sys.osc = (uint8_t)(s.osc % (NP_COUNT + 1u));
        sys.np_x = (uint8_t)(s.np_x & 127u);
        sys.np_y = (uint8_t)(s.np_y & 127u);
        sys.np_level = (uint8_t)(s.np_level & 127u);
        sys.menv_len = (uint8_t)(s.menv_len & 127u);
        sys.menv_skew = (uint8_t)(s.menv_skew & 127u);
        sys.menv_height = (uint8_t)(s.menv3 ? s.menv_height : menv_old(s.menv_height));   /* (bit 7: down) */
        sys.menv_bend = (uint8_t)(s.menv_bend & 127u);
        sys.basis = (uint8_t)(s.basis & 127u);
        sys.orbit = (uint8_t)(s.orbit & 127u);
        sys.arp_on = 0;                          /* (PLAY off at power-on: the ARP and KARMA wait for PLAY) */
        sys.arp_type = (uint8_t)(s.arp_type % ARP_NTYPE);
        sys.arp_bpm = (uint8_t)clamp(s.arp_bpm, 40, 250);
        if (s.snd < 14u && sys.arp_bpm == 120u)     /* (14: 128 BPM the default; the old default moved, once) */
            sys.arp_bpm = 128;
        sys.arp_div = (uint8_t)(s.snd >= 5u ? s.arp_div % ARP_NDIV : 4u);   /* (SUBDIVIDE 1/8 now, once) */
        sys.arp_gate = (uint8_t)(s.arp_gate & 127u);
        sys.arp_swing = (uint8_t)(s.arp_swing & 127u);
        sys.arp_hold = (uint8_t)(s.snd >= 7u ? s.arp_hold != 0 : 1u);   /* (LATCH on after this install, once) */
        sys.midi_trs = 1;                        /* (TRS MIDI IN: always on now) */
        sys.midi_usb = (uint8_t)(s.midi_usb & 3u);
        sys.midi_rx = (uint8_t)(s.midi_rx > 16u ? 0u : s.midi_rx);
        sys.midi_tx = (uint8_t)(s.midi_tx & 15u);
        sys.feg_len = (uint8_t)(s.feg_len % FEG_NLEN);
        sys.feg_depth = (uint8_t)(s.feg_depth & 127u);
        sys.feg_target = (uint8_t)(s.feg_target % FEG_NT);
        sys.feg_on = (uint8_t)(s.feg_on != 0);
        sys.feg_has = (uint8_t)(s.feg_has != 0);
        sys.feg_npts = (uint8_t)(s.feg_npts & 127u);
        for (uint32_t i = 0; i < FEG_N; i++)
            fegs[0].curve[i] = (uint8_t)(s.feg_curve[i] & 127u);
        if (s.snd >= 28u) {                      /* (28: FREE EG 2 saved; before: empty, off) */
            feg_po.len = (uint8_t)(s.feg2_len % FEG_NLEN);
            feg_po.depth = (uint8_t)(s.feg2_depth & 127u);
            feg_po.target = (uint8_t)(s.feg2_target % FEG_NT);
            feg_po.on = (uint8_t)(s.feg2_on != 0);
            feg_po.has = (uint8_t)(s.feg2_has != 0);
            feg_po.npts = (uint8_t)(s.feg2_npts & 127u);
            for (uint32_t i = 0; i < FEG_N; i++)
                fegs[1].curve[i] = (uint8_t)(s.feg2_curve[i] & 127u);
        }
        sys.k_on = 0;
        sys.k_ge = (uint8_t)(s.k_ge % KGE_N);
        sys.k_latch = (uint8_t)(s.k_latch != 0);
        sys.mw_target = (uint8_t)(s.mw_target % FEG_NT);
        sys.mw_depth = (uint8_t)(s.mw_depth & 127u);
        sys.vel_to = (uint8_t)(s.vel_to & 3u);
        sys.vel_amt = (uint8_t)(s.vel_amt & 127u);
        for (uint32_t i = 0; i < 4u; i++)
            sys.tape_lv[i] = (uint8_t)(s.tape_lv[i] & 127u);
        sys.tape_len = (uint8_t)(s.tape4 ? s.tape_len % 5u : 3u);
        if (s.snd < 15u && sys.tape_len == 4u)    /* (15: 2 BARS the default; the old 4 BARS moved, once) */
            sys.tape_len = 3;
        sys.spk_bass = (uint8_t)(s.snd >= 2u ? s.spk_bass & 127u : 4u);   /* (1.9.2 saved 64: 4 now, once) */
        sys.lvl_match = (uint8_t)(s.snd ? s.lvl_match != 0 : 1u);
        sys.k_logic = (uint8_t)(s.snd >= 6u ? s.k_logic % 7u : 1u);   /* (LOGIC OR, NOTE QUANT SCALE now, once) */
        sys.k_lq = (uint8_t)(s.snd >= 9u ? s.k_lq % LQ_N : LQ_CHORD);   /* (NOTE QUANT CHORD after this install, once) */
        sys.k_root = (uint8_t)(s.snd >= 9u ? s.k_root % 13u : 0u);
        sys.arp_slen = (uint8_t)(s.snd >= 12u && s.arp_slen <= 8u ? s.arp_slen : 0u);   /* (12: the VELOCITY SEQ's) */
        sys.k_inv = (uint8_t)(s.snd >= 11u && s.k_inv);
        sys.menv_target = (uint8_t)(s.snd >= 16u && s.menv_target <= FEG_NT ? s.menv_target : 0u);
        sys.clk_int = (uint8_t)(s.snd < 23u || s.clk_int);   /* (23: EXTERNAL strict now: INTERNAL, once) */
        sys.autoplay = (uint8_t)(s.snd < 24u || s.autoplay);   /* (on after this install, once; then as saved) */
        if (s.snd >= 25u) {
            sys.mfx_mode = (uint8_t)(s.mfx[0] % MF_COUNT);
            sys.mfx_rate = (uint8_t)(s.mfx[1] & 127u);
            sys.mfx_slew = (uint8_t)(s.mfx[2] & 127u);
            sys.mfx_rect = (uint8_t)(s.mfx[3] & 127u);
            sys.mfx_mix = (uint8_t)(s.mfx[4] & 127u);
        }
        sys.tune432 = (uint8_t)(s.snd >= 26u && s.tune432);
        sys.analog = (uint8_t)(s.snd >= 29u && s.analog);
        part_mono[0] = (uint8_t)(s.snd >= 27u && (s.mono & 1u));
        part_mono[1] = (uint8_t)(s.snd >= 27u && (s.mono & 2u));
        if (s.snd >= 17u) {                      /* (20: kept; before: PSYCHEDELIC, once, after this install) */
            sys.ui_style = (uint8_t)(s.snd >= 20u ? s.ui_style % US_COUNT : US_PSYCHE);
            sys.ui_motion = (uint8_t)(s.ui_motion & 127u);
            sys.ui_backdrop = (uint8_t)(s.ui_backdrop & 127u);
            sys.ui_react = (uint8_t)(s.ui_react & 127u);
        }
        sys.arp_gp = (uint8_t)(s.snd >= 13u ? s.arp_gp % ARP_NGP : 0u);   /* (13: the GATE SEQ; before: the NOTE OFF page's) */
        sys.arp_glen = (uint8_t)(s.snd >= 13u && s.arp_gd <= 8u ? s.arp_gd : 0u);
        if (s.snd >= 8u) {
            sys.arp_vp = (uint8_t)(s.arp_vp % ARP_NVP);
            sys.arp_vd = (uint8_t)(s.arp_vd & 127u);
        }
        sys.bend_range = (uint8_t)(s.snd >= 3u ? (s.bend_range > 12u ? 12u : s.bend_range) : 12u);
        sys.tape_arm = (uint8_t)(s.tape_arm & 3u);
        sys.tape_spd = (uint8_t)(s.tape_spd % TAPE_NSPD);
        sys.transpose = (int8_t)clamp(s.transpose, -3, 3);
        for (uint32_t i = 0; i < 4u; i++)
            sys.k_m[i] = (uint8_t)(s.k_m[i] & 127u);
    }
    if (menv3_save) {                            /* (the mark: the turn above done once) */
        menv3_save = 0;
        settings_save();
    }
}

static settings_t set_saved;                     /* the state in flash (as last written, or found at boot) */
static void settings_fill_now(settings_t *sp);
static void settings_fill(settings_t *sp)        /* the state now, as saved: FREE EG's and the wheel's targets at the
                                                  * values they give back (not where they move them); A's sound as
                                                  * A's while B is edited */
{
    uint32_t pe = part_edit, f, fd = feg_ed;
    part_edit_set(0);
    feg_edit_to(0);                              /* (FREE EG 1 in sys, 2 in feg_po: as saved) */
    f = feg_hold();
    volatile uint8_t *mw = mwd.on ? feg_field(mwd.target) : 0;
    volatile uint8_t *me = menv_p.target && menv_p.moved ? feg_field(menv_p.target - 1u) : 0;
    uint8_t mv = mw ? *mw : 0, ev = me ? *me : 0;
    if (me)
        *me = (uint8_t)clamp((int32_t)*me - menv_p.moved, 0, 127);
    if (mw)
        *mw = mwd.base;
    settings_fill_now(sp);
    if (mw)
        *mw = mv;
    if (me)
        *me = ev;
    feg_unhold(f);
    feg_edit_to(fd);
    part_edit_set(pe);
}
static void settings_fill_now(settings_t *sp)
{
#define s (*sp)
    memset(&s, 0, sizeof s);
    s.magic = SET_MAGIC;
    s.menv3 = 1;
    s.tape4 = 1;
    s.snd = 29;
    s.autoplay = sys.autoplay;
    s.mfx[0] = sys.mfx_mode; s.mfx[1] = sys.mfx_rate; s.mfx[2] = sys.mfx_slew; s.mfx[3] = sys.mfx_rect; s.mfx[4] = sys.mfx_mix;
    s.tune432 = sys.tune432;
    s.analog = sys.analog;
    s.mono = (uint8_t)(part_mono[0] | part_mono[1] << 1);
    s.clk_int = sys.clk_int;
    s.k_inv = sys.k_inv;
    s.menv_target = sys.menv_target;
    s.ui_style = sys.ui_style;
    s.ui_motion = sys.ui_motion;
    s.ui_backdrop = sys.ui_backdrop;
    s.ui_react = sys.ui_react;
    s.arp_slen = sys.arp_slen;
    s.arp_gp = sys.arp_gp;
    s.arp_gd = sys.arp_glen;
    s.arp_vp = sys.arp_vp;
    s.arp_vd = sys.arp_vd;
    s.k_logic = sys.k_logic;
    s.k_lq = sys.k_lq;
    s.k_root = sys.k_root;
    s.bend_range = sys.bend_range;
    s.spk_bass = sys.spk_bass;
    s.lvl_match = sys.lvl_match;
    s.octave = sys.octave;
    s.chaos = sys.chaos;
    s.shape = sys.shape;
    s.output = sys.output;
    s.attack = sys.attack;
    s.release = sys.release;
    s.decay = sys.decay;
    s.sustain = sys.sustain;
    s.ring = sys.ring;
    s.view = sys.view;
    s.preset = (uint8_t)(sys.preset & 127u);
    s.bank = (uint8_t)(sys.preset >> 7);
    s.multi = sys.multi;
    s.ch_b = sys.ch_b;
    s.preset_b = part_preset(1);
    s.cpu2 = sys.cpu2;
    s.fx_model = sys.fx_model;
    s.fx_struct = sys.fx_struct;
    s.fx_bright = sys.fx_bright;
    s.fx_damp = sys.fx_damp;
    s.fx_pos = sys.fx_pos;
    s.fx_mix = sys.fx_mix;
    s.sh_size = sys.sh_size;
    s.sh_decay = sys.sh_decay;
    s.sh_shimmer = sys.sh_shimmer;
    s.sh_mix = sys.sh_mix;
    s.sh_interval = sys.sh_interval;
    s.sh_space = sys.sh_space;
    s.sh_tone = sys.sh_tone;
    s.osc = sys.osc;
    s.np_x = sys.np_x;
    s.np_y = sys.np_y;
    s.np_level = sys.np_level;
    s.menv_len = sys.menv_len;
    s.menv_skew = sys.menv_skew;
    s.menv_height = sys.menv_height;
    s.menv_bend = sys.menv_bend;
    s.basis = sys.basis;
    s.orbit = sys.orbit;
    s.arp_on = sys.arp_on;
    s.arp_type = sys.arp_type;
    s.arp_bpm = sys.arp_bpm;
    s.arp_div = sys.arp_div;
    s.arp_gate = sys.arp_gate;
    s.arp_swing = sys.arp_swing;
    s.arp_hold = sys.arp_hold;
    s.midi_trs = sys.midi_trs;
    s.midi_usb = sys.midi_usb;
    s.midi_rx = sys.midi_rx;
    s.midi_tx = sys.midi_tx;
    s.feg_len = sys.feg_len;
    s.feg_depth = sys.feg_depth;
    s.feg_target = sys.feg_target;
    s.feg_on = sys.feg_on;
    s.feg_has = sys.feg_has;
    s.feg_npts = sys.feg_npts;
    memcpy(s.feg_curve, fegs[0].curve, sizeof s.feg_curve);
    s.feg2_len = feg_po.len;                     /* (FREE EG 2: settings_fill has 1 in sys) */
    s.feg2_depth = feg_po.depth;
    s.feg2_target = feg_po.target;
    s.feg2_on = feg_po.on;
    s.feg2_has = feg_po.has;
    s.feg2_npts = feg_po.npts;
    memcpy(s.feg2_curve, fegs[1].curve, sizeof s.feg2_curve);
    s.k_on = sys.k_on;
    s.k_ge = sys.k_ge;
    s.k_latch = sys.k_latch;
    s.mw_target = sys.mw_target;
    s.mw_depth = sys.mw_depth;
    s.vel_to = sys.vel_to;
    s.vel_amt = sys.vel_amt;
    for (uint32_t i = 0; i < 4u; i++)
        s.tape_lv[i] = sys.tape_lv[i];
    s.tape_len = sys.tape_len;
    s.tape_arm = sys.tape_arm;
    s.tape_spd = sys.tape_spd;
    s.transpose = sys.transpose;
    for (uint32_t i = 0; i < 4u; i++)
        s.k_m[i] = sys.k_m[i];
#undef s
}
static int settings_save(void)     /* 0 = saved */
{
    settings_t s;
    int e;
    if (!flash_ok)
        return -1;
    settings_fill(&s);
    e = st_save(OBJ_SETTINGS, &s, sizeof s);
    if (!e)
        set_saved = s;
    return e;
}

/* -------------------------------------------------------------- text --- */
enum { T_HEAD, T_C1, T_C2, T_C3, T_C4, T_STATUS, NTEXT };
static char ui_shown[NTEXT][48];
static uint8_t ui_force = 1;       /* redraw everything (after the splash, the update screen) */
static char ui_event[24];
static uint32_t ui_event_ms, ui_hot_ms;
static uint8_t ui_hot = 0xFF;      /* the cell whose knob turned last (its value in the accent colour, 1 s) */

static void ui_say(const char *s)                    /* the status line, for 2 s */
{
    str_cpy(ui_event, s, sizeof ui_event);
    ui_event_ms = fm1_ms | 1u;
}

static void ui_cat(char *d, uint32_t n, const char *s)
{
    uint32_t i = str_len(d);
    while (*s && i + 1u < n)
        d[i++] = *s++;
    d[i] = 0;
}

static int ui_changed(uint32_t i, const char *a, const char *b)   /* the text of slot i is new (and kept) */
{
    char sig[48];
    str_cpy(sig, a, sizeof sig);
    ui_cat(sig, sizeof sig, "|");
    ui_cat(sig, sizeof sig, b);
    if (!ui_force && str_eq(ui_shown[i], sig))
        return 0;
    str_cpy(ui_shown[i], sig, sizeof ui_shown[i]);
    return 1;
}

static void ui_head(const char *name, const char *note)
{
    if (!ui_changed(T_HEAD, name, note))
        return;
    cv_begin(240, AF_M.h, T_BG);
    cv_text(8, 0, &AF_M, name, T_TEXT);
    cv_text_r(232, 0, &AF_M, note, T_THEME, T_BG);
    cv_blit(0, 2);
    lcd_sync();
}

/* the MOD ENV page's colour (the pitch envelope, the FM of the noise programs): the DX7's green, for overview */
#define T_FM ((uint16_t)(7u << 11 | 49u << 5 | 17u))    /* (60, 196, 140) */
#define T_FM_IN T_THEME                           /* a program's own FM (not the MOD oscillator's): the theme's orange */
static uint16_t ui_val_c;                        /* the cells' values in a page's own colour (0: T_TEXT) */
static int np_is_fm(const char *s)              /* a noise program's knob that is FM: its name has FM or MOD in it */
{
    for (; *s; s++)
        if ((s[0] == 'F' && s[1] == 'M') || (s[0] == 'M' && s[1] == 'O' && s[2] == 'D'))
            return 1;
    return 0;
}
#define FEG_FM(t) ((t) % FEG_NT == 7u)   /* FREE EG's MOD ENV HEIGHT (green, as its page) */

/* PSYCHEDELIC (UI STYLE): a value's colour by what it is: a number round the hues (0 red .. 127 violet, and on), a
 * word its own hue, OFF and - quiet; pale enough to read */
static uint16_t ui_val_hue(const char *v)
{
    uint32_t n = 0, h = 0, i, r, g, b, seg, f;
    int num = 0;
    if (!v[0] || (v[0] == '-' && !v[1]) || (v[0] == 'O' && v[1] == 'F' && v[2] == 'F' && !v[3]))
        return T_MID;                            /* (- and OFF) */
    for (i = 0; v[i] && i < 20u; i++) {
        if (v[i] >= '0' && v[i] <= '9') {
            n = n * 10u + (uint32_t)(v[i] - '0');
            num = 1;
        } else if (num)
            break;
        else if (v[i] != '+' && v[i] != '-' && v[i] != ' ')
            break;
    }
    if (num)
        h = (n * 2u) & 255u;
    else
        for (i = 0; v[i]; i++)
            h = (h * 31u + (uint8_t)v[i]) & 255u;
    seg = h * 6u / 256u;                         /* (the hue round six sides, as ui_style.c's us_hue) */
    f = (h * 6u) % 256u;
    switch (seg) {
    case 0: r = 255; g = f; b = 0; break;
    case 1: r = 255 - f; g = 255; b = 0; break;
    case 2: r = 0; g = 255; b = f; break;
    case 3: r = 0; g = 255 - f; b = 255; break;
    case 4: r = f; g = 0; b = 255; break;
    default: r = 255; g = 0; b = 255 - f; break;
    }
    r = (r * 3u + 255u) / 4u;                    /* (a quarter white: readable on the dark) */
    g = (g * 3u + 255u) / 4u;
    b = (b * 3u + 255u) / 4u;
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* a knob's cell: its label small, its value under it */
static uint16_t ui_hot_col;                      /* (the turned knob's value colour: PSYCHEDELIC lights up what it moves
                                                  * in it, ui_style.c) */
static void ui_cell(uint32_t i, const char *label, const char *value)
{
    static const uint8_t X[4] = {8, 124, 8, 124}, Y[4] = {152, 152, 186, 186};
    uint32_t c = i - T_C1;
    char hot[2] = {(char)('0' + (ui_hot == c && fm1_ms - ui_hot_ms < 1000u)), 0};
    char v[24];
    str_cpy(v, value, sizeof v);
    if (hot[0] == '1') {
        ui_hot_col = ux_mix(ui_val_hue(value), 0xFFFFu, 45);   /* (as its value shows it, just turned: one colour) */
    }
    ui_cat(v, sizeof v, hot);                               /* (the highlight is part of what is shown) */
    if (!ui_changed(i, label, v))
        return;
    cv_begin(112, AF_S.h + AF_M.h, T_BG);
    cv_text(0, 0, &AF_S, label, T_MID);
    cv_text(0, AF_S.h, &AF_M, value, sys.ui_style % US_COUNT == US_PSYCHE ? (hot[0] == '1' ? ux_mix(ui_val_hue(value), 0xFFFFu, 45)
                                                                                         : ui_val_hue(value))
                                   : hot[0] == '1' ? T_ACCENT : ui_val_c ? ui_val_c : T_TEXT);   /* (PSYCHEDELIC: the value's
                                                  * own colour; just turned: paler) */
    cv_blit(X[c], Y[c]);
    lcd_sync();
}

/* the status line; a button with more than one page (ENV: + MOD ENV, FX: + SHIMMER) shows its layers as
 * small blocks in the right corner: the page shown filled in the theme's orange, the other an empty outline */
static void ui_status(const char *s)
{
    /* the pages behind one button: each page its place (1..) and how many (0: one alone) — HOME (BANK), ENV (MOD
     * ENV), FX (SHIMMER), ARP (GROOVE), REC (the TAPE's two menus), GLO (MIDI, MULTI, SOUND), SEQ (KARMA, LOGIC); LFO
     * (FREE EG 1, 2: below) */
    static const uint8_t LAYER[21] = {1, 1, 2, 3, 0, 2, 1, 1, 0, 1, 0, 0, 1, 2, 2, 2, 2, 2, 2, 3, 1};
    static const uint8_t COUNT[21] = {2, 2, 3, 3, 0, 2, 2, 3, 0, 2, 0, 0, 2, 2, 2, 4, 3, 2, 2, 3, 3};
    uint32_t pg = sys.page < 21u ? sys.page : 0u, l = LAYER[pg], nb = COUNT[pg], w;
    if (pg == 8u) {                              /* LFO: FREE EG 1, 2 (one page, the two in turn) */
        l = 1u + feg_ed;
        nb = 2;
    }
    w = 6u + nb * 10u;
    char sig[3] = {(char)('0' + l), (char)('0' + nb), 0};
    if (!ui_changed(T_STATUS, s, sig))
        return;
    if (!nb) {
        draw_text_line(0, 223, 240, &AF_S, s, T_MID, T_BG, 1);
        return;
    }
    draw_text_line(0, 223, 240u - w, &AF_S, s, T_MID, T_BG, 1);
    cv_begin(w, AF_S.h, T_BG);
    {
        uint32_t i, y = (AF_S.h - 7u) / 2u;
        for (i = 0; i < nb; i++) {
            if (i + 1u == l)
                cv_rect((int32_t)(4u + i * 10u), (int32_t)y, 7, 7, T_THEME);
            else
                cv_frame((int32_t)(4u + i * 10u), (int32_t)y, 7, 7, T_MID);
        }
    }
    cv_blit(240u - w, 223);
}

/* -------------------------------------------------------------- plot --- */
static int32_t sc_c[3], sc_r[3] = {2 << 8, 1 << 7, 3 << 8};   /* each axis: its centre and half range, Q8 (smoothed) */

/* the axes' ranges over the points drawn: the centre follows, the range grows at once and shrinks slowly */
static void plot_scale(const int16_t *const *a, uint32_t w, uint32_t n)
{
    uint32_t k, i;
    for (k = 0; k < 3u; k++) {
        int32_t lo = 32767, hi = -32768, c, r;
        for (i = w - n; i != w; i++) {
            int32_t v = a[k][i & (TRACE_N - 1u)];
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        c = (lo + hi) / 2;
        r = (hi - lo) / 2 + 8;
        sc_c[k] += (c - sc_c[k]) / 4;
        sc_r[k] = r > sc_r[k] ? r : sc_r[k] - (sc_r[k] - r) / 6;
    }
}
static int32_t plot_norm(int32_t v, uint32_t k)             /* -> -4096..4096 */
{
    return clamp((v - sc_c[k]) * 4096 / sc_r[k], -4096, 4096);
}

static int16_t plot_sin(uint32_t a)                          /* a: 256 per turn, Q15 */
{
    uint32_t q = a & 255u;
    static const int16_t S[65] = {0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278,
        11039, 11793, 12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403,
        22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268,
        29621, 29956, 30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678,
        32728, 32757, 32767};
    int16_t s = S[q & 64u ? 64u - (q & 63u) : q & 63u];
    return (int16_t)(q & 128u ? -s : s);
}

/* the silent collapse: as the sound dies away the waveform pictures fold in on themselves (graphics only; the
 * sound is not touched), as an old screen's picture shrinks to a point: 256 open .. 0 a point. It follows the audio
 * out's level (synth.c's peak), in octaves: a point below ~-54 dB, fully open from ~-24 dB; the release, the reverb's
 * tail fold it slowly as they fade. It opens in ~60 ms, folds at most ~400 ms a full fold (smoothing the level) */
static int32_t wave_open(void)
{
    static int32_t c = 256;
    static uint32_t last;
    uint32_t dt = fm1_ms - last, l = synth_out_lvl, b = 0u;
    int32_t lg, tg;
    while (b < 31u && (l >> (b + 1u)))           /* (log2 of the level, Q8: its top bit, then the next 8 bits) */
        b++;
    lg = l ? (int32_t)(b * 256u + ((b >= 8u ? l >> (b - 8u) : l << (8u - b)) & 255u)) : 0;
    tg = (lg - 6 * 256) / 5;                     /* (64 .. 2048: 0 .. 256) */
    tg = tg < 0 ? 0 : tg > 256 ? 256 : tg;
    if (dt) {
        int32_t up = (int32_t)(dt > 200u ? 200u : dt) * 256 / 60, dn = (int32_t)(dt > 200u ? 200u : dt) * 256 / 400;
        last = fm1_ms;
        c = tg > c ? (c + up < tg ? c + up : tg) : (c - dn > tg ? c - dn : tg);
    }
    return c;
}

static void draw_plot(void)
{
    static const int16_t *const AX[3] = {chua_trace.x, chua_trace.y, chua_trace.z};
    uint32_t w = chua_trace.w, n = w < TRACE_DRAW ? w : TRACE_DRAW, i, view = sys.view % V_COUNT;
    int32_t px = 0, py = 0, cx = 120, cy = PLOT_H / 2, sx = 112, sy = PLOT_H / 2 - 6;
    uint16_t band[4];
    int32_t ca = 0, sa = 0;
    int32_t op = wave_open();
    if (n < 2u)
        return;
    sx = sx * (op + 4) / 260;                    /* (the silent collapse: folded towards the middle) */
    sy = sy * (op + 4) / 260;
    band[0] = ux_mix(T_BG, T_THEME, 22);
    band[1] = ux_mix(T_BG, T_THEME, 45);
    band[2] = ux_mix(T_BG, T_THEME, 72);
    band[3] = T_THEME;
    if (sys.page == 0u && ui_hot < 4u && fm1_ms - ui_hot_ms < 800u) {   /* (an oscillator knob just turned: the trace
                                                  * brighter, in its own orange, fading over 0.8 s: what it changes, seen) */
        int32_t hl = 80 - (int32_t)(fm1_ms - ui_hot_ms) / 10;
        for (i = 0; i < 3u; i++)
            band[i] = ux_mix(band[i], T_THEME, hl * (int32_t)(i + 1u) / 4);
    }
    plot_scale(AX, w, n);
    cv_begin(240, PLOT_H, T_BG);
    if (view == V_3D) {
        uint32_t ang = (fm1_ms / 47u) & 255u;               /* one turn in ~12 s */
        ca = plot_sin(ang + 64u);
        sa = plot_sin(ang);
    }
    if (view == V_TIME) {                                    /* x (bright) and y (faint) against time */
        uint32_t m = n < 240u ? n : 240u, k;
        for (k = 0; k < 2u; k++) {
            uint32_t ax = k ? 1u : 0u;
            for (i = 0; i < m; i++) {
                int32_t v = AX[ax][(w - m + i) & (TRACE_N - 1u)];
                int32_t x = (int32_t)i, y = cy - plot_norm(v, ax) * sy / 4096;
                if (i)
                    cv_line(px, py, x, y, k ? band[1] : band[3]);
                px = x;
                py = y;
            }
        }
    } else {
        for (i = 0; i < n; i++) {
            uint32_t at = (w - n + i) & (TRACE_N - 1u);
            int32_t x = plot_norm(chua_trace.x[at], 0), y = plot_norm(chua_trace.y[at], 1), z = plot_norm(chua_trace.z[at], 2);
            int32_t u, v;
            switch (view) {
            case V_3D: {                                     /* turning about the vertical, tilted ~25 degrees */
                int32_t d = (-x * sa + z * ca) >> 15;
                u = (x * ca + z * sa) >> 15;
                v = (y * 29696 + d * 13848) >> 15;
                break;
            }
            case V_XY: u = x; v = y; break;
            case V_XZ: u = x; v = z; break;
            default:   u = y; v = z; break;
            }
            u = cx + u * sx / 4096;
            v = cy - v * sy / 4096;
            if (i)
                cv_line(px, py, u, v, band[i * 4u / n]);
            px = u;
            py = v;
        }
    }
    if (op < 32)                                 /* (folded: a point left glowing, fading) */
        cv_rect(cx - 1, cy - 1, 3, 3, ux_mix(T_BG, T_THEME, 40 + op * 2));
    cv_blit(0, PLOT_Y);                                      /* (DMA: runs on while the next frame is prepared) */
}

/* an ADSR time as text: 0.5 ms .. 10 s */
static void env_time(char *b, uint32_t n, uint32_t v)
{
    uint32_t us = ENV_FRAMES[v & 127u] * 1000u / 44u;        /* (frames / 44.1 kHz, close enough to show) */
    char d[12];
    if (us < 10000u) {                                       /* 0.5 .. 9.9 ms */
        fmt_int(d, (int32_t)(us / 1000u));
        str_cpy(b, d, n);
        ui_cat(b, n, ".");
        fmt_int(d, (int32_t)(us / 100u % 10u));
        ui_cat(b, n, d);
        ui_cat(b, n, " ms");
    } else if (us < 1000000u) {
        fmt_int(d, (int32_t)(us / 1000u));
        str_cpy(b, d, n);
        ui_cat(b, n, " ms");
    } else {
        fmt_int(d, (int32_t)(us / 1000000u));
        str_cpy(b, d, n);
        ui_cat(b, n, ".");
        fmt_int(d, (int32_t)(us / 100000u % 10u));
        ui_cat(b, n, d);
        ui_cat(b, n, " s");
    }
}

/* the ENV page's picture: the gate's lamp. A round light in the theme's orange (no highlight), as the LED inside a vactrol, lit by the newest voice's
 * gate control: full when the gate is open, dimming as the envelope falls (with the vactrol's own lag), a dark
 * glass when nothing sounds; its glow spreads with the level. Beside it the cutoff the gate sets now. */
#define LAMP_X 120
#define LAMP_Y 58
#define LAMP_R 30                                /* the glass */
#define LAMP_G 56                                /* the glow's reach */
static uint16_t rgb565(int32_t r, int32_t g, int32_t b)
{
    r = clamp(r, 0, 255); g = clamp(g, 0, 255); b = clamp(b, 0, 255);
    return (uint16_t)(((uint32_t)r >> 3) << 11 | ((uint32_t)g >> 2) << 5 | (uint32_t)b >> 3);
}
static void us_gate_eye(int32_t ex, int32_t ey, int32_t lv);   /* (ui_style.c: PSYCHEDELIC's lamp) */
static void draw_env(void)
{
    int32_t lv = sys.env_stage != 0u ? sys.env_view : 0;       /* 0..255 */
    int32_t br = (T_BG >> 11 & 31) * 255 / 31, bg_ = (T_BG >> 5 & 63) * 255 / 63, bb = (T_BG & 31) * 255 / 31;
    int32_t x, y, rr = LAMP_R * LAMP_R, gg = LAMP_G * LAMP_G;
    /* the lit glass's colour at this level: the theme's orange, from a quarter (dark) to full */
    int32_t tr = (T_THEME >> 11 & 31) * 255 / 31, tg = (T_THEME >> 5 & 63) * 255 / 63, tb = (T_THEME & 31) * 255 / 31;
    int32_t cr = tr * (64 + 192 * lv / 255) / 256, cg = tg * (64 + 192 * lv / 255) / 256, cb = tb * (64 + 192 * lv / 255) / 256;
    cv_begin(240, PLOT_H, T_BG);
    if (sys.ui_style % US_COUNT == US_PSYCHE)    /* (PSYCHEDELIC: the lamp is the eye, opening with the gate) */
        us_gate_eye(LAMP_X, LAMP_Y, lv);
    else
    for (y = LAMP_Y - LAMP_G; y <= LAMP_Y + LAMP_G; y++)
        for (x = LAMP_X - LAMP_G; x <= LAMP_X + LAMP_G; x++) {
            int32_t dx = x - LAMP_X, dy = y - LAMP_Y, d2 = dx * dx + dy * dy, r, g, b;
            if (d2 > gg)
                continue;
            if (d2 <= rr) {
                /* the glass: brighter in the middle (1 - 0.4 (d / R)^2), and a soft rim */
                int32_t sh = 256 - 102 * d2 / rr;
                r = cr * sh / 256; g = cg * sh / 256; b = cb * sh / 256;
                if (d2 > rr - 2 * LAMP_R) {                      /* the edge: antialiased against the glow */
                    int32_t k = (rr - d2) * 256 / (2 * LAMP_R), a = lv * 7 / 10;   /* (the glow at the rim) */
                    int32_t or_ = br + (tr - br) * a / 256, og = bg_ + (tg - bg_) * a / 256, ob = bb + (tb - bb) * a / 256;
                    r = or_ + (r - or_) * k / 256; g = og + (g - og) * k / 256; b = ob + (b - ob) * k / 256;
                }
            } else {
                /* the glow: (1 - t)^2 of the level, t from the rim (0) to its reach (1) */
                int32_t t = (d2 - rr) * 256 / (gg - rr), f = (256 - t) * (256 - t) / 256, a = f * lv / 255;
                a = a * 7 / 10;                                  /* (a glow, not a second lamp) */
                r = br + (tr - br) * a / 256;
                g = bg_ + (tg - bg_) * a / 256;
                b = bb + (tb - bb) * a / 256;
            }
            cv_pset(x, y, rgb565(r, g, b));
        }
    if (sys.env_stage != 0u) {                               /* the cutoff the gate sets now */
        uint32_t i = (uint32_t)lv, hz = 25u, k;
        char b2[16], d[12];
        for (k = 0; k < i; k++)                              /* 25 Hz x 600^(i / 255): x 1.0254 a step */
            hz = hz + (hz * 26u + 512u) / 1024u;
        if (hz < 1000u) {
            fmt_int(d, (int32_t)hz);
            str_cpy(b2, d, sizeof b2);
            ui_cat(b2, sizeof b2, " Hz");
        } else {
            fmt_int(d, (int32_t)(hz / 1000u));
            str_cpy(b2, d, sizeof b2);
            ui_cat(b2, sizeof b2, ".");
            fmt_int(d, (int32_t)(hz / 100u % 10u));
            ui_cat(b2, sizeof b2, d);
            ui_cat(b2, sizeof b2, " kHz");
        }
        cv_text_r(232, LAMP_Y - AF_S.h / 2, &AF_S, b2, T_MID, T_BG);
    }
    cv_text(8, LAMP_Y - AF_S.h / 2, &AF_S, "GATE", T_DIM);
    cv_blit(0, PLOT_Y);
}

#include "fx_art.c"                              /* the FX page's pictures: each model drawn, moving with its sound */
#include "ui_style.c"                            /* the UI STYLE: palettes and backdrops (GLO's fourth page) */

/* the SHIMMER page's picture (fx_art.c art_shimmer): the room in 3D, the tail's wavefronts spreading and climbing;
 * the tail's level over the last ~4 s (one value a UI frame) is what the wavefronts carry */
static uint8_t sh_hist[200];
static uint32_t sh_seen;
/* MOD FX (FX once: the first FX page): a picture a mode, each style its own. The data: the circuit slowly (mf_view.m, ~0.37 s), the
 * circuit and the sound it makes in step (mf_view.mm, .x, the last ~12 ms)
 *   INITIAL (the oscilloscope)  VIBRATO the trace with its time bent and the graticule bent with it (a Doppler grid);
 *                RING MOD the scope in XY (the sound against the circuit: the product's figure); TREMOLO the trace
 *                inside the envelope the circuit cuts
 *   PSYCHEDELIC  VIBRATO rings swaying, each later than the one inside it; RING MOD two rings linked, turning, the XY
 *                figure in hues between them; TREMOLO polygons breathing with the circuit, one after the other */
static int32_t mfx_ms(uint32_t i) { return mf_view.m[(mf_view.w - 64u + i) & 63u]; }    /* (0 oldest .. 63 now) */
static int32_t mfx_mf(uint32_t i) { return mf_view.mm[(mf_view.mw - 256u + i) & 255u]; }
static int32_t mfx_x(uint32_t i) { return mf_view.x[(mf_view.xw - 256u + i) & 255u]; }
static int32_t mfx_g = 256, mfx_mg = 256, mfx_mm, mfx_xm;   /* (gains, Q8: the sound and the circuit shown full; the circuit's middle) */
static int32_t mfx_mfs(uint32_t i)
{
    int32_t v = (mfx_mf(i) - mfx_mm) * mfx_mg >> 8;
    return v > 32767 ? 32767 : v < -32767 ? -32767 : v;
}
static int32_t mfx_xs(uint32_t i)
{
    int32_t v = (mfx_x(i) - mfx_xm) * mfx_g >> 8;
    return v > 32767 ? 32767 : v < -32767 ? -32767 : v;
}
static int32_t mfx_lvl(void)                     /* the sound's level now, 0 .. 32767 */
{
    uint32_t i;
    int32_t pk = 0;
    for (i = 0; i < 256u; i += 4u) {
        int32_t a = mfx_x(i) < 0 ? -mfx_x(i) : mfx_x(i);
        pk = a > pk ? a : pk;
    }
    return pk;
}

static void mfx_scope(uint32_t mode, int on)
{
    uint32_t i, k;
    int32_t cy = PLOT_H / 2, px = 0, py = 0;
    uint16_t grid = ux_mix(T_BG, T_THEME, 22), sig = on ? T_THEME : ux_mix(T_BG, T_THEME, 40);
    uint16_t mod = on ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 35);
    for (k = 0; k <= 4u; k++)                    /* the graticule's rows */
        for (i = 8; i < 232u; i += 4u)
            cv_pset((int32_t)i, 12 + (int32_t)k * (PLOT_H - 24) / 4, grid);
    if (!on)
        mode = MF_TREMOLO;                       /* (off: a plain trace) */
    switch (mode) {
    case MF_VIBRATO:                             /* time bent: the graticule's columns and the trace with them */
        for (k = 0; k < 12u; k++) {
            for (i = 0; i <= 24u; i++) {
                int32_t y = 12 + (int32_t)i * (PLOT_H - 24) / 24;
                int32_t x = 10 + (int32_t)k * 20 + mfx_ms(39u + i) * 18 / 20000;
                if (i)
                    cv_line(px, py, x, y, grid);
                px = x;
                py = y;
            }
        }
        for (i = 0; i < 224u; i++) {
            int32_t j = (int32_t)i + 16 + mfx_ms(i * 63u / 223u) * 16 / 20000;
            int32_t x = 8 + (int32_t)i, y = cy - mfx_xs((uint32_t)(j < 0 ? 0 : j > 255 ? 255 : j)) * (PLOT_H / 3) / 32768;
            if (i)
                cv_line(px, py, x, y, sig);
            px = x;
            py = y;
        }
        for (i = 0; i < 64u; i++) {              /* (the circuit, small, along the top) */
            int32_t x = 8 + (int32_t)i * 224 / 63, y = 10 - mfx_ms(i) * 7 / 32768;
            if (i)
                cv_line(px, py, x, y, mod);
            px = x;
            py = y;
        }
        break;
    case MF_RING:                                /* XY: the sound across, the circuit up (SHIFT: as RING) */
    case MF_SHIFT:
        cv_line(120, 10, 120, PLOT_H - 10, grid);
        cv_line(60, cy, 180, cy, grid);
        for (i = 0; i < 256u; i++) {
            int32_t x = 120 + mfx_xs(i) * 56 / 32768, y = cy - mfx_mfs(i) * (PLOT_H / 2 - 10) / 32768;
            if (i)
                cv_aline(px * 16, py * 16, x * 16, y * 16, sig);
            px = x;
            py = y;
        }
        for (k = 0; k < 16u; k++) {              /* (the multiplier's ring, in the corner) */
            int32_t a0 = us_sin(k * 0x10000000u + (1u << 30), 9), b0 = us_sin(k * 0x10000000u, 9);
            int32_t a1 = us_sin((k + 1u) * 0x10000000u + (1u << 30), 9), b1 = us_sin((k + 1u) * 0x10000000u, 9);
            cv_line(220 + a0, 22 + b0, 220 + a1, 22 + b1, mod);
        }
        cv_line(214, 16, 226, 28, mod);
        cv_line(226, 16, 214, 28, mod);
        break;
    default: {                                   /* TREMOLO: the envelope the circuit cuts, the trace inside */
        int32_t qx = 0, qy = 0;
        for (i = 0; i < 64u; i++) {
            int32_t x = 8 + (int32_t)i * 224 / 63, e = on ? 16384 + mfx_ms(i) / 2 : 32767;
            int32_t y = cy - e * (PLOT_H / 2 - 12) / 32768, y2 = cy + e * (PLOT_H / 2 - 12) / 32768;
            if (i) {
                cv_line(px, py, x, y, mod);
                cv_line(qx, qy, x, y2, mod);
            }
            px = x; py = y; qx = x; qy = y2;
        }
        for (i = 0; i < 224u; i++) {
            int32_t x = 8 + (int32_t)i, y = cy - mfx_xs(i + 16u) * (PLOT_H / 2 - 12) / 32768;
            if (i)
                cv_line(px, py, x, y, sig);
            px = x;
            py = y;
        }
        break;
    }
    }
}

static void mfx_psy(uint32_t mode, int on)
{
    uint32_t i, k, t = us_t();
    int32_t cy = PLOT_H / 2, gf = us_gf(), ev = us_env(0), lvl = mfx_lvl(), px = 0, py = 0;
    uint32_t hue = t * 3u + mode * 51u + (us_groove() ? us_bn * 24u : 0u);
    int32_t lit = on ? 85 : 35;
    uint16_t glow = ux_mix(T_BG, us_hue(hue + 40u, 255u), (on ? 40 : 18) + gf / 6);
    us_poly(120, cy, 50 + ev / 40, 32u, t << 21, 200, glow);   /* (the halo, as the eye's lids) */
    switch (on ? mode : MF_OFF) {
    case MF_VIBRATO:                             /* rings swaying, each later than the one inside it */
        for (k = 0; k < 8u; k++) {
            int32_t cx = 120 + mfx_ms(63u - k * 7u) * 34 / 32768, r = 6 + (int32_t)k * 6 + lvl / 3000;
            uint16_t c = ux_mix(T_BG, us_hue(hue + k * 22u, 255u), lit - (int32_t)k * 5);
            us_poly(cx, cy, r, 32u, t << 20, 230, c);
            us_poly(cx + 1, cy, r, 32u, t << 20, 230, ux_mix(T_BG, c, 50));
        }
        break;
    case MF_RING:                                /* two rings linked, turning; the product's figure between (SHIFT too) */
    case MF_SHIFT: {
        uint32_t a = t << 22;
        int32_t d = 16 + (mfx_mf(255) < 0 ? -mfx_mf(255) : mfx_mf(255)) * 10 / 32768;
        int32_t dx = us_sin(a + (1u << 30), d), dy = us_sin(a, d) / 2;
        for (k = 0; k < 2u; k++) {
            int32_t sg = k ? -1 : 1;
            uint16_t c = ux_mix(T_BG, us_hue(hue + k * 128u, 255u), lit);
            us_poly(120 + sg * dx, cy + sg * dy, 30 + ev / 60, 40u, a * (k ? 1u : 3u), 256, c);
            us_poly(121 + sg * dx, cy + sg * dy, 30 + ev / 60, 40u, a * (k ? 1u : 3u), 256, ux_mix(T_BG, c, 50));
        }
        for (i = 0; i < 256u; i += 2u) {
            int32_t x = 120 + mfx_xs(i) * 40 / 32768, y = cy - mfx_mfs(i) * 40 / 32768;
            if (i)
                cv_aline(px * 16, py * 16, x * 16, y * 16, ux_mix(T_BG, us_hue(hue + i / 2u, 255u), lit - 20));
            px = x;
            py = y;
        }
        break;
    }
    case MF_TREMOLO:                             /* polygons breathing with the circuit, one after the other */
        for (k = 0; k < 6u; k++) {
            int32_t e = 16384 + mfx_ms(63u - k * 6u) / 2, r = (10 + (int32_t)k * 9) * (12288 + e) / 32768;
            uint16_t c = ux_mix(T_BG, us_hue(hue + k * 30u, 255u), lit - (int32_t)k * 4);
            us_poly(120, cy, r, 6u, (t << 20) + k * 0x0AAAAAAAu, 256, c);
            us_poly(120, cy, r + 1, 6u, (t << 20) + k * 0x0AAAAAAAu, 256, ux_mix(T_BG, c, 45));
        }
        break;
    default:                                     /* (off: the halo alone, a still ring) */
        us_poly(120, cy, 26, 32u, 0, 256, ux_mix(T_BG, T_TEXT, 30));
        break;
    }
}

static void draw_mfx(void)
{
    uint32_t mode = mf.on ? mf.mode : sys.mfx_mode % MF_COUNT;   /* (the mod wheel's VIBRATO, RING: shown too) */
    int on = mf.on != 0, psy = sys.ui_style % US_COUNT == US_PSYCHE;
    {
        int32_t l = mfx_lvl();
        uint32_t i;
        int32_t lo = 32767, hi = -32768;
        int32_t xl = 32767, xh = -32768;
        (void)l;
        for (i = 0; i < 256u; i += 2u) {
            int32_t v = mfx_x(i);
            xl = v < xl ? v : xl;
            xh = v > xh ? v : xh;
        }
        mfx_xm = (xl + xh) / 2;
        mfx_g = 56000 * 256 / (xh - xl > 3500 ? xh - xl : 3500);
        for (i = 0; i < 256u; i += 2u) {
            int32_t v = mfx_mf(i);
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        mfx_mm = (lo + hi) / 2;
        mfx_mg = 56000 * 256 / (hi - lo > 3500 ? hi - lo : 3500);
    }
    cv_begin(240, PLOT_H, T_BG);                 /* (PSYCHEDELIC: the oscillator's mash behind, as on the FX pages) */
    if (psy)
        mfx_psy(mode, on);
    else
        mfx_scope(mode, on);
    if (psy)
        cv_text(8, 4, &AF_S, MF_NAME[mode], ux_mix(T_BG, us_hue(us_t() * 3u + mode * 51u, 255u), 80));
    if (!on)
        cv_text(8, psy ? 4 + AF_S.h : 2, &AF_S, mode == MF_OFF ? "ALGORITHM: A MODE" : "MIX 0: KNOB 4", T_MID);
    cv_blit(0, PLOT_Y);
}

static void draw_sh(void)
{
    uint32_t i, w = sh_view.w, pk = 0;
    if (w - sh_seen > SH_ENV)
        sh_seen = w - SH_ENV;
    for (; sh_seen != w; sh_seen++) {
        uint32_t e = sh_view.env[sh_seen & (SH_ENV - 1u)];
        pk = e > pk ? e : pk;
    }
    for (i = 0; i + 1u < sizeof sh_hist; i++)
        sh_hist[i] = sh_hist[i + 1u];
    {   /* the level in decibels, roughly: 6 dB a bit of the peak, 2 bits of fraction; 0 .. 255 over ~70 dB */
        uint32_t b = 0, t = pk, lv;
        while (t) { b++; t >>= 1; }
        lv = b > 4u ? (b - 4u) * 20u + (b >= 3u ? ((pk >> (b - 3u)) & 3u) * 5u : 0u) : 0u;
        sh_hist[sizeof sh_hist - 1u] = (uint8_t)(sys.sh_mix ? (lv > 255u ? 255u : lv) : 0u);
    }
    if (!sys.sh_mix)
        memset(sh_hist, 0, sizeof sh_hist);      /* (off: the history starts again when it is turned on) */
    art_t++;
    art_c = sys.sh_mix ? T_THEME : ux_mix(T_BG, T_THEME, 45);
    art_c2 = ux_mix(T_BG, art_c, 75);
    art_c3 = ux_mix(T_BG, art_c, 45);
    if (sys.ui_style % US_COUNT == US_PSYCHE) {  /* (PSYCHEDELIC: the space as a simple line drawing) */
        us_fx_pic(1);
        return;
    }
    cv_begin(240, PLOT_H, T_BG);
    art_shimmer(sh_hist, sizeof sh_hist);
    if (!sys.sh_mix)
        cv_text(160, 4, &AF_S, "MIX OFF", T_MID);
    cv_blit(0, PLOT_Y);
}

/* the FREE EG page's picture: the recording (128 points over the length), the play head, the DEPTH's base */
static void draw_feg(void)
{
    /* the time across it as the length is: a cycle's (STEPS, BARS, PHRASE, LFO) its whole width, NOTE as long as the
     * contour rounded up to sixteenths at the tempo; lines at the sixteenths (or the beats, the bars, as many as fit),
     * the beats' lighter, the bars' brighter */
    const int32_t top = 8, bot = PLOT_H - 16, hh = bot - top;
    int32_t i, px = 0, py = 0;
    uint32_t len = sys.feg_len % FEG_NLEN, pos = feg.pos, n = len ? FEG_N : (feg.rec ? FEG_N : sys.feg_npts);
    uint32_t bpm = arp_bpm_now(), s16, L = 0, W, pt, g;
    uint16_t c = feg.rec ? T_ACCENT : sys.feg_on ? (FEG_FM(sys.feg_target) ? T_FM : T_THEME) : T_DIM;
    bpm = bpm < 20u ? 20u : bpm > 300u ? 300u : bpm;
    s16 = 2646000u / bpm / 4u;                   /* (samples a sixteenth) */
    if (!len) {                                  /* NOTE: its points, the window whole sixteenths */
        uint32_t t = (n ? n : 1u) * FEG_NOTE_STEP;
        W = (t + s16 - 1u) / s16 * s16;
        pt = FEG_NOTE_STEP;
    } else {
        if (len >= FEG_AUD0)
            L = 0;                               /* (AM, FM: at the note's rate, no grid) */
        else if (len >= FEG_LFO0)
            L = FS * 100u / FEG_LFO_CHZ[len - FEG_LFO0];
        else if (len == FEG_PHRASE)
            L = kar.running && kar.phrase_len ? kar.phrase_len : s16 * 16u;
        else
            L = feg_ticks(len) * s16 / 6u;       /* (6 clocks a sixteenth) */
        W = L ? L : FEG_N;
        pt = L ? 0u : 1u;
    }
    cv_begin(240, PLOT_H, T_BG);
    if (L || !len) {                             /* the grid: sixteenths, beats or bars, at most 32 lines */
        uint32_t k, nl;
        g = s16;
        while (W / g > 32u && g < s16 * 64u)
            g *= 4u;
        nl = W / g;
        for (k = 1; k <= nl && g; k++) {
            uint32_t t = k * g, b16 = t / s16;
            int32_t x = 8 + (int32_t)(t * 224u / W);   /* (32 bits: t <= W < 2^23) */
            if (x >= 232)
                break;
            if (b16 % 4u)                        /* a sixteenth: a tick at the bottom; a beat a line; a bar brighter */
                cv_rect(x, bot - 6, 1, 6, T_LINE);
            else
                cv_rect(x, top, 1, hh, b16 % 16u ? T_LINE : T_MID);
        }
    }
    cv_rect(8, bot, 224, 1, T_LINE);
    cv_rect(8, top, 224, 1, T_LINE);
#define FEG_X(i) (8 + (int32_t)((len ? (uint32_t)(i) * (L ? L : FEG_N) / FEG_N : (uint32_t)(i) * pt) * 224u / W))
    if (sys.feg_has || feg.rec)
        for (i = 0; i < (int32_t)n && i < (int32_t)FEG_N; i++) {
            int32_t x = FEG_X(i), y = bot - (int32_t)feg.curve[i] * hh / 127;
            if (i && (!feg.rec || i <= (int32_t)pos))
                cv_line(px, py, x, y, c);
            px = x;
            py = y;
        }
    if (!len && n && (sys.feg_has || feg.rec) && px < 232)   /* (NOTE: the last point holds, to the end) */
        cv_line(px, py, 231, py, c);
    if (pos < FEG_N)
        cv_rect(FEG_X(pos), top, 1, hh, T_ACCENT);
#undef FEG_X
    cv_text(8, bot + 3, &AF_S, FEG_T_NAME[sys.feg_target % FEG_NT], FEG_FM(sys.feg_target) ? T_FM : T_DIM);
    cv_text_r(232, bot + 3, &AF_S, FEG_LEN_NAME[sys.feg_len % FEG_NLEN], T_DIM, T_BG);
    cv_blit(0, PLOT_Y);
}

/* the KARMA page's picture: the GE's rhythm (each step: a hit's weight its height, a long one wide, ties joined, the
 * step playing lit), under it the last notes made as a roll, the newest bright */
static void draw_karma(void)
{
    const ge_t *G = k_ge();
    const char *r = G->rhy;
    const int32_t rt = 6, rh = 26, top = 42, bot = PLOT_H - 14;
    uint32_t len = str_len(r), i, wv = kar_view.w, lo = 127, hi = 0, cur = kar.view_step;
    int32_t w = 224 / (int32_t)(len ? len : 1u);
    char a[40], v[12];
    cv_begin(240, PLOT_H, T_BG);
    for (i = 0; i < len; i++) {
        char h = r[i];
        int32_t x = 8 + (int32_t)i * w, hh = h == '1' || h == 'L' ? rh : h == '2' ? rh * 2 / 3 : h == '3' ? rh / 3 : 0;
        uint16_t c = i == cur && sys.k_on ? T_ACCENT : sys.k_on ? T_THEME : T_DIM;
        if (h == '-')                            /* a tie: joined to the step before */
            cv_rect(x - 1, rt + rh - 3, w, 3, ux_mix(T_BG, c, 60));
        else if (hh)
            cv_rect(x, rt + rh - hh, h == 'L' ? w * 2 - 1 : w - 1, hh, c);
        else
            cv_rect(x + w / 2 - 1, rt + rh - 2, 2, 2, T_LINE);
    }
    for (i = 0; i < 32u; i++) {
        uint32_t nt = kar_view.note[(wv - 32u + i) & 31u];
        if (nt && wv >= 32u - i) {
            lo = nt < lo ? nt : lo;
            hi = nt > hi ? nt : hi;
        }
    }
    if (hi < lo + 12u)
        hi = lo + 12u;
    cv_rect(8, bot, 224, 1, T_LINE);
    for (i = 0; i < 32u; i++) {
        uint32_t nt = kar_view.note[(wv - 32u + i) & 31u];
        if (nt && wv >= 32u - i) {
            int32_t y = bot - 4 - (int32_t)(nt - lo) * (bot - top - 8) / (int32_t)(hi - lo);
            cv_rect(8 + (int32_t)i * 7, y, 6, 4, i == 31u && sys.k_on ? T_ACCENT :
                    sys.k_on ? ux_mix(T_BG, T_THEME, 40 + (int32_t)i * 2) : T_DIM);
        }
    }
    str_cpy(a, !sys.k_on ? "OFF  " : arp_ext() ? "EXT " : "", sizeof a);
    fmt_int(v, (int32_t)(arp_ext() ? arp_bpm_now() : sys.arp_bpm));
    ui_cat(a, sizeof a, v);
    ui_cat(a, sizeof a, " BPM   ");
    fmt_int(v, (int32_t)len);
    ui_cat(a, sizeof a, v);
    ui_cat(a, sizeof a, G->res == 3u ? " x 1/32" : G->res == 6u ? " x 1/16" : G->res == 8u ? " x 1/8T" :
                        G->res == 12u ? " x 1/8" : " x 1/4");
    cv_text(8, bot + 1, &AF_S, a, T_DIM);
    cv_blit(0, PLOT_Y);
}

/* the PAN page's picture: the stereo field from left to right, BASIS's seats (the next one tall), the
 * voices sounding where they are, each with its note */
static void draw_pan(void)
{
    const int32_t x0 = 16, w = 208, yc = 58;
    int32_t basis = (int32_t)(sys.basis & 127u), k;
    char nm[8], v[8];
    cv_begin(240, PLOT_H, T_BG);
    cv_rect(x0, yc, w, 1, T_LINE);
    cv_rect(x0 + w / 2, yc - 30, 1, 61, T_LINE);
    cv_text(x0 - 8, yc + 34, &AF_S, "L", T_DIM);
    cv_text_r(x0 + w + 8, yc + 34, &AF_S, "R", T_DIM, T_BG);
    for (k = 0; k < (int32_t)NSEAT; k++) {       /* BASIS: the seats (the next note's: tall) */
        int32_t x = x0 + w / 2 + (int32_t)SEAT_PAN[k] * basis / 127 * (w / 2) / 32767;
        int nx = (uint32_t)k == seat_next;
        cv_rect(x, yc - (nx ? 12 : 6), nx ? 2 : 1, nx ? 25 : 13, nx ? T_MID : ux_mix(T_BG, T_THEME, 50));
    }
    for (k = 0; k < (int32_t)NVOICE; k++)        /* the voices sounding, where they are */
        if (voice[k].stage != ST_OFF && voice[k].stage != ST_TAIL) {
            int32_t x = x0 + w / 2 + vpan[k] * (w / 2) / 32767, y = yc - 26 + k * 17;
            cv_rect(x - 4, y - 4 + 13, 9, 9, T_ACCENT);
            str_cpy(nm, NOTE_NAME[voice[k].note % 12u], sizeof nm);
            fmt_int(v, (int32_t)voice[k].note / 12 - 1);
            ui_cat(nm, sizeof nm, v);
            cv_text(x + 8 > 200 ? x - 40 : x + 8, y + 8, &AF_S, nm, T_MID);
        }
    cv_blit(0, PLOT_Y);
}

/* the EDIT page's picture: the mod wheel now and the parameter it moves, the last velocity and what it does */
static const char *const VEL_TO_NAME[4] = {"OFF", "BRIGHT", "SWEEP", "BRIGHT + SWEEP"};
static void draw_edit(void)
{
    const int32_t x0 = 8, w = 224, h = 14;
    int32_t mw = (int32_t)(sys.mw & 127u), vel = sys.last_vel;
    char a[40];
    cv_begin(240, PLOT_H, T_BG);
    cv_text(x0, 6, &AF_S, "MOD WHEEL (CC 1)", T_DIM);
    cv_frame(x0, 24, w, h, T_LINE);
    if (mw)
        cv_rect(x0 + 1, 25, (w - 2) * mw / 127, h - 2, FEG_FM(sys.mw_target) ? T_FM : T_THEME);
    if (sys.mw_seen && feg.playing && feg.target == sys.mw_target % FEG_NT) {
        str_cpy(a, "FREE EG DEPTH ON ", sizeof a);
        ui_cat(a, sizeof a, FEG_T_NAME[sys.mw_target % FEG_NT]);
    } else {
        str_cpy(a, "MOVES ", sizeof a);
        ui_cat(a, sizeof a, FEG_T_NAME[sys.mw_target % FEG_NT]);
    }
    cv_text(x0, 42, &AF_S, a, FEG_FM(sys.mw_target) ? T_FM : T_MID);
    cv_text(x0, 66, &AF_S, "LAST VELOCITY", T_DIM);
    cv_frame(x0, 84, w, h, T_LINE);
    if (vel && sys.last_note < 128u)
        cv_rect(x0 + 1, 85, (w - 2) * vel / 127, h - 2, T_THEME);
    str_cpy(a, "ON THE SOUND: ", sizeof a);
    ui_cat(a, sizeof a, VEL_TO_NAME[sys.vel_to & 3u]);
    cv_text(x0, 102, &AF_S, a, T_MID);
    cv_blit(0, PLOT_Y);
}

/* the MIDI page's picture: where notes come in and go out, and the clock */
/* the BANK menu (HOME on HOME): the four banks, each with the program playing as it is there, the bank playing lit */
static const char *const BANK_NAME[NBANKS] = {"GM", "GM ALT", "SYNTH", "LAB"};
static void draw_bank(void)
{
    uint32_t k, cur = sys.preset >> 7, pg = sys.preset & 127u;
    char a[40], v[8];
    cv_begin(240, PLOT_H, T_BG);
    for (k = 0; k < NBANKS; k++) {
        int32_t y = 6 + (int32_t)k * 29;
        uint16_t c = k == cur ? T_ACCENT : T_MID;
        if (k == cur)
            cv_frame(4, y - 3, 232, 27, T_THEME);
        fmt_int(v, (int32_t)k + 1);
        str_cpy(a, v, sizeof a);
        ui_cat(a, sizeof a, " ");
        ui_cat(a, sizeof a, BANK_NAME[k]);
        cv_text(12, y, &AF_M, a, c);
        cv_text(100, y + 3, &AF_S, PRESETS[(k * 128u + pg) % NPRESETS].name, k == cur ? T_TEXT : T_DIM);
    }
    cv_blit(0, PLOT_Y);
}

/* the MULTI page: the two parts, their channels and their sounds */
static void draw_multi(void)
{
    char a[40], v[8];
    uint32_t p;
    cv_begin(240, PLOT_H, T_BG);
    cv_text(8, 6, &AF_M, sys.multi ? "MULTI ON" : "MULTI OFF", sys.multi ? T_ACCENT : T_DIM);
    for (p = 0; p < 2u; p++) {
        int32_t y = 40 + (int32_t)p * 40;
        uint16_t c = sys.multi || !p ? T_TEXT : T_DIM;
        str_cpy(a, p ? "B  CH " : "A  CH ", sizeof a);
        if (p ? sys.ch_b : sys.midi_rx) {
            fmt_int(v, p ? sys.ch_b : sys.midi_rx);
            ui_cat(a, sizeof a, v);
        } else
            ui_cat(a, sizeof a, "OMNI");
        cv_text(8, y, &AF_S, a, T_MID);
        cv_text(96, y, &AF_S, PRESETS[part_preset(p) % NPRESETS].name, c);
    }
    cv_text(8, 124, &AF_S, sys.multi ? "2 VOICES EACH, ONE SHIMMER" : "ALL 4 VOICES: A", T_DIM);
    cv_blit(0, PLOT_Y);
}

/* GROOVE: the ARP VELOCITY SEQ and the ARP GATE SEQ, each as eight steps (the pattern repeated), their levels as
 * heights (a velocity 0: empty), past their lengths dark; the step playing lit */
static void groove_row(int32_t y, const char *title, const arp_groove_t *P, uint32_t len, uint32_t pos, int on)
{
    const char *vs = P->s;
    uint32_t nv = str_len(vs), i, cur = pos ? (pos - 1u) % len : 0u;
    cv_text(8, y, &AF_S, title, T_DIM);
    cv_text(140, y, &AF_S, P->name, nv ? T_TEXT : T_DIM);
    for (i = 0; i < 8u; i++) {
        int32_t lv = nv ? vs[i % nv] - '0' : 0, hgt = 2 + lv * 4, x = 8 + (int32_t)i * 28;
        uint16_t col = on && i == cur ? T_ACCENT : i < len ? T_THEME : ux_mix(T_BG, T_THEME, 35);
        cv_frame(x, y + 14, 24, 42, ux_mix(T_BG, T_DIM, 30));
        if (lv)
            cv_rect(x + 2, y + 54 - hgt, 20, hgt, col);
        else if (on && nv && i == cur)
            cv_frame(x + 1, y + 15, 22, 40, T_ACCENT);
    }
}
static void draw_groove(void)
{
    int on = sys.arp_on && arp.n;
    cv_begin(240, PLOT_H, T_BG);
    groove_row(2, "ARP VELOCITY SEQ", &ARP_VPAT[sys.arp_vp % ARP_NVP], arp_vlen(), arp.gpos, on);
    groove_row(64, "ARP GATE SEQ", &ARP_GPAT[sys.arp_gp % ARP_NGP], arp_glen(), arp.ggpos, on);
    cv_blit(0, PLOT_Y);
}
/* LOGIC: the mode, the last steps where either hit: the ARP's row, KARMA's, and what came through */
static void draw_logic(void)
{
    static const char *const LG[7] = {"OFF", "OR", "AND", "XOR", "NOR", "ALT", "S/R"};
    static const char *const LD[7] = {"ONE OR THE OTHER", "BOTH PLAY", "ONLY WHERE BOTH HIT", "ONLY WHERE ONE HITS",
                                      "THE GAPS OF BOTH", "IN TURN", "ARP SETS, KARMA RESETS"};
    uint32_t w = logic_view.w, i;
    int on = logic_on();
    cv_begin(240, PLOT_H, T_BG);
    cv_text(8, 4, &AF_M, "ARP", on ? T_THEME : T_DIM);
    cv_text(64, 4, &AF_M, LG[sys.k_logic % 7u], on ? T_ACCENT : T_DIM);
    if (sys.k_inv)
        cv_text(64, 22, &AF_S, "INVERSE", on ? T_ACCENT : T_DIM);
    cv_text(140, 4, &AF_M, "KARMA", on ? T_THEME : T_DIM);
    cv_text(8, 30, &AF_S, LD[sys.k_logic % 7u], T_DIM);
    {
        char q[24];
        str_cpy(q, logic_q_name(sys.k_lq), sizeof q);
        if (sys.k_lq >= LQ_AUTO) {
            ui_cat(q, sizeof q, " ");
            ui_cat(q, sizeof q, sys.k_root ? NOTE_NAME[(sys.k_root - 1u) % 12u] : "");
        }
        cv_text(150, 30, &AF_S, q, sys.k_lq ? T_TEXT : T_DIM);
    }
    cv_text(8, 52, &AF_S, "A", T_DIM);
    cv_text(8, 72, &AF_S, "K", T_DIM);
    cv_text(8, 96, &AF_S, "OUT", T_DIM);
    for (i = 0; i < 24u; i++) {                  /* (the oldest left) */
        uint8_t b = logic_view.v[(w - 24u + i) & 31u];
        int32_t x = 40 + (int32_t)i * 8;
        if (w < 24u - i)
            continue;
        cv_rect(x, 54, 6, 10, b & 1u ? (b & 4u ? T_THEME : ux_mix(T_BG, T_THEME, 30)) : ux_mix(T_BG, T_DIM, 25));
        cv_rect(x, 74, 6, 10, b & 2u ? (b & 8u ? T_THEME : ux_mix(T_BG, T_THEME, 30)) : ux_mix(T_BG, T_DIM, 25));
        cv_rect(x, 98, 6, 10, b & 12u ? T_ACCENT : ux_mix(T_BG, T_DIM, 25));
    }
    if (!on)
        cv_text(8, 112, &AF_S, sys.k_logic ? "PLAY: ARP + KARMA" : "KNOB 1: A LOGIC", T_DIM);
    cv_blit(0, PLOT_Y);
}
static void draw_sound(void)
{
    char a[40], v[8];
    cv_begin(240, PLOT_H, T_BG);
    cv_text(8, 6, &AF_M, "SOUND", T_ACCENT);
    str_cpy(a, "SPEAKER BASS ", sizeof a);
    fmt_int(v, (int32_t)(sys.spk_bass & 127u));
    ui_cat(a, sizeof a, sys.spk_bass & 127u ? v : "OFF");
    cv_text(8, 38, &AF_S, a, sys.spk_bass & 127u ? T_TEXT : T_DIM);
    cv_text(8, 56, &AF_S, "THE BASS'S OVERTONES: HEARD ON", T_DIM);
    cv_text(8, 72, &AF_S, "THE SMALL SPEAKER", T_DIM);
    cv_text(8, 96, &AF_S, sys.lvl_match ? "LEVEL MATCH ON: PRESETS ALIKE" : "LEVEL MATCH OFF", sys.lvl_match ? T_TEXT : T_DIM);
    str_cpy(a, "PITCH BEND +- ", sizeof a);
    fmt_int(v, (int32_t)sys.bend_range);
    ui_cat(a, sizeof a, v);
    ui_cat(a, sizeof a, " SEMITONES");
    cv_text(8, 114, &AF_S, a, sys.bend_range ? T_TEXT : T_DIM);
    cv_blit(0, PLOT_Y);
}
/* UI STYLE: the style's name over its own backdrop, what it is, the knobs */
static void draw_style(void)
{
    uint32_t st = sys.ui_style % US_COUNT;
    cv_begin(240, PLOT_H, T_BG);
    cv_text(8, 6, &AF_S, "UI STYLE", T_MID);
    cv_text(9, 27, &AF_M, US_NAME[st], T_BG);   /* (a shadow: it stays readable over any backdrop) */
    cv_text(10, 28, &AF_M, US_NAME[st], T_BG);
    cv_text(8, 26, &AF_M, US_NAME[st], T_THEME);
    cv_text(9, 59, &AF_S, US_WHAT[st], T_BG);
    cv_text(8, 58, &AF_S, US_WHAT[st], T_TEXT);
    cv_text(9, 99, &AF_S, sys.tune432 ? "TUNING A = 432 HZ (ALGORITHM)" : "TUNING A = 440 HZ (ALGORITHM)", T_BG);
    cv_text(8, 98, &AF_S, sys.tune432 ? "TUNING A = 432 HZ (ALGORITHM)" : "TUNING A = 440 HZ (ALGORITHM)", T_TEXT);
    cv_text(9, 113, &AF_S, sys.analog ? "SOUND ANALOG (SELECT)" : "SOUND INITIAL (SELECT)", T_BG);
    cv_text(8, 112, &AF_S, sys.analog ? "SOUND ANALOG (SELECT)" : "SOUND INITIAL (SELECT)", sys.analog ? T_THEME : T_TEXT);
    cv_blit(0, PLOT_Y);
}
static void draw_midi(void)
{
    char a[40], v[12];
    int32_t y = 6;
    cv_begin(240, PLOT_H, T_BG);
    cv_text(8, y, &AF_S, "IN", T_DIM);
    cv_text(48, y, &AF_S, "TRS JACK", T_TEXT);
    cv_text(140, y, &AF_S, sys.midi_usb & 1u ? "USB" : "USB (OFF)", sys.midi_usb & 1u ? T_TEXT : T_DIM);
    y += 19;
    str_cpy(a, "A ", sizeof a);
    if (sys.midi_rx) {
        fmt_int(v, sys.midi_rx);
        ui_cat(a, sizeof a, "CH ");
        ui_cat(a, sizeof a, v);
    } else
        ui_cat(a, sizeof a, "OMNI");
    ui_cat(a, sizeof a, sys.multi ? "   B " : "   B (OFF) ");
    if (sys.ch_b) {
        fmt_int(v, sys.ch_b);
        ui_cat(a, sizeof a, "CH ");
        ui_cat(a, sizeof a, v);
    } else
        ui_cat(a, sizeof a, "OMNI");
    cv_text(48, y, &AF_S, a, T_MID);
    y += 24;
    cv_text(8, y, &AF_S, "OUT", T_DIM);
    cv_text(48, y, &AF_S, sys.midi_usb & 2u ? "USB: THE KEYS" : "USB (OFF)", sys.midi_usb & 2u ? T_TEXT : T_DIM);
    y += 19;
    str_cpy(a, "CHANNEL ", sizeof a);
    fmt_int(v, (int32_t)(sys.midi_tx & 15u) + 1);
    ui_cat(a, sizeof a, v);
    cv_text(48, y, &AF_S, a, T_MID);
    y += 24;
    cv_text(8, y, &AF_S, sys.clk_int ? "CLOCK  INTERNAL (SELECT)" : arp_ext_alive() ? "CLOCK  EXTERNAL: MIDI IN" : "CLOCK  EXTERNAL: WAITING", T_DIM);
    cv_text(8, y + 17, &AF_S, sys.autoplay ? "AUTOPLAY  ON (PRESETS): START PLAYS" : "AUTOPLAY  OFF (PRESETS)", T_DIM);
    cv_blit(0, PLOT_Y);
}

/* the ARP page's picture: the last 32 steps as a roll (their notes' heights, rests empty), the newest bright; ON / OFF
 * and the tempo's beat flashing */
static void draw_arp(void)
{
    const int32_t top = 8, bot = PLOT_H - 14, hh = bot - top, w = 7;
    uint32_t i, wv = arp_view.w, lo = 127, hi = 0;
    cv_begin(240, PLOT_H, T_BG);
    for (i = 0; i < 32u; i++) {
        uint32_t nt = arp_view.note[(wv - 32u + i) & 31u];
        if (nt) {
            lo = nt < lo ? nt : lo;
            hi = nt > hi ? nt : hi;
        }
    }
    if (hi < lo + 12u)                           /* (at least an octave tall) */
        hi = lo + 12u;
    cv_rect(8, bot, 224, 1, T_LINE);
    for (i = 0; i < 32u; i++) {
        uint32_t nt = arp_view.note[(wv - 32u + i) & 31u];
        if (nt && wv >= 32u - i) {
            int32_t y = bot - 4 - (int32_t)(nt - lo) * (hh - 8) / (int32_t)(hi - lo);
            cv_rect(8 + (int32_t)i * w, y, w - 1, 4, i == 31u && sys.arp_on ? T_ACCENT :
                    sys.arp_on ? ux_mix(T_BG, T_THEME, 40 + (int32_t)i * 2) : T_DIM);
        }
    }
    cv_text(8, bot + 1, &AF_S, sys.arp_on ? (arp_ext() ? "MIDI CLOCK" : "INTERNAL") : "OFF: PRESS PLAY", T_DIM);
    cv_blit(0, PLOT_Y);
}

/* the MOD ENV page's picture: the pitch sweep over the envelope's LENGTH (the time axis: 0.5 ms .. 10 s, log), the
 * note the floor, HEIGHT the peak (3 octaves the top) at SKEW, BEND its curve; on the right the newest voice's now */
static void draw_menv(void)
{
    const int32_t top = 10, bot = PLOT_H - 16, hh = bot - top, x0 = 8;
    int32_t xl = x0 + 20 + (int32_t)(sys.menv_len & 127u) * 170 / 127;           /* (the length's end) */
    int32_t xp = x0 + (xl - x0) * (int32_t)(sys.menv_skew & 127u) / 127;          /* the peak */
    int32_t hm = (int32_t)(sys.menv_height & 127u), dn = (sys.menv_height & 0x80u) != 0;
    int32_t dh = hm * hm * hm / 127;             /* (cubic, as noise.c: / 127^3 of the half) */
    int32_t mid = top + hh / 2, yf = mid, yp = dn ? mid + dh * (hh / 2) / 16129 : mid - dh * (hh / 2) / 16129;
    int32_t lv = sys.menv_target ? (int32_t)menv_p.moved * (hh / 2) / 127   /* (on a parameter: how far it is pushed) */
                                 : (int32_t)sys.menv_view * (hh / 2) / 32767 * (dn ? -1 : 1);
    uint32_t on = sys.osc && hm;
    uint16_t c = on ? T_FM : T_DIM;
    cv_begin(240, PLOT_H, T_BG);
    cv_rect(x0, mid, 196, 1, T_LINE);            /* (the note: up above it, down below)*/
    {   /* the rise and the fall, BEND curving them as noise.c does */
        int32_t i, n = xl - x0, px = x0, py = yf;
        for (i = 1; i <= n; i++) {
            int32_t x = x0 + i, lvq = x <= xp ? (xp > x0 ? (x - x0) * 32768 / (xp - x0) : 32768)
                                               : (xl > xp ? (xl - x) * 32768 / (xl - xp) : 0);
            int32_t y = yf + (yp - yf) * np_bend(lvq, sys.menv_bend) / 32768;
            cv_line_t(px, py, x, y, c, 2);
            px = x;
            py = y;
        }
    }
    cv_line_t(xl, yf, x0 + 196, yf, c, 2);
    cv_frame(212, top, 20, hh + 1, T_LINE);      /* the depth now */
    cv_rect(212, mid, 20, 1, T_LINE);
    if (lv > 0)
        cv_rect(214, mid - lv, 16, lv, on ? T_FM : T_DIM);
    else if (lv < 0)
        cv_rect(214, mid + 1, 16, -lv, on ? T_FM : T_DIM);
    cv_text(x0, bot + 3, &AF_S, "NOTE", T_DIM);
    cv_text_r(xl < 60 ? 60 : xl, bot + 3, &AF_S, "END", T_DIM, T_BG);
    cv_text(sys.menv_target ? 206 : 200, bot + 3, &AF_S, sys.menv_target ? "PUSH" : "PITCH", T_DIM);
    cv_blit(0, PLOT_Y);
}

/* the FX page's SELECT (POSITION) per model: its name, its value */
static void fx_pos_text(char *d, uint32_t n, int brief)
{
    uint32_t md = sys.fx_model % RZ_COUNT, p = sys.fx_pos & 127u;
    char v[12];
    static const char *const L[RZ_COUNT] = {"POSITION", "AMOUNT", "POSITION", "VOWEL", "BREATH", "TWO VOICE", "RASP", "MUTE", "BOW AT", "BORE",
                                                  "STRIKE", "HAMMER", "REGISTER", "DRONES IN", "BUZZ", "MOUTH",
                                                  "MERI"};
    static const char *const S[RZ_COUNT] = {"POS", "AMT", "POS", "VOWEL", "BREATH", "TWO", "RASP", "MUTE", "BOW", "BORE", "AT", "HAM",
                                            "REG", "KEY", "BUZZ", "MOUTH", "MERI"};
    str_cpy(d, brief ? S[md] : L[md], n);
    ui_cat(d, n, " ");
    if (md == RZ_VOICE) {                       /* the vowel: U O A E I, or between two */
        uint32_t t = p * 4u * 256u / 127u, k = t >> 8, f = t & 255u;
        k = k > 3u ? 3u : k;
        if (t >= 4u * 256u) { k = 3u; f = 256u; }
        if (f < 40u)
            ui_cat(d, n, WG_VOWEL[k]);
        else if (f > 216u)
            ui_cat(d, n, WG_VOWEL[k + 1u]);
        else {
            ui_cat(d, n, WG_VOWEL[k]);
            ui_cat(d, n, ">");
            ui_cat(d, n, WG_VOWEL[k + 1u]);
        }
        return;
    }
    if (md == RZ_ACCORD) {                       /* the register: its reeds */
        static const char *const REG[5] = {"CLARINET", "VIOLIN", "MUSETTE", "BASSOON", "MASTER"};
        ui_cat(d, n, REG[p * 5u / 128u]);
        return;
    }
    if (md == RZ_BAGPIPE) {                      /* the drones' key */
        ui_cat(d, n, NOTE_NAME[(9u + p * 12u / 128u) % 12u]);
        return;
    }
    if (md == RZ_DRUM) {                         /* where it is struck: the middle .. the edge */
        ui_cat(d, n, p < 16u ? "MIDDLE" : p > 110u ? "EDGE" : "");
        if (p >= 16u && p <= 110u) {
            fmt_int(v, (int32_t)p);
            ui_cat(d, n, v);
        }
        return;
    }
    if (md == RZ_REED) {                         /* the bore: the clarinet's cylinder, or the cone's reed place */
        if (p < 16u)
            ui_cat(d, n, brief ? "CYL" : "CYLINDER");
        else {
            ui_cat(d, n, brief ? "CONE " : "CONE ");
            fmt_int(v, (int32_t)(6u + (p - 16u) * 39u / 111u));
            ui_cat(d, n, v);
            ui_cat(d, n, "%");
        }
        return;
    }
    if (md == RZ_BOW) {                          /* where the bow is: by the bridge .. over the fingerboard */
        ui_cat(d, n, p < 24u ? (brief ? "PONT" : "PONTICELLO") : p > 100u ? (brief ? "TASTO" : "SUL TASTO") : "");
        if (p >= 24u && p <= 100u) {
            fmt_int(v, (int32_t)p);
            ui_cat(d, n, v);
        }
        return;
    }
    if (md == RZ_SYRINX) {                       /* the second whistle: semitones above the first */
        uint32_t c = p * 700u / 127u;
        fmt_int(v, (int32_t)(c / 100u));
        ui_cat(d, n, "+");
        ui_cat(d, n, v);
        ui_cat(d, n, ".");
        fmt_int(v, (int32_t)(c / 10u % 10u));
        ui_cat(d, n, v);
        return;
    }
    fmt_int(v, (int32_t)p);
    ui_cat(d, n, v);
}

/* the MOD ENV's targets: the pitch, then what the oscillator and the FX have now (their knobs' own names): 0 the
 * pitch, else 1 + feg.c's parameter */
static uint32_t menv_targets(uint8_t *t, const char **nm)
{
    static const char *const K1[RZ_COUNT] = {"STRUCTURE", "CHORD", "STRUCTURE", "SIZE", "JET", "CHIRP", "SCREAM", "LIP",
                                             "PRESSURE", "REED", "KETTLE", "UNISON", "MUSETTE", "SOFT REED", "TINE", "LIPS",
                                             "MURAIKI"};
    uint32_t n = 0, md = sys.fx_model % RZ_COUNT, alg = sys.osc ? (sys.osc - 1u) % NP_COUNT : 0u;
#define MT(f, s) do { t[n] = (uint8_t)((f) + 1u); nm[n] = (s); n++; } while (0)
    t[n] = 0; nm[n] = sys.osc ? "PITCH" : "PITCH (NOISE ONLY)"; n++;
    if (!sys.osc) {
        MT(0, "CHAOS"); MT(1, "SHAPE"); MT(2, "OUTPUT");
    } else {
        MT(4, NP_X[alg]); MT(5, NP_Y[alg]); MT(6, "NOISE LEVEL");
    }
    MT(3, "RING");
    MT(8, K1[md]); MT(9, "FX BRIGHTNESS");
    MT(10, md == RZ_BAGPIPE ? "DRONES" : md == RZ_ACCORD ? "CHAMBER" : "FX DAMPING");
    {   /* (the POSITION knob's name, as fx_pos_text's) */
        static const char *const L[RZ_COUNT] = {"FX POSITION", "AMOUNT", "FX POSITION", "VOWEL", "BREATH", "TWO VOICE",
                                                "RASP", "MUTE", "BOW AT", "BORE", "STRIKE", "HAMMER", "REGISTER",
                                                "DRONES IN", "BUZZ", "MOUTH", "MERI"};
        MT(11, L[md]);
    }
    MT(12, "FX MIX");
    MT(15, sys.sh_space % SS_COUNT == SS_MOUTH ? "VOWEL (MOUTH)" : SS_IS_DELAY(sys.sh_space % SS_COUNT) ? "DELAY FILTER" : "SHIMMER");
    MT(16, "SPACE MIX");
#undef MT
    return n;
}
static const char *menv_target_name(void)
{
    uint8_t t[20];
    const char *nm[20];
    uint32_t n = menv_targets(t, nm), i;
    for (i = 0; i < n; i++)
        if (t[i] == sys.menv_target)
            return nm[i];
    return FEG_T_NAME[(sys.menv_target - 1u) % FEG_NT];
}

/* the oscillator's short name: "CHUA", or the noise program's bank and number ("A5") */
static void osc_code(char *d, uint32_t n, uint32_t osc)
{
    char c[3] = {(char)('A' + (osc - 1u) / 10u), (char)('0' + (osc - 1u) % 10u), 0};
    str_cpy(d, osc ? c : "CHUA", n);
}

static void ui_draw(void)
{
    char a[40], b[24], v[12];
    uint32_t note = sys.last_note, reg = chua_regime(sys.chaos);
    style_frame();                               /* (the UI STYLE's moving colours) */
    {   /* the GLO pages (MIDI, MULTI, SOUND, UI STYLE) keep the plain ground; the others the style's, all over */
        uint8_t off = sys.page == 7u || sys.page == 15u || sys.page == 16u || sys.page == 19u;
        if (off != cv_style_off) {
            cv_style_off = off;
            ui_force = 1;
        }
    }
    if (ui_force)
        lcd_fill(0, 0, 240, 240, T_BG);
    if (sys.page == 0u || sys.page == 14u || sys.page > 20u) {   /* HOME (and its BANK): the preset; elsewhere the page */
        str_cpy(a, sys.multi ? (part_edit ? "2 " : "1 ") : "", sizeof a);   /* (MULTI: the part the knobs edit) */
        ui_cat(a, sizeof a, PRESETS[sys.preset % NPRESETS].name);
        if (preset_edited())
            ui_cat(a, sizeof a, " *");
        if (part_mono[sys.multi ? part_edit : 0u])
            ui_cat(a, sizeof a, " MONO");
    } else {
        static const char *const TITLE[21] = {"", "LPG", "RESONATORS", "REVERB", "", "PITCH MOD", "ARPEGGIATOR", "MIDI",
                                              "LFO FREE EG", "SEQUENCER", "PANNING", "EDIT", "TAPE", "TAPE SPEED", "",
                                              "MULTI", "SOUND", "SEQ LOGIC", "ARP GROOVE", "UI STYLE", "MOD FX"};
        str_cpy(a, sys.page == 3u && SS_IS_DELAY(sys.sh_space % SS_COUNT) ? "DELAY" : sys.page == 8u && feg_ed ? "LFO FREE EG 2" :
                   sys.page == 8u ? "LFO FREE EG 1" : TITLE[sys.page], sizeof a);
    }
    if (note < 128u && sys.voices) {
        str_cpy(b, NOTE_NAME[note % 12u], sizeof b);
        fmt_int(v, (int32_t)note / 12 - 1);
        ui_cat(b, sizeof b, v);
    } else
        str_cpy(b, usb.config && !usb.suspended ? "USB" : "", sizeof b);
    ui_head(a, b);

    ui_val_c = sys.page == 5u ? T_FM : 0;   /* (the MOD ENV: its values green) */
    if (sys.page == 14u) {                       /* BANK: which, the program, how */
        fmt_int(v, (int32_t)(sys.preset >> 7) + 1);
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " ");
        ui_cat(b, sizeof b, BANK_NAME[(sys.preset >> 7) % NBANKS]);
        ui_cell(T_C1, "1 BANK", b);
        if (!sys.multi)
            str_cpy(b, "OFF", sizeof b);
        else if (!sys.ch_b)
            str_cpy(b, "OMNI", sizeof b);
        else {
            fmt_int(v, sys.ch_b);
            str_cpy(b, "CH ", sizeof b);
            ui_cat(b, sizeof b, v);
        }
        ui_cell(T_C2, "2 PART B", b);
        ui_cell(T_C3, "3 EDIT PART", sys.multi ? (part_edit ? "2" : "1") : "1 (MULTI OFF)");   /* (as HOME held + OP1, OP2) */
        ui_cell(T_C4, "4 VOICES", part_mono[sys.multi ? part_edit : 0u] ? "MONO" : "POLY");   /* (+ MONO, POLY) */
    } else if (sys.page == 13u) {                       /* TAPE's SPEED: the BPM, the ratio, the tape's own tempo, the pitch */
        const uint8_t *q = TAPE_SPD[sys.tape_spd % TAPE_NSPD];
        int32_t st = (int32_t)(sys.tape_spd % TAPE_NSPD) - 12, c = tape_cents(tape.speed);
        fmt_int(v, (int32_t)arp_bpm_now());
        str_cpy(b, arp_ext() ? "EXT " : "", sizeof b);
        ui_cat(b, sizeof b, v);
        ui_cell(T_C1, "1 BPM", b);
        fmt_int(v, q[0]);
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, "/");
        fmt_int(v, q[1]);
        ui_cat(b, sizeof b, v);
        ui_cat(b, sizeof b, st > 0 ? "  +" : "  ");
        fmt_int(v, st);
        ui_cat(b, sizeof b, v);
        ui_val_c = tape.lock_q12 ? T_ACCENT : 0;
        ui_cell(T_C2, "2 SPEED", b);
        ui_val_c = 0;
        {
            static const char *const LN[5] = {"1 BEAT", "2 BEATS", "1 BAR", "2 BARS", "4 BARS"};
            uint32_t fit = tape_fit();                           /* (as long as the tape holds: lit if shorter) */
            ui_val_c = fit < sys.tape_len % 5u ? T_ACCENT : 0;
            ui_cell(T_C3, "3 LENGTH", LN[fit]);
            ui_val_c = 0;
        }
        str_cpy(b, c > 0 ? "+" : c < 0 ? "-" : "", sizeof b);
        c = ((c < 0 ? -c : c) + 5) / 10;                         /* (tenths of a semitone, rounded) */
        fmt_int(v, c / 10);
        ui_cat(b, sizeof b, v);
        ui_cat(b, sizeof b, ".");
        v[0] = (char)('0' + c % 10); v[1] = 0;
        ui_cat(b, sizeof b, v);
        ui_cat(b, sizeof b, " ST");
        ui_cell(T_C4, "4 PITCH", b);
    } else if (sys.page == 12u) {                /* TAPE: the tracks' faders; the armed one marked */
        static const uint8_t CELL[4] = {T_C1, T_C2, T_C3, T_C4};
        for (uint32_t i = 0; i < 4u; i++) {
            str_cpy(a, "1 TRACK 1", sizeof a);
            a[0] = a[8] = (char)('1' + i);
            fmt_int(v, sys.tape_lv[i]);
            str_cpy(b, tape.has[i] ? v : "EMPTY", sizeof b);
            if (i == sys.tape_arm % 4u)
            {
                if (tape.rec)
                    str_cpy(b, "REC", sizeof b);
                else if (tape.has[i])
                    ui_cat(b, sizeof b, "  ARM");
                else
                    str_cpy(b, "ARMED", sizeof b);
            }
            ui_val_c = i == sys.tape_arm % 4u ? T_ACCENT : 0;
            ui_cell(CELL[i], a, b);
        }
        ui_val_c = 0;
    } else if (sys.page == 11u) {
        ui_val_c = FEG_FM(sys.mw_target) ? T_FM : 0;
        ui_cell(T_C1, "1 MOD WHEEL", FEG_T_NAME[sys.mw_target % FEG_NT]);
        ui_val_c = 0;
        fmt_int(v, sys.mw_depth);
        ui_cell(T_C2, "2 DEPTH", v);
        ui_cell(T_C3, "3 VELOCITY", VEL_TO_NAME[sys.vel_to & 3u]);
        fmt_int(v, sys.vel_amt);
        ui_cell(T_C4, "4 AMOUNT", sys.vel_to ? v : "-");
    } else if (sys.page == 10u) {
        if (sys.basis)
            fmt_int(v, sys.basis);
        else
            str_cpy(v, "MONO", sizeof v);
        ui_cell(T_C1, "1 BASIS", v);
        if (sys.orbit)
            fmt_int(v, sys.orbit);
        else
            str_cpy(v, "OFF", sizeof v);
        ui_cell(T_C2, "2 ORBIT", v);
        ui_cell(T_C3, "3", "");
        ui_cell(T_C4, "4", "");
    } else if (sys.page == 9u) {
        static const uint8_t CELL[4] = {T_C1, T_C2, T_C3, T_C4};
        const ge_t *G = k_ge();
        for (uint32_t i = 0; i < 4u; i++) {
            str_cpy(a, "1 ", sizeof a);
            a[0] = (char)('1' + i);
            ui_cat(a, sizeof a, MC_NAME[G->knob[i] % MC_COUNT]);
            if (G->knob[i] == MC_TRANSPOSE) {    /* (in octaves) */
                int32_t o = ((int32_t)(sys.k_m[i] & 127u) - 64) / 32 + G->transpose;
                fmt_int(v, o);
                str_cpy(b, o > 0 ? "+" : "", sizeof b);
                ui_cat(b, sizeof b, v);
                ui_cat(b, sizeof b, " OCT");
                ui_cell(CELL[i], a, b);
                continue;
            }
            fmt_int(v, sys.k_m[i] & 127u);
            ui_cell(CELL[i], a, v);
        }
    } else if (sys.page == 8u) {
        fmt_int(v, sys.feg_val);
        ui_cell(T_C1, "1 VALUE", v);
        ui_cell(T_C2, "2 LENGTH", FEG_LEN_NAME[sys.feg_len % FEG_NLEN]);
        fmt_int(v, sys.feg_depth);
        ui_cell(T_C3, "3 DEPTH", v);
        ui_val_c = FEG_FM(sys.feg_target) ? T_FM : 0;   /* (an FM target: green) */
        ui_cell(T_C4, "4 TARGET", FEG_T_NAME[sys.feg_target % FEG_NT]);
        ui_val_c = 0;
    } else if (sys.page == 18u) {                /* GROOVE: the VELOCITY SEQ, the GATE SEQ */
        fmt_int(v, (int32_t)arp_vlen());
        ui_cell(T_C1, "1 SEQ LENGTH", v);
        fmt_int(v, (int32_t)(sys.arp_vd & 127u));
        ui_cell(T_C2, "2 DEPTH", v);
        fmt_int(v, (int32_t)arp_glen());
        ui_cell(T_C3, "3 GATE LENGTH", v);
        ui_cell(T_C4, "4 GATE SEQ", ARP_GPAT[sys.arp_gp % ARP_NGP].name);
    } else if (sys.page == 17u) {                /* LOGIC: how the ARP and KARMA meet */
        static const char *const LG[7] = {"OFF", "OR", "AND", "XOR", "NOR", "ALT", "S/R"};
        ui_cell(T_C1, "1 LOGIC", LG[sys.k_logic % 7u]);
        ui_cell(T_C2, "2 NOTE QUANT", logic_q_name(sys.k_lq));
        ui_cell(T_C3, "3 ROOT", sys.k_root ? NOTE_NAME[(sys.k_root - 1u) % 12u] : "AUTO");
        ui_cell(T_C4, "4 INVERSE", sys.k_inv ? "ON" : "OFF");
    } else if (sys.page == 16u) {                /* SOUND: SPEAKER BASS, LEVEL MATCH, BEND RANGE */
        fmt_int(v, (int32_t)(sys.spk_bass & 127u));
        ui_cell(T_C1, "1 SPEAKER BASS", sys.spk_bass & 127u ? v : "OFF");
        ui_cell(T_C2, "2 LEVEL MATCH", sys.lvl_match ? "ON" : "OFF");
        fmt_int(v, (int32_t)sys.bend_range);
        str_cpy(b, sys.bend_range ? "+-" : "OFF", sizeof b);
        if (sys.bend_range)
            ui_cat(b, sizeof b, v);
        ui_cell(T_C3, "3 BEND RANGE", b);
        ui_cell(T_C4, "4", "");
    } else if (sys.page == 19u) {                /* UI STYLE: the look, its motion, its backdrop */
        ui_cell(T_C1, "1 STYLE", US_NAME[sys.ui_style % US_COUNT]);
        fmt_int(v, (int32_t)(sys.ui_motion & 127u));
        ui_cell(T_C2, "2 MOTION", sys.ui_motion & 127u ? v : "STILL");
        fmt_int(v, (int32_t)((sys.ui_backdrop & 127u) * 100u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C3, "3 BACKDROP", sys.ui_style % US_COUNT ? b : "-");
        fmt_int(v, (int32_t)(sys.ui_react & 127u));
        ui_cell(T_C4, "4 REACT", sys.ui_react & 127u ? v : "OFF");
    } else if (sys.page == 15u) {                /* MULTI: on, part B's channel, its bank, its program */
        ui_cell(T_C1, "1 MULTI", sys.multi ? "ON" : "OFF");
        fmt_int(v, sys.ch_b);
        ui_cell(T_C2, "2 B CHANNEL", sys.ch_b ? v : "OMNI");
        fmt_int(v, (int32_t)(part_preset(1) >> 7) + 1);
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " ");
        ui_cat(b, sizeof b, BANK_NAME[(part_preset(1) >> 7) % NBANKS]);
        ui_cell(T_C3, "3 B BANK", b);
        fmt_int(v, (int32_t)(part_preset(1) & 127u) + 1);
        ui_cell(T_C4, "4 B PROGRAM", v);
    } else if (sys.page == 7u) {
        static const char *const USBN[4] = {"OFF", "IN", "OUT", "IN + OUT"};
        if (sys.midi_rx) {
            fmt_int(v, sys.midi_rx);
            ui_cell(T_C1, "1 CHANNEL A", v);
        } else
            ui_cell(T_C1, "1 CHANNEL A", "OMNI");
        ui_cell(T_C2, "2 USB MIDI", USBN[sys.midi_usb & 3u]);
        fmt_int(v, sys.ch_b);
        ui_cell(T_C3, "3 CHANNEL B", sys.ch_b ? v : "OMNI");
        fmt_int(v, (int32_t)(sys.midi_tx & 15u) + 1);
        ui_cell(T_C4, "4 TX CHANNEL", v);
    } else if (sys.page == 6u) {
        uint32_t bpm = arp_bpm_now();
        if (arp_ext()) {                         /* (CLOCK EXTERNAL: the knob does nothing; the tempo coming in) */
            str_cpy(b, "EXT", sizeof b);
            if (bpm) {
                fmt_int(v, (int32_t)bpm);
                ui_cat(b, sizeof b, " ");
                ui_cat(b, sizeof b, v);
            }
        } else {
            fmt_int(v, sys.arp_bpm);
            str_cpy(b, v, sizeof b);
        }
        ui_cell(T_C1, "1 BPM", b);
        ui_cell(T_C2, "2 SUBDIVIDE", ARP_DIV_NAME[sys.arp_div % ARP_NDIV]);
        fmt_int(v, (int32_t)(8u + (sys.arp_gate & 127u) * 92u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C3, "3 GATE", b);
        fmt_int(v, (int32_t)(50u + (sys.arp_swing & 127u) * 33u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C4, "4 SWING", b);
    } else if (sys.page == 5u) {
        env_time(b, sizeof b, sys.menv_len);
        ui_cell(T_C1, "1 LENGTH", b);
        {
            uint32_t k = sys.menv_skew & 127u;
            fmt_int(v, (int32_t)(k * 100u / 127u));
            str_cpy(b, k == 0u ? "FALL" : k == 127u ? "RISE" : v, sizeof b);
            if (k && k < 127u)
                ui_cat(b, sizeof b, " %");
        }
        ui_cell(T_C2, "2 SKEW", b);
        if ((sys.menv_height & 127u) && sys.menv_target) {   /* (on a parameter: how far it moves it, up or down) */
            str_cpy(b, sys.menv_height & 0x80u ? "-" : "+", sizeof b);
            fmt_int(v, (int32_t)(sys.menv_height & 127u));
            ui_cat(b, sizeof b, v);
        } else if (sys.menv_height & 127u) {        /* (the sweep at the peak, in semitones: HEIGHT^3, 36 the top, up or
                                                  * down; tenths under 10) */
            uint32_t h = sys.menv_height & 127u, t = (uint32_t)(((uint64_t)h * h * h * 11794u) >> 26);   /* tenths */
            str_cpy(b, sys.menv_height & 0x80u ? "-" : "+", sizeof b);
            fmt_int(v, (int32_t)(t / 10u));
            ui_cat(b, sizeof b, v);
            if (t < 100u) {
                ui_cat(b, sizeof b, ".");
                fmt_int(v, (int32_t)(t % 10u));
                ui_cat(b, sizeof b, v);
            }
            ui_cat(b, sizeof b, " ST");
        } else
            str_cpy(b, "OFF", sizeof b);
        ui_cell(T_C3, "3 HEIGHT", b);
        {
            int32_t bd = (int32_t)(sys.menv_bend & 127u) - 64;
            fmt_int(v, bd);
            str_cpy(b, bd > 0 ? "+" : "", sizeof b);
            ui_cat(b, sizeof b, bd ? v : "STRAIGHT");
            ui_cell(T_C4, "4 BEND", b);
        }
    } else if (sys.page == 0u && sys.osc) {
        uint32_t alg = (sys.osc - 1u) % NP_COUNT;
        str_cpy(a, "1 ", sizeof a);
        ui_cat(a, sizeof a, NP_X[alg]);
        fmt_int(v, sys.np_x);
        ui_val_c = np_is_fm(NP_X[alg]) ? T_FM_IN : 0;   /* (the program's own FM: orange) */
        ui_cell(T_C1, a, v);
        str_cpy(a, "2 ", sizeof a);
        ui_cat(a, sizeof a, NP_Y[alg]);
        fmt_int(v, alg == 10u ? 2 + (int32_t)(sys.np_y & 127u) * 258 * 14 / 32767 : sys.np_y);   /* (CLUSTER SAW: how many) */
        ui_val_c = np_is_fm(NP_Y[alg]) ? T_FM_IN : 0;
        ui_cell(T_C2, a, v);
        ui_val_c = 0;
        if (alg == NP_PIANO) {                   /* PIANO: DECAY (x 1/4 .. 4), STIFF (x 0 .. 2.5 of a piano's) */
            int32_t d = (int32_t)(sys.chaos & 127u) - 64;
            str_cpy(b, d < 0 ? "X1/" : "X", sizeof b);
            {
                uint32_t q = (uint32_t)(((uint64_t)100u * px_exp2((d < 0 ? -d : d) * 2048)) >> 16);   /* x 100 */
                fmt_int(v, (int32_t)(q / 100u));
                ui_cat(b, sizeof b, v);
                ui_cat(b, sizeof b, ".");
                fmt_int(v, (int32_t)(q / 10u % 10u));
                ui_cat(b, sizeof b, v);
            }
            ui_cell(T_C3, "3 DECAY", b);
            fmt_int(v, sys.output);
            ui_cell(T_C4, "4 STIFF", v);
        } else if (alg == 30u) {                 /* BIG BANG: its own two where LEVEL and RING are */
            if (sys.chaos & 127u) {
                uint32_t ms = (uint32_t)(((uint64_t)30u * px_exp2((int32_t)((sys.chaos & 127u) * 463339u / 127u))) >> 16);
                fmt_int(v, (int32_t)(ms < 1000u ? ms : ms / 100u));
                str_cpy(b, v, sizeof b);
                if (ms >= 1000u) {               /* (seconds with one decimal) */
                    char d[2] = {b[str_len(b) - 1u], 0};
                    b[str_len(b) - 1u] = 0;
                    ui_cat(b, sizeof b, ".");
                    ui_cat(b, sizeof b, d);
                    ui_cat(b, sizeof b, " S");
                } else
                    ui_cat(b, sizeof b, " MS");
            } else
                str_cpy(b, "OFF", sizeof b);
            ui_cell(T_C3, "3 BANG", b);
            fmt_int(v, sys.output);
            ui_cell(T_C4, "4 EXPAND", v);
        } else {
            fmt_int(v, sys.np_level);
            ui_cell(T_C3, "3 LEVEL", v);
            fmt_int(v, sys.ring);
            ui_cell(T_C4, "4 RING", v);
        }
    } else if (sys.page == 0u) {
        str_cpy(a, "1 CHAOS ", sizeof a);
        fmt_int(v, sys.chaos);
        ui_cat(a, sizeof a, v);
        ui_cell(T_C1, a, REG_NAME[reg]);
        {   /* beta with one decimal */
            int32_t bq = chua_beta_q16(sys.shape), t = (bq * 10 + 32768) >> 16;
            fmt_int(v, t / 10);
            str_cpy(b, "BETA ", sizeof b);
            ui_cat(b, sizeof b, v);
            ui_cat(b, sizeof b, ".");
            fmt_int(v, t % 10);
            ui_cat(b, sizeof b, v);
        }
        ui_cell(T_C2, "2 SHAPE", b);
        {
            uint32_t o = sys.output;
            ui_cell(T_C3, "3 OUTPUT", o < 16u ? "X" : o < 48u ? "X + Y" : o < 80u ? "Y" : o < 112u ? "Y + Z" : "Z");
        }
        fmt_int(v, sys.ring);
        ui_cell(T_C4, "4 RING", v);
    } else if (sys.page == 3u && SS_IS_DELAY(sys.sh_space % SS_COUNT)) {   /* a delay: TIME, FEEDBACK, FILTER, MIX */
        ui_cell(T_C1, "1 TIME", SH_DTIME_NAME[sh_dtime()]);
        fmt_int(v, (int32_t)((sys.sh_decay & 127u) * 95u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C2, "2 FEEDBACK", b);
        if ((sys.sh_shimmer & 127u) >= 127u)
            str_cpy(b, "OPEN", sizeof b);
        else {
            uint32_t fc = (uint32_t)(((uint64_t)150u * px_exp2((int32_t)(sys.sh_shimmer & 127u) * 268697 / 127)) >> 16);
            fmt_int(v, (int32_t)fc);
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, " HZ");
        }
        ui_cell(T_C3, "3 FILTER", b);
        if (sys.sh_mix) {
            fmt_int(v, (int32_t)(sys.sh_mix * 100u / 127u));
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, " %");
        } else
            str_cpy(b, "OFF", sizeof b);
        ui_cell(T_C4, "4 MIX", b);
    } else if (sys.page == 3u) {
        fmt_int(v, sys.sh_size);
        ui_cell(T_C1, "1 SIZE", v);
        {   /* RT60, as the DSP has it: 0.4 s x 75^(d / 127) */
            uint32_t ms = (uint32_t)(((uint64_t)400u * px_exp2((int32_t)((sys.sh_decay & 127u) * 408220u / 127u))) >> 16);
            fmt_int(v, (int32_t)(ms / 1000u));
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, ".");
            fmt_int(v, (int32_t)(ms / 100u % 10u));
            ui_cat(b, sizeof b, v);
            ui_cat(b, sizeof b, " s");
        }
        ui_cell(T_C2, "2 DECAY", b);
        if (sys.sh_space % SS_COUNT == SS_MOUTH) {   /* (MOUTH: SHIMMER is the vowel, along the INTERVAL's path) */
            static const char VW[5] = {'U', 'O', 'A', 'E', 'I'};
            uint32_t iv = sys.sh_interval % SI_COUNT, nv = SH_MPATH[iv][0];
            const uint8_t *path = iv ? &SH_MPATH[iv][1] : SH_MPATH5;
            uint32_t q = ((sys.sh_shimmer & 127u) * (nv - 1u) + 63u) / 127u;
            v[0] = VW[path[q]]; v[1] = 0;
            ui_cell(T_C3, "3 VOWEL", v);
        } else {
            fmt_int(v, sys.sh_shimmer);
            ui_cell(T_C3, "3 SHIMMER", v);
        }
        if (sys.sh_mix) {
            fmt_int(v, (int32_t)(sys.sh_mix * 100u / 127u));
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, " %");
        } else
            str_cpy(b, "OFF", sizeof b);
        ui_cell(T_C4, "4 MIX", b);
    } else if (sys.page == 20u) {                /* MOD FX: the note value or ratio, the slew, the rectifier, the mix */
        uint32_t mode = mf.on ? mf.mode : sys.mfx_mode % MF_COUNT;
        if (MF_KEYED(mode)) {                    /* (RING, SHIFT: the ratio to the note; VIBRATO, TREMOLO: the tempo's note value) */
            str_cpy(b, "NOTE X ", sizeof b);
            ui_cat(b, sizeof b, MF_RATIO_NAME[mf_ratio()]);
        } else
            str_cpy(b, MF_DIV_NAME[mf_div()], sizeof b);
        ui_cell(T_C1, MF_KEYED(mode) ? "1 RATIO" : "1 SYNC", b);
        if (sys.mfx_slew) {                      /* (the slew's corner: 0.9 Hz .. full) */
            fmt_int(v, (int32_t)(sys.mfx_slew * 100u / 127u));
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, " %");
        } else
            str_cpy(b, "OFF", sizeof b);
        ui_cell(T_C2, "2 SLEW", b);
        ui_cell(T_C3, "3 RECTIFY", MF_RECT[mf_rect_of(sys.mfx_rect)]);
        fmt_int(v, (int32_t)(sys.mfx_mix * 100u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C4, "4 MIX", b);
    } else if (sys.page == 2u) {
        uint32_t md = sys.fx_model % RZ_COUNT;
        static const char *const K1[RZ_COUNT] = {"1 STRUCTURE", "1 CHORD", "1 STRUCTURE", "1 SIZE", "1 JET", "1 CHIRP", "1 SCREAM", "1 LIP",
                                                   "1 PRESSURE", "1 REED", "1 KETTLE", "1 UNISON", "1 MUSETTE", "1 SOFT REED", "1 TINE", "1 LIPS",
                                                   "1 MURAIKI"};
        if (md == RZ_SYMP)
            str_cpy(b, RZ_CHORD_NAME[(sys.fx_struct & 127u) >> 4], sizeof b);
        else if (md == RZ_VOICE)
            str_cpy(b, WG_VSIZE[(sys.fx_struct & 127u) * WV_COUNT / 128u], sizeof b);
        else {
            fmt_int(v, sys.fx_struct);
            str_cpy(b, v, sizeof b);
        }
        ui_cell(T_C1, K1[md], b);
        fmt_int(v, sys.fx_bright);
        ui_cell(T_C2, "2 BRIGHTNESS", v);
        fmt_int(v, sys.fx_damp);
        ui_cell(T_C3, md == RZ_BAGPIPE ? "3 DRONES" : md == RZ_ACCORD ? "3 CHAMBER" : "3 DAMPING", v);
        if (sys.fx_mix) {
            fmt_int(v, (int32_t)(sys.fx_mix * 100u / 127u));
            str_cpy(b, v, sizeof b);
            ui_cat(b, sizeof b, " %");
        } else
            str_cpy(b, "OFF", sizeof b);
        ui_cell(T_C4, "4 MIX", b);
    } else {
        env_time(b, sizeof b, sys.attack);
        ui_cell(T_C1, "1 ATTACK", b);
        env_time(b, sizeof b, sys.decay);
        ui_cell(T_C2, "2 DECAY", b);
        fmt_int(v, (int32_t)(sys.sustain * 100u / 127u));
        str_cpy(b, v, sizeof b);
        ui_cat(b, sizeof b, " %");
        ui_cell(T_C3, "3 SUSTAIN", b);
        env_time(b, sizeof b, sys.release);
        ui_cell(T_C4, "4 RELEASE", b);
    }

    if (ui_event_ms && fm1_ms - ui_event_ms < 2000u)
        ui_status(ui_event);
    else {
        if (sys.page == 3u) {
            str_cpy(a, SH_SPACE_NAME[sys.sh_space % SS_COUNT], sizeof a);
            ui_cat(a, sizeof a, " ");
            ui_cat(a, sizeof a, sh_iv_name(sys.sh_space % SS_COUNT, sys.sh_interval));
            ui_cat(a, sizeof a, SS_IS_DELAY(sys.sh_space % SS_COUNT) ? "  RESO " : "  TONE ");
            fmt_int(v, sys.sh_tone);
            ui_cat(a, sizeof a, v);
        } else if (sys.page == 20u) {
            str_cpy(a, "MOD FX ", sizeof a);
            ui_cat(a, sizeof a, MF_NAME[sys.mfx_mode % MF_COUNT]);
            ui_cat(a, sizeof a, " (ALGO)");
        } else if (sys.page == 2u) {
            str_cpy(a, RZ_NAME[sys.fx_model % RZ_COUNT], sizeof a);
            ui_cat(a, sizeof a, "   ");
            fx_pos_text(b, sizeof b, 1);
            ui_cat(a, sizeof a, b);
        } else if (sys.page == 9u) {             /* KARMA: on or off, the GE, LATCH */
            str_cpy(a, sys.k_on ? "KARMA ON  " : "KARMA OFF  ", sizeof a);
            ui_cat(a, sizeof a, k_ge()->name);
            if (sys.k_latch)
                ui_cat(a, sizeof a, "  LATCH");
        } else if (sys.page == 11u) {            /* EDIT: the sound's octave (SELECT) */
            str_cpy(a, "SELECT: SOUND OCTAVE ", sizeof a);
            fmt_int(v, sys.transpose);
            if (sys.transpose > 0)
                ui_cat(a, sizeof a, "+");
            ui_cat(a, sizeof a, v);
        } else if (sys.page == 8u) {             /* FREE EG: recording, playing, or what to do */
            str_cpy(a, feg.rec ? "FREE EG   RECORDING: TURN KNOB 1" : !sys.feg_has ? "FREE EG   REC: RECORD" :
                    sys.feg_on ? "FREE EG   PLAYING" : "FREE EG   SELECT: PLAY", sizeof a);
            a[8] = (char)('1' + feg_ed);           /* (which: FREE EG 1, 2) */
        } else if (sys.page == 7u) {             /* MIDI: the CLOCK (SELECT) */
            str_cpy(a, sys.clk_int ? "CLOCK INT" : "CLOCK EXT", sizeof a);
            ui_cat(a, sizeof a, sys.autoplay ? "  AUTOPLAY ON" : "  AUTOPLAY OFF");
        } else if (sys.page == 12u || sys.page == 13u) {   /* TAPE: the transport, the LENGTH, the speed */
            static const char *const LN[5] = {"1 BEAT", "2 BEATS", "1 BAR", "2 BARS", "4 BARS"};
            uint32_t sp = (tape.speed * 100u + 2048u) / 4096u;
            str_cpy(a, tape.rec ? "TAPE REC  " : tape.playing ? "TAPE PLAY  " : "TAPE STOP  ", sizeof a);
            if (sys.page == 13u && tape.spb) {   /* (the SPEED menu: the tempo it was recorded at) */
                fmt_int(v, (int32_t)((2646000u * 2u / tape.spb + 1u) / 2u));
                ui_cat(a, sizeof a, "AT ");
                ui_cat(a, sizeof a, v);
            } else
                ui_cat(a, sizeof a, LN[tape_fit()]);
            ui_cat(a, sizeof a, "  x");
            fmt_int(v, (int32_t)(sp / 100u));
            ui_cat(a, sizeof a, v);
            ui_cat(a, sizeof a, ".");
            v[0] = (char)('0' + sp / 10u % 10u); v[1] = (char)('0' + sp % 10u); v[2] = 0;
            ui_cat(a, sizeof a, v);
        } else if (sys.page == 6u) {             /* ARP: on or off, the pattern, LATCH */
            str_cpy(a, sys.arp_on ? "ARP ON  " : "ARP OFF  ", sizeof a);
            ui_cat(a, sizeof a, arp_type_name(sys.arp_type));
            if (sys.arp_hold)
                ui_cat(a, sizeof a, "  LATCH");
        } else if (sys.page == 5u) {             /* MOD ENV: on what */
            str_cpy(a, "MOD ENV > ", sizeof a);  /* (SELECT: on what) */
            ui_cat(a, sizeof a, menv_target_name());
        } else if (sys.page) {
            str_cpy(a, "BASIS ", sizeof a);
            if (sys.basis)
                fmt_int(v, sys.basis);
            else
                str_cpy(v, "MONO", sizeof v);
            ui_cat(a, sizeof a, v);
            ui_cat(a, sizeof a, "  ORBIT ");
            if (sys.orbit)
                fmt_int(v, sys.orbit);
            else
                str_cpy(v, "OFF", sizeof v);
            ui_cat(a, sizeof a, v);
        } else {                                 /* HOME: the oscillator and its octave (SELECT) */
            osc_code(a, sizeof a, sys.osc);
            if (sys.osc) {                       /* (the program's name: no room for the octave as well) */
                ui_cat(a, sizeof a, " ");
                ui_cat(a, sizeof a, NP_NAME[(sys.osc - 1u) % NP_COUNT]);
            } else {
                ui_cat(a, sizeof a, "   SOUND ");
                if (sys.transpose > 0)
                    ui_cat(a, sizeof a, "+");
                fmt_int(v, sys.transpose);
                ui_cat(a, sizeof a, v);
            }
        }
        ui_cat(a, sizeof a, "   OCT ");
        if (sys.octave > 0)
            ui_cat(a, sizeof a, "+");
        fmt_int(v, sys.octave);
        ui_cat(a, sizeof a, v);
        ui_status(a);                            /* (the CPU no longer shown: sys.cpu_q8 still kept) */
    }
    if (sys.page == 1u)
        draw_env();
    else if (sys.page == 2u) {
        if (sys.ui_style % US_COUNT == US_PSYCHE)
            us_fx_pic(0);                        /* (PSYCHEDELIC: the model as a simple line drawing) */
        else
            art_draw();
    }
    else if (sys.page == 3u)
        draw_sh();
    else if (sys.page == 20u)
        draw_mfx();
    else if (sys.page == 5u)
        draw_menv();
    else if (sys.page == 6u)
        draw_arp();
    else if (sys.page == 7u)
        draw_midi();
    else if (sys.page == 8u)
        draw_feg();
    else if (sys.page == 9u)
        draw_karma();
    else if (sys.page == 10u)
        draw_pan();
    else if (sys.page == 11u)
        draw_edit();
    else if (sys.page == 14u)
        draw_bank();
    else if (sys.page == 15u)
        draw_multi();
    else if (sys.page == 16u)
        draw_sound();
    else if (sys.page == 19u)
        draw_style();
    else if (sys.page == 17u)
        draw_logic();
    else if (sys.page == 18u)
        draw_groove();
    else if (sys.page == 12u || sys.page == 13u) {
        art_t++;
        art_c = T_THEME;
        art_c2 = ux_mix(T_BG, art_c, 75);
        art_c3 = ux_mix(T_BG, art_c, 45);
        cv_begin(240, PLOT_H, T_BG);
        art_tape();
        cv_blit(0, PLOT_Y);
    }
    else if (cv_backdrop && !cv_style_off) {   /* (a UI style: its scene instead of the picture, the oscillator played
                                                * into it: ui_style.c) */
        cv_begin(240, PLOT_H, T_BG);
        cv_blit(0, PLOT_Y);
    } else
        draw_plot();
    ui_force = 0;
}

/* the splash's 8-bit letters: 7 x 7 cells, strokes two cells wide (rows, bit 6 the left cell) */
static const struct { char ch; uint8_t row[7]; } PIX_FONT[] = {
    {'A', {0x3E, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x63}}, {'C', {0x3E, 0x63, 0x60, 0x60, 0x60, 0x63, 0x3E}},
    {'E', {0x7F, 0x60, 0x60, 0x7E, 0x60, 0x60, 0x7F}}, {'H', {0x63, 0x63, 0x63, 0x7F, 0x63, 0x63, 0x63}},
    {'I', {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E}}, {'K', {0x63, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x63}},
    {'L', {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7F}}, {'P', {0x7E, 0x63, 0x63, 0x7E, 0x60, 0x60, 0x60}},
    {'S', {0x3F, 0x60, 0x60, 0x3E, 0x03, 0x03, 0x7E}}, {'U', {0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x3E}},
    {'W', {0x63, 0x63, 0x6B, 0x6B, 0x7F, 0x77, 0x63}},
};

/* s in those letters, cells of c px, centred at screen row y: an orange outline, no fill, a white glint inside each
 * top left corner (the highlight) */
static void pix_text(const char *s, uint32_t y, int32_t c, int32_t gap)
{
    const uint8_t *g[16];
    int32_t n = 0, l, r, k, t = c >= 6 ? 2 : 1, w, x0;
    for (; *s && n < 16; s++) {
        uint32_t i;
        for (i = 0; i < sizeof PIX_FONT / sizeof PIX_FONT[0]; i++)
            if (PIX_FONT[i].ch == *s)
                g[n++] = PIX_FONT[i].row;
    }
    w = n * 7 * c + (n - 1) * gap;
    x0 = (240 - w) / 2;
    cv_begin(240, (uint32_t)(7 * c), T_BG);
    for (l = 0; l < n; l++)
        for (r = 0; r < 7; r++)
            for (k = 0; k < 7; k++) {
                int32_t x = x0 + l * (7 * c + gap) + k * c, yy = r * c;
#define ON(rr, kk) ((rr) >= 0 && (rr) < 7 && (kk) >= 0 && (kk) < 7 && (g[l][rr] >> (6 - (kk)) & 1u))
                if (!ON(r, k))
                    continue;
                if (!ON(r - 1, k))                      /* the outline: the edges facing an empty cell */
                    cv_rect(x, yy, c, t, T_THEME);
                if (!ON(r + 1, k))
                    cv_rect(x, yy + c - t, c, t, T_THEME);
                if (!ON(r, k - 1))
                    cv_rect(x, yy, t, c, T_THEME);
                if (!ON(r, k + 1))
                    cv_rect(x + c - t, yy, t, c, T_THEME);
                if (!ON(r - 1, k - 1) && ON(r - 1, k) && ON(r, k - 1))   /* (inner corners closed) */
                    cv_rect(x, yy, t, t, T_THEME);
                if (!ON(r - 1, k + 1) && ON(r - 1, k) && ON(r, k + 1))
                    cv_rect(x + c - t, yy, t, t, T_THEME);
                if (!ON(r + 1, k - 1) && ON(r + 1, k) && ON(r, k - 1))
                    cv_rect(x, yy + c - t, t, t, T_THEME);
                if (!ON(r + 1, k + 1) && ON(r + 1, k) && ON(r, k + 1))
                    cv_rect(x + c - t, yy + c - t, t, t, T_THEME);
                if (!ON(r, k - 1) && !ON(r - 1, k)) {   /* the highlight: a white glint inside the letter, in its */
                    int32_t hl = c > 2 * t ? c : 1, ht = t > 1 ? t - 1 : 1;   /* top left corners, along the stroke */
                    cv_rect(x + t, yy + t, ON(r, k + 1) ? hl : c - 2 * t, ht, T_TEXT);
                    cv_rect(x + t, yy + t, ht, ON(r + 1, k) ? hl : c - 2 * t, T_TEXT);
                }
#undef ON
            }
    cv_blit(0, y);
}

/* PSYCHEDELIC's splash, a frame at ms into it (main.c: ~1.3 s): from a point in the dark the figure of the oscillator
 * the preset plays blooms out of an eye, ring by ring, its tunnel's lines with it (ui_style.c, as a note out of silence
 * does); the eye opens, blinks near the end; HAWK and CHUA turn through the hues under it; the rest drawn once */
#define SPLASH_PSY_MS 1300u
static void splash_psy(uint32_t ms)
{
    uint16_t th = ux.theme, tx = ux.text;
    int32_t W = 30, H = W / 2, open = ms < 300u ? 1 : ms < 800u ? 1 + (H + 2) * (int32_t)(ms - 300u) / 500 :
                                   ms > 1050u && ms < 1150u ? 1 : H + 2;
    uint8_t pg = sys.page;
    us_splash = 1;
    us_splash_ms = ms;
    sys.page = 0;                                /* (HOME's backdrop, whatever page was saved) */
    cv_begin(240, 124, T_BG);                    /* (the figure and its tunnel: the backdrop, ui_style.c) */
    sys.page = pg;
    if (ms < 300u)                               /* (the point it comes from) */
        cv_rect(119, 61, 3, 3, us_hue(ms * 2u, 255u));
    else
        us_eye(120, 62, W * (int32_t)(ms < 800u ? ms : 800u) / 800, open, 2 + (int32_t)(ms / 250u), ms * 2u,
               ms > 900u ? 120 : 0, 0);
    cv_blit(0, 0);
    us_splash = 0;
    ux.theme = us_hue(ms / 3u, 255u);            /* (the letters through the hues) */
    ux.text = us_hue(ms / 3u + 96u, 120u);
    pix_text("HAWK", 128, 5, 5);
    ux.theme = us_hue(ms / 3u + 128u, 255u);
    pix_text("CHUA", 167, 3, 3);
    ux.theme = th;
    ux.text = tx;
}

/* the splash: HAWK, CHUA smaller, what it is, PLIEUWSKI, all in the 8-bit letters (PSYCHEDELIC: its own, moving) */
static void draw_splash(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    if (sys.ui_style % US_COUNT == US_PSYCHE) {
        draw_text_box(0, 190, 240, &AF_S, "MODELING SYNTHESIZER", T_MID, 1);
        pix_text("PLIEUWSKI", 210, 2, 2);
        splash_psy(0);
        return;
    }
    pix_text("HAWK", 30, 7, 8);
    pix_text("CHUA", 96, 4, 5);
    draw_text_box(0, 138, 240, &AF_S, "MODELING SYNTHESIZER", T_MID, 1);
    pix_text("PLIEUWSKI", 166, 3, 3);
    draw_text_box(0, 216, 240, &AF_S, UI_VERSION, T_DIM, 1);
}

/* -------------------------------------------------------------- LEDs --- */
/* The picture is built off-line and written one byte per column: clearing and relighting would let the
 * 10 kHz scan catch the dark gap and flicker. Every LED glows dim (fm1_led_dim), lit while pressed. */
static void led_on(uint8_t *nl, uint32_t id)            /* matrix id: button 0..13, note key 14..40 */
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id)
                nl[p] |= (uint8_t)(1u << r);
}

static void ui_leds(void)
{
    uint8_t nl[FM1_NCOL];
    uint32_t i, notes = fm1_in.notes, btns = fm1_in.buttons;
    if (fm1_led_anim_on())                               /* the power-on sweep still runs (hal/fm1_led_anim.h) */
        return;
    memset(nl, 0, sizeof nl);
    {   /* the ARP or KARMA on (PLAY) with nothing to play: the keys flash on each beat, waiting for notes */
        uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm);
        int wait = (sys.arp_on || sys.k_on) && !((sys.arp_on && arp.n) || (sys.k_on && kar.n));
        if (wait && fm1_ms % beat < beat / 6u)
            notes = 0xFFFFFFFFu;
    }
    if (fm1_in.buttons & btn_bit(B_HOME)) {     /* HOME held: OP1, OP2 (the part edited: lit; the other: blinking,
                                                  * MULTI on), MONO, POLY (the part's: lit; the other: blinking) */
        uint32_t bl = fm1_ms / 250u & 1u, mo = part_mono[sys.multi ? part_edit : 0u];
        notes = 0;
        if (!part_edit || bl)
            notes |= 1u << KEY_OP1;
        if (sys.multi && (part_edit || bl))
            notes |= 1u << KEY_OP2;
        if (mo || bl)
            notes |= 1u << KEY_MONO;
        if (!mo || bl)
            notes |= 1u << KEY_POLY;
    } else if (sys.page == 0u && ui_hot < 4u && fm1_ms - ui_hot_ms < 800u) {   /* an oscillator knob turned: the keys a bar of
                                                  * its value, low to high */
        uint32_t nk = 1u + (uint32_t)us_kn(ui_hot) * (NKEYS - 1u) / 127u;
        notes |= nk >= 32u ? 0xFFFFFFFFu : (1u << nk) - 1u;
    }
    for (i = 0; i < NKEYS; i++)
        if ((notes >> i) & 1u)
            led_on(nl, 14u + i);
    for (i = 0; i < 14u; i++)
        if ((btns >> i) & 1u)
            led_on(nl, i);
    if (sys.octave < 0 && sys.page != 12u && sys.page != 13u)   /* (TAPE: they blink, BOUNCE) */
        led_on(nl, panel.btn[B_OCTDN]);
    if (sys.octave > 0 && sys.page != 12u && sys.page != 13u)
        led_on(nl, panel.btn[B_OCTUP]);
    if (sys.page == 0u || sys.page == 14u)
        led_on(nl, panel.btn[B_HOME]);
    if ((sys.env_stage != 0u && sys.env_view > 48u) || synth_out_lvl > 160u)   /* ENV: lit while the envelope is open,
                                                  * as the ENV page's lamp glows, and on while the sound rings out (an
                                                  * FX's or the reverb's tail: the output over ~-46 dB) */
        led_on(nl, panel.btn[B_ENV]);
    {   /* the next to start blinks in the tempo: at first SEQ; KARMA playing, the ARP (LOGIC: the two together); the ARP
         * playing, SEQ; both playing: both lit */
        uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm);
        int on = fm1_ms % beat < beat / 2u;
        int inv_seq = !sys.k_on && (!sys.arp_on || sys.k_logic), inv_arp = sys.k_on && !sys.arp_on && sys.k_logic;
        if (inv_arp ? on : sys.arp_on || sys.page == 6u || sys.page == 18u)
            led_on(nl, panel.btn[B_ARP]);
        if (inv_seq ? on : sys.k_on || sys.page == 9u || sys.page == 17u)
            led_on(nl, panel.btn[B_SEQ]);
    }
    if (sys.page == 7u || sys.page == 15u || sys.page == 16u || sys.page == 19u || sys.multi)   /* (lit: MULTI on too) */
        led_on(nl, panel.btn[B_GLO]);
    {   /* SCL (BASIS, ORBIT) while in use: a blink at each note (on its page: lit, dark a moment at each note); LFO
         * follows FREE EG: lit while its curve is over the middle, as the ENV light follows the envelope */
        static uint32_t ons_seen, on_ms;
        uint8_t fp = feg.pos, op = fegs[feg_ed ^ 1u].pos;
        int scl = (sys.basis & 127u) || sys.orbit, blink;
        int lfo = (sys.feg_on && sys.feg_has && fp < FEG_N && feg.curve[fp] > 64u) ||   /* (either FREE EG) */
                  (feg_po.on && feg_po.has && op < FEG_N && fegs[feg_ed ^ 1u].curve[op] > 64u);
        if (synth_ons != ons_seen) {
            ons_seen = synth_ons;
            on_ms = fm1_ms;
        }
        blink = fm1_ms - on_ms < 90u;
        if (sys.page == 10u ? !(scl && blink) : scl && blink)
            led_on(nl, panel.btn[B_SCL]);
        if (lfo || (sys.page == 8u && !(sys.feg_on && sys.feg_has)))   /* (its page, FREE EG not playing: lit) */
            led_on(nl, panel.btn[B_LFO]);
    }
    if (sys.page == 11u)
        led_on(nl, panel.btn[B_EDIT]);
    {   /* the blinking, all in the tempo (the BPM, or the MIDI clock's): on for the first half of each beat; REC
         * recording: twice as fast */
        uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm), ph = fm1_ms % beat;
        int on = ph < beat / 2u, on2 = ph % (beat / 2u) < beat / 4u;
        if ((sys.arp_on || sys.k_on) && on) {    /* PLAY (and its green): the ARP or KARMA on */
            led_on(nl, panel.btn[B_PLAY]);
            nl[LED_PLAY_GREEN_COL] |= (uint8_t)(1u << LED_PLAY_GREEN_ROW);
        } else if (!sys.arp_on && !sys.k_on && on &&   /* (stopped, on the ARP's or KARMA's pages: red, ready) */
                   (sys.page == 6u || sys.page == 18u || sys.page == 9u || sys.page == 17u))
            led_on(nl, panel.btn[B_PLAY]);
        if (sys.page == 12u || sys.page == 13u || ((tape.rec || feg.rec) && on2) || (sys.page == 8u && !feg.rec && on) ||
            (tape.playing && !tape.rec && on))   /* REC: the TAPE's pages; blinking: recording, the tape running, FREE EG's page (REC records there) */
            led_on(nl, panel.btn[B_REC]);
        if (fm1_ms % (4u * beat) < beat / 2u)    /* SAVE: always, once a bar (4/4: on its first beat) */
            led_on(nl, panel.btn[B_SAVE]);
        if ((sys.page == 12u || sys.page == 13u) && on) {   /* TAPE: both OCTs (press both: BOUNCE) */
            led_on(nl, panel.btn[B_OCTDN]);
            led_on(nl, panel.btn[B_OCTUP]);
        }
    }
    if ((sys.page >= 2u && sys.page <= 3u) || sys.page == 20u || sys.fx_mix || sys.sh_mix)   /* (FX lit: its pages, or FX on) */
        led_on(nl, panel.btn[B_FX]);
    for (i = 0; i < FM1_NCOL; i++)
        fm1_led[i] = nl[i];
}

static void ui_leds_init(void)                         /* the idle glow under every LED */
{
    uint8_t nl[FM1_NCOL];
    uint32_t i;
    memset(nl, 0, sizeof nl);
    for (i = 0; i < 14u + NKEYS; i++)
        led_on(nl, i);
    for (i = 0; i < FM1_NCOL; i++)
        fm1_led_dim[i] = nl[i];
}

/* ------------------------------------------------------------- input --- */
/* a knob turned on (the same way within 40 ms) speeds up to x4: 0 .. 127 in a flick, one detent still one step
 * (after M-VAVE's fm1_open, app/screen.c read_encoders); every knob and encoder (ui_enc), lists too */
static uint32_t enc_t[NE];
static int8_t enc_dir[NE];
static uint8_t enc_speed[NE];
static int32_t knob_accel(uint32_t role, int32_t s)
{
    int8_t d = (int8_t)(s > 0 ? 1 : -1);
    enc_speed[role] = (uint8_t)(d == enc_dir[role] && fm1_ms - enc_t[role] <= 40u ?
                                (enc_speed[role] < 4u ? enc_speed[role] + 1u : 4u) : 1u);
    enc_dir[role] = d;
    enc_t[role] = fm1_ms;
    return s * enc_speed[role];
}

static int32_t ui_enc(uint32_t role)            /* every knob and encoder: detents, accelerated */
{
    int32_t s = panel_enc(role);
    return s ? knob_accel(role, s) : 0;
}

/* a knob that picks among a few things (the VOICE's size, SYMP's chord, the ACCORDION's register, the BAGPIPE's key,
 * the CLUSTER's voices, KARMA's octave, a delay's TIME, MOUTH's vowel, MOD FX's note value, ratio and rectifier):
 * which one a value means (-1: a range, not choices) */
static int32_t knob_choice(volatile uint8_t *p, int32_t v)
{
    uint32_t i;
    if (p == &sys.fx_struct && sys.fx_model == RZ_VOICE)
        return v * (int32_t)WV_COUNT / 128;
    if (p == &sys.fx_pos && sys.fx_model == RZ_ACCORD)
        return v * 5 / 128;
    if (p == &sys.fx_pos && sys.fx_model == RZ_BAGPIPE)
        return v * 12 / 128;
    if (p == &sys.np_y && sys.osc == 11u)        /* (CLUSTER SAW: noise.c's 2 + y x 14 / 32767 voices) */
        return v * 258 * 14 / 32767;
    if (p == &sys.fx_struct && sys.fx_model == RZ_SYMP)   /* (SYMP STRING: the chord) */
        return v >> 4;
    if (p == &sys.sh_size && SS_IS_DELAY(sys.sh_space % SS_COUNT))   /* (a delay's TIME: a note value) */
        return v * (int32_t)SH_DTIMES / 128;
    if (p == &sys.sh_shimmer && sys.sh_space % SS_COUNT == SS_MOUTH) {   /* (MOUTH: the vowel along its path) */
        uint32_t nv = SH_MPATH[sys.sh_interval % SI_COUNT][0];
        return ((v * (int32_t)(nv - 1u)) + 63) / 127;
    }
    if (p == &sys.mfx_rate)                      /* (MOD FX: the note value, or RING's ratio) */
        return MF_KEYED(mf.on ? mf.mode : sys.mfx_mode % MF_COUNT) ? v * MF_NRATIO / 128 : v * MF_NDIV / 128;
    if (p == &sys.mfx_rect)                      /* (MOD FX: the rectifier) */
        return (int32_t)mf_rect_of((uint32_t)v);
    if (sys.page == 9u)
        for (i = 0; i < 4u; i++)
            if (p == &sys.k_m[i] && k_ge()->knob[i] == MC_TRANSPOSE)
                return (v - 64) / 32;
    return -1;
}
/* a turn of such a knob: one detent, the next choice (not a range's ten detents a choice) */
static int knob_step(volatile uint8_t *p, int32_t s)
{
    int32_t v = *p & 127, c = knob_choice(p, v), d = s > 0 ? 1 : -1;
    if (c < 0)
        return 0;
    while (v + d >= 0 && v + d <= 127) {
        v += d;
        if (knob_choice(p, v) != c)
            break;
    }
    *p = (uint8_t)v;
    return 1;
}

static void knob(volatile uint8_t *p, uint32_t role, uint32_t cell)
{
    int32_t s = ui_enc(role);
    if (!s)
        return;
    if (!knob_step(p, s))
        *p = (uint8_t)clamp(*p + s, 0, 127);
    if (cell < 4u) {
        ui_hot = (uint8_t)cell;
        ui_hot_ms = fm1_ms;
    }
}

static int8_t oct_was;                           /* the octave before the OCTs went down (BOUNCE gives it back) */
static uint8_t oct_both;                         /* OCT- and OCT+ both held: 1, 2 (a BOUNCE made) */
static uint8_t play_last;                        /* PLAY outside the ARP / KARMA pages: 0 the ARP, 1 KARMA (the last opened) */
/* AUTO SAVE: the state kept, so switching the FM-1 off (its switch: no warning) loses nothing: 3 s after the last
 * change the settings saved (the preset, the knobs, the pages' settings; power-on applies its own rules on them as
 * ever: PLAY off ...). The flash write stops the CPU for a moment (a hiccup in the sound, the LEDs flashing): only
 * once all is quiet, the ARP, KARMA and the TAPE not playing and no note started for 2 s */
static void preset_keep(void)
{
    static settings_t seen;
    static uint8_t started, tick;
    static uint32_t since, ons, ons_t;
    settings_t now;
    if (!flash_ok)
        return;
    if (!started) {                              /* (the first call: the state booted counts as saved) */
        started = 1;
        settings_fill(&set_saved);
        seen = set_saved;
        return;
    }
    if (synth_ons != ons) {
        ons = synth_ons;
        ons_t = fm1_ms;
    }
    if ((sys.arp_on && arp.n) || (sys.k_on && (kar.running || kar.n)) || tape.playing || tape.rec)
        ons_t = fm1_ms;
    if (++tick & 7u)                             /* (the state looked at every eighth frame) */
        return;
    settings_fill(&now);
    if (!memcmp(&now, &set_saved, sizeof now))
        return;
    if (memcmp(&now, &seen, sizeof now)) {       /* (still changing: from this change) */
        seen = now;
        since = fm1_ms;
    }
    if (fm1_ms - since > 3000u && fm1_ms - ons_t > 2000u)
        settings_save();
}
/* the pages' own presets (PRESETS on ENV, MOD ENV, FREE EG, SCL): starting points for that page's knobs only */
typedef struct { const char *name; uint8_t a, d, s, r; } env_pre_t;
static const env_pre_t ENV_PRE[] = {
    {"PLUCK", 0, 56, 0, 50},      {"PIANO", 0, 80, 20, 60},     {"ORGAN", 0, 40, 127, 30},   {"PAD", 92, 100, 115, 104},
    {"STRINGS", 62, 90, 118, 80}, {"BRASS", 40, 70, 100, 60},   {"PERC", 0, 48, 0, 40},      {"STAB", 0, 66, 40, 44},
    {"ACID", 0, 72, 24, 40},      {"SWELL", 110, 100, 127, 110}, {"BELL", 0, 100, 0, 104},   {"GATE", 0, 30, 127, 20},
    {"REVERSE", 104, 30, 0, 30},  {"LONG TAIL", 20, 90, 80, 118},
};
typedef struct { const char *name; uint8_t len, skew, height, bend; } menv_pre_t;   /* (HEIGHT bit 7: down) */
static const menv_pre_t MENV_PRE[] = {
    {"OFF", 64, 0, 0, 64},           {"DIVE UP", 84, 0, 216, 96},   {"ZAP", 62, 0, 110, 100},     {"KICK DROP", 56, 0, 90, 100},
    {"BLIP", 40, 0, 64, 64},         {"OCTAVE CHIRP", 50, 0, 88, 64}, {"RISE", 90, 127, 60, 64},  {"SLOW SWEEP", 100, 127, 80, 40},
    {"FALL", 96, 127, 200, 64},      {"SCOOP", 70, 0, 180, 110},
};
typedef struct { const char *name; uint8_t basis, orbit; } scl_pre_t;
static const scl_pre_t SCL_PRE[] = {
    {"MONO", 0, 0}, {"NARROW", 30, 0}, {"WIDE", 90, 0}, {"FULL", 127, 0}, {"SUBTLE DRIFT", 40, 15}, {"DRIFT", 60, 30},
    {"SWIRL", 100, 80}, {"SPACE", 127, 127},
};
enum { FS_SINE, FS_TRI, FS_UP, FS_DOWN, FS_SQUARE, FS_SH, FS_SWELL, FS_LATE, FS_HIT, FS_DIP, FS_SNAP, FS_FLAM, FS_LONG,
       FS_SCOOP };
/* (LATE: still for an eighth, up by a quarter, then held: NOTE ~0.7 s, ~1.5 s. The attacks (HIT .. SCOOP: NOTE, around the
 * parameter where it is, HI how far; a point ~46 ms): HIT up at the note's start, a third of it ~46 ms on, gone by
 * ~140 ms; DIP the same down; SNAP up and gone by ~46 ms; FLAM struck twice, ~90 ms apart; LONG up, dying away over
 * ~280 ms; SCOOP down, coming back over ~280 ms) */
static const uint8_t FS_PROF[6][8] = {          /* (the attacks: sixths of HI at each point, then 0; [7] the points) */
    {6, 2, 1, 0, 0, 0, 0, 4}, {6, 2, 1, 0, 0, 0, 0, 4}, {6, 0, 0, 0, 0, 0, 0, 2},
    {6, 1, 4, 1, 0, 0, 0, 5}, {6, 5, 4, 3, 2, 1, 0, 7}, {6, 5, 4, 3, 2, 1, 0, 7},
};
#define FS_ATTACK(s) ((s) >= FS_HIT && (s) <= FS_SCOOP)
typedef struct { const char *name; uint8_t shape, lo, hi, len, target; } feg_pre_t;
static const feg_pre_t FEG_PRE[] = {
    {"OFF", 0, 0, 0, 0, 0},
    {"RING SWEEP", FS_SINE, 30, 120, 15, 3},       /* (LENGTH: 15 LFO 0.25 HZ; TARGET 3 RING) */
    {"WOBBLE 1/8", FS_DOWN, 20, 127, 2, 3},        /* (2 STEPS) */
    {"SQUARE TRILL", FS_SQUARE, 30, 120, 1, 3},
    {"BAR RISE", FS_UP, 20, 120, 9, 3},            /* (1 BAR) */
    {"AUTO PAN", FS_TRI, 0, 127, 16, 18},          /* (0.5 HZ, BASIS) */
    {"ORBIT DRIFT", FS_SINE, 0, 90, 14, 19},       /* (0.1 HZ, ORBIT) */
    {"S&H CHAOS", FS_SH, 40, 100, 9, 0},           /* (CHAOS) */
    {"BRIGHT LFO", FS_SINE, 30, 110, 18, 9},       /* (2 HZ, FX BRIGHTNESS) */
    {"NOISE SCAN", FS_TRI, 10, 120, 10, 4},        /* (2 BARS, NOISE X) */
    {"GATE PUMP", FS_UP, 20, 127, 4, 20},          /* (4 STEPS, ARP GATE) */
    {"SHIMMER SWELL", FS_SWELL, 0, 110, 0, 16},    /* (NOTE, SHIMMER MIX) */
    {"SCREAM RISE", FS_UP, 0, 127, 9, 27},         /* (SCREAM) */
    {"TALK", FS_SINE, 0, 127, 17, 15},             /* (1 HZ, SHIMMER: MOUTH's vowel) */
    {"YOI YOI", FS_TRI, 10, 120, 2, 15},           /* (2 STEPS, the vowel) */
    /* MOD FX's VIBRATO and RING MOD (their rate, ratio, slew, rectify: the MOD FX page's) */
    {"DELAYED VIB", FS_LATE, 0, 70, 0, 28},        /* (NOTE, VIBRATO: in after a moment, as a player's) */
    {"VIB SWELL BAR", FS_UP, 0, 90, 9, 28},        /* (1 BAR) */
    {"VIB WAVES", FS_SINE, 0, 80, 15, 28},         /* (0.25 HZ) */
    {"RING BLOOM", FS_SWELL, 0, 110, 0, 29},       /* (NOTE, RING MOD opening) */
    {"RING GATE 1/8", FS_SQUARE, 0, 120, 2, 29},   /* (2 STEPS) */
    {"RING DRIFT", FS_SINE, 0, 100, 16, 29},       /* (0.5 HZ) */
    {"RING S&H", FS_SH, 0, 120, 10, 29},           /* (2 BARS) */
    /* at audio rate (AM, FM: the note's level or pitch moved by the drawing, x the note) */
    {"AM X1/2 GROWL", FS_SINE, 20, 127, 23, 0},    /* (AM X1/2) */
    {"AM X2 RING", FS_SQUARE, 0, 127, 25, 0},
    {"FM X1 WARM", FS_SINE, 50, 78, 29, 0},        /* (FM X1: a little) */
    {"FM X2 BELL", FS_SINE, 34, 94, 30, 0},
    {"FM X3 METAL", FS_TRI, 24, 104, 31, 0},
    /* attacks (each note: a burst around where the parameter is) */
    {"HIT CHAOS", FS_DIP, 0, 40, 0, 0},            /* (CHAOS down: the cycle's thump) */
    {"HIT NOISE Y", FS_HIT, 0, 45, 0, 5},          /* (the exciter struck harder: pick, mallet) */
    {"HIT LEVEL", FS_HIT, 0, 30, 0, 6},            /* (NOISE LEVEL: a click) */
    {"HIT BRIGHT", FS_HIT, 0, 50, 0, 9},           /* (FX BRIGHTNESS) */
    {"HIT RING MOD", FS_HIT, 0, 110, 0, 29},       /* (a clang of RING MOD on the attack) */
    {"HIT SCREAM", FS_HIT, 0, 90, 0, 27},          /* (the waveguides' breath: a bark) */
    {"SNAP SHAPE", FS_SNAP, 0, 40, 0, 1},          /* (the circuit's SHAPE flicked: a click of tone) */
    {"SNAP OUTPUT", FS_SNAP, 0, 45, 0, 2},         /* (OUTPUT: the circuit's other voice, a crack) */
    {"SNAP NOISE X", FS_SNAP, 0, 50, 0, 4},        /* (a noise program's X: its spread, detune, rate) */
    {"FLAM NOISE Y", FS_FLAM, 0, 45, 0, 5},        /* (struck twice: a flam, a double pluck) */
    {"FLAM LEVEL", FS_FLAM, 0, 30, 0, 6},
    {"FLAM RING MOD", FS_FLAM, 0, 100, 0, 29},
    {"THUMP CHAOS", FS_SCOOP, 0, 45, 0, 0},        /* (CHAOS down, coming back: a long thump) */
    {"BLOOM RING", FS_LONG, 0, 50, 0, 3},          /* (RING: the gate's resonance opening on the attack) */
    {"BITE DAMP", FS_HIT, 0, 40, 0, 10},           /* (FX DAMPING: a bitten attack, then the ring) */
    {"STRIKE POS", FS_HIT, 0, 40, 0, 11},          /* (FX POSITION: where it is struck, moved) */
    {"BODY STRUCT", FS_LONG, 0, 35, 0, 8},         /* (FX STRUCTURE: the body moving as it rings in) */
    {"SPLASH", FS_LONG, 0, 60, 0, 16},             /* (SHIMMER MIX: a splash of the space on each attack) */
    {"VIB KICK", FS_HIT, 0, 80, 0, 28},            /* (VIBRATO: the pitch wobbles on the attack) */
    {"SCREAM BLOOM", FS_LONG, 0, 70, 0, 27},       /* (SCREAM dying away: a growl on the attack) */
};
#define NPAGE_PRE(t) ((int32_t)(sizeof t / sizeof t[0]))
static void page_preset(int32_t s)
{
    static uint8_t pe = 0xFF, pm = 0xFF, pfs[2] = {0xFF, 0xFF}, ps = 0xFF;   /* (none chosen yet: the first turn right the
                                                                              * first; FREE EG 1, 2: each its own place) */
    char t[28];
    const char *nm;
    s = s > 0 ? 1 : -1;
    if (sys.page == 1u) {
        const env_pre_t *e;
        pe = (uint8_t)(pe == 0xFFu ? (s > 0 ? 0 : NPAGE_PRE(ENV_PRE) - 1) : (pe + NPAGE_PRE(ENV_PRE) * 8 + s) % NPAGE_PRE(ENV_PRE));
        e = &ENV_PRE[pe];
        sys.attack = e->a; sys.decay = e->d; sys.sustain = e->s; sys.release = e->r;
        str_cpy(t, "ENV: ", sizeof t);
        nm = e->name;
    } else if (sys.page == 5u) {
        const menv_pre_t *m;
        pm = (uint8_t)(pm == 0xFFu ? (s > 0 ? 0 : NPAGE_PRE(MENV_PRE) - 1) : (pm + NPAGE_PRE(MENV_PRE) * 8 + s) % NPAGE_PRE(MENV_PRE));
        m = &MENV_PRE[pm];
        sys.menv_len = m->len; sys.menv_skew = m->skew; sys.menv_height = m->height; sys.menv_bend = m->bend;
        str_cpy(t, "MOD ENV: ", sizeof t);
        nm = m->name;
    } else if (sys.page == 10u) {
        ps = (uint8_t)(ps == 0xFFu ? (s > 0 ? 0 : NPAGE_PRE(SCL_PRE) - 1) : (ps + NPAGE_PRE(SCL_PRE) * 8 + s) % NPAGE_PRE(SCL_PRE));
        sys.basis = SCL_PRE[ps].basis; sys.orbit = SCL_PRE[ps].orbit;
        str_cpy(t, "PAN: ", sizeof t);
        nm = SCL_PRE[ps].name;
    } else {
        const feg_pre_t *f;
        uint32_t k;
        uint8_t pf = pfs[feg_ed & 1u];
        pf = (uint8_t)(pf == 0xFFu ? (s > 0 ? 0 : NPAGE_PRE(FEG_PRE) - 1) : (pf + NPAGE_PRE(FEG_PRE) * 8 + s) % NPAGE_PRE(FEG_PRE));
        pfs[feg_ed & 1u] = pf;
        f = &FEG_PRE[pf];
        if (!pf)
            sys.feg_on = 0;
        else {
            uint32_t rng = 0x2545F491u, sh = 0;
            uint8_t c[FEG_N];
            for (k = 0; k < FEG_N; k++) {        /* (the shape, 0 .. 1024 over the cycle) */
                int32_t ph = (int32_t)k * 1024 / (int32_t)FEG_N, u;
                switch (f->shape) {
                case FS_SINE: u = 512 + (px_sin((uint32_t)k << 25) >> 21); break;   /* (Q30 -> +-512) */
                case FS_TRI: u = ph < 512 ? ph * 2 : (1024 - ph) * 2; break;
                case FS_UP: u = ph; break;
                case FS_DOWN: u = 1024 - ph; break;
                case FS_SQUARE: u = ph < 512 ? 1024 : 0; break;
                case FS_SH:
                    if (!(k & 7u)) { rng = rng * 1664525u + 1013904223u; sh = rng >> 22; }
                    u = (int32_t)sh; break;
                case FS_LATE: u = ph < 128 ? 0 : ph < 256 ? (ph - 128) * 8 : 1024; break;
                case FS_HIT:
                case FS_DIP:
                case FS_SNAP:
                case FS_FLAM:
                case FS_LONG:
                case FS_SCOOP: {                 /* (around the parameter's base: what FREE EG moves it from) */
                    volatile uint8_t *fl = feg_field(f->target);
                    int32_t base = fegs[0].playing && feg_field(fegs[0].target) == fl ? fegs[0].base :
                                   fegs[1].playing && feg_field(fegs[1].target) == fl ? fegs[1].base : *fl;
                    const uint8_t *pr = FS_PROF[f->shape - FS_HIT];
                    int32_t o = k < 7u ? f->hi * pr[k] / 6 : 0;
                    u = base + (f->shape == FS_DIP || f->shape == FS_SCOOP ? -o : o);
                    c[k] = (uint8_t)(u < 0 ? 0 : u > 127 ? 127 : u);
                    continue;
                }
                default: u = ph < 768 ? ph * 4 / 3 : 1024; break;   /* (SWELL: up over three quarters, then held) */
                }
                u = u < 0 ? 0 : u > 1024 ? 1024 : u;
                c[k] = (uint8_t)(f->lo + ((int32_t)(f->hi - f->lo) * u >> 10));
            }
            {
                uint32_t q = irq_save();
                for (k = 0; k < FEG_N; k++)
                    feg.curve[k] = c[k];
                sys.feg_npts = (uint8_t)(FS_ATTACK(f->shape) ? FS_PROF[f->shape - FS_HIT][7] : 127u);
                sys.feg_len = f->len;
                sys.feg_target = f->target;
                sys.feg_depth = 127;
                sys.feg_has = 1;
                sys.feg_on = 1;
                if (!feg_ed)
                    eg_from_preset = 0;          /* (chosen here: kept over the sound presets, as a recording) */
                irq_restore(q);
            }
        }
        str_cpy(t, feg_ed ? "FREE EG 2: " : "FREE EG 1: ", sizeof t);
        nm = f->name;
    }
    ui_cat(t, sizeof t, nm);
    ui_say(t);
    ui_force = 1;
}
/* the ARP's presets (PRESETS on the ARP and GROOVE pages): a pattern, its note value, gate and swing, and its GROOVE
 * (velocity and gate seqs); the BPM stays (KNOB 1) */
typedef struct { const char *name; uint8_t type, div, gate, swing, vp, gp; } arp_preset_t;
static const arp_preset_t ARP_PRE[] = {
    {"CLASSIC UP", 1, 7, 70, 0, 0, 0},           {"TRANCE 16THS", 1, 7, 45, 0, 1, 0},
    {"ACID BASS", 26, 7, 60, 10, 8, 1},          {"HOUSE STABS", 19, 7, 35, 15, 3, 10},
    {"TECHNO DRIVE", 15, 7, 50, 0, 9, 0},        {"SWING UPDOWN", 7, 7, 65, 55, 13, 4},
    {"HARP RUN", 45, 9, 90, 0, 4, 12},           {"TRIPLET DREAM", 8, 6, 80, 0, 2, 14},
    {"RANDOM SPARKLE", 14, 7, 30, 0, 23, 13},    {"WALKING BASS", 35, 4, 75, 40, 0, 5},
    {"REGGAE SKANK", 43, 7, 40, 20, 13, 10},     {"STRUMMER", 37, 7, 85, 10, 5, 0},
    {"CLAVE", 40, 7, 55, 25, 17, 6},             {"WALTZ", 41, 4, 70, 0, 7, 14},
    {"BOUNCING BALL", 47, 7, 60, 0, 5, 8},       {"SLOW FALL", 4, 1, 95, 0, 10, 11},
};
#define ARP_NPRE (sizeof ARP_PRE / sizeof ARP_PRE[0])
/* KARMA's presets (PRESETS on the KARMA and LOGIC pages): a GE and its four realtime knobs (a TRANSPOSE knob stays
 * put) */
typedef struct { const char *name; uint8_t ge, m[4]; } seq_preset_t;
static const seq_preset_t SEQ_PRE[] = {
    {"PULSE ARP", 0, {64, 64, 64, 64}},          {"ACID LINE", 6, {90, 100, 40, 90}},
    {"TRANCE GATE", 9, {100, 50, 64, 80}},       {"GAMELAN", 13, {70, 90, 50, 60}},
    {"BOSSA NOVA", 36, {64, 50, 64, 50}},        {"MONTUNO", 39, {80, 64, 40, 70}},
    {"FUNK CLAV", 15, {90, 40, 64, 90}},         {"POLY 3 OVER 4", 16, {64, 80, 64, 64}},
    {"RAIN DROPS", 10, {40, 110, 30, 40}},       {"CHAOS MACHINE", 18, {110, 110, 90, 100}},
    {"ECHO CANYON", 14, {50, 64, 110, 50}},      {"DUB CHORDS", 40, {50, 40, 90, 60}},
    {"HOUSE PIANO", 34, {80, 50, 50, 80}},       {"AFROBEAT", 44, {90, 64, 64, 80}},
    {"SWELL PAD", 17, {30, 64, 64, 20}},         {"BIRD CALLS", 21, {60, 120, 70, 60}},
};
#define SEQ_NPRE (sizeof SEQ_PRE / sizeof SEQ_PRE[0])
static uint8_t arp_pre = 0xFF, seq_pre = 0xFF, mfx_pre = 0xFF;   /* (none chosen yet: the first turn right is the first) */

static void ui_input(void)
{
    kb_home = (fm1_in.buttons & btn_bit(B_HOME)) != 0u;   /* (keys.c: HOME held, its black keys not notes) */
    if (!sys.multi && part_edit)                 /* (MULTI off: the knobs on A) */
        part_edit_set(0);
    if (home_key_req) {                          /* HOME held + OP1, OP2, MONO, POLY */
        uint32_t k = home_key_req - 1u;
        home_key_req = 0;
        if (k == KEY_OP1 || k == KEY_OP2) {
            if (k == KEY_OP2 && !sys.multi)
                ui_say("PART 2: MULTI OFF (GLO)");
            else {
                part_edit_set(k == KEY_OP2);
                ui_say(part_edit ? "EDIT PART 2" : "EDIT PART 1");
            }
        } else {
            uint32_t pp = sys.multi ? part_edit : 0u;
            part_mono[pp] = (uint8_t)(k == KEY_MONO);
            mono_n[pp] = 0;
            ui_say(part_mono[pp] ? "MONO" : "POLY");
        }
        ui_force = 1;
    }
    preset_keep();
    uint32_t pressed = fm1_input_edges(0) | rm_btn, b;
    int32_t s;
    char t[24], v[12];
    rm_btn = 0;
    if (keys_prog_req_b) {                              /* MULTI: a program change on B's channel: B's sound */
        uint32_t pg = keys_prog_req_b - 1u;
        keys_prog_req_b = 0;
        part_b_load(pg);
        str_cpy(t, "B: ", sizeof t);
        ui_cat(t, sizeof t, PRESETS[part_preset(1) % NPRESETS].name);
        ui_say(t);
    }
    if (keys_prog_req) {                                /* MIDI program change: that preset */
        uint32_t pg = keys_prog_req - 1u;               /* (bank x 128 + program) */
        keys_prog_req = 0;
        preset_load(pg);
        str_cpy(t, PRESETS[sys.preset % NPRESETS].name, sizeof t);
        ui_say(t);
    }
    {   /* TAPE: OCT- and OCT+ both down (in any order, however close) = BOUNCE, once a hold; the octave the first of
         * them moved goes back; said when they are let go (the screen waits while both are held, main.c) */
        uint32_t both = btn_bit(B_OCTDN) | btn_bit(B_OCTUP), held = fm1_in.buttons & both;
        if (held == both) {
            if (!oct_both && (sys.page == 12u || sys.page == 13u)) {
                oct_both = 2;
                tape.req = 4;
                sys.octave = oct_was;
            } else if (!oct_both)
                oct_both = 1;
        } else if (!held) {
            if (oct_both == 2u)
                ui_say("BOUNCE: ALL ON TRACK 1");
            if (oct_both)
                ui_force = 1;
            oct_both = 0;
            oct_was = sys.octave;                       /* (none held: the octave a first press would leave) */
        }
    }
    for (b = 0; b < NB; b++) {
        if (!(pressed & btn_bit(b)))
            continue;
        switch (b) {
        case B_OCTDN:
        case B_OCTUP:
            if ((fm1_in.buttons & (btn_bit(B_OCTDN) | btn_bit(B_OCTUP))) == (btn_bit(B_OCTDN) | btn_bit(B_OCTUP)))
                break;                                  /* both: BOUNCE on the TAPE pages (below), the UBOOT combo
                                                         * (main.c), not an octave */
            sys.octave = (int8_t)clamp(sys.octave + (b == B_OCTUP ? 1 : -1), OCT_MIN, OCT_MAX);
            break;
        case B_SAVE: {                                  /* the settings, and the sound over the preset playing */
            int e = settings_save();
            e |= flash_ok ? upre_save(sys.preset) : 1;
            ui_say(!e ? "PRESET SAVED" : flash_ok ? "SAVE FAILED" : "NO FLASH");
            break;
        }
        case B_HOME:                                    /* the oscillator's page; there: the BANK menu, again: back */
            sys.page = sys.page == 0u ? 14u : 0u;
            ui_force = 1;
            break;
        case B_ENV:                                     /* the ENV page, the MOD ENV page, back HOME */
            sys.page = sys.page == 1u ? 5u : sys.page == 5u ? 0u : 1u;
            ui_force = 1;
            break;
        case B_FX:                                      /* MOD FX, the resonator, the shimmer, back HOME */
            sys.page = sys.page == 20u ? 2u : sys.page == 2u ? 3u : sys.page == 3u ? 0u : 20u;   /* (MOD FX, FX, SHIMMER: the
                                                                                   * signal's order) */
            ui_force = 1;
            break;
        case B_REC:                                     /* the TAPE page; there (and on FREE EG's): record */
            if (sys.page == 12u || sys.page == 13u) {   /* TAPE: record the armed track (PLAY held: erase it) */
                int er = (fm1_in.buttons & btn_bit(B_PLAY)) != 0u;
                tape.req = er ? 3u : 2u;
                str_cpy(t, er ? "ERASED TRACK " : tape.rec ? "REC STOP TRACK " : "REC TRACK ", sizeof t);
                fmt_int(v, (int32_t)(sys.tape_arm % 4u) + 1);
                ui_cat(t, sizeof t, v);
                ui_say(t);
            } else if (sys.page == 8u) {
                feg_req = feg.rec ? 2u : 1u;            /* (done by the audio ISR, at its next block) */
                ui_say(feg.rec ? "RECORDED" : "RECORDING");
            } else {
                sys.page = 12;
                ui_force = 1;
            }
            break;
        case B_ARP:                                     /* the ARP page, the GROOVE page, back HOME; PLAY starts it */
            if (sys.k_on && !sys.arp_on && sys.page != 6u && sys.page != 18u) {   /* (KARMA playing: the ARP joins it
                                                         * at once, no PLAY needed, as LOGIC's PLAY does) */
                if (!sys.k_logic)
                    sys.k_logic = 1;
                sys.arp_on = 1;
                ui_say("ARP + KARMA ON");
            }
            if (sys.page != 6u && sys.page != 18u && !sys.arp_on) {   /* (opened: it plays at once, AUTOPLAY: no notes
                                                         * chosen yet: C major, latched) */
                sys.arp_on = 1;
                rt_auto = 3;
                ui_say(sys.k_on ? "ARP + KARMA ON" : "ARP ON");
            }
            sys.page = sys.page == 6u ? 18u : sys.page == 18u ? 0u : 6u;   /* (the ARP, its GROOVE, back) */
            play_last = 0;
            ui_force = 1;
            break;
        case B_SEQ:                                     /* the KARMA page, the LOGIC page, back HOME; PLAY starts it */
            if (sys.arp_on && !sys.k_on && sys.page != 9u && sys.page != 17u) {   /* (the ARP playing: KARMA joins it
                                                         * at once, no PLAY needed, as LOGIC's PLAY does) */
                if (!sys.k_logic)
                    sys.k_logic = 1;
                sys.k_on = 1;
                ui_say("ARP + KARMA ON");
            }
            if (sys.page != 9u && sys.page != 17u && !sys.k_on) {   /* (opened: it plays at once, as the ARP) */
                sys.k_on = 1;
                rt_auto = 3;
                ui_say(sys.arp_on ? "ARP + KARMA ON" : "KARMA ON");
            }
            sys.page = sys.page == 9u ? 17u : sys.page == 17u ? 0u : 9u;   /* (KARMA, LOGIC, back) */
            play_last = 1;
            ui_force = 1;
            break;
        case B_PLAY:                                    /* start / stop: the ARP or KARMA (the page's, else the last
                                                         * one opened); what runs stops (the other one off) */
            if (sys.page == 12u || sys.page == 13u) {   /* TAPE: its transport */
                tape.req = 1;
                ui_say(tape.playing ? "TAPE STOP" : "TAPE PLAY");
            } else if (sys.page == 17u) {               /* LOGIC: the two together (with OR / AND / XOR), or stop */
                int both = !(sys.arp_on && sys.k_on);
                if (both && !sys.k_logic)
                    sys.k_logic = 1;
                sys.arp_on = sys.k_on = (uint8_t)both;
                ui_say(both ? "ARP + KARMA ON" : "STOP");
            } else if (sys.page == 6u || sys.page == 18u || (sys.page != 9u && !play_last && !sys.k_on)) {
                sys.arp_on = (uint8_t)!sys.arp_on;
                ui_say(sys.arp_on ? (sys.k_on ? (sys.k_logic ? "ARP ON + KARMA" : "ARP ON, KARMA OFF") : "ARP ON") : "ARP OFF");
            } else if (sys.page == 9u || (play_last && !sys.arp_on)) {
                sys.k_on = (uint8_t)!sys.k_on;
                ui_say(sys.k_on ? (sys.arp_on ? (sys.k_logic ? "KARMA ON + ARP" : "KARMA ON, ARP OFF") : "KARMA ON") : "KARMA OFF");
            } else {                                    /* (elsewhere, the other one running: it stops) */
                sys.arp_on = sys.k_on = 0;
                ui_say("STOP");
            }
            break;
        case B_LFO:                                     /* FREE EG 1, again FREE EG 2, again back (REC records there) */
            if (sys.page != 8u) {
                feg_edit_to(0);
                sys.page = 8;
            } else if (!feg_ed)
                feg_edit_to(1);
            else {
                feg_edit_to(0);
                sys.page = 0;
            }
            ui_force = 1;
            break;
        case B_EDIT:                                    /* the EDIT page (mod wheel, velocity) and back */
            sys.page = sys.page == 11u ? 0u : 11u;
            ui_force = 1;
            break;
        case B_SCL:                                     /* the PAN page (BASIS, ORBIT) and back */
            sys.page = sys.page == 10u ? 0u : 10u;
            ui_force = 1;
            break;
        case B_GLO:                                     /* the MIDI page, the SOUND page, UI STYLE, back */
            sys.page = sys.page == 7u ? 16u : sys.page == 16u ? 19u : sys.page == 19u ? 0u : 7u;   /* (MULTI: on BANK) */
            ui_force = 1;
            break;
        default:
            str_cpy(t, "BUTTON ", sizeof t);
            ui_cat(t, sizeof t, B_NAME[b]);
            ui_cat(t, sizeof t, ": FREE");
            ui_say(t);
            break;
        }
    }
    if (sys.page == 13u) {                       /* TAPE's second menu: the global BPM, the SPEED, the LENGTH */
        if ((s = ui_enc(EN_K3)) != 0) {
            sys.tape_len = (uint8_t)clamp((int32_t)sys.tape_len + (s > 0 ? 1 : -1), 0, 4);
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K1)) != 0) {
            if (arp_ext())
                ui_say("CLOCK EXTERNAL: GLO");
            else
                sys.arp_bpm = (uint8_t)clamp((int32_t)sys.arp_bpm + s, 40, 250);
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.tape_spd = (uint8_t)clamp((int32_t)sys.tape_spd + s, 0, TAPE_NSPD - 1);
            ui_hot = 1; ui_hot_ms = fm1_ms;
            if (tape.spb) {                      /* the tempo the ratio gives: the global BPM follows, the speed exact */
                const uint8_t *q = TAPE_SPD[sys.tape_spd % TAPE_NSPD];
                uint32_t b = (2646000u * q[0] / q[1] * 2u / tape.spb + 1u) / 2u;
                sys.arp_bpm = (uint8_t)(b < 40u ? 40u : b > 250u ? 250u : b);
                tape.lock_q12 = 4096u * q[0] / q[1];
                tape.lock_bpm = sys.arp_bpm;
            } else
                ui_say("TAPE BLANK: RECORD FIRST");
        }
    } else if (sys.page == 12u) {                /* TAPE: a blank track's knob arms it; a recorded one's is its fader */
        static const uint8_t KN[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        uint32_t k;
        for (k = 0; k < 4u; k++) {
            if (tape.has[k] && !(tape.rec && k == sys.tape_arm % 4u))
                knob(&sys.tape_lv[k], KN[k], k);
            else if (ui_enc(KN[k]) != 0 && sys.tape_arm != k && !tape.rec) {
                sys.tape_arm = (uint8_t)k;
                str_cpy(t, "ARMED TRACK ", sizeof t);
                fmt_int(v, (int32_t)k + 1);
                ui_cat(t, sizeof t, v);
                ui_say(t);
            }
        }
    } else if (sys.page == 11u) {
        if ((s = ui_enc(EN_K1)) != 0) {
            sys.mw_target = (uint8_t)clamp((int32_t)sys.mw_target + s, 0, FEG_NT - 1);
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        knob(&sys.mw_depth, EN_K2, 1);
        if ((s = ui_enc(EN_K3)) != 0) {
            sys.vel_to = (uint8_t)clamp((int32_t)sys.vel_to + (s > 0 ? 1 : -1), 0, 3);
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        knob(&sys.vel_amt, EN_K4, 3);
    } else if (sys.page == 10u) {
        knob(&sys.basis, EN_K1, 0);
        knob(&sys.orbit, EN_K2, 1);
    } else if (sys.page == 9u) {
        knob(&sys.k_m[0], EN_K1, 0);
        knob(&sys.k_m[1], EN_K2, 1);
        knob(&sys.k_m[2], EN_K3, 2);
        knob(&sys.k_m[3], EN_K4, 3);
    } else if (sys.page == 8u) {
        knob(&sys.feg_val, EN_K1, 0);
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.feg_len = (uint8_t)clamp((int32_t)sys.feg_len + s, 0, FEG_NLEN - 1);
            ui_hot = 1; ui_hot_ms = fm1_ms;
        }
        knob(&sys.feg_depth, EN_K3, 2);
        if ((s = ui_enc(EN_K4)) != 0) {
            sys.feg_target = (uint8_t)clamp((int32_t)sys.feg_target + s, 0, FEG_NT - 1);
            ui_hot = 3; ui_hot_ms = fm1_ms;
        }
    } else if (sys.page == 18u) {               /* GROOVE */
        if ((s = ui_enc(EN_K1)) != 0) {
            sys.arp_slen = (uint8_t)clamp((int32_t)arp_vlen() + (s > 0 ? 1 : -1), 1, 8);   /* (1 .. 8 steps) */
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        knob(&sys.arp_vd, EN_K2, 1);
        if ((s = ui_enc(EN_K3)) != 0) {
            sys.arp_glen = (uint8_t)clamp((int32_t)arp_glen() + (s > 0 ? 1 : -1), 1, 8);   /* (1 .. 8 steps) */
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K4)) != 0) {         /* (another GATE SEQ: its own length) */
            sys.arp_gp = (uint8_t)clamp((int32_t)sys.arp_gp + (s > 0 ? 1 : -1), 0, (int32_t)ARP_NGP - 1);
            sys.arp_glen = 0;
            ui_hot = 3; ui_hot_ms = fm1_ms;
        }
    } else if (sys.page == 17u) {               /* LOGIC */
        if ((s = ui_enc(EN_K1)) != 0) {
            static const char *const LG[7] = {"LOGIC OFF: ONE OR THE OTHER", "LOGIC OR: BOTH", "LOGIC AND: WHERE BOTH HIT",
                                              "LOGIC XOR: WHERE ONE HITS", "LOGIC NOR: THE GAPS", "LOGIC ALT: IN TURN",
                                              "LOGIC S/R: ARP SETS"};
            sys.k_logic = (uint8_t)clamp((int32_t)sys.k_logic + (s > 0 ? 1 : -1), 0, 6);
            if (!sys.k_logic && sys.arp_on && sys.k_on)   /* (OFF: one of them, KARMA) */
                sys.arp_on = 0;
            ui_say(LG[sys.k_logic % 7u]);
        }
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.k_lq = (uint8_t)clamp((int32_t)sys.k_lq + (s > 0 ? 1 : -1), 0, (int32_t)LQ_N - 1);
            str_cpy(t, "NOTE QUANT ", sizeof t);
            ui_cat(t, sizeof t, logic_q_name(sys.k_lq));
            ui_say(t);
        }
        if ((s = ui_enc(EN_K3)) != 0) {
            sys.k_root = (uint8_t)clamp((int32_t)sys.k_root + (s > 0 ? 1 : -1), 0, 12);
            str_cpy(t, "ROOT ", sizeof t);
            ui_cat(t, sizeof t, sys.k_root ? NOTE_NAME[(sys.k_root - 1u) % 12u] : "AUTO (LOWEST)");
            ui_say(t);
        }
        if ((s = ui_enc(EN_K4)) != 0) {
            sys.k_inv = (uint8_t)(s > 0);
            ui_say(sys.k_inv ? "INVERSE ON" : "INVERSE OFF");
        }
    } else if (sys.page == 19u) {               /* UI STYLE */
        if ((s = ui_enc(EN_K1)) != 0) {
            sys.ui_style = (uint8_t)((sys.ui_style + (s > 0 ? 1u : US_COUNT - 1u)) % US_COUNT);
            style_apply();
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        knob(&sys.ui_motion, EN_K2, 1);
        knob(&sys.ui_backdrop, EN_K3, 2);
        knob(&sys.ui_react, EN_K4, 3);
    } else if (sys.page == 16u) {               /* SOUND */
        knob(&sys.spk_bass, EN_K1, 0);
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.lvl_match = (uint8_t)(s > 0);
            lvl_set();
            ui_hot = 1; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K3)) != 0) {
            sys.bend_range = (uint8_t)clamp((int32_t)sys.bend_range + s, 0, 12);
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
    } else if (sys.page == 15u) {               /* MULTI */
        if ((s = ui_enc(EN_K1)) != 0) {
            sys.multi = (uint8_t)(s > 0);
            keys_panic_req = 1;                  /* (the voices re-divided: all let go) */
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.ch_b = (uint8_t)clamp((int32_t)sys.ch_b + s, 0, 16);   /* (0: OMNI, layered on A) */
            keys_panic_req = 1;
            ui_hot = 1; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K3)) != 0) {
            part_b_load((uint32_t)(((int32_t)(part_preset(1) >> 7) + (int32_t)NBANKS * 8 + (s > 0 ? 1 : -1)) %
                                   (int32_t)NBANKS) * 128u + (part_preset(1) & 127u));
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K4)) != 0) {
            part_b_load((part_preset(1) & ~127u) + (uint32_t)(((int32_t)(part_preset(1) & 127u) + 128 * 8 + s) % 128));
            ui_hot = 3; ui_hot_ms = fm1_ms;
        }
    } else if (sys.page == 7u) {                /* MIDI: an input changed: what it held let go (no note hangs) */
        if ((s = ui_enc(EN_K1)) != 0) {          /* part A's channel (OMNI, 1 .. 16; TRS MIDI IN always on) */
            sys.midi_rx = (uint8_t)clamp((int32_t)sys.midi_rx + s, 0, 16);
            keys_panic_req = 1;
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.midi_usb = (uint8_t)clamp((int32_t)sys.midi_usb + s, 0, 3);
            keys_panic_req = 1;
            ui_hot = 1; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K3)) != 0) {          /* part B's channel: OMNI (layered on A), 1 .. 16 (MULTI on or off:
                                                  * BANK's PART B) */
            sys.ch_b = (uint8_t)clamp((int32_t)sys.ch_b + s, 0, 16);
            keys_panic_req = 1;
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K4)) != 0) {
            sys.midi_tx = (uint8_t)clamp((int32_t)sys.midi_tx + s, 0, 15);
            ui_hot = 3; ui_hot_ms = fm1_ms;
        }
    } else if (sys.page == 6u) {
        if ((s = ui_enc(EN_K1)) != 0) {
            if (arp_ext())                       /* (CLOCK EXTERNAL: the tempo is the MIDI clock's) */
                ui_say("CLOCK EXTERNAL: GLO");
            else
                sys.arp_bpm = (uint8_t)clamp((int32_t)sys.arp_bpm + s, 40, 250);
            ui_hot = 0; ui_hot_ms = fm1_ms;
        }
        if ((s = ui_enc(EN_K2)) != 0) {
            sys.arp_div = (uint8_t)clamp((int32_t)sys.arp_div + s, 0, ARP_NDIV - 1);
            ui_hot = 1; ui_hot_ms = fm1_ms;
        }
        knob(&sys.arp_gate, EN_K3, 2);
        knob(&sys.arp_swing, EN_K4, 3);
    } else if (sys.page == 5u) {
        knob(&sys.menv_len, EN_K1, 0);
        knob(&sys.menv_skew, EN_K2, 1);
        if ((s = ui_enc(EN_K3)) != 0) {          /* HEIGHT: -127 .. 127 (64 steps a side ... the middle 0: OFF) */
            int32_t h = (int32_t)(sys.menv_height & 127u) * (sys.menv_height & 0x80u ? -1 : 1) + s;
            h = clamp(h, -127, 127);
            sys.menv_height = (uint8_t)(h < 0 ? 0x80u | (uint32_t)-h : (uint32_t)h);
            ui_hot = 2; ui_hot_ms = fm1_ms;
        }
        knob(&sys.menv_bend, EN_K4, 3);
    } else if (sys.page == 0u && sys.osc) {
        knob(&sys.np_x, EN_K1, 0);
        knob(&sys.np_y, EN_K2, 1);
        if (sys.osc > NP_BIGBANG) {              /* BIG BANG: BANG, EXPAND; PIANO: DECAY, STIFF (on the circuit's
                                                  * CHAOS and OUTPUT) */
            knob(&sys.chaos, EN_K3, 2);
            knob(&sys.output, EN_K4, 3);
        } else {
            knob(&sys.np_level, EN_K3, 2);
            knob(&sys.ring, EN_K4, 3);
        }
    } else if (sys.page == 0u) {
        knob(&sys.chaos, EN_K1, 0);
        knob(&sys.shape, EN_K2, 1);
        knob(&sys.output, EN_K3, 2);
        knob(&sys.ring, EN_K4, 3);
    } else if (sys.page == 3u) {
        knob(&sys.sh_size, EN_K1, 0);
        knob(&sys.sh_decay, EN_K2, 1);
        knob(&sys.sh_shimmer, EN_K3, 2);
        knob(&sys.sh_mix, EN_K4, 3);
    } else if (sys.page == 20u) {               /* MOD FX: RATE, SLEW, RECTIFY, MIX */
        knob(&sys.mfx_rate, EN_K1, 0);
        knob(&sys.mfx_slew, EN_K2, 1);
        knob(&sys.mfx_rect, EN_K3, 2);
        knob(&sys.mfx_mix, EN_K4, 3);
    } else if (sys.page == 2u) {
        knob(&sys.fx_struct, EN_K1, 0);
        knob(&sys.fx_bright, EN_K2, 1);
        knob(&sys.fx_damp, EN_K3, 2);
        knob(&sys.fx_mix, EN_K4, 3);
    } else if (sys.page == 14u) {               /* BANK: its knobs below (the bank, part B) */
    } else {
        knob(&sys.attack, EN_K1, 0);
        knob(&sys.decay, EN_K2, 1);
        knob(&sys.sustain, EN_K3, 2);
        knob(&sys.release, EN_K4, 3);
    }
    if (sys.page == 14u) {                       /* BANK: part B (MULTI) too: KNOB 2 off / OMNI / its channel, 3 the part
                                                  * edited, 4 MONO / POLY (as HOME held + OP1, OP2, MONO, POLY) */
        if ((s = ui_enc(EN_K2)) != 0) {
            int32_t m = sys.multi ? (sys.ch_b ? (int32_t)sys.ch_b + 1 : 1) : 0;
            m = clamp(m + (s > 0 ? 1 : -1), 0, 17);
            sys.multi = (uint8_t)(m != 0);
            sys.ch_b = (uint8_t)(m > 1 ? m - 1 : 0);
            keys_panic_req = 1;
            ui_say(!m ? "PART B OFF" : m == 1 ? "PART B OMNI: LAYERED" : "PART B ON ITS CHANNEL");
        }
        if ((s = ui_enc(EN_K3)) != 0)            /* the part the knobs edit (PRESETS then picks its preset) */
            home_key_req = (uint8_t)((s > 0 ? KEY_OP2 : KEY_OP1) + 1u);
        if ((s = ui_enc(EN_K4)) != 0)            /* its voices */
            home_key_req = (uint8_t)((s > 0 ? KEY_MONO : KEY_POLY) + 1u);
    }
    if (sys.page == 14u) {                       /* BANK: SELECT and KNOB 1 turn it too (as PRESETS) */
        int32_t s2 = ui_enc(EN_SELECT) + ui_enc(EN_K1);
        if (s2) {
            uint32_t bk = (uint32_t)(((int32_t)(sys.preset >> 7) + (int32_t)NBANKS * 8 + (s2 > 0 ? 1 : -1)) % (int32_t)NBANKS);
            preset_load(bk * 128u + (sys.preset & 127u));
            str_cpy(t, "BANK ", sizeof t);
            fmt_int(v, (int32_t)bk + 1);
            ui_cat(t, sizeof t, v);
            ui_cat(t, sizeof t, ": ");
            ui_cat(t, sizeof t, PRESETS[sys.preset % NPRESETS].name);
            ui_say(t);
        }
    }
    if ((s = ui_enc(EN_SELECT)) != 0 && (sys.page == 12u || sys.page == 13u)) {   /* TAPE: its two menus */
        uint8_t pg = (uint8_t)(s > 0 ? 13u : 12u);
        if (pg != sys.page) {
            sys.page = pg;
            ui_force = 1;
        }
    } else if (s && (sys.page == 0u || sys.page == 11u)) {   /* HOME, EDIT: the sound's octave, -3 .. +3 (every
                                                              * oscillator; notes held keep theirs) */
        sys.transpose = (int8_t)clamp((int32_t)sys.transpose + (s > 0 ? 1 : -1), -3, 3);
        str_cpy(t, "SOUND OCTAVE ", sizeof t);
        if (sys.transpose > 0)
            ui_cat(t, sizeof t, "+");
        fmt_int(v, sys.transpose);
        ui_cat(t, sizeof t, v);
        ui_say(t);
    } else if (s && sys.page == 9u) {           /* KARMA: LATCH (right on, left off) */
        sys.k_latch = (uint8_t)(s > 0);
        ui_say(sys.k_latch ? "LATCH ON" : "LATCH OFF");
    } else if (s && sys.page == 19u) {          /* UI STYLE: the sound INITIAL (left: exact) or ANALOG (right) */
        sys.analog = (uint8_t)(s > 0);
        ui_say(sys.analog ? "ANALOG: EVERY VOICE A LITTLE OFF" : "INITIAL: EXACT");
        ui_force = 1;
    } else if (s && sys.page == 8u) {           /* FREE EG: PLAY (right on, left off) */
        sys.feg_on = (uint8_t)(s > 0);
        ui_say(sys.feg_on ? (sys.feg_has ? "PLAY ON" : "NOTHING RECORDED") : "PLAY OFF");
    } else if (s && sys.page == 7u) {           /* MIDI: the CLOCK: EXTERNAL (the tempo follows MIDI clock when it
                                                 * comes) or INTERNAL (the BPM always) */
        sys.clk_int = (uint8_t)(s < 0);
        ui_say(sys.clk_int ? "CLOCK INTERNAL" : "CLOCK EXTERNAL");
    } else if (s && sys.page == 5u) {           /* MOD ENV: on what (the pitch, an oscillator's or an FX's parameter) */
        uint8_t tl[20];
        const char *nm[20];
        uint32_t n = menv_targets(tl, nm), i, k = 0;
        for (i = 0; i < n; i++)
            if (tl[i] == sys.menv_target)
                k = i;
        k = (uint32_t)clamp((int32_t)k + (s > 0 ? 1 : -1), 0, (int32_t)n - 1);
        sys.menv_target = tl[k];
        str_cpy(t, "MOD ENV: ", sizeof t);
        ui_cat(t, sizeof t, nm[k]);
        ui_say(t);
    } else if (s && sys.page == 6u) {           /* ARP: LATCH (right on, left off; as KARMA's) */
        sys.arp_hold = (uint8_t)(s > 0);
        ui_say(sys.arp_hold ? "LATCH ON" : "LATCH OFF");
    } else if (s && (sys.page == 2u || sys.page == 3u)) {   /* (MOD, MOD ENV: nothing; RING is on HOME) */
        volatile uint8_t *p = sys.page == 3u ? &sys.sh_tone : &sys.fx_pos;
        if (!knob_step(p, s))
            *p = (uint8_t)clamp(*p + s, 0, 127);
        if (sys.page == 3u) {
            str_cpy(t, "TONE ", sizeof t);
            fmt_int(v, *p);
            ui_cat(t, sizeof t, v);
        } else
            fx_pos_text(t, sizeof t, 0);
        ui_say(t);
    }
    if ((s = ui_enc(EN_ALGO)) != 0 && sys.page == 12u) {   /* TAPE: the armed track */
        sys.tape_arm = (uint8_t)clamp((int32_t)sys.tape_arm + (s > 0 ? 1 : -1), 0, 3);
        str_cpy(t, "ARMED TRACK ", sizeof t);
        fmt_int(v, (int32_t)sys.tape_arm + 1);
        ui_cat(t, sizeof t, v);
        ui_say(t);
    } else if (s && sys.page == 3u) {
        int32_t q = (int32_t)(sys.sh_space % SS_COUNT) * SI_COUNT + (int32_t)(sys.sh_interval % SI_COUNT);
        q = (q + SS_COUNT * SI_COUNT * 8 + s) % (SS_COUNT * SI_COUNT);   /* (the SPACE, each with its INTERVALs) */
        if (SS_IS_DELAY(q / SI_COUNT) != (SS_IS_DELAY(sys.sh_space % SS_COUNT)))
            sys.sh_shimmer = SS_IS_DELAY(q / SI_COUNT) ? SH_DFILT : 70u;   /* (into a delay: its filter ~1 kHz) */
        sys.sh_space = (uint8_t)(q / SI_COUNT);
        sys.sh_interval = (uint8_t)(q % SI_COUNT);
        str_cpy(t, SH_SPACE_NAME[sys.sh_space], sizeof t);
        ui_cat(t, sizeof t, " ");
        ui_cat(t, sizeof t, sh_iv_name(sys.sh_space, sys.sh_interval));
        ui_say(t);
    } else if (s && sys.page == 19u) {          /* UI STYLE: the tuning, A 440 (left) or A 432 (right) */
        sys.tune432 = (uint8_t)(s > 0);
        ui_say(sys.tune432 ? "TUNING A = 432 HZ" : "TUNING A = 440 HZ");
        ui_force = 1;
    } else if (s && sys.page == 20u) {          /* MOD FX: the mode (OFF, VIBRATO, RING MOD, TREMOLO, SHIFT +/-) */
        sys.mfx_mode = (uint8_t)(((int32_t)sys.mfx_mode + MF_COUNT * 8 + s) % MF_COUNT);
        str_cpy(t, "MOD FX ", sizeof t);
        ui_cat(t, sizeof t, MF_NAME[sys.mfx_mode]);
        ui_say(t);
    } else if (s && sys.page == 2u) {
        duck_begin();
        sys.fx_model = (uint8_t)((sys.fx_model + RZ_COUNT * 8 + s) % RZ_COUNT);
        duck_end();
        str_cpy(t, "MODEL ", sizeof t);
        ui_cat(t, sizeof t, RZ_NAME[sys.fx_model]);
        ui_say(t);
    } else if (s && sys.page == 9u) {           /* KARMA: the GE */
        sys.k_ge = (uint8_t)(((int32_t)sys.k_ge + (int32_t)KGE_N * 8 + s) % (int32_t)KGE_N);
        str_cpy(t, "GE ", sizeof t);
        ui_cat(t, sizeof t, k_ge()->name);
        ui_say(t);
    } else if (s && sys.page == 18u) {          /* GROOVE: the VELOCITY pattern */
        sys.arp_vp = (uint8_t)clamp((int32_t)sys.arp_vp + (s > 0 ? 1 : -1), 0, (int32_t)ARP_NVP - 1);
        sys.arp_slen = 0;                        /* (SEQ LENGTH: the pattern's own) */
        str_cpy(t, "ARP VEL SEQ ", sizeof t);
        ui_cat(t, sizeof t, ARP_VPAT[sys.arp_vp % ARP_NVP].name);
        ui_say(t);
    } else if (s && sys.page == 6u) {           /* ARP: the pattern */
        sys.arp_type = (uint8_t)(((int32_t)sys.arp_type + (int32_t)ARP_NTYPE * 8 + s) % (int32_t)ARP_NTYPE);
        str_cpy(t, "ARP ", sizeof t);
        ui_cat(t, sizeof t, arp_type_name(sys.arp_type));
        ui_say(t);
    } else if (s && !sys.page) {                 /* HOME: the oscillator, CHUA and the 30 noise programs */
        sys.osc = (uint8_t)(((int32_t)sys.osc + (int32_t)(NP_COUNT + 1u) * 8 + s) % (int32_t)(NP_COUNT + 1u));
        osc_code(t, sizeof t, sys.osc);
        if (sys.osc) {
            ui_cat(t, sizeof t, " ");
            ui_cat(t, sizeof t, NP_NAME[sys.osc - 1u]);
        } else
            ui_cat(t, sizeof t, " CIRCUIT");
        ui_say(t);
    }
    if ((s = ui_enc(EN_PRESET)) != 0 && (sys.page == 6u || sys.page == 18u)) {   /* ARP, GROOVE: the ARP's presets */
        const arp_preset_t *P;
        arp_pre = (uint8_t)(arp_pre == 0xFFu ? (s > 0 ? 0 : (int32_t)ARP_NPRE - 1) :
                            ((int32_t)arp_pre + (int32_t)ARP_NPRE * 8 + s) % (int32_t)ARP_NPRE);
        P = &ARP_PRE[arp_pre];
        sys.arp_type = (uint8_t)(P->type % ARP_NTYPE);
        sys.arp_div = P->div;
        sys.arp_gate = P->gate;
        sys.arp_swing = P->swing;
        sys.arp_vp = (uint8_t)(P->vp % ARP_NVP);
        sys.arp_vd = 100;
        sys.arp_slen = 0;
        sys.arp_gp = (uint8_t)(P->gp % ARP_NGP);
        sys.arp_glen = 0;
        str_cpy(t, "ARP PRESET ", sizeof t);
        ui_cat(t, sizeof t, P->name);
        ui_say(t);
        ui_force = 1;
    } else if (s && (sys.page == 9u || sys.page == 17u)) {   /* KARMA, LOGIC: KARMA's presets */
        const seq_preset_t *P;
        uint32_t i;
        seq_pre = (uint8_t)(seq_pre == 0xFFu ? (s > 0 ? 0 : (int32_t)SEQ_NPRE - 1) :
                            ((int32_t)seq_pre + (int32_t)SEQ_NPRE * 8 + s) % (int32_t)SEQ_NPRE);
        P = &SEQ_PRE[seq_pre];
        sys.k_ge = (uint8_t)(P->ge % KGE_N);
        for (i = 0; i < 4u; i++)
            if (k_ge()->knob[i] != MC_TRANSPOSE)
                sys.k_m[i] = P->m[i];
        str_cpy(t, "SEQ PRESET ", sizeof t);
        ui_cat(t, sizeof t, P->name);
        ui_say(t);
        ui_force = 1;
    } else if (s && (sys.page == 12u || sys.page == 13u)) {   /* TAPE: PRESETS is the BPM */
        if (arp_ext())                           /* (CLOCK EXTERNAL: the tempo is the MIDI clock's) */
            ui_say("CLOCK EXTERNAL: GLO");
        else {
            sys.arp_bpm = (uint8_t)clamp((int32_t)sys.arp_bpm + s, 40, 250);
            str_cpy(t, "BPM ", sizeof t);
            fmt_int(v, sys.arp_bpm);
            ui_cat(t, sizeof t, v);
            ui_say(t);
        }
    } else if (s && sys.page == 7u) {                   /* MIDI: PRESETS turns AUTOPLAY on (right) or off (left) */
        sys.autoplay = (uint8_t)(s > 0);
        ui_say(sys.autoplay ? "AUTOPLAY ON" : "AUTOPLAY OFF");
    } else if (s && sys.page == 20u) {                  /* MOD FX: its presets */
        mfx_pre = (uint8_t)(mfx_pre == 0xFFu ? (s > 0 ? 0 : (int32_t)MFX_NPRE - 1) :
                            ((int32_t)mfx_pre + (int32_t)MFX_NPRE * 8 + s) % (int32_t)MFX_NPRE);
        mfx_set(mfx_pre + 1u);
        str_cpy(t, "MOD FX ", sizeof t);
        ui_cat(t, sizeof t, MFX_PRE[mfx_pre].name);
        ui_say(t);
        ui_force = 1;
    } else if (s && sys.page == 2u) {                   /* FX: PRESETS goes through the FX presets */
        const fx_preset_t *f;
        fx_pre = (uint8_t)(((int32_t)fx_pre + (int32_t)NFX_PRESETS * 8 + s) % (int32_t)NFX_PRESETS);
        f = &FX_PRESETS[fx_pre];
        duck_begin();
        sys.fx_mix = 0;                                 /* (the model changed silent, then its knobs, then heard) */
        sys.fx_model = f->model; sys.fx_struct = f->st; sys.fx_bright = f->br;
        sys.fx_damp = f->dm; sys.fx_pos = f->pos;
        sys.fx_mix = f->mix;
        duck_end();
        str_cpy(t, "FX: ", sizeof t);
        ui_cat(t, sizeof t, f->name);
        ui_say(t);
        ui_force = 1;
    } else if (s && sys.page == 3u) {                   /* SHIMMER: through the shimmer presets */
        const sh_preset_t *h;
        sh_pre = (uint8_t)(((int32_t)sh_pre + (int32_t)NSH_PRESETS * 8 + s) % (int32_t)NSH_PRESETS);
        h = &SH_PRESETS[sh_pre];
        duck_begin();
        sys.sh_size = h->size; sys.sh_decay = h->decay; sys.sh_shimmer = h->shimmer;
        sys.sh_interval = h->interval; sys.sh_tone = h->tone; sys.sh_space = h->space; sys.sh_mix = h->mix;
        duck_end();
        str_cpy(t, "SHIMMER: ", sizeof t);
        ui_cat(t, sizeof t, h->name);
        ui_say(t);
        ui_force = 1;
    } else if (s && (sys.page == 1u || sys.page == 5u || sys.page == 8u || sys.page == 10u)) {   /* ENV, MOD ENV, FREE EG,
                                                              * SCL: their own presets */
        page_preset(s);
    } else if (s && ((fm1_in.buttons & btn_bit(B_HOME)) || sys.page == 14u)) {   /* the BANK menu (or HOME held):
                                                              * the bank (the same program in it) */
        uint32_t bk = (uint32_t)(((int32_t)(sys.preset >> 7) + (int32_t)NBANKS * 8 + (s > 0 ? 1 : -1)) % (int32_t)NBANKS);
        preset_load(bk * 128u + (sys.preset & 127u));
        str_cpy(t, "BANK ", sizeof t);
        fmt_int(v, (int32_t)bk + 1);
        ui_cat(t, sizeof t, v);
        ui_cat(t, sizeof t, ": ");
        ui_cat(t, sizeof t, PRESETS[sys.preset % NPRESETS].name);
        ui_say(t);
    } else if (s)                                       /* (through the bank playing) */
        preset_load((sys.preset & ~127u) + (uint32_t)(((int32_t)(sys.preset & 127u) + 128 * 8 + s) % 128));
}
