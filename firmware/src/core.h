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


#include "sys.h"                 /* the shared state (HAWK CHUA's own) */
