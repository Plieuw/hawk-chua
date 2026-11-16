/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: what the FM1 SDK calls in an app.
 *
 * groove_screen_init / groove_screen_control: the control task's start and its round (the Felucca build's main.c
 * and main loop): the panel, the queued MIDI out, step 2's upload, the MASTER pot, then the UI (in HAWK CHUA's data:
 * hc_data.c) ~60 times a second at most, or without the data a plain screen. The instrument hooks (projects, the DX7 library, the sampler, the sequencer's MIDI) are FM1 Open's
 * own instrument's: HAWK CHUA renders none of it, so they do nothing; they stay because the SDK calls them. */
#include "instrument.h"
#include "fm_patch.h"

/* MASTER: the pot (0 .. 1023) smoothed, a square law (its middle about -12 dB) -> sys.master_q12 (0 .. 4096) */
static int32_t master_acc = -1;
static void master_poll(void)
{
    int32_t k = hc_knob > 1023u ? 1023 : (int32_t)hc_knob;
    master_acc = master_acc < 0 ? k * 16 : master_acc + (k * 16 - master_acc) / 8;
    k = master_acc / 16;
    sys.master_q12 = (uint16_t)((uint32_t)(k * k) >> 8);
}

static uint32_t hc_plain_key = 0xFFFFFFFFu;     /* (hc_plain: what it shows; all again from 0xFFFFFFFF) */

/* OCT- + OCT+ held 5 s: the SDK's recovery (update mode), a countdown on the screen from 2 s; letting go cancels */
static void recovery_combo(void)
{
    static uint32_t t0, shown;
    uint32_t both = btn_bit(B_OCTDN) | btn_bit(B_OCTUP), held = fm1_ms - t0, left;
    if ((fm1_in.buttons & both) != both) {
        if (shown) {
            shown = 0;
            ui_force = 1;
            hc_plain_key = 0xFFFFFFFFu;
            ui_say("UPDATE MODE CANCELLED");
        }
        t0 = fm1_ms;
        return;
    }
    if (held > 5000u) {
        lcd_fill(0, 0, 240, 240, T_BG);
        draw_text_box(0, 110, 240, &AF_M, "UPDATE MODE", T_THEME, 1);
        fm1_device_recovery();
        return;
    }
    if (held <= 2000u)
        return;
    left = (5000u - held + 999u) / 1000u;
    if (left != shown) {
        char d[24] = "UPDATE MODE IN  ";
        d[16] = (char)('0' + left);
        d[17] = 0;
        shown = left;
        lcd_fill(0, 0, 240, 240, T_BG);
        draw_text_box(0, 100, 240, &AF_M, d, T_THEME, 1);
        draw_text_box(0, 130, 240, &AF_S, "LET GO TO CANCEL", T_MID, 1);
    }
}

static uint32_t hc_boot_ms, hc_frame_ms;
static uint8_t hc_splash;                        /* 1 while the splash shows (the keys silent) */
static uint8_t hc_ui_on;                         /* the UI started (its data checked out) */

/* the UI, in HAWK CHUA's data (hc_data.c): its two ways in, called only through hc_ovl_entry and only once the data
 * checked out (tools/ovl_split.py keeps everything the firmware calls itself out of it) */
static void hc_ui_boot(void)
{
    settings_boot();                             /* the saved settings, the preset */
    style_apply();                               /* ui_style.c: the UI STYLE's palette */
    draw_splash();
    ui_leds_init();
}
static void hc_ui_round(uint32_t now)
{
    if (hc_splash) {                             /* the splash; MIDI in already plays (PSYCHEDELIC: longer, moving) */
        int psy = sys.ui_style % US_COUNT == US_PSYCHE;
        uint32_t t = now - hc_boot_ms;
        if (t < (psy ? SPLASH_PSY_MS : 600u)) {
            if (psy && now - hc_frame_ms >= 40u) {
                hc_frame_ms = now;
                splash_psy(t);
            }
            return;
        }
        hc_splash = 0;
        kb_boot_hold = 0;
        ui_force = 1;
    }
    if (now - hc_frame_ms < 15u)                 /* ~60 UI frames a second at most */
        return;
    hc_frame_ms = now;
    ui_input();
    ui_leds();
    if (!(fm1_in.buttons & btn_bit(B_OCTDN) && fm1_in.buttons & btn_bit(B_OCTUP)))
        ui_draw();                               /* (the countdown has the screen while both are held) */
}
static void (*volatile hc_ovl_entry_boot)(void) __attribute__((section(".data.hc_ovl_entry"))) = hc_ui_boot;
static void (*volatile hc_ovl_entry_round)(uint32_t) __attribute__((section(".data.hc_ovl_entry"))) = hc_ui_round;

static void hc_ui_start(void)
{
    palette_set(0);
    kb_boot_hold = 1;                            /* no notes from the keys under the splash */
    hc_ovl_entry_boot();
    hc_boot_ms = fm1_ms;
    hc_splash = 1;
    hc_ui_on = 1;
}

/* without the data: a plain screen, the state of step 2, an upload's progress (the sound is silent) */
static void hc_plain(uint32_t now)
{
    static uint32_t t;
    uint32_t p = hc_data_progress(), key = hc_data_state << 24 | p >> 10;
    char b[40], v[12];
    if (key == hc_plain_key && now - t < 1000u)
        return;
    t = now;
    if (hc_plain_key == 0xFFFFFFFFu) {
        lcd_fill(0, 0, 240, 240, T_BG);
        draw_text_box(0, 40, 240, &AF_M, "HAWK CHUA", T_THEME, 1);
        draw_text_box(0, 64, 240, &AF_S, "FM1 OPEN  " HC_VERSION, T_MID, 1);
        draw_text_box(0, 150, 240, &AF_S, "INSTALL STEP 2: HAWK CHUA DATA", T_TEXT, 1);
        draw_text_box(0, 170, 240, &AF_S, "(THE INSTALL PAGE, CHROME OR EDGE)", T_DIM, 1);
    }
    hc_plain_key = key;
    str_cpy(b, "DATA: ", sizeof b);
    ui_cat(b, sizeof b, HC_DATA_SAY[hc_data_state % 7u]);
    if (hc_data_state == HC_DATA_LOADING) {
        ui_cat(b, sizeof b, " ");
        fmt_int(v, (int32_t)(p / 1024u));
        ui_cat(b, sizeof b, v);
        ui_cat(b, sizeof b, " KB");
    } else if (hc_data_state == HC_DATA_ADDR) {  /* (where the SDK maps it, for the developer) */
        uint32_t i;
        ui_cat(b, sizeof b, " ");
        for (i = 0; i < 8u; i++)
            v[i] = "0123456789ABCDEF"[hc_data_mapped >> (28u - 4u * i) & 15u];
        v[8] = 0;
        ui_cat(b, sizeof b, v);
    }
    draw_text_box(0, 110, 240, &AF_S, b, hc_data_state == HC_DATA_LOADING ? T_THEME : T_ACCENT, 1);
}

void groove_screen_init(void)
{
    uint32_t k;
    fm1_controls_init();
    for (k = 0; k < 41u; k++)
        fm1_led_set(k, 0);
    fm1_screen_init();
    palette_set(0);
    st_boot();
    keys_init();
    tape_init();
    hc_panel_read();
    master_acc = -1;
    master_poll();
    hc_data_boot();
    if (hc_data_ok())
        hc_ui_start();
}

void groove_screen_control(void)
{
    uint32_t now = fm1_ms;
    hc_panel_read();
    hc_midi_poll();
    hc_data_poll();
    master_poll();
    if (hc_ui_on && !hc_data_ok()) {             /* an upload began: the UI's code is being written */
        hc_ui_on = 0;
        hc_plain_key = 0xFFFFFFFFu;
    }
    if (up_done) {                               /* an upload checked out: the UI starts */
        up_done = 0;
        if (hc_data_ok())
            hc_ui_start();
    }
    recovery_combo();
    if (fm1_in.buttons & btn_bit(B_OCTDN) && fm1_in.buttons & btn_bit(B_OCTUP))
        return;                                  /* (the countdown has the screen) */
    if (!hc_ui_on)
        hc_plain(now);
    else
        hc_ovl_entry_round(now);
}

/* the instrument's data and transfers: none */
void groove_project_init(void) {}
void groove_project_poll(void) {}
void groove_project_metadata(const struct groove_project *p) { (void)p; }
void groove_project_name(char name[13]) { memcpy(name, "HAWK CHUA   ", 13); name[12] = 0; }
struct groove_project *groove_project_scratch(void)
{
    static struct groove_project scratch;
    return &scratch;
}
void groove_samples_init(void) {}
void groove_samples_poll(void) {}
void groove_transfer_init(void) {}
void groove_transfer_poll(void) {}
void groove_fm_library_init(void) {}
FM1_DSP_CODE void groove_fm_program(uint8_t patch[156], unsigned program)
{
    unsigned i;
    (void)program;
    for (i = 0; i < 156u; i++)
        patch[i] = 0;
}
/* the sequencer's MIDI output and clock (the SDK's instrument would call these while it renders) */
void groove_midi_init(void) {}
void groove_midi_clock_reset(void) {}
void groove_midi_poll(void) {}
void groove_midi_panic(void) {}
FM1_AUDIO_CODE unsigned groove_midi_mode(unsigned track) { (void)track; return 0; }
FM1_AUDIO_CODE void groove_midi_note(unsigned track, unsigned note, unsigned velocity)
{
    (void)track; (void)note; (void)velocity;
}
FM1_AUDIO_CODE void groove_midi_track_reset(unsigned track) { (void)track; }
FM1_AUDIO_CODE void groove_midi_transport(unsigned owner, uint8_t status) { (void)owner; (void)status; }
FM1_AUDIO_CODE void groove_midi_clock(unsigned owner, unsigned tempo) { (void)owner; (void)tempo; }
