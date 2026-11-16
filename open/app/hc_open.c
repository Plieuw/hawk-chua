/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on M-VAVE FM1 Open: one compilation unit. The sound and the UI are the shared sources in
 * firmware/src (synth.c with everything it includes, keys.c, ui.c); the rest is this directory's thin layer on the
 * FM1 SDK, which owns the platform: audio out, MIDI (USB, TRS, Bluetooth LE), the panel, the screen, the flash and
 * the updates.
 *
 *   hc_core.h   sizes, sys          hc_panel.h  buttons, encoders, LEDs (on app/board/panel.c's scan)
 *   hc_gfx.c    canvas, text        hc_store.c  the settings and presets in the user flash
 *   hc_midi.c   MIDI in / out       hc_sdk.c    the hooks the SDK calls (control task, MIDI, the instrument)
 *   hc_audio.c  the render the audio interrupt (app/board/audio.c) calls */
#include "hc_core.h"
#include "fm1_device.h"
#include "hc_libc.h"
/* the control task's short holds against the audio interrupt (ui.c: FREE EG's, the presets' hand-over) */
static inline uint32_t irq_save(void) { return fm1_device_irq_save(); }
static inline void irq_restore(uint32_t f) { fm1_device_irq_restore(f); }
#include "hc_panel.h"
#include "hc_gfx.c"
#include "hc_midi.c"

/* ----------------------------------------------------------- sound --- */
#include "synth.c"
#include "keys.c"
#include "hc_audio.c"

/* -------------------------------------------------------- store, UI --- */
#include "hc_store.c"
#include "ui.c"
#include "hc_sdk.c"
