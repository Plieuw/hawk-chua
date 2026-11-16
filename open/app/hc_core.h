/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the sizes and the few shared values the sound and the UI use (the FM1 Open build's own; the
 * Felucca build has firmware/src/core.h). The platform (audio, MIDI, panel, screen, flash, updates) is M-VAVE's SDK. */
#ifndef HC_CORE_H
#define HC_CORE_H
#include <stdint.h>
#include <string.h>
#include "fm1.h"
#include "fm1_block.h"

/* ------------------------------------------------------------ audio --- */
#define FS 44100                 /* FM1_SAMPLE_RATE */
#define HALF_FRAMES FM1_BLOCK_FRAMES   /* the SDK's block: 128 frames (2.9 ms), one audio interrupt renders one */
#define CTL 32                   /* frames per sub-block: keys and MIDI are handled once a sub-block (~0.7 ms) */
#ifndef NVOICE
#define NVOICE 4                 /* synth voices (synth.c), as the FX resonators / waveguides (one a note) */
#endif

/* --------------------------------------------------------- keyboard --- */
#define NKEYS 27u                /* the note keys: bit n of fm1_in.notes (app/board/fm1_input.h), 0 = F3 .. 26 = G5 */
#define KEY_BASE_NOTE 53         /* key 0 at octave 0: F3 = MIDI note 53 */
#define OCT_MIN (-3)
#define OCT_MAX 3

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
#define fm1_ms (fm1_millis())    /* milliseconds (the SDK's clock) */
static inline int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }

/* the platform's settings as the shared sources know them */
#define FELUCCA_CPU2 0           /* no second core: the SDK runs its own tasks there */
#define FELUCCA_UAC 0            /* USB audio: the SDK's (audio_output.c) */
#define FELUCCA_UART 0           /* TRS MIDI: the SDK's */
#define FELUCCA_OTA 0            /* updates: the SDK's */
#define FELUCCA_FLASH 1
#ifndef TAPE_N
#define TAPE_N 26624u            /* tape.c: cells a track, 2.4 s at 11 kHz (the Felucca build: 7.2 s; the SDK's RAM) */
#endif
#define HC_OPEN 1                /* (the shared sources: the FM1 Open build) */
#ifndef HC_VERSION
#define HC_VERSION "OPEN"        /* shown on the screen (tools/build_open.py: HC_BUILD) */
#endif
#define FELUCCA_VERSION HC_VERSION

#include "sys.h"                 /* the shared state */
#endif
