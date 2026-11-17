/* SPDX-License-Identifier: GPL-3.0-only */
/* HAWK CHUA in the browser: the firmware's own sources (synth.c, keys.c, ui.c, gfx.c: the sound, the keyboard and
 * MIDI, the screen, the LEDs) compiled to WebAssembly, the FM-1's hardware replaced by a few buffers:
 *   the LCD        a 240 x 240 RGB565 framebuffer (emu_fb)
 *   the panel      buttons, the 27 keys, the 7 encoders (emu_button, emu_key, emu_turn)
 *   MIDI IN        Web MIDI's messages (emu_midi), as from USB
 *   the audio      blocks of 32 stereo frames, 24-bit in int32 (emu_render, emu_audio)
 *   the flash      the settings and the saved presets, kept by the page (js_store, emu_store)
 * web/emu/build.sh builds it (clang --target=wasm32); web/emu/index.html plays it. */
#include <stdint.h>

#include "../../firmware/src/libc.c"

/* --------------------------------------------------------- the page --- */
__attribute__((import_module("env"), import_name("js_store"))) void js_store(uint32_t obj, const void *p, uint32_t n);

/* ------------------------------------------------------------ the LCD --- */
static uint16_t fb[240 * 240];
static uint32_t fb_dirty;
static void lcd_sync(void) {}
static void lcd_wake_now(void) {}
static uint8_t lcd_fresh = 1;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    lcd_fresh = 1;
    fb_dirty = 1;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    fb_dirty = 1;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = (uint16_t)(px[j * w + i] >> 8 | px[j * w + i] << 8);   /* (byte-swapped) */
}
#include "../../firmware/src/gfx.c"

/* ------------------------------------------- the hardware ui.c reads --- */
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "web"
#endif
#include "../../firmware/src/core.h"
static uint32_t fm1_ms_frac;
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static struct { volatile uint32_t notes, buttons; } fm1_in;
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static int fm1_led_anim_on(void) { return 0; }
static int32_t enc_steps[7];
static int32_t fm1_enc_take(uint32_t e) { int32_t s = enc_steps[e]; enc_steps[e] = 0; return s; }
static uint32_t edges;
static uint32_t fm1_input_edges(uint32_t *released) { uint32_t p = edges; (void)released; edges = 0; return p; }
static struct { uint8_t config, suspended; } usb = {0, 0};   /* (no USB here: the screen shows none) */
static uint8_t flash_ok = 1;
enum { OBJ_SETTINGS, OBJ_USER0, OBJ_COUNT = OBJ_USER0 + 8 };
#define OBJ_MAX 3840u
static uint8_t flash_store[OBJ_COUNT][OBJ_MAX];
static uint16_t flash_len[OBJ_COUNT];
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    uint32_t n;
    if (obj >= OBJ_COUNT || !flash_len[obj])
        return -1;
    n = flash_len[obj] < max ? flash_len[obj] : max;
    memcpy(dst, flash_store[obj], n);
    return (int)n;
}
static int st_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_COUNT || len > OBJ_MAX)
        return -1;
    memcpy(flash_store[obj], src, len);
    flash_len[obj] = (uint16_t)len;
    js_store(obj, flash_store[obj], len);        /* (the page keeps it: localStorage) */
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec(void) { return 0x856014u; }
static int fl_id_ok(uint32_t id) { return id == 0x856014u || id == 0xB36014u; }
static int fl_emulated(void) { return 0; }      /* (a host build: not the emulated FM-1 of fm1-emu) */
#define FL_FAR(f) fl_jedec

/* the USB-MIDI rings of usb.c (MIDI IN), MIDI out: nowhere */
#define MQ 256u
static uint32_t midi_in_q[MQ];
static uint8_t midi_in_source[MQ];
static volatile uint32_t mi_w, mi_r;
static volatile uint8_t midi_in_overflow;
static void midi_out_event(uint32_t pkt) { (void)pkt; }

#include "../../firmware/src/panel.c"
#include "../../firmware/src/synth.c"
#include "../../firmware/src/keys.c"
#include "../../firmware/src/ui.c"

/* ------------------------------------------------------- the exports --- */
#define EXPORT(n) __attribute__((export_name(#n)))
#define EMU_BLOCKS 64u
static int32_t abuf[2u * CTL * EMU_BLOCKS];
static uint8_t store_in[OBJ_MAX];

