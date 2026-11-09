/* SPDX-License-Identifier: GPL-3.0-only */
/* The screen without the hardware: firmware/src/ui.c and gfx.c (the real drawing code) against a 240 x 240
 * framebuffer instead of the LCD, with synth.c running (the attractor's trace), a few states drawn and saved as raw
 * RGB565 (tests/ui_shot.py makes PNGs): every view, every regime, the presets, the widest values.
 * Also checks that no line is drawn outside the screen.
 *   tests/ui_shot OUT_DIR      (run_tests.sh: PNGs in build/host/ui) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define memset felucca_memset
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp

/* ------------------------------------------------------------ the LCD --- */
static uint16_t fb[240 * 240];
static int off_screen;
static void lcd_sync(void) {}
static void lcd_wake_now(void) {}
static uint8_t lcd_fresh = 1;          /* (as lcd.c: gfx.c's cv_blit sends only the bands that changed) */
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    lcd_fresh = 1;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    uint32_t i, j;
    if (x + w > 240u || y + h > 240u)
        off_screen++;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            if (x + i < 240u && y + j < 240u)
                fb[(y + j) * 240u + x + i] = (uint16_t)(px[j * w + i] >> 8 | px[j * w + i] << 8);   /* (byte-swapped) */
}
#include "../firmware/src/gfx.c"

/* ------------------------------------------------- what ui.c reads --- */
#define FELUCCA_VERSION "v0.1"
#include "../firmware/src/core.h"
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
static struct { uint8_t config, suspended; } usb = {1, 0};
static uint8_t flash_ok = 1;
enum { OBJ_SETTINGS, OBJ_USER0, OBJ_COUNT = OBJ_USER0 + 8 };
static uint8_t flash_store[OBJ_COUNT][3840];
static int saved[OBJ_COUNT];
static int st_load(uint32_t obj, void *dst, uint32_t max) { if (obj >= OBJ_COUNT || !saved[obj]) return -1; memcpy(dst, flash_store[obj], max); return (int)max; }
static int st_save(uint32_t obj, const void *src, uint32_t len) { if (obj >= OBJ_COUNT || len > 3840u) return -1; memcpy(flash_store[obj], src, len); saved[obj] = 1; return 0; }
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec(void) { return 0xB36014u; }   /* (the newer units' chip: saving as on the first) */
static int fl_id_ok(uint32_t id) { return id == 0x856014u || id == 0xB36014u; }
static int fl_emulated(void) { return 0; }      /* (a host build: not the emulated FM-1 of fm1-emu) */
#define FL_FAR(f) fl_jedec
static void midi_out_event(uint32_t pkt) { (void)pkt; }   /* (usb.c: not in this test) */
#include "../firmware/src/panel.c"
#include "../firmware/src/synth.c"
static volatile uint8_t keys_panic_req;   /* (keys.c: not in this test) */
static volatile uint16_t keys_prog_req, keys_prog_req_b;
#include "../firmware/src/ui.c"
/* remote.c over a fake USB: frames in from rm_in, messages out into rm_log */
#define FELUCCA_OTA 1
#define FELUCCA_ID "FM-1_900"
static uint8_t rm_inbuf[64];
static uint32_t rm_inlen, rm_inready;
static uint8_t rm_log[80000];
static uint32_t rm_logn, rm_msgs;
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rm_inready) return 0; *p = rm_inbuf; *n = rm_inlen; return 1; }
static void ota_frame_done(void) { rm_inready = 0; }
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (rm_logn + n <= sizeof rm_log) { memcpy(rm_log + rm_logn, p, n); rm_logn += n; }
    rm_msgs++;
    return 0;
}
#include "../firmware/src/remote.c"
static void rm_cmd(const uint8_t *b, uint32_t n)       /* one host message (without F0 / F7), handled */
{
    memcpy(rm_inbuf, b, n);
    rm_inlen = n;
    rm_inready = 1;
    rm_logn = rm_msgs = 0;
    rm_service();
}

static int32_t abuf_[2 * CTL];
static void run_ms(uint32_t ms)                  /* the audio for ms, the clock with it */
{
    uint32_t n = ms * FS / 1000u / CTL + 1u;
    while (n--) {
        tape_block();                            /* (keys.c's part of the interrupt: the tape's transport) */
        synth_render(abuf_, CTL);
        fm1_ms_frac += CTL;
        while (fm1_ms_frac >= FS / 1000u) {
            fm1_ms_frac -= FS / 1000u;
            fm1_ms++;
        }
    }
}

static int stale_bands;                          /* frames whose plot on the panel is not the canvas (cv_blit's bands) */
static void frames(uint32_t n)                  /* n UI frames, 40 ms of audio each (the scales settle) */
{
    while (n--) {
        run_ms(40);
        ui_draw();
        if (cv_w == 240u && cv_h == PLOT_H) {    /* (the plot drawn last: the panel must show it all) */
            uint32_t i;
            for (i = 0; i < 240u * PLOT_H; i++)
                if (fb[PLOT_Y * 240u + i] != (uint16_t)(cv_px[i] >> 8 | cv_px[i] << 8)) {
                    stale_bands++;
                    break;
                }
        }
    }
}

