/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Basis core: the sizes and the few shared values every other file uses.
 * Everything here is deliberately small; grow it as your firmware grows. */
#include <stdint.h>

/* ------------------------------------------------------------ audio --- */
#define FS 44100                 /* nominal sample rate (the I2S clock runs at ~44,117.6 Hz) */
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms; one audio interrupt renders one half */
#define CTL 32                   /* frames per block: keys and MIDI are handled once a block (~0.7 ms) */
#define NVOICE 4                 /* synth voices (synth.c): 4, as the FX resonators / waveguides (one a note): the CPU */

/* --------------------------------------------------------- keyboard --- */
#define NKEYS 27u                /* the note keys: bit n of fm1_in.notes, 0 = F3 .. 26 = G5 */
#define KEY_BASE_NOTE 53         /* key 0 at octave 0: F3 = MIDI note 53 */
#define OCT_MIN (-3)
#define OCT_MAX 3

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */

/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));

static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* What the main loop and the audio interrupt share. Single writer per field:
 *   main loop writes:  octave, chaos, shape, output, attack, decay, sustain, release, ring, master_q12, view, preset,
 *                      cpu2, page, fx_*, sh_*, osc, np_*, mod_*
 *   audio ISR writes:  last_note, last_vel, cpu_q8, voices, env_view, env_stage */
