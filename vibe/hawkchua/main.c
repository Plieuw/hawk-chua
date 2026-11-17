/* SPDX-License-Identifier: GPL-3.0-only */
/* HAWK CHUA as a VIBE FM firmware (one .wasm: the browser's Playground at play.thefirmwarehub.com, VIBE FM VST, the
 * VIBE FM): the FM-1 firmware's own sources (firmware/src: the synth, the keys, the screen, the presets) unchanged,
 * this file in place of the FM-1's hardware (hal/, main.c, audio.c, usb.c, the flash):
 *   - the audio flow (vibe_on_audio) is the FM-1's audio interrupt: keys_block + synth_render in blocks of 32 frames
 *     at 44.1 kHz, resampled to the host's rate; it also runs the FM-1's main loop's ui_input every 15 ms, so all
 *     the firmware's state has one writer, as on the FM-1 (where the interrupt and the loop never run at once)
 *   - the control flow (vibe_on_event, vibe_on_draw) only hands the controls over (one ring, one writer) and draws:
 *     ui_draw into the FM-1's 240 x 240 picture, shown 280 x 280 in the middle of the 480 x 320 display with the
 *     pages on the left, the encoders on the right and the soft keys' captions below, in the firmware's colours
 *   - the flash: the settings and the 8 objects of user presets kept in RAM, written to the host's store
 *     (vibe_store_*) from the control flow
 * The FM-1's panel on the VIBE: K1..K6 (SOUND ENV LFO ARP FX SEQ) = HOME ENV LFO ARP FX SEQ, F1..F6 = EDIT GLO SCL
 * SAVE REC PLAY, encoders 1..4 = KNOB 1..4, A = SELECT, B = ALGORITHM, DATA = PRESETS, OCT -/+, RETURN = the HOME
 * page, REC / PLAY / STOP the transport, the keys and pads play (velocity), MIDI in as on the FM-1's USB. */
#include "vibe.h"
#include "vibe_gfx.h"

VIBE_PARALLEL_AUDIO

/* ------------------------------------------------------------ libc --- */
#define memset felucca_memset              /* (libc.c's own, beside the SDK's) */
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "../../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
#include <stddef.h>
void *memset(void *d, int c, size_t n);          /* (the SDK's) */
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);

/* ------------------------------------------------- the LCD: a picture --- */
static uint16_t fb240[240 * 240];                /* RGB565, as the FM-1's panel shows it */
static void lcd_sync(void) {}
static void lcd_wake_now(void) {}
static uint8_t lcd_fresh = 1;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    lcd_fresh = 1;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb240[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb240[(y + j) * 240u + x + i] = (uint16_t)(px[j * w + i] >> 8 | px[j * w + i] << 8);   /* (byte-swapped) */
}
#include "../../firmware/src/gfx.c"

/* ------------------------------------------------- the FM-1's inputs --- */
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "v2.1.7"
#endif
#include "../../firmware/src/core.h"
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static struct { volatile uint32_t notes, buttons; } fm1_in;   /* (the audio flow's: buttons from the ring) */
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static int fm1_led_anim_on(void) { return 0; }
static int32_t enc_acc[7];                       /* detents per matrix encoder (the audio flow's) */
static int32_t fm1_enc_take(uint32_t e) { int32_t s = enc_acc[e]; enc_acc[e] = 0; return s; }
static uint32_t in_edges;                        /* buttons pressed since ui_input last looked (the audio flow's) */
static uint32_t fm1_input_edges(uint32_t *released) { uint32_t p = in_edges; (void)released; in_edges = 0; return p; }
static struct { uint8_t config, suspended; } usb = {0, 0};
static uint8_t flash_ok = 1;

