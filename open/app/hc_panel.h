/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the panel as the UI knows it, on M-VAVE's scan (app/board/panel.c, fm1_input.h): its
 * debounced keys and buttons, its encoders (fm1_controls_read) and its LED columns.
 *
 * The scan numbers the matrix as the FM-1's own firmware does: buttons 0..13 (OCT- OCT+ FX SCL ENV LFO EDIT GLO HOME
 * SAVE ARP SEQ PLAY REC), note keys 14..40 (F3 .. G5), encoders 0..6 (SELECT, ALGORITHM, KNOB 1..4, PRESETS). */
#ifndef HC_PANEL_H
#define HC_PANEL_H
#include <stdint.h>

/* app/board/panel.c (HAWK CHUA's additions): the scan's state and LED columns */
struct hc_scan { volatile uint32_t notes, buttons; };   /* (the first two words of fm1_input.h's fm1_in) */
extern volatile struct hc_scan *const fm1_panel_scan;
extern volatile uint8_t *const fm1_panel_leds;          /* one byte a column: bit r = row r */
#define fm1_in (*fm1_panel_scan)                         /* .notes (bit n: key n), .buttons (bit i: button i) */
#define FM1_NCOL 11u

/* the labels (the UI's order) and the matrix button each is */
enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, NB };
enum { EN_SELECT, EN_ALGO, EN_PRESET, EN_K1, EN_K2, EN_K3, EN_K4, NE };
static const char *const B_NAME[NB] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE",
                                        "ARP", "SEQ", "PLAY", "REC", "OCT-", "OCT+"};
static const char *const E_NAME[NE] = {"SELECT", "ALGORITHM", "PRESETS", "KNOB 1", "KNOB 2", "KNOB 3", "KNOB 4"};
static const struct {
    uint8_t btn[NB];             /* the matrix button a label is */
    uint8_t enc[NE];             /* the SDK's encoder a role is (struct fm1_controls .encoders) */
} panel = {
    {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 1},
    {0, 1, 6, 2, 3, 4, 5},
};
static uint32_t btn_bit(uint32_t label) { return 1u << panel.btn[label]; }

/* the LED matrix: the id (button 0..13, note key 14..40) at [row][column] (the scan's pins: column = pin >> 4,
 * row = pin & 15, app/board/fm1_input.h fm1_key_pin), -1: none */
static const int8_t FM1_KEYMAP[5][FM1_NCOL] = {
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
};
#define LED_PLAY_GREEN_COL 8u    /* PLAY's green LED: not a key of the matrix */
#define LED_PLAY_GREEN_ROW 1u
#define fm1_led fm1_panel_leds
static uint8_t fm1_led_dim[FM1_NCOL];   /* (the idle glow: the SDK's scan has none; kept for the UI, unused) */
static int fm1_led_anim_on(void) { return 0; }

/* the control task's reading, once a frame (hc_sdk.c): the encoders' detents gathered, buttons' presses */
static int32_t hc_enc[7];
static uint32_t hc_btn_prev, hc_btn_pressed;
static uint16_t hc_knob = 512, hc_batt;
static void hc_panel_read(void)
{
    struct fm1_controls c;
    uint32_t i;
    fm1_controls_read(&c);
    for (i = 0; i < 7u; i++)
        hc_enc[i] += c.encoders[i];
    hc_btn_pressed |= c.buttons & ~hc_btn_prev;
    hc_btn_prev = c.buttons;
    hc_knob = c.knob;
    hc_batt = c.battery;
}
static uint32_t rm_btn;                          /* (ui.c: buttons from a remote; none here) */
static int32_t panel_enc(uint32_t role)          /* detents turned since the last call, + = clockwise */
{
    int32_t r;
    if (role >= NE)
        return 0;
    r = hc_enc[panel.enc[role]];
    hc_enc[panel.enc[role]] = 0;
    return r;
}
static uint32_t fm1_input_edges(int unused)     /* buttons pressed since the last call (bit: matrix button) */
{
    uint32_t p = hc_btn_pressed;
    (void)unused;
    hc_btn_pressed = 0;
    return p;
}
#endif