static volatile struct {
    int8_t octave;               /* OCT- / OCT+: -3..+3 */
    uint8_t chaos, shape, output;   /* synth.c: the circuit's knobs, 0..127 */
    uint8_t attack, decay, sustain, release;   /* synth.c: the ADSR, 0..127 */
    uint8_t ring;                /* synth.c: the low pass gate's resonance, 0..127 (Q 0.5 .. 6) */
    uint16_t master_q12;         /* MASTER pot: 0..4096 */
    uint8_t last_note, last_vel; /* the last note started (keys or MIDI); 0xFF = none yet */
    uint8_t voices;              /* voices sounding */
    uint8_t env_view, env_stage; /* the newest voice's gate control (0..255) and stage (ui.c ENV page) */
    uint32_t cpu_q8;             /* audio load, 256 = 100 % */
    int32_t batt_raw;            /* battery ADC, smoothed (main loop) */
    uint8_t view;                /* ui.c: the screen's view */
    uint16_t preset;             /* ui.c: the last preset loaded: bank x 128 + program (0 .. 511) */
    uint8_t cpu2;                /* 2ND CORE: 1 all (always; main.c starts it at power-on), 0 when it stopped answering */
    uint8_t page;                /* ui.c: 0 the oscillator (HOME), 1 ENV, 2 FX: resonator, 3 FX: shimmer, 4 MOD (HOME 2x), 5 MOD ENV (ENV 2x), 6 ARP, 7 MIDI (GLO), 8 FREE EG (REC), 9 KARMA (SEQ),
                                  * 10 PAN, 11 EDIT, 12 / 13 TAPE, 14 BANK (HOME on HOME) */
    uint8_t fx_model, fx_struct, fx_bright, fx_damp, fx_pos, fx_mix;   /* resonator.c: the FX resonator, 0..127 */
    uint8_t sh_size, sh_decay, sh_shimmer, sh_mix, sh_interval, sh_tone, sh_space;   /* shimmer.c: the shimmer reverb */
    uint8_t osc;                 /* the oscillator: 0 the CHUA circuit, 1 .. 30 a noise program (noise.c: osc - 1) */
    uint8_t np_x, np_y, np_level;   /* noise.c: its X, Y and level, 0..127 */
    uint8_t menv_len, menv_skew, menv_height;   /* noise.c: its AD envelope (ENV again: the MOD ENV page) */
    uint8_t menv_bend;           /* noise.c: its curve, 0 (bent up: fast, then slow) .. 64 straight .. 127 (bent down) */
    uint8_t ui_style, ui_motion, ui_backdrop, ui_react;   /* ui_style.c: the screen's look (GLO's fourth page) */
    uint8_t menv_target;         /* keys.c: the MOD ENV on what: 0 the pitch (noise.c), else 1 + feg.c's parameter (an
                                  * oscillator's or an FX's: the MOD ENV page's SELECT) */
    uint8_t basis;               /* synth.c: the voices' stereo spread, 0 (mono) .. 127 (ENV page, SELECT) */
    uint8_t orbit;               /* synth.c: how far each voice's picture moves it in the stereo field (ENV, ALGORITHM) */
    uint8_t arp_on, arp_type, arp_bpm, arp_div, arp_gate, arp_swing, arp_hold;   /* arp.c: the arpeggiator (ARP) */
    uint8_t multi, ch_b;         /* MULTI (GLO page): two sounds; part B's MIDI channel (1 .. 16; A: RX) */
    uint8_t midi_trs, midi_usb, midi_rx, midi_tx;   /* keys.c, GLO page: TRS in on; USB 0 off 1 in 2 out 3 both; rx 0 omni
                                                  * 1..16; tx channel 0..15 */
    uint8_t feg_val, feg_len, feg_depth, feg_target, feg_on, feg_has, feg_npts;   /* feg.c: FREE EG (REC) */
    uint8_t k_on, k_ge, k_latch, k_m[4];
    uint8_t arp_vp, arp_vd;      /* arp.c, the ARP's GROOVE page (ARP 2x): the ARP VELOCITY SEQ (ALGORITHM; 0 off), its DEPTH */
    uint8_t arp_slen;            /* arp.c, the GROOVE page's KNOB 1: the VELOCITY SEQ's length, 1 .. 8 (0: the pattern's own) */
    uint8_t arp_gp, arp_glen;    /* arp.c, the GROOVE page's KNOB 4: the ARP GATE SEQ (0 off: GATE), KNOB 3 its length */
    uint8_t k_logic, k_inv;      /* karma.c, the LOGIC page (SEQ 2x): the ARP with KARMA: 0 one or the other, OR, AND, XOR,
                                  * NOR, ALT, S/R; KNOB 4 INVERSE: what it would let through held back, and the other way */
    uint8_t k_lq, k_root;        /* arp.c, the LOGIC page: their notes to a key: 0 off, 1 the chord, 2 AUTO, 3.. a scale;
                                  * its root: 0 AUTO (the lowest held), 1..12 C..B */
    uint8_t mw, mw_target, mw_depth;   /* keys.c: MIDI CC 1 (the mod wheel), the parameter it moves (feg.c's list), how far */
    uint8_t mw_seen;
    int8_t transpose;              /* synth.c: the sound's octave (-3 .. +3: every oscillator, the FX tuned with it;
                                  * the presets set it for their instrument's register; EDIT page SELECT) */
    uint8_t pedal;                 /* keys.c: the sustain pedal (MIDI CC 64): the PIANO's dampers stay up */
    uint8_t at, at_seen, scream;   /* keys.c: MIDI aftertouch (channel pressure; poly per note: at_poly), one has come
                                  * (from then on it is the breath: resonator.c); SCREAM (feg.c's list): the voices' own
                                  * sound driving the waveguides' breath */
    uint8_t tape_lv[4], tape_len, tape_arm, tape_spd;   /* tape.c: the tracks' levels, LENGTH (1 2 4 8 16 beats; 3: 2 bars), the armed track */             /* a mod wheel has sent (until then it leaves FREE EG's depth alone) */
    uint8_t bend_range;          /* keys.c, the SOUND page: MIDI pitch bend up and down, 0 .. 12 semitones */
    uint8_t spk_bass, lvl_match; /* synth.c, the SOUND page (GLO 3x): SPEAKER BASS 0 .. 127 (its overtones for a small
                                  * speaker), LEVEL MATCH on (each preset at its measured level) */
    uint8_t vel_to, vel_amt;     /* synth.c: velocity also on the sound: 0 off, 1 BRIGHT (the gate), 2 FM, 3 both; how much */   /* karma.c: KARMA (SEQ): on, the GE, LATCH, its four realtime controls */
    uint16_t menv_view;          /* the newest voice's MOD ENV pitch sweep now, 0..32767 = 3 octaves (ui.c) */
} sys = {.octave = 0, .chaos = 85, .shape = 76, .output = 0, .attack = 6, .decay = 80, .sustain = 110, .release = 70,
         .ring = 50, .master_q12 = 2048, .last_note = 0xFF, .cpu2 = 1,
         .fx_model = 0, .fx_struct = 70, .fx_bright = 90, .fx_damp = 100, .fx_pos = 24, .fx_mix = 0,
         .sh_size = 100, .sh_decay = 90, .sh_shimmer = 70, .sh_mix = 0, .sh_interval = 0, .sh_tone = 80, .sh_space = 0,
         .osc = 0, .np_x = 64, .np_y = 64, .np_level = 64,
         
         .menv_len = 80, .menv_skew = 10, .menv_height = 0, .menv_bend = 64, .ui_style = 1 /* PSYCHEDELIC first */, .ui_motion = 64, .ui_backdrop = 70, .ui_react = 80, .basis = 0, .orbit = 0,
         .arp_on = 0, .arp_type = 0, .arp_bpm = 128, .arp_div = 4, .arp_gate = 80, .arp_swing = 0, .arp_hold = 1,
         .multi = 0, .ch_b = 2, .midi_trs = 1, .midi_usb = 3, .midi_rx = 0, .midi_tx = 0,
         .feg_val = 64, .feg_len = 9, .feg_depth = 127, .feg_target = 0, .feg_on = 0, .feg_has = 0, .feg_npts = 0,
         .k_on = 0, .k_ge = 0, .k_latch = 1, .k_m = {64, 64, 64, 64},
         .mw = 0, .mw_target = 0, .mw_depth = 127, .vel_to = 0, .vel_amt = 64,
         .tape_lv = {100, 100, 100, 100}, .tape_len = 3, .tape_arm = 0, .tape_spd = 12, .spk_bass = 4, .bend_range = 12, .k_lq = 1, .k_logic = 1, .arp_vd = 100, .lvl_match = 1};