EXPORT(emu_fb) uint16_t *emu_fb(void) { return fb; }
EXPORT(emu_audio) int32_t *emu_audio(void) { return abuf; }
EXPORT(emu_leds) uint8_t *emu_leds(void) { return fm1_led; }
EXPORT(emu_store_buf) uint8_t *emu_store_buf(void) { return store_in; }
EXPORT(emu_store) void emu_store(uint32_t obj, uint32_t len)   /* before emu_init: what the page kept */
{
    if (obj < OBJ_COUNT && len <= OBJ_MAX) {
        memcpy(flash_store[obj], store_in, len);
        flash_len[obj] = (uint16_t)len;
    }
}

EXPORT(emu_init) void emu_init(void)          /* main.c's fm1_main, up to the main loop */
{
    settings_boot();
    style_apply();                               /* (the UI STYLE: its palette and backdrop; INITIAL: AMBER) */
    keys_init();
    tape_init();
    sys.master_q12 = 2048;
    ui_leds_init();
    ui_force = 1;
}

EXPORT(emu_render) uint32_t emu_render(uint32_t blocks)   /* blocks of CTL frames into emu_audio: audio.c */
{
    uint32_t b;
    if (blocks > EMU_BLOCKS)
        blocks = EMU_BLOCKS;
    for (b = 0; b < blocks; b++) {
        keys_block();
        synth_render(abuf + 2u * CTL * b, CTL);
        fm1_ms_frac += CTL;
        while (fm1_ms_frac >= FS / 1000u) {
            fm1_ms_frac -= FS / 1000u;
            fm1_ms++;
        }
    }
    return blocks;
}

EXPORT(emu_frame) uint32_t emu_frame(void)    /* one UI frame: the main loop's; 1 if the screen changed */
{
    uint32_t d;
    ui_input();
    ui_leds();
    if (!(fm1_in.buttons & btn_bit(B_OCTDN) && fm1_in.buttons & btn_bit(B_OCTUP)))
        ui_draw();
    d = fb_dirty;
    fb_dirty = 0;
    return d;
}

EXPORT(emu_button) void emu_button(uint32_t label, uint32_t down)   /* a button (panel.c's B_*) */
{
    uint32_t bit;
    if (label >= NB)
        return;
    bit = btn_bit(label);
    if (down) {
        if (!(fm1_in.buttons & bit))
            edges |= bit;
        fm1_in.buttons |= bit;
    } else
        fm1_in.buttons &= ~bit;
}

EXPORT(emu_key) void emu_key(uint32_t k, uint32_t down)   /* a note key, 0 = F3 .. 26 = G5 (at octave 0) */
{
    if (k >= NKEYS)
        return;
    if (down)
        fm1_in.notes |= 1u << k;
    else
        fm1_in.notes &= ~(1u << k);
}

EXPORT(emu_turn) void emu_turn(uint32_t role, int32_t steps)   /* an encoder (panel.c's EN_*), + = clockwise */
{
    if (role < NE)
        enc_steps[panel.enc[role]] += steps * panel.dir[role];
}