static int shot(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    snprintf(path, sizeof path, "%s/%s.rgb565", dir, name);
    if (!(f = fopen(path, "wb")))
        return 1;
    fwrite(fb, 2, 240 * 240, f);
    fclose(f);
    return 0;
}


/* every preset: a chord held 1 s, its peak and RMS; none much quieter than the others (14 dB under their median
 * peak), none into the clip (PRESET_LEVELS=1 prints all) */
#include <math.h>
static int cmp_d(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static int preset_levels(void)
{
    static double pdb[NPRESETS], rdb[NPRESETS], srt[NPRESETS];
    static uint32_t clip[NPRESETS];
    uint32_t i, b, j, bad = 0;
    int verbose = getenv("PRESET_LEVELS") != 0;
    double med;
    for (i = 0; i < NPRESETS; i++) {
        double e = 0;
        int32_t pk = 0;
        uint32_t n = 0;
        synth_kill();
        preset_load(i);
        synth_note_on(48, 100); synth_note_on(55, 100); synth_note_on(60, 100);
        for (b = 0; b < FS * 3u / 2u / CTL; b++) {
            if (b == FS / CTL)
                synth_note_off(48), synth_note_off(55), synth_note_off(60);
            synth_render(abuf_, CTL);
            for (j = 0; j < 2u * CTL; j++) {
                int32_t v = abuf_[j], a = v < 0 ? -v : v;
                pk = a > pk ? a : pk;
                clip[i] += a >= 8388000;
                e += (double)v * v;
                n++;
            }
        }
        pdb[i] = srt[i] = 20 * log10((pk + 1) / 8388608.0);
        rdb[i] = 10 * log10(e / n / (8388608.0 * 8388608.0) + 1e-20);
    }
    synth_kill();
    qsort(srt, NPRESETS, sizeof srt[0], cmp_d);
    med = srt[NPRESETS / 2];
    for (i = 0; i < NPRESETS; i++) {
        int w = pdb[i] < med - 14 || clip[i] > 0u;
        if (verbose || w)
            printf("  %3u %-16s peak %6.1f dB  rms %6.1f dB  clip %u%s\n", i, PRESETS[i].name, pdb[i] - med,
                   rdb[i] - med, clip[i], w ? "  <-" : "");
        bad |= w;
    }
    if (bad)
        printf("presets: some much quieter than the rest, or clipping\n");
    return bad != 0;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    int bad = 0;
    uint32_t i;
    char name[32];
    palette_set(UI_PALETTE);
    {   /* SAVE overwrites the preset playing (its name kept); PRESETS / program change load the saved one; the remote's
         * PRESETS sends all 128 as C lines (the saved one as saved), RESET PRESETS brings the built-in back */
        static const uint8_t PRE[] = {0x7D, 0x43, 0x48, 0x06}, CLR[] = {0x7D, 0x43, 0x48, 0x07, 'C', 'L', 'R'};
        int ok = 1;
        uint32_t k, lines = 0;
        preset_load(5);
        sys.chaos = 17; sys.sh_space = SS_MOSQUE; sys.sh_mix = 50; sys.fx_damp = 33;
        ok &= preset_edited();
        edges = btn_bit(B_SAVE);
        ui_input();
        ok &= upre_has(5) && !upre_has(6) && !preset_edited();
        preset_load(6);
        ok &= sys.chaos != 17u || sys.sh_space != SS_MOSQUE;
        preset_load(5);
        ok &= sys.chaos == 17u && sys.sh_space == SS_MOSQUE && sys.sh_mix == 50u && sys.fx_damp == 33u && !preset_edited();
        memset(upre_bits, 0, sizeof upre_bits);          /* (as after a power cycle: from the flash) */
        memset(&upre_buf, 0, sizeof upre_buf); upre_cur = 0xFF;
        upre_boot();
        ok &= upre_has(5);
        rm_cmd(PRE, sizeof PRE);
        rm_log[rm_logn < sizeof rm_log ? rm_logn : sizeof rm_log - 1u] = 0;
        for (k = 0; k + 4u < rm_logn; k++)
            if (rm_log[k] == 0xF0 && rm_log[k + 4] == 0x52)
                lines++;
        ok &= lines == NPRESETS && strstr((char *)rm_log, "SAVED") && strstr((char *)rm_log, "{\"GRAND PIANO\", ");
        rm_cmd(CLR, sizeof CLR);
        ok &= !upre_has(5);
        {   /* a preset's FREE EG contour: FLUTE's jet moves with each note; the edited mark and SAVE see its base; a
             * preset without one turns it off */
            uint32_t b2;
            uint8_t lo = 255, hi = 0, st0;
            preset_load(73);                             /* FLUTE: EG_OVERTONE on FX STRUCTURE */
            st0 = sys.fx_struct;
            ok &= sys.feg_on && sys.feg_target == 8u && sys.feg_len == 0u;
            synth_note_on(57, 100);
            for (b2 = 0; b2 < 2u * FS / CTL; b2++) {
                feg_block(CTL, voice_clock);
                synth_render(abuf_, CTL);
                lo = sys.fx_struct < lo ? sys.fx_struct : lo;
                hi = sys.fx_struct > hi ? sys.fx_struct : hi;
            }
            ok &= lo < st0 && hi > st0 + 15u && !preset_edited();
            synth_kill();
            preset_load(0);                              /* GRAND PIANO: none */
            ok &= !sys.feg_on && sys.fx_struct == (PRESETS[0].fx_mix ? PRESETS[0].fx_struct : st0);   /* (no FX in
                                                  * it: the resonator's knobs left, FLUTE's base given back) */
            if (!ok) printf("preset FREE EG contour: jet %u .. %u around %u\n", lo, hi, st0);
        }
        {   /* the sound's octave: ACOUSTIC BASS two down; the key played still ends its note */
            uint32_t k3, found = 0;
            preset_load(32);
            ok &= sys.transpose == -2;
            synth_note_on(60, 100);
            for (k3 = 0; k3 < NVOICE; k3++)
                found |= voice[k3].stage == ST_ATTACK && voice[k3].note == 36u && voice[k3].key == 60u;
            synth_note_off(60);
            for (k3 = 0; k3 < NVOICE; k3++)
                ok &= !(voice[k3].key == 60u && (voice[k3].stage == ST_ATTACK || voice[k3].stage == ST_SUSTAIN));
            ok &= found;
            synth_kill();
            preset_load(0);
            ok &= sys.transpose == 0;
            if (!ok) printf("the sound's octave: failed\n");
        }
        {   /* the banks: program change after BANK SELECT (keys.c: bank x 128 + program + 1); PRESETS turns within the
             * bank, HOME held + PRESETS goes to the same program in the next bank; a SAVE in bank 4 stays there */
            uint32_t pg = sys.page;
            sys.page = 0;
            keys_prog_req = 2u * 128u + 5u + 1u;
            ui_input();
            ok &= sys.preset == 261u;
            fm1_ms += 1000;
            enc_steps[panel.enc[EN_PRESET]] = 1;
            ui_input();
            ok &= sys.preset == 262u;
            preset_load(383);
            fm1_ms += 1000;
            enc_steps[panel.enc[EN_PRESET]] = 1;
            ui_input();
            ok &= sys.preset == 256u;                        /* (wraps inside bank 3) */
            rm_btn = btn_bit(B_HOME);                        /* HOME on HOME: the BANK menu */
            ui_input();
            ok &= sys.page == 14u;
            fm1_ms += 1000;
            enc_steps[panel.enc[EN_PRESET]] = 1;             /* PRESETS there: the next bank, the same program */
            ui_input();
            ok &= sys.preset == 384u && !strcmp(PRESETS[384].name, "DOUBLE SCROLL");
            frames(3);
            bad |= shot(dir, "c_bank_menu");
            fm1_ms += 1000;
            enc_steps[panel.enc[EN_SELECT]] = 1;             /* SELECT too: round to bank 1 */
            ui_input();
            ok &= sys.preset == 0u;
            rm_btn = btn_bit(B_HOME);                        /* HOME again: back */
            ui_input();
            ok &= sys.page == 0u;
            preset_load(384);
            sys.chaos = 7;
            ok &= upre_save(384) == 0 && upre_has(384) && !upre_has(256) && !upre_has(0) && !upre_has(128);
            preset_load(0);
            preset_load(384);
            ok &= sys.chaos == 7u;
            ok &= upre_clear() == 0 && !upre_has(384);
            preset_load(0);
            sys.page = (uint8_t)pg;
            if (!ok) printf("banks: failed (preset %u)\n", sys.preset);
        }
        if (!ok) { printf("SAVE over a preset / remote PRESETS / RESET PRESETS: failed\n"); return 1; }
        printf("SAVE over a preset, remote PRESETS (512 lines), RESET PRESETS, 4 banks ok\n");
    }
    {   /* REMOTE: PING, a BUTTON (the ARP page), STATE, a TURN, SCREEN (the plot, back as it is), not ours ignored */
        static const uint8_t PING[] = {0x7D, 0x43, 0x48, 0x05}, BTN[] = {0x7D, 0x43, 0x48, 0x03, B_ARP},
                             ST[] = {0x7D, 0x43, 0x48, 0x02}, TURN[] = {0x7D, 0x43, 0x48, 0x04, EN_K3, 64 + 5},
                             SCR[] = {0x7D, 0x43, 0x48, 0x01}, OTHER[] = {0x00, 0x59, 0x11};
        int ok = 1;
        uint32_t y = 0, k, rows = 0;
        rm_cmd(PING, sizeof PING);
        ok &= rm_msgs == 1u && rm_log[4] == 0x51 && !memcmp(rm_log + 5, "FM-1_900", 8);
        rm_cmd(BTN, sizeof BTN);
        ui_input();
        ok &= sys.page == 6u;
        frames(2);
        rm_cmd(ST, sizeof ST);
        rm_log[rm_logn < sizeof rm_log ? rm_logn : sizeof rm_log - 1u] = 0;
        ok &= rm_msgs == 1u && rm_log[4] == 0x50 && strstr((char *)rm_log + 5, "PAGE 6") && strstr((char *)rm_log + 5, "BPM ");
        {
            uint8_t g0 = sys.arp_gate;
            rm_cmd(TURN, sizeof TURN);
            ui_input();
            ok &= sys.arp_gate > g0;
        }
        frames(2);
        rm_cmd(SCR, sizeof SCR);
        ok &= rm_msgs == (PLOT_H + 3u) / 4u + 1u;
        for (k = 0; k + 6u < rm_logn && ok; ) {             /* each message: unpack, compare with the canvas */
            uint32_t e = k + 5u, o = 0, i;
            static uint8_t un[2000];
            if (rm_log[k + 4] == 0x42) { rows = rm_log[k + 5] | rm_log[k + 6] << 7; break; }
            while (rm_log[e] != 0xF7) {
                uint8_t hi = rm_log[e++];
                for (i = 0; i < 7u && rm_log[e] != 0xF7; i++)
                    un[o++] = (uint8_t)(rm_log[e++] | ((hi >> i) & 1u) << 7);
            }
            y = un[0];
            for (i = 0; 1u + 2u * i + 1u < o; i++)
                ok &= (uint16_t)(un[1 + 2 * i] | un[2 + 2 * i] << 8) == cv_px[y * 240u + i];
            k = e + 1u;
        }
        ok &= rows == (uint32_t)PLOT_H;
        rm_cmd(OTHER, sizeof OTHER);
        ok &= rm_msgs == 0u && rm_inready;                  /* (left for the update's handshake) */
        rm_inready = 0;
        sys.page = 0; sys.arp_gate = 64;
        if (!ok) { printf("REMOTE: PING / BUTTON / STATE / TURN / SCREEN failed (msgs %u, page %u)\n", rm_msgs, sys.page); bad = 1; }
    }
    tape_init();
    bad |= preset_levels();

    draw_splash();
    bad |= shot(dir, "a_splash");
    sys.ui_style = US_PSYCHE;            /* PSYCHEDELIC's splash: shut, opening, open */
    style_apply();
    draw_splash();
    bad |= shot(dir, "a_splash_psy_0");
    splash_psy(500);
    bad |= shot(dir, "a_splash_psy_1");
    splash_psy(740);                     /* (docs/hawk-chua.png and card.png: this frame) */
    bad |= shot(dir, "a_splash_psy_2");
    sys.ui_style = US_INITIAL;
    style_apply();

    ui_force = 1;                        /* power-on: the ghost circuit, DOUBLE SCROLL, every view */
    run_ms(1500);
    for (i = 0; i < V_COUNT; i++) {
        sys.view = (uint8_t)i;
        frames(12);
        snprintf(name, sizeof name, "b_view_%u_%s", i, VIEW_NAME[i]);
        for (char *c = name; *c; c++)
            if (*c == '-') *c = '_';
        bad |= shot(dir, name);
    }

    sys.view = V_XY;                     /* every preset, as the PRESETS encoder loads them, a note held */
    synth_note_on(48, 100);
    for (i = 0; i < NPRESETS; i++) {
        enc_steps[panel.enc[EN_PRESET]] = i ? 1 : 0;
        ui_input();
        run_ms(700);
        frames(12);
        snprintf(name, sizeof name, "c_preset_%u", i);
        bad |= shot(dir, name);
    }
    synth_kill();

    sys.page = 1;                        /* the ENV page: a held note (its gate's level and cutoff), then a pluck */
    preset_load(0);
    synth_note_on(55, 100);
    frames(10);
    bad |= shot(dir, "e_env_held");
    preset_load(1);
    sys.release = 100;
    enc_steps[panel.enc[EN_K2]] = 30;    /* (DECAY turned: its cell lit) */
    ui_input();
    synth_note_on(60, 100);
    frames(3);
    bad |= shot(dir, "e_env_pluck");
    frames(12);
    bad |= shot(dir, "e_env_dimming");
    synth_kill();
    frames(2);
    bad |= shot(dir, "e_env_dark");
    sys.page = 0;
    edges = btn_bit(B_SCL);              /* the PAN page (SCL): BASIS turned, ORBIT, three notes where they are */
    ui_input();
    frames(2);
    bad |= shot(dir, "e_pan_mono");
    enc_steps[panel.enc[EN_K1]] = 90;
    enc_steps[panel.enc[EN_K2]] = 30;
    ui_input();
    synth_note_on(48, 100); synth_note_on(55, 100); synth_note_on(64, 100);
    frames(8);
    bad |= shot(dir, "e_pan_basis");
    ui_event_ms = 0;
    frames(2);
    bad |= shot(dir, "e_pan_status");
    edges = btn_bit(B_SCL);
    ui_input();
    sys.orbit = 0;
    sys.basis = 0;
    edges = btn_bit(B_REC);              /* the TAPE page: recording track 2 under a note */
    ui_input();
    enc_steps[panel.enc[EN_K2]] = 1;     /* (the blank track 2's knob arms it) */
    ui_input();
    if (sys.tape_arm != 1u) { printf("TAPE: KNOB 2 did not arm track 2\n"); bad = 1; }
    edges = btn_bit(B_REC);
    ui_input();
    synth_note_on(55, 110);
    frames(20);
    bad |= shot(dir, "k_tape_rec");
    synth_note_off(55);
    frames(40);
    bad |= shot(dir, "k_tape_play");
    enc_steps[panel.enc[EN_SELECT]] = 1; /* its SPEED menu: 3/4 (a fourth down): the BPM follows, the speed exact */
    ui_input();
    if (sys.page != 13u) { printf("TAPE: SELECT did not open the SPEED menu\n"); bad = 1; }
    {
        uint32_t bpm0 = sys.arp_bpm;
        enc_steps[panel.enc[EN_K2]] = -5;
        ui_input();
        frames(3);
        if (sys.tape_spd != 7u || sys.arp_bpm != (bpm0 * 3u + 2u) / 4u || tape.speed != 3072u) {
            printf("TAPE SPEED 3/4: spd %u bpm %u (from %u) speed %u\n", sys.tape_spd, sys.arp_bpm, bpm0, tape.speed);
            bad = 1;
        }
        bad |= shot(dir, "k_tape_speed");
        sys.arp_bpm = (uint8_t)bpm0;
        frames(2);
        if (tape.lock_q12 || tape.speed != 4096u) {
            printf("TAPE SPEED: the BPM back, not free again (speed %u)\n", tape.speed);
            bad = 1;
        }
        sys.tape_spd = 12;
    }
    {   /* BOUNCE from the panel: OCT- down, then OCT+ (and both at once): tracks 2 .. 4 blank, track 1 holds all, the
         * octave the first press moved given back, said when they are let go */
        uint32_t both = btn_bit(B_OCTDN) | btn_bit(B_OCTUP), pass;
        for (pass = 0; pass < 2u; pass++) {
            int8_t o0 = 1;
            sys.octave = o0;
            tape.has[1] = 1; tape.has[2] = 1;
            memset(tape_buf[2], 0x47, sizeof tape_buf[2]);
            fm1_in.buttons = 0; ui_input();          /* (none held: the octave noted) */
            if (pass == 0u) {
                fm1_in.buttons = btn_bit(B_OCTDN); edges = btn_bit(B_OCTDN); ui_input();
                frames(1);
                fm1_in.buttons = both; edges = btn_bit(B_OCTUP); ui_input();
            } else {
                fm1_in.buttons = both; edges = both; ui_input();
            }
            run_ms(300);
            fm1_in.buttons = 0; ui_input();
            if (tape.bounce || !tape.has[0] || tape.has[1] || tape.has[2] || tape_buf[2][5] != 0 || sys.octave != o0 ||
                !strstr(ui_event, "BOUNCE")) {
                printf("TAPE BOUNCE from OCT- + OCT+ (%s): has %u %u %u, octave %d, msg '%s'\n", pass ? "at once" : "one, then the other",
                       tape.has[0], tape.has[1], tape.has[2], sys.octave, ui_event);
                bad = 1;
            }
        }
        sys.octave = 0;
        frames(2);
    }
    edges = btn_bit(B_PLAY);
    ui_input();
    frames(2);
    edges = btn_bit(B_HOME);
    ui_input();
    synth_kill();
    edges = btn_bit(B_EDIT);             /* the EDIT page: the mod wheel half up, velocity BRIGHT */
    ui_input();
    sys.mw = 64; sys.vel_to = 1; sys.last_vel = 90;
    frames(2);
    bad |= shot(dir, "e_edit");
    sys.mw = 0; sys.vel_to = 0;
    sys.page = 7u;                       /* GLO twice: the MULTI page, on, part B a bass */
    edges = btn_bit(B_GLO);
    ui_input();
    if (sys.page != 15u) { printf("GLO: no MULTI page\n"); bad = 1; }
    sys.multi = 1;
    part_b_load(256u + 16u);
    frames(2);
    bad |= shot(dir, "e_multi");
    sys.multi = 0;
    sys.page = 0;
    edges = btn_bit(B_EDIT);
    ui_input();
    synth_kill();
    sys.page = 0;
    ui_force = 1;

    sys.page = 2;                        /* the FX page: off, then each model on a held note */
    preset_load(0);
    frames(3);
    bad |= shot(dir, "f_fx_off");
    for (i = 0; i < RZ_COUNT; i++) {
        static const uint8_t PI_[RZ_COUNT] = {11, 104, 24, 52, 73, 123, 101, 56, 40, 71, 47, 0};   /* VIBRAPHONE, SITAR, NYLON GUITAR, */
        preset_load(PI_[i]);                                             /* CHOIR AAHS, FLUTE, BIRD TWEET,
                                                                          * GOBLINS, TRUMPET, VIOLIN, CLARINET, TIMPANI, GRAND PIANO */
        synth_note_on(48, 100);
        frames(14);
        snprintf(name, sizeof name, "f_fx_%u", i);
        bad |= shot(dir, name);
        synth_kill();
    }
    preset_load(65);                     /* ALTO SAX (REED: the cone) */
    synth_note_on(48, 110);
    frames(14);
    bad |= shot(dir, "f_fx_sax");
    synth_kill();
    preset_load(101);                    /* GOBLINS (HAWK) */
    synth_note_on(48, 110);
    frames(14);
    bad |= shot(dir, "f_hawk");
    synth_kill();
    for (i = 0; i < WV_COUNT; i++) {     /* VOICE: each size, a vowel each */
        preset_load(52);                 /* CHOIR AAHS (VOICE) */
        sys.fx_struct = (uint8_t)(i * 128u / WV_COUNT + 4u);
        sys.fx_pos = (uint8_t)(i * 18u);
        synth_note_on(48, 110);
        frames(14);
        snprintf(name, sizeof name, "f_throat_%u", i);
        bad |= shot(dir, name);
        synth_kill();
    }
    sys.page = 3;                        /* the SHIMMER page: NEW AGE PAD, a note and its tail rising, then off */
    preset_load(88);                     /* NEW AGE PAD */
    synth_note_on(48, 110);
    frames(30);
    synth_note_off(48);
    frames(80);
    bad |= shot(dir, "g_shimmer");
    for (i = 0; i < SS_COUNT; i++) {     /* each SPACE's ornament, a tail flowing out */
        sys.sh_space = (uint8_t)i;
        synth_note_on(48, 110);
        frames(16);
        synth_note_off(48);
        frames(14);
        snprintf(name, sizeof name, "g_space_%u", i);
        bad |= shot(dir, name);
    }
    {   /* the knobs' range on the ornament: small dry, big with shimmer, dark, bright */
        static const uint8_t K[4][5] = {{0, 0, 0, 20, 30}, {127, 127, 127, 127, 127}, {64, 60, 0, 70, 0},
                                        {100, 127, 64, 100, 127}};   /* SIZE, DECAY, SHIMMER, MIX, TONE */
        sys.sh_space = SS_CATHEDRAL;
        for (i = 0; i < 4u; i++) {
            sys.sh_size = K[i][0]; sys.sh_decay = K[i][1]; sys.sh_shimmer = K[i][2]; sys.sh_mix = K[i][3]; sys.sh_tone = K[i][4];
            synth_note_on(48, 110);
            frames(16);
            synth_note_off(48);
            frames(14);
            snprintf(name, sizeof name, "g_knobs_%u", i);
            bad |= shot(dir, name);
        }
    }
    sys.sh_space = 0;
    sys.sh_mix = 0;
    frames(4);
    bad |= shot(dir, "g_shimmer_off");
    synth_kill();
    {   /* PRESETS on FX and SHIMMER: their own presets, the sound preset kept */
        uint16_t pr = sys.preset;
        sys.page = 2; fx_pre = 0;
        enc_steps[panel.enc[EN_PRESET]] = 1;
        ui_input();
        if (sys.preset != pr || sys.fx_model != RZ_MODAL || sys.fx_mix != 100u) {
            printf("PRESETS on FX: not the FX preset BELL\n"); bad = 1;
        }
        frames(3);
        bad |= shot(dir, "f_fx_preset");
        enc_steps[panel.enc[EN_PRESET]] = -1;
        ui_input();
        if (sys.fx_mix != 0u) { printf("PRESETS on FX: back to FX OFF\n"); bad = 1; }
        sys.page = 3; sh_pre = 0;
        enc_steps[panel.enc[EN_PRESET]] = 3;
        ui_input();
        if (sys.preset != pr || sys.sh_interval != SI_FIFTH || sys.sh_space != SS_TEMPLE || sys.sh_mix != 68u) {
            printf("PRESETS on SHIMMER: not ORACLE CHAMBER\n"); bad = 1;
        }
        sys.sh_mix = 0;
    }
    sys.page = 0;
    sys.fx_mix = 0;
    ui_force = 1;

    preset_load(0);                      /* the noise programs: ALGORITHM on HOME, then a few of them drawn */
    sys.view = V_XY;
    synth_note_on(48, 100);
    enc_steps[panel.enc[EN_ALGO]] = 6;   /* CHUA -> A5 RESONOISE */
    ui_input();
    frames(10);
    bad |= shot(dir, "h_noise_select");
    {
        static const uint8_t O[] = {6, 14, 21, 28};   /* RESONOISE, SINE FM CLUST, BASURA TOTAL, SATAN WORKOUT */
        for (i = 0; i < sizeof O; i++) {
            sys.osc = O[i];
            ui_event_ms = 0;
            frames(14);
            snprintf(name, sizeof name, "h_noise_%u", O[i]);
            bad |= shot(dir, name);
        }
    }
    sys.view = V_TIME;
    frames(8);
    bad |= shot(dir, "h_noise_time");
    preset_load(118);                    /* the MOD ENV page (ENV twice): SYNTH DRUM, a note, its envelope falling */
    edges = btn_bit(B_ENV);
    ui_input();
    edges = btn_bit(B_ENV);
    ui_input();
    synth_note_on(55, 100);
    frames(2);
    bad |= shot(dir, "j_menv_strike");
    frames(10);
    bad |= shot(dir, "j_menv_fallen");
    sys.menv_len = 115; sys.menv_skew = 80; sys.menv_height = 90;
    ui_event_ms = 0;
    frames(8);
    bad |= shot(dir, "j_menv_slow");
    synth_note_on(60, 100);
    frames(20);
    bad |= shot(dir, "j_menv_rising");
    synth_kill();
    sys.page = 0;
    ui_force = 1;
    synth_kill();
    preset_load(0);
    ui_force = 1;

    sys.page = 6;                        /* the ARP page: off, then on with a roll of steps (no keys.c here: filled in) */
    ui_force = 1;
    frames(2);
    bad |= shot(dir, "k_arp_off");
    sys.arp_on = 1; sys.arp_type = AT_UPDOWN; sys.arp_swing = 40; sys.arp_hold = 1;
    for (i = 0; i < 32u; i++) {
        static const uint8_t N[10] = {60, 64, 67, 72, 76, 79, 76, 72, 67, 64};
        arp_view.note[arp_view.w++ & 31u] = i % 8u == 7u ? 0u : N[i % 10u];
    }
    ui_event_ms = 0;
    frames(2);
    bad |= shot(dir, "k_arp_on");
    sys.arp_on = 0; sys.arp_hold = 0; sys.page = 0;
    ui_force = 1;

    edges = btn_bit(B_LFO);              /* the FREE EG page: a recorded curve playing onto FX DAMPING */
    ui_input();
    for (i = 0; i < FEG_N; i++)
        feg.curve[i] = (uint8_t)(64 + plot_sin(i * 2u) / 600 + (int32_t)(i % 32u));
    sys.feg_has = 1; sys.feg_on = 1; sys.feg_target = 10; sys.feg_len = 10; feg.pos = 40;
    ui_event_ms = 0;
    frames(2);
    bad |= shot(dir, "m_feg");
    sys.feg_has = 0; sys.feg_on = 0; feg.pos = 255;
    edges = btn_bit(B_HOME);
    ui_input();

    edges = btn_bit(B_SEQ);              /* the KARMA page: off, then on with a phrase playing (filled in) */
    ui_input();
    frames(2);
    bad |= shot(dir, "n_karma_off");
    edges = btn_bit(B_PLAY);             /* (PLAY starts it) */
    ui_input();
    if (!sys.k_on) {
        printf("ui shots: PLAY on the KARMA page did not start KARMA\n");
        bad = 1;
    }
    sys.k_ge = 5; sys.k_latch = 1; sys.k_m[0] = 90; kar.view_step = 6;
    for (i = 0; i < 32u; i++) {
        static const uint8_t N[8] = {48, 55, 60, 64, 67, 60, 72, 64};
        kar_view.note[kar_view.w++ & 31u] = N[(i * 3u) % 8u];
    }
    ui_event_ms = 0;
    frames(2);
    bad |= shot(dir, "n_karma_on");
    sys.k_ge = 22;
    ui_event_ms = 0;
    frames(2);
    bad |= shot(dir, "n_karma_minimal");
    sys.k_on = 0; sys.k_latch = 0; sys.k_m[0] = 64; sys.k_ge = 0; kar.view_step = 255;
    edges = btn_bit(B_HOME);
    ui_input();

    edges = btn_bit(B_GLO);              /* the MIDI page */
    ui_input();
    sys.midi_rx = 3; sys.midi_tx = 9;
    frames(2);
    bad |= shot(dir, "l_midi");
    sys.midi_rx = 0; sys.midi_tx = 0;
    edges = btn_bit(B_GLO);
    ui_input();

    sys.octave = 2;                      /* the widest values: a knob just turned, a message */
    sys.release = 127;
    sys.cpu_q8 = 255;
    enc_steps[panel.enc[EN_K2]] = 127;
    ui_input();
    edges = btn_bit(B_SAVE);
    ui_input();
    frames(8);
    bad |= shot(dir, "d_saved");
    sys.octave = -3;                     /* (after the save: the round trip below must give +2) */

    {   /* settings: SAVE wrote them, a power-on reads them back */
        uint8_t sh = sys.shape;
        sys.octave = 0; sys.shape = 0; sys.release = 0;
        settings_boot();
        if (sys.octave != 2 || sys.shape != sh || sys.release != 127) {
            printf("settings round trip FAIL\n");
            bad = 1;
        }
    }
    {   /* MOUTH: the vowel heard (I: its F2 ~2.3 kHz strong, A: weak), as loud as the dry within a few dB */
        double eI = 0, eA = 0, lI = 0, lA = 0, ld = 0;
        int vw, ok = 1;
        for (vw = 0; vw < 3; vw++) {
            double b0, b1, b2, a1, a2, z1 = 0, z2 = 0, e = 0, l = 0;
            uint32_t b, k;
            { double w = 2 * M_PI * 2300.0 / FS, al = sin(w) / (2 * 4.0), a0 = 1 + al;
              b0 = al / a0; b1 = 0; b2 = -al / a0; a1 = -2 * cos(w) / a0; a2 = (1 - al) / a0; }
            synth_kill(); preset_load(0); sys.attack = 0; sys.sustain = 127; sys.decay = 90; sys.release = 20;
            sys.osc = NP_COUNT > 10 ? 11 : 0; sys.np_x = 0; sys.np_y = 0; sys.fx_mix = 0; sys.master_q12 = 4095;
            synth_pgain[0] = 4096; sys.lvl_match = 0;
            sys.sh_space = SS_MOUTH; sys.sh_interval = 0; sys.sh_size = 64; sys.sh_decay = 30; sys.sh_tone = 64;
            sys.sh_mix = vw == 2 ? 0 : 127; sys.sh_shimmer = vw == 0 ? 127 : 64;   /* (I, A, the dry) */
            run_ms(200);
            synth_note_on(48, 100);
            run_ms(300);
            for (b = 0; b < 600u * FS / 1000u / CTL; b++) {
                synth_render(abuf_, CTL);
                for (k = 0; k < CTL; k++) {
                    double x = (abuf_[2 * k] + abuf_[2 * k + 1]) / 65536.0, y = b0 * x + z1;
                    z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y;
                    e += y * y; l += x * x;
                }
            }
            synth_note_off(48);
            if (vw == 0) { eI = e; lI = l; } else if (vw == 1) { eA = e; lA = l; } else ld = l;
        }
        ok &= eI > eA * 2.0 && lI > ld / 4.0 && lI < ld * 4.0 && lA > ld / 4.0 && lA < ld * 4.0;
        if (!ok) { printf("MOUTH FAIL (I/A at 2.3 kHz %.1f dB; I, A against the dry %.1f %.1f dB)\n", 10 * log10(eI / (eA + 1e-20)),
                          10 * log10(lI / (ld + 1e-20)), 10 * log10(lA / (ld + 1e-20))); bad = 1; }
        sys.sh_mix = 0; synth_kill();
    }
    {   /* a knob of choices: one detent, the next (the VOICE's size, the CLUSTER's voices) */
        int ok = 1;
        uint8_t m0 = sys.fx_model, st0 = sys.fx_struct, o0 = sys.osc, y0 = sys.np_y;
        sys.fx_model = RZ_VOICE; sys.fx_struct = 0;
        knob_step(&sys.fx_struct, 1); ok &= knob_choice(&sys.fx_struct, sys.fx_struct) == 1;
        knob_step(&sys.fx_struct, 1); ok &= knob_choice(&sys.fx_struct, sys.fx_struct) == 2;
        knob_step(&sys.fx_struct, -1); ok &= knob_choice(&sys.fx_struct, sys.fx_struct) == 1;
        sys.osc = 11; sys.np_y = 0;
        { int k; for (k = 0; k < 14; k++) knob_step(&sys.np_y, 1); }
        ok &= sys.np_y == 127 && knob_choice(&sys.np_y, sys.np_y) == 13;   /* (2 .. 15 voices) */
       
        sys.fx_model = RZ_MODAL; ok &= !knob_step(&sys.fx_struct, 1);
        sys.fx_model = m0; sys.fx_struct = st0; sys.osc = o0; sys.np_y = y0;
        if (!ok) { printf("knob choices FAIL (struct %u y %u)\n", sys.fx_struct, sys.np_y); bad = 1; }
    }
    {   /* the pages' presets: PRESETS on ENV, MOD ENV, SCL, FREE EG sets that page's knobs */
        int ok = 1;
        uint8_t pg = sys.page;
        sys.page = 1; page_preset(1); ok &= sys.attack == ENV_PRE[0].a && sys.decay == ENV_PRE[0].d;
        page_preset(1); ok &= sys.sustain == ENV_PRE[1].s;
        sys.page = 5; page_preset(1); page_preset(1); ok &= sys.menv_height == MENV_PRE[1].height;
        sys.page = 10; page_preset(-1); ok &= sys.basis == SCL_PRE[NPAGE_PRE(SCL_PRE) - 1].basis;
        sys.page = 8; page_preset(1); ok &= !sys.feg_on;
        page_preset(1); ok &= sys.feg_on && sys.feg_has && sys.feg_target == 3u && feg.curve[32] > feg.curve[96];
        sys.feg_on = 0; sys.page = pg;
        if (!ok) { printf("page presets FAIL\n"); bad = 1; }
    }
    {   /* AUTO SAVE: a change kept 3 s after it, once all is quiet; not while the ARP plays */
        settings_t st;
        uint32_t i;
        int ok = 1;
        sys.arp_on = 0; sys.k_on = 0; arp.n = 0;
        for (i = 0; i < 20u; i++) { run_ms(40); ui_input(); }   /* (the state as it is now: saved) */
        sys.decay = 33;
        for (i = 0; i < 25u; i++) { run_ms(40); ui_input(); }   /* (1 s: not yet) */
        st_load(OBJ_SETTINGS, &st, sizeof st);
        ok &= st.decay != 33u;
        for (i = 0; i < 125u; i++) { run_ms(40); ui_input(); }  /* (6 s: kept) */
        st_load(OBJ_SETTINGS, &st, sizeof st);
        ok &= st.decay == 33u;
        sys.arp_on = 1; arp.n = 1; sys.decay = 44;
        for (i = 0; i < 150u; i++) { run_ms(40); ui_input(); }  /* (the ARP playing: never) */
        st_load(OBJ_SETTINGS, &st, sizeof st);
        ok &= st.decay == 33u;
        arp.n = 0; sys.arp_on = 0;
        for (i = 0; i < 150u; i++) { run_ms(40); ui_input(); }  /* (stopped: kept) */
        st_load(OBJ_SETTINGS, &st, sizeof st);
        ok &= st.decay == 44u;
        if (!ok) { printf("auto save FAIL (%u)\n", st.decay); bad = 1; }
    }
    if (off_screen) {
        printf("%d strips drawn outside the screen FAIL\n", off_screen);
        bad = 1;
    }
    if (stale_bands) {
        printf("ui shots: %d frames left part of the plot stale on the panel\n", stale_bands);
        bad = 1;
    }
    printf(bad ? "ui shots: FAILED\n" : "ui shots: ok (%s)\n", dir);
    return bad;
}