/* the flash: the settings and the user presets' objects, in RAM, the host's store behind */
enum { OBJ_SETTINGS, OBJ_USER0, OBJ_COUNT = OBJ_USER0 + 8 };
#define OBJ_MAX 3840u
static uint8_t flash_store[OBJ_COUNT][OBJ_MAX];
static int32_t saved_len[OBJ_COUNT];             /* (-1: none) */
static volatile uint32_t saved_n[OBJ_COUNT];     /* saves (the audio flow counts) */
static uint32_t stored_n[OBJ_COUNT];             /* ... written to the store (the control flow's) */
static uint8_t store_tmp[OBJ_MAX];
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    if (obj >= OBJ_COUNT || saved_len[obj] < 0 || (uint32_t)saved_len[obj] > max)
        return -1;
    memcpy(dst, flash_store[obj], (uint32_t)saved_len[obj]);
    return saved_len[obj];
}
static int st_save(uint32_t obj, const void *src, uint32_t len)
{
    if (obj >= OBJ_COUNT || len > OBJ_MAX)
        return -1;
    memcpy(flash_store[obj], src, len);
    saved_len[obj] = (int32_t)len;
    VIBE_RELEASE();
    saved_n[obj]++;                              /* (the control flow writes it out) */
    return 0;
}
static void store_name(char *b, uint32_t obj)
{
    str_cpy(b, "obj0.bin", 12);
    b[3] = (char)('0' + obj);
}
static void store_flush(void)                    /* (the control flow: what the firmware saved, to the host) */
{
    uint32_t k;
    for (k = 0; k < OBJ_COUNT; k++) {
        uint32_t n = saved_n[k];
        if (n != stored_n[k]) {
            char nm[12];
            int32_t len;
            VIBE_ACQUIRE();
            len = saved_len[k];
            if (len < 0)
                continue;
            memcpy(store_tmp, flash_store[k], (uint32_t)len);
            if (saved_n[k] != n)                 /* (saved again meanwhile: next time) */
                continue;
            store_name(nm, k);
            vibe_store_write(nm, store_tmp, len);
            stored_n[k] = n;
        }
    }
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec(void) { return 0xB36014u; }
static int fl_id_ok(uint32_t id) { return id == 0x856014u || id == 0xB36014u; }
static int fl_emulated(void) { return 0; }
#define FL_FAR(f) fl_jedec

/* the MIDI ring keys.c reads (usb.c's): filled in the audio flow from the controls' ring */
#define MQ 64u
static uint32_t midi_in_q[MQ];
static uint8_t midi_in_source[MQ];
static volatile uint32_t mi_w, mi_r;
static volatile uint8_t midi_in_overflow;
static void midi_out_event(uint32_t pkt) { (void)pkt; }
static void midi_push(uint32_t pkt)              /* (the audio flow) */
{
    if (mi_w - mi_r >= MQ)
        return;
    midi_in_q[mi_w % MQ] = pkt;
    midi_in_source[mi_w % MQ] = 0;               /* (taken whatever the GLO page's inputs) */
    mi_w++;
}

/* one core: the second's half of the voices on the first */
#define C1_ON() 0
#define C1_RUN(fn, arg) ((void)(fn), (void)(arg), 0)
#define C1_WAIT() 0

#include "../../firmware/src/panel.c"
#include "../../firmware/src/synth.c"
#include "../../firmware/src/keys.c"
#include "../../firmware/src/ui.c"

/* ------------------------------------------------ control -> audio ring --- */
enum { M_BTN_DOWN = 1, M_BTN_UP, M_ENC };
#define VQ 256u
static uint32_t cq[VQ];
static volatile uint32_t cq_w, cq_r;             /* (cq_w: the control flow's, cq_r: the audio flow's) */
static void cq_put(uint32_t kind, uint32_t a, uint32_t b)
{
    if (cq_w - cq_r >= VQ)
        return;
    cq[cq_w % VQ] = kind << 28 | (a & 0xFFFu) << 16 | (b & 0xFFFFu);
    VIBE_RELEASE();
    cq_w++;
}
static void cq_midi(uint32_t st, uint32_t d1, uint32_t d2)   /* a USB-MIDI packet, its top bit set (d2 < 128 leaves it free) */
{
    if (cq_w - cq_r >= VQ)
        return;
    cq[cq_w % VQ] = (st >> 4) | st << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24 | 0x80000000u;
    VIBE_RELEASE();
    cq_w++;
}
static void cq_take(void)                        /* (the audio flow, each block) */
{
    while (cq_r != cq_w) {
        uint32_t m;
        VIBE_ACQUIRE();
        m = cq[cq_r % VQ];
        cq_r++;
        if (m & 0x80000000u) {                   /* a MIDI packet (d2 < 128: its top bit free) */
            midi_push(m & 0x7FFFFFFFu);
            continue;
        }
        switch (m >> 28) {
        case M_BTN_DOWN:
            fm1_in.buttons |= btn_bit((m >> 16) & 0xFFu);
            in_edges |= btn_bit((m >> 16) & 0xFFu);
            break;
        case M_BTN_UP:
            fm1_in.buttons &= ~btn_bit((m >> 16) & 0xFFu);
            break;
        case M_ENC:
            enc_acc[panel.enc[(m >> 16) & 0xFFu]] += (int16_t)(m & 0xFFFFu);
            break;
        }
    }
}

/* ------------------------------------------------------------- audio --- */
static float host_sr = 44100.0f;
static int32_t ablk[2 * CTL];                    /* the last block, Q15 stereo */
static uint32_t ablk_i = CTL;                    /* frames of it used */
static float rs_pos;                             /* the resampler: between prev and the next frame */
static float rs_prev[2], rs_next[2];
static uint32_t ms_frac, ui_last;
static int booted;

static void fm1_block(void)                      /* the FM-1's audio interrupt, one block */
{
    cq_take();
    keys_block();
    synth_render(ablk, CTL);
    ms_frac += CTL;
    while (ms_frac >= FS / 1000u) {
        ms_frac -= FS / 1000u;
        fm1_ms++;
    }
    if (fm1_ms - ui_last >= 15u) {               /* the FM-1's main loop: the controls, the presets, the settings */
        ui_last = fm1_ms;
        ui_input();
    }
    ablk_i = 0;
}
static void next_frame(void)
{
    if (ablk_i >= CTL)
        fm1_block();
    rs_prev[0] = rs_next[0];
    rs_prev[1] = rs_next[1];
    rs_next[0] = (float)ablk[2 * ablk_i] * (1.0f / 32768.0f);
    rs_next[1] = (float)ablk[2 * ablk_i + 1] * (1.0f / 32768.0f);
    ablk_i++;
}

void vibe_on_audio(const float *in, float *out_l, float *out_r, int frames)
{
    float step = (float)FS / host_sr;
    int i;
    (void)in;
    for (i = 0; i < frames; i++) {
        float l, r;
        while (rs_pos >= 1.0f) {
            rs_pos -= 1.0f;
            next_frame();
        }
        l = rs_prev[0] + (rs_next[0] - rs_prev[0]) * rs_pos;
        r = rs_prev[1] + (rs_next[1] - rs_prev[1]) * rs_pos;
        rs_pos += step;
        l = l > 1.0f ? 1.0f : l < -1.0f ? -1.0f : l;
        r = r > 1.0f ? 1.0f : r < -1.0f ? -1.0f : r;
        out_l[i] = l;
        out_r[i] = r;
    }
}

/* ------------------------------------------------------------- init --- */
void vibe_on_init(float sample_rate)
{
    uint32_t k;
    host_sr = sample_rate > 8000.0f ? sample_rate : 44100.0f;
    for (k = 0; k < OBJ_COUNT; k++) {            /* the flash, from the host's store */
        char nm[12];
        int n;
        store_name(nm, k);
        n = vibe_store_read(nm, flash_store[k], OBJ_MAX);
        saved_len[k] = n;
    }
    settings_boot();
    style_apply();
    keys_init();
    tape_init();
    sys.master_q12 = 4088;                       /* (the MASTER pot up: the host has the volume) */
    ui_force = 1;
    vf_mode(VF_ON_HOLD);                         /* (DATA is PRESETS: FOCUS on ENTER held) */
    vibe_label(VB_F1, "EDIT");
    vibe_label(VB_F2, "GLO");
    vibe_label(VB_F3, "SCL");
    vibe_label(VB_F4, "SAVE");
    vibe_label(VB_F5, "REC");
    vibe_label(VB_F6, "PLAY");
    vibe_label(VB_K1, "SOUND");
    vibe_label(VB_K2, "ENV");
    vibe_label(VB_K3, "LFO");
    vibe_label(VB_K4, "ARP");
    vibe_label(VB_K5, "FX");
    vibe_label(VB_K6, "SEQ");
    booted = 1;
}

/* ------------------------------------------------------------ events --- */
static int help;                                 /* HOME: the controls' overview open */
static float enc_frac[VE_COUNT];
static uint8_t key_note[VIBE_KEYS], pad_note[VIBE_PADS];
static int btn_of(int vb)                        /* the FM-1 button a VIBE button is (-1: none) */
{
    switch (vb) {
    case VB_K1: return B_HOME;
    case VB_K2: return B_ENV;
    case VB_K3: return B_LFO;
    case VB_K4: return B_ARP;
    case VB_K5: return B_FX;
    case VB_K6: return B_SEQ;
    case VB_F1: return B_EDIT;
    case VB_F2: return B_GLO;
    case VB_F3: return B_SCL;
    case VB_F4: return B_SAVE;
    case VB_F5: case VB_REC: return B_REC;
    case VB_F6: case VB_PLAY: return B_PLAY;
    case VB_STOP: return sys.arp_on || sys.k_on ? B_PLAY : -1;   /* (STOP: only what plays) */
    case VB_OCT_DOWN: return B_OCTDN;
    case VB_OCT_UP: return B_OCTUP;
    case VB_RETURN: return B_HOME;
    case VB_MENU: return B_GLO;
    default: return -1;
    }
}
static int role_of(int ve)                       /* the FM-1 encoder a VIBE encoder is */
{
    static const int8_t R[VE_COUNT] = {EN_K1, EN_K2, EN_K3, EN_K4, EN_SELECT, EN_ALGO, EN_PRESET};
    return ve >= 0 && ve < VE_COUNT ? R[ve] : -1;
}
static uint32_t rx_ch(void) { return sys.midi_rx ? (sys.midi_rx - 1u) % 16u : 0u; }
static void play(uint8_t *slot, int note, int down, float vel)
{
    if (down) {
        int v = (int)(vel * 127.0f + 0.5f);
        if (note < 0 || note > 127)
            return;
        *slot = (uint8_t)(note + 1);
        cq_midi(0x90u | rx_ch(), (uint32_t)note, (uint32_t)(v < 1 ? 1 : v > 127 ? 127 : v));
    } else if (*slot) {
        cq_midi(0x80u | rx_ch(), *slot - 1u, 0);
        *slot = 0;
    }
}

void vibe_on_event(const vibe_event *e)
{
    int id, r = vf_event(e, &id);
    if (r != VF_NO)
        return;
    switch (e->type) {
    case VIBE_EV_KEY:                            /* the keys: C3 .. C5 at OCT 0 */
        if (e->id < VIBE_KEYS)
            play(&key_note[e->id], 48 + e->id + 12 * sys.octave, e->down, e->value);
        break;
    case VIBE_EV_PAD:                            /* the pads: C2 .. D#3, a row a fourth */
        if (e->id < VIBE_PADS) {
            play(&pad_note[e->id], 36 + e->id + 12 * sys.octave, e->down, e->value > 0 ? e->value : 0.8f);
            vibe_led_pad(e->id, 0xFF8A1Fu, e->down ? 1.0f : 0.15f);
        }
        break;
    case VIBE_EV_ENCODER: {
        int ro = role_of(e->id), n;
        if (ro < 0)
            break;
        enc_frac[e->id] += e->value;
        n = (int)enc_frac[e->id];
        enc_frac[e->id] -= (float)n;
        if (n)
            cq_put(M_ENC, (uint32_t)ro, (uint32_t)(n & 0xFFFF));
        break;
    }
    case VIBE_EV_BUTTON:
        if (e->id == VB_HOME) {                  /* HOME: what every control does (again / RETURN: closed) */
            if (e->down)
                help = !help;
            break;
        }
        if (e->id == VB_RETURN && help) {
            if (e->down)
                help = 0;
            break;
        }
        if (e->id == VB_LEFT || e->id == VB_RIGHT) {   /* ◀ ▶: the presets */
            if (e->down)
                cq_put(M_ENC, EN_PRESET, (uint32_t)((e->id == VB_RIGHT ? 1 : -1) & 0xFFFF));
            break;
        }
        {
            int b = btn_of(e->id);
            if (b >= 0)
                cq_put(e->down ? M_BTN_DOWN : M_BTN_UP, (uint32_t)b, 0);
        }
        break;
    case VIBE_EV_MIDI:                           /* MIDI in, as the FM-1's USB */
        if (e->midi[0] >= 0x80u && e->midi[0] < 0xF0u)
            cq_midi(e->midi[0], e->midi[1], e->midi[2]);
        else if (e->midi[0] >= 0xF8u)
            cq_midi(e->midi[0], 0, 0);
        break;
    case VIBE_EV_SAVE:                           /* the host keeps the state now (no block runs meanwhile) */
        if (booted)
            (void)settings_save();
        store_flush();
        break;
    default:
        break;
    }
}

/* ------------------------------------------------------------- draw --- */
static uint32_t rgb_of(uint16_t c)               /* RGB565 -> 0xRRGGBB */
{
    uint32_t r = c >> 11 & 31u, g = c >> 5 & 63u, b = c & 31u;
    return (r << 3 | r >> 2) << 16 | (g << 2 | g >> 4) << 8 | (b << 3 | b >> 2);
}
#define SC 280                                   /* the FM-1's picture, scaled */
#define SX 100
#define SY 0
static uint16_t sc_x0[SC], sc_w[SC];             /* (the source column and the weight of the next, Q8) */
static int sc_ready;
static void scale_tables(void)
{
    int i;
    for (i = 0; i < SC; i++) {
        uint32_t p = (uint32_t)i * 239u * 256u / (SC - 1u);
        sc_x0[i] = (uint16_t)(p >> 8);
        sc_w[i] = (uint16_t)(p & 255u);
    }
    sc_ready = 1;
}
static uint16_t mix565(uint16_t a, uint16_t b, uint32_t w)   /* a .. b, w of 256 */
{
    uint32_t ra = a >> 11, ga = a >> 5 & 63u, ba = a & 31u, rb = b >> 11, gb = b >> 5 & 63u, bb = b & 31u;
    uint32_t r = (ra * (256u - w) + rb * w) >> 8, g = (ga * (256u - w) + gb * w) >> 8, bl = (ba * (256u - w) + bb * w) >> 8;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static void blit_scaled(uint16_t *fb)            /* the 240 x 240 picture, bilinear, at SX, SY */
{
    int x, y;
    if (!sc_ready)
        scale_tables();
    for (y = 0; y < SC; y++) {
        uint32_t y0 = sc_x0[y], y1 = y0 + 1u < 240u ? y0 + 1u : y0, wy = sc_w[y];
        const uint16_t *r0 = &fb240[y0 * 240u], *r1 = &fb240[y1 * 240u];
        uint16_t *d = &fb[(SY + y) * 480 + SX];
        for (x = 0; x < SC; x++) {
            uint32_t x0 = sc_x0[x], x1 = x0 + 1u < 240u ? x0 + 1u : x0, wx = sc_w[x];
            uint16_t t = wx ? mix565(r0[x0], r0[x1], wx) : r0[x0];
            uint16_t b = wy ? (wx ? mix565(r1[x0], r1[x1], wx) : r1[x0]) : t;
            d[x] = wy ? mix565(t, b, wy) : t;
        }
    }
}

static const char *const HELP[] = {
    "KEYS, PADS", "play, MIDI in",
    "ENCODER 1-4", "KNOB 1-4",
    "A / B", "SELECT / ALGO",
    "DATA, < >", "PRESETS",
    "SOUND", "HOME, BANK",
    "ENV", "ENV, MOD ENV",
    "LFO", "FREE EG",
    "ARP", "ARP, GROOVE",
    "FX", "MOD FX, FX, SPACE",
    "SEQ", "SEQ, LOGIC",
    "F1 EDIT", "wheel, velocity",
    "F2 GLO", "MIDI, SOUND, STYLE",
    "F3 SCL", "BASIS, ORBIT",
    "F4 SAVE", "the sound",
    "F5 REC", "TAPE, record",
    "F6 PLAY", "start / stop",
    "OCT - / +", "octave",
    "RETURN", "HOME",
    "ENTER held", "FOCUS",
};
static const char *const K_NAME[6] = {"SOUND", "ENV", "LFO", "ARP", "FX", "SEQ"};
static const uint32_t K_PAGES[6] = {1u << 0 | 1u << 14, 1u << 1 | 1u << 5, 1u << 8, 1u << 6 | 1u << 18,
                                    1u << 20 | 1u << 2 | 1u << 3, 1u << 9 | 1u << 17};   /* (the pages each key cycles) */
static const char *const F_NAME[6] = {"EDIT", "GLO", "SCL", "SAVE", "REC", "PLAY"};
static const uint32_t F_PAGES[6] = {1u << 11, 1u << 7 | 1u << 16 | 1u << 19, 1u << 10, 0, 1u << 12 | 1u << 13, 0};

void vibe_on_draw(uint16_t *fb)
{
    uint32_t bg = rgb_of(T_BG), th = rgb_of(T_THEME), dim = rgb_of(T_DIM), mid = rgb_of(T_MID), tx = rgb_of(T_TEXT);
    int i;
    store_flush();
    vg_begin(fb);
    if (help) {
        vg_clear(bg);
        vg_controls("HAWK CHUA", HELP, (int)(sizeof HELP / sizeof HELP[0] / 2u));
        return;
    }
    ui_draw();                                   /* the FM-1's screen, into its picture */
    vg_clear(bg);
    blit_scaled(fb);
    vg_rect(SX - 2, 0, 1, SC, vg_mix(bg, dim, 0.5f));
    vg_rect(SX + SC + 1, 0, 1, SC, vg_mix(bg, dim, 0.5f));
    /* left: the pages on K1..K6 */
    vg_text("HAWK", 50, 6, VG_BOLD, th, VG_CENTER);
    vg_text("CHUA", 50, 22, VG_BOLD, th, VG_CENTER);
    for (i = 0; i < 6; i++) {
        int y = 46 + i * 38, on = (K_PAGES[i] >> (sys.page & 31u)) & 1u;
        vg_round(8, y, 84, 32, 6, on ? th : vg_mix(bg, dim, 0.35f));
        vg_text(K_NAME[i], 50, y + 8, VG_BOLD, on ? bg : mid, VG_CENTER);
    }
    /* right: the encoders A, B, DATA, the octave */
    {
        static const char *const EN[3][2] = {{"A", "SELECT"}, {"B", "ALGO"}, {"DATA", "PRESETS"}};
        char b[12];
        for (i = 0; i < 3; i++) {
            int y = 10 + i * 56;
            vg_text(EN[i][0], 430, y, VG_SMALL, dim, VG_CENTER);
            vg_text(EN[i][1], 430, y + 16, VG_BOLD, tx, VG_CENTER);
        }
        vg_text("OCT", 430, 186, VG_SMALL, dim, VG_CENTER);
        b[0] = sys.octave > 0 ? '+' : sys.octave < 0 ? '-' : ' ';
        vg_itoa(sys.octave < 0 ? -sys.octave : sys.octave, b + 1);
        vg_text(sys.octave ? b : "0", 430, 202, VG_BIG, th, VG_CENTER);
    }
    /* below: the soft keys */
    for (i = 0; i < 6; i++) {
        int x = 4 + i * 79, on = (F_PAGES[i] >> (sys.page & 31u)) & 1u;
        vg_round(x, 286, 73, 30, 5, on ? th : vg_mix(bg, dim, 0.35f));
        vg_text(F_NAME[i], x + 36, 293, VG_BOLD, on ? bg : i == 3 ? th : mid, VG_CENTER);
    }
    /* FOCUS: every control reachable with DATA + ENTER */
    vf_begin();
    for (i = 0; i < 4; i++)                      /* (the screen's four knob cells, KNOB 1..4) */
        vf_encoder(VE_1 + i, SX + i * 70, SY + 175, 70, 105);
    for (i = 0; i < 6; i++)
        vf_key(VB_K1 + i, 8, 46 + i * 38, 84, 32);
    vf_encoder(VE_A, 384, 8, 92, 50);
    vf_encoder(VE_B, 384, 64, 92, 50);
    for (i = 0; i < 6; i++)
        vf_key(VB_F1 + i, 4 + i * 79, 286, 73, 30);
    vf_end(th);
}