EXPORT(emu_midi) void emu_midi(uint32_t st, uint32_t d1, uint32_t d2)   /* a MIDI message in (as USB-MIDI) */
{
    if (mi_w - mi_r >= MQ)
        return;
    midi_in_q[mi_w % MQ] = (st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
    midi_in_source[mi_w % MQ] = 1u;
    mi_w++;
}

EXPORT(emu_now_ms) uint32_t emu_now_ms(void) { return fm1_ms; }
EXPORT(emu_master) void emu_master(uint32_t q12) { sys.master_q12 = (uint16_t)(q12 > 4095u ? 4095u : q12); }
EXPORT(emu_led_id) uint32_t emu_led_id(uint32_t label) { return label < NB ? panel.btn[label] : 255u; }

/* ------------------------------------------------- the preset editor (web/presets.html) --- */
/* The presets as the firmware keeps them: names, the saved ones, load, save, rename, delete; the sound's values,
 * grouped as the editor shows them, with the firmware's own text for each (its names, its times). */
static char ed_str[64];                          /* strings both ways */
EXPORT(emu_str) char *emu_str(void) { return ed_str; }
EXPORT(emu_pr_count) uint32_t emu_pr_count(void) { return NPRESETS; }
EXPORT(emu_pr_name) char *emu_pr_name(uint32_t i) { str_cpy(ed_str, preset_name(i), sizeof ed_str); return ed_str; }
EXPORT(emu_pr_factory) char *emu_pr_factory(uint32_t i) { str_cpy(ed_str, PRESETS[i % NPRESETS].name, sizeof ed_str); return ed_str; }
EXPORT(emu_pr_user) uint32_t emu_pr_user(uint32_t i) { return (uint32_t)upre_has(i); }
EXPORT(emu_pr_cur) uint32_t emu_pr_cur(void) { return sys.preset % NPRESETS; }
EXPORT(emu_pr_edited) uint32_t emu_pr_edited(void) { return (uint32_t)preset_edited(); }
EXPORT(emu_pr_load) void emu_pr_load(uint32_t i) { preset_load(i % NPRESETS); ui_force = 1; }
EXPORT(emu_pr_save) int32_t emu_pr_save(uint32_t i)      /* the sound now over preset i (it is then the one playing) */
{
    int e = upre_save(i % NPRESETS);
    sys.preset = (uint16_t)(i % NPRESETS);
    ui_force = 1;
    return e;
}
EXPORT(emu_pr_rename) int32_t emu_pr_rename(uint32_t i) { ui_force = 1; return upre_name(i % NPRESETS, ed_str); }
EXPORT(emu_pr_del) int32_t emu_pr_del(uint32_t i) { ui_force = 1; return upre_del(i % NPRESETS); }

enum { EK_NUM, EK_LIST, EK_TIME_A, EK_TIME_D, EK_TIME_R, EK_SIGNED, EK_OCT, EK_HEIGHT };
typedef struct { const char *group, *name; volatile uint8_t *p; int16_t lo, hi; uint8_t kind; const char *const *list; } ed_par_t;
static const ed_par_t ED_PAR[] = {
    {"SOUND", "OSCILLATOR", &sys.osc, 0, NP_COUNT, EK_LIST, 0},
    {"SOUND", "CHAOS", &sys.chaos, 0, 127, EK_NUM, 0},
    {"SOUND", "SHAPE", &sys.shape, 0, 127, EK_NUM, 0},
    {"SOUND", "OUTPUT", &sys.output, 0, 127, EK_NUM, 0},
    {"SOUND", "NOISE X", &sys.np_x, 0, 127, EK_NUM, 0},
    {"SOUND", "NOISE Y", &sys.np_y, 0, 127, EK_NUM, 0},
    {"SOUND", "NOISE LEVEL", &sys.np_level, 0, 127, EK_NUM, 0},
    {"SOUND", "OCTAVE", (volatile uint8_t *)&sys.transpose, -3, 3, EK_OCT, 0},
    {"GATE", "ATTACK", &sys.attack, 0, 127, EK_TIME_A, 0},
    {"GATE", "DECAY", &sys.decay, 0, 127, EK_TIME_D, 0},
    {"GATE", "SUSTAIN", &sys.sustain, 0, 127, EK_NUM, 0},
    {"GATE", "RELEASE", &sys.release, 0, 127, EK_TIME_R, 0},
    {"GATE", "RING", &sys.ring, 0, 127, EK_NUM, 0},
    {"FX", "MODEL", &sys.fx_model, 0, RZ_COUNT - 1, EK_LIST, RZ_NAME},
    {"FX", "STRUCTURE", &sys.fx_struct, 0, 127, EK_NUM, 0},
    {"FX", "BRIGHTNESS", &sys.fx_bright, 0, 127, EK_NUM, 0},
    {"FX", "DAMPING", &sys.fx_damp, 0, 127, EK_NUM, 0},
    {"FX", "POSITION", &sys.fx_pos, 0, 127, EK_NUM, 0},
    {"FX", "SCREAM", &sys.scream, 0, 127, EK_NUM, 0},
    {"FX", "MIX", &sys.fx_mix, 0, 127, EK_NUM, 0},
    {"SPACE", "SPACE", &sys.sh_space, 0, SS_COUNT - 1, EK_LIST, SH_SPACE_NAME},
    {"SPACE", "SIZE", &sys.sh_size, 0, 127, EK_NUM, 0},
    {"SPACE", "DECAY", &sys.sh_decay, 0, 127, EK_NUM, 0},
    {"SPACE", "SHIMMER", &sys.sh_shimmer, 0, 127, EK_NUM, 0},
    {"SPACE", "INTERVAL", &sys.sh_interval, 0, SI_COUNT - 1, EK_LIST, SH_INTERVAL_NAME},
    {"SPACE", "TONE", &sys.sh_tone, 0, 127, EK_NUM, 0},
    {"SPACE", "MIX", &sys.sh_mix, 0, 127, EK_NUM, 0},
    {"MOD ENV", "LENGTH", &sys.menv_len, 0, 127, EK_NUM, 0},
    {"MOD ENV", "SKEW", &sys.menv_skew, 0, 127, EK_NUM, 0},
    {"MOD ENV", "HEIGHT", &sys.menv_height, -127, 127, EK_HEIGHT, 0},
    {"MOD ENV", "BEND", &sys.menv_bend, 0, 127, EK_NUM, 0},
    {"MOD FX", "MODE", &sys.mfx_mode, 1, MF_COUNT - 1, EK_LIST, MF_NAME},
    {"MOD FX", "SYNC / RATIO", &sys.mfx_rate, 0, 127, EK_NUM, 0},
    {"MOD FX", "SLEW", &sys.mfx_slew, 0, 127, EK_NUM, 0},
    {"MOD FX", "RECTIFY", &sys.mfx_rect, 0, 127, EK_NUM, 0},
    {"MOD FX", "MIX", &sys.mfx_mix, 0, 127, EK_NUM, 0},
    {"PLAY", "WHEEL TARGET", &sys.mw_target, 0, FEG_NT - 1, EK_LIST, FEG_T_NAME},
    {"PLAY", "WHEEL DEPTH", &sys.mw_depth, 0, 127, EK_NUM, 0},
    {"PLAY", "VELOCITY", &sys.vel_to, 0, VEL_TO_N - 1, EK_LIST, VEL_TO_NAME},
    {"PLAY", "VEL AMOUNT", &sys.vel_amt, 0, 127, EK_NUM, 0},
};
#define ED_N (sizeof ED_PAR / sizeof ED_PAR[0])
EXPORT(emu_par_count) uint32_t emu_par_count(void) { return ED_N; }
EXPORT(emu_par_group) char *emu_par_group(uint32_t i) { str_cpy(ed_str, ED_PAR[i % ED_N].group, sizeof ed_str); return ed_str; }
EXPORT(emu_par_name) char *emu_par_name(uint32_t i) { str_cpy(ed_str, ED_PAR[i % ED_N].name, sizeof ed_str); return ed_str; }
EXPORT(emu_par_lo) int32_t emu_par_lo(uint32_t i) { return ED_PAR[i % ED_N].lo; }
EXPORT(emu_par_hi) int32_t emu_par_hi(uint32_t i) { return ED_PAR[i % ED_N].hi; }
EXPORT(emu_par_get) int32_t emu_par_get(uint32_t i)
{
    const ed_par_t *P = &ED_PAR[i % ED_N];
    uint32_t v = *P->p;
    if (P->kind == EK_OCT)
        return (int8_t)v;
    if (P->kind == EK_HEIGHT)
        return (int32_t)(v & 127u) * (v & 0x80u ? -1 : 1);
    return P->kind == EK_LIST ? (int32_t)v : (int32_t)(v & 127u);
}
EXPORT(emu_par_set) void emu_par_set(uint32_t i, int32_t v)
{
    const ed_par_t *P = &ED_PAR[i % ED_N];
    v = clamp(v, P->lo, P->hi);
    if (P->kind == EK_HEIGHT)
        *P->p = (uint8_t)(v < 0 ? 0x80u | (uint32_t)-v : (uint32_t)v);
    else
        *P->p = (uint8_t)v;
    ui_force = 1;
}
EXPORT(emu_par_text) char *emu_par_text(uint32_t i)   /* the value as the firmware shows it */
{
    const ed_par_t *P = &ED_PAR[i % ED_N];
    int32_t v = emu_par_get(i);
    char b[24];
    switch (P->kind) {
    case EK_LIST:
        if (P->p == &sys.osc)
            str_cpy(ed_str, v ? NP_NAME[(v - 1) % NP_COUNT] : "CHUA CIRCUIT", sizeof ed_str);
        else
            str_cpy(ed_str, P->list[v], sizeof ed_str);
        break;
    case EK_TIME_A: env_time(b, sizeof b, (uint32_t)v); str_cpy(ed_str, b, sizeof ed_str); break;
    case EK_TIME_D: env_time_f(b, sizeof b, DEC_FRAMES[v & 127]); str_cpy(ed_str, b, sizeof ed_str); break;
    case EK_TIME_R: env_time_f(b, sizeof b, REL_FRAMES[v & 127]); str_cpy(ed_str, b, sizeof ed_str); break;
    case EK_OCT:
    case EK_HEIGHT:
        str_cpy(ed_str, v > 0 ? "+" : "", sizeof ed_str);
        fmt_int(b, v);
        ui_cat(ed_str, sizeof ed_str, b);
        break;
    default:
        fmt_int(ed_str, v);
        break;
    }
    return ed_str;
}
