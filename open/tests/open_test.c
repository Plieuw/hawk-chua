/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open without the hardware: open/app/hc_open.c (the sound, the UI and the layer on the SDK) against
 * a stand-in for the FM1 SDK: a 240 x 240 framebuffer for the screen, RAM for the user flash, a scripted panel.
 *
 *   data          step 2: without HAWK CHUA's data a plain screen and silence; an upload for another build or damaged
 *                 refused; a good one starts the UI and the sound
 *   boot          the splash, then the home page; the keys silent under the splash
 *   sound         keys and MIDI in (USB, TRS, Bluetooth) play; every block in range; MIDI out reaches USB and BLE
 *   pages         every button's page drawn (PNGs), nothing drawn off the screen
 *   store         settings and presets saved and loaded; a save cut off leaves the copy before it
 *   OUT_DIR       open_test OUT_DIR: screens as .rgb (tests/rgb_png.py makes PNGs), a .wav of the sound */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "fm1.h"
#include "fm1_device.h"

/* ---------------------------------------------------------- the SDK --- */
static uint32_t now_ms;
uint32_t fm1_millis(void) { return now_ms; }
static uint16_t fb[240 * 240];
static int off_screen, blits;
void fm1_screen_init(void) {}
void fm1_screen_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t c)
{
    unsigned i, j;
    if (x + w > 240u || y + h > 240u)
        off_screen++;
    for (j = y; j < y + h && j < 240u; j++)
        for (i = x; i < x + w && i < 240u; i++)
            fb[j * 240u + i] = c;
}
int fm1_screen_blit(unsigned x, unsigned y, unsigned w, unsigned h, const uint16_t *px, size_t count)
{
    unsigned i, j;
    blits++;
    if (x + w > 240u || y + h > 240u || count < (size_t)w * h) {
        off_screen++;
        return -1;
    }
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++)
            fb[(y + j) * 240u + x + i] = px[j * w + i];
    return 0;
}
static struct hc_scan_t { volatile uint32_t notes, buttons; } scan;
static volatile uint8_t leds[11];
volatile struct hc_scan *const fm1_panel_scan = (volatile struct hc_scan *)(volatile void *)&scan;
volatile uint8_t *const fm1_panel_leds = leds;
static int16_t enc[7];
void fm1_controls_init(void) {}
void fm1_controls_read(struct fm1_controls *c)
{
    memset(c, 0, sizeof *c);
    c->keys = scan.notes;
    c->buttons = scan.buttons;
    memcpy(c->encoders, enc, sizeof enc);
    memset(enc, 0, sizeof enc);
    c->knob = 700;
}
void fm1_led_set(unsigned key, int on) { (void)key; (void)on; }
uint32_t fm1_device_irq_save(void) { return 0; }
void fm1_device_irq_restore(uint32_t s) { (void)s; }
int fm1_device_usb_connected(void) { return 1; }
static int recovered;
void fm1_device_recovery(void) { recovered++; }
static unsigned sent[3];
static uint8_t last_reply[80];
static size_t last_reply_n;
unsigned fm1_midi_ready_ports(void) { return 7u; }
int fm1_midi_send_port(unsigned port, const uint8_t *m, size_t n)
{
    if (port < 3u)
        sent[port]++;
    if (n >= 5u && m[0] == 0xF0 && n <= sizeof last_reply) {
        memcpy(last_reply, m, n);
        last_reply_n = n;
    }
    return 0;
}
unsigned fm1_midi_input_source(void) { return FM1_MIDI_USB; }
static unsigned bt_features;
int fm1_bluetooth_configure(unsigned f) { bt_features = f; return 0; }
int fm1_ble_set_role(enum fm1_ble_role r) { (void)r; return 0; }
#define FLASH_SIZE 536576u
static uint8_t flash[FLASH_SIZE];
static int flash_fail_after = -1;                /* (a write cut off: this many more writes go through) */
uint32_t fm1_flash_size(void) { return FLASH_SIZE; }
const uint8_t *fm1_flash_map(uint32_t o, size_t n) { return o + n <= FLASH_SIZE ? flash + o : NULL; }
int fm1_flash_invalidate(uint32_t o, size_t n) { (void)o; (void)n; return 0; }
int fm1_flash_read(uint32_t o, void *d, size_t n) { if (o + n > FLASH_SIZE) return -1; memcpy(d, flash + o, n); return 0; }
int fm1_flash_erase(uint32_t o) { if (o % 4096u || o >= FLASH_SIZE) return -1; memset(flash + o, 0xFF, 4096); return 0; }
int fm1_flash_write(uint32_t o, const void *d, size_t n)
{
    size_t i;
    if (o + n > FLASH_SIZE)
        return -1;
    if (flash_fail_after == 0)
        return -2;
    if (flash_fail_after > 0)
        flash_fail_after--;
    for (i = 0; i < n; i++)
        flash[o + i] &= ((const uint8_t *)d)[i];
    return 0;
}
volatile uint16_t hc_load_q8;

#include "../app/hc_open.c"

/* ------------------------------------------------------------ helpers --- */
static int fails;
static void check(const char *what, int ok)
{
    printf("%-78s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
}
static int32_t blk[2 * HALF_FRAMES];
static FILE *wav;
static uint32_t wav_frames;
static int32_t peak;
static void audio(uint32_t blocks)              /* blocks of 128 frames, and the control task every ~3 ms */
{
    while (blocks--) {
        uint32_t i;
        hc_render(blk);
        for (i = 0; i < 2u * HALF_FRAMES; i++) {
            int32_t v = blk[i];
            int16_t s = (int16_t)clamp(v >> 8, -32768, 32767);
            if ((v < 0 ? -v : v) > peak)
                peak = v < 0 ? -v : v;
            if (wav)
                fwrite(&s, 2, 1, wav);
        }
        wav_frames += HALF_FRAMES;
        now_ms += 3;
        groove_screen_control();
    }
}
static void press(uint32_t label)               /* a button pressed and let go */
{
    scan.buttons |= btn_bit(label);
    audio(4);
    scan.buttons &= ~btn_bit(label);
    audio(8);
}
static void shot(const char *dir, const char *name)
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/%s.rgb", dir, name);
    f = fopen(p, "wb");
    if (f) {
        fwrite(fb, 2, 240 * 240, f);
        fclose(f);
    }
}
static void midi(unsigned src, uint8_t a, uint8_t b, uint8_t c)
{
    uint8_t m[3] = {a, b, c};
    fm1_app_midi_received(src, m, midi_len(a));
}
/* step 2: an image as tools/build_open.py makes it, uploaded as the install page does (hc_data.c's protocol) */
static const unsigned char test_build_id[32] = "HAWK CHUA HOST TEST BUILD ID 123";
static uint8_t image[4096 + 9000];
static uint32_t make_image(int other_build)
{
    struct hc_data_header h;
    uint32_t i;
    memset(image, 0xFF, sizeof image);
    for (i = 4096; i < sizeof image; i++)
        image[i] = (uint8_t)(i * 31u >> 3);
    memset(&h, 0, sizeof h);
    h.magic = HC_DATA_MAGIC; h.version = 1; h.header_bytes = sizeof h; h.total = sizeof image;
    h.size = sizeof image - 4096u; h.vma = 0x2079000u; h.commit = 0xFFFFFFFFu;
    h.crc = st_crc32(image + 4096, h.size);
    memcpy(h.build_id, test_build_id, 32);
    if (other_build)
        h.build_id[0] ^= 1;
    memcpy(image, &h, sizeof h);
    return sizeof image;
}
static int upload_msg(const uint8_t *body, uint32_t n)   /* a message, the control task's round: 1 if acknowledged */
{
    uint8_t m[1100];
    m[0] = 0xF0; m[1] = 0x7D; m[2] = 0x48; m[3] = 0x43;
    memcpy(m + 4, body, n);
    m[4 + n] = 0xF7;
    last_reply_n = 0;
    fm1_app_sysex(m, n + 5u);
    audio(1);
    return last_reply_n > 5u && last_reply[4] == 0x7E;
}
static int upload(uint32_t total)
{
    uint8_t b[1100];
    uint32_t off = 0, k;
    b[0] = 0x20;
    put7(b + 1, total, 5);
    if (!upload_msg(b, 6))
        return 0;
    while (off < total) {
        uint32_t cnt = total - off > 512u ? 512u : total - off, n = 0, i, sum = 0;
        b[n++] = 0x21;
        n += put7(b + n, off, 5);
        n += put7(b + n, cnt, 2);
        for (i = 0; i < cnt; i += 7u) {
            uint8_t top = 0;
            for (k = 0; k < 7u && i + k < cnt; k++)
                top |= (uint8_t)((image[off + i + k] >> 7) << k);
            b[n++] = top;
            for (k = 0; k < 7u; k++) {
                b[n++] = i + k < cnt ? (uint8_t)(image[off + i + k] & 0x7Fu) : 0;
                if (i + k < cnt)
                    sum ^= image[off + i + k];
            }
        }
        b[n++] = (uint8_t)(sum & 0x7Fu);
        if (!upload_msg(b, n))
            return 0;
        off += cnt;
    }
    b[0] = 0x22;
    return upload_msg(b, 1);
}
static uint32_t lit(void)                        /* pixels that are not the ground */
{
    uint32_t i, n = 0;
    for (i = 0; i < 240u * 240u; i++)
        n += fb[i] != T_BG;
    return n;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char p[512];
    uint32_t i, k;
    memset(flash, 0xFF, sizeof flash);
    snprintf(p, sizeof p, "%s/open.wav", dir);
    wav = fopen(p, "wb");
    if (wav)
        fseek(wav, 44, SEEK_SET);

    /* ---- step 2: without HAWK CHUA's data a plain screen, the keys play; uploaded, the UI starts */
    hc_test_build_id = test_build_id;
    hc_ovl_test_vma = 0x2079000u;
    hc_ovl_test_size = sizeof image - 4096u;
    groove_screen_init();
    audio(20);
    check("Bluetooth: MIDI and audio switched on", bt_features == (FM1_BT_AUDIO | FM1_BT_MIDI) && hc_bt_pending == 0);
    check("data: none at first, the plain screen", !hc_ui_on && hc_data_state == HC_DATA_NONE && lit() > 1000u);
    shot(dir, "00-plain");
    scan.notes = 1u;
    audio(60);
    scan.notes = 0;
    audio(20);
    check("data: without it, silence (the sound's code is in it)", peak == 0);
    peak = 0;
    make_image(1);
    check("data: an image for another build is refused", !upload(sizeof image) && hc_data_state == HC_DATA_OLD && !hc_ui_on);
    make_image(0);
    image[5000] ^= 1;                            /* (corrupted after its CRC was taken) */
    check("data: a damaged image is refused", !upload(sizeof image) && hc_data_state == HC_DATA_CRC && !hc_ui_on);
    make_image(0);
    check("data: the image uploads, checks out, the UI and the sound start", upload(sizeof image) && hc_data_ok() && hc_ui_on &&
          hc_ovl_audio);
    {
        struct hc_data_header h;
        memcpy(&h, flash, sizeof h);
        check("data: committed, and kept out of the store", h.commit == HC_DATA_COMMIT && hc_data_room() >= sizeof image);
    }

    /* ---- boot */
    scan.notes = 1u;                             /* a key held under the splash: silent */
    peak = 0;
    audio(40);
    check("boot: the splash drawn", lit() > 2000u);
    shot(dir, "01-splash");
    check("boot: a key under the splash stays silent", peak < 2000);
    scan.notes = 0;
    audio(600);                                  /* (past the splash, PSYCHEDELIC's too) */
    check("boot: the home page drawn", lit() > 2000u && !hc_splash);
    shot(dir, "02-home");

    /* ---- sound */
    peak = 0;
    scan.notes = 1u << 12;
    audio(200);
    scan.notes = 0;
    audio(100);
    printf("  (peak %d, preset %u %s)\n", (int)peak, sys.preset, PRESETS[sys.preset % NPRESETS].name);
    check("sound: a key plays", peak > 20000);
    check("sound: in range (24-bit)", peak < 8388608);
    for (k = 0; k < 3u; k++) {                   /* USB, TRS, Bluetooth: each source plays */
        peak = 0;
        midi(k == 0 ? FM1_MIDI_USB : k == 1 ? FM1_MIDI_DIN : FM1_MIDI_BLE, 0x90, 60, 100);
        audio(150);
        midi(k == 0 ? FM1_MIDI_USB : k == 1 ? FM1_MIDI_DIN : FM1_MIDI_BLE, 0x80, 60, 0);
        audio(100);
        snprintf(p, sizeof p, "sound: MIDI in from %s plays", k == 0 ? "USB" : k == 1 ? "TRS" : "Bluetooth LE");
        printf("  (peak %d)\n", (int)peak);
        check(p, peak > 20000);
    }
    sys.midi_tx = 0;
    sent[0] = sent[1] = sent[2] = 0;
    scan.notes = 1u << 5;
    audio(20);
    scan.notes = 0;
    audio(20);
    check("sound: the keys' notes go out on USB and Bluetooth LE", sent[FM1_MIDI_USB] >= 2u && sent[FM1_MIDI_BLE] >= 2u);
    for (k = 0; k < 40u; k++) {                  /* presets through: every block in range */
        sys.preset = (uint16_t)(k * 13u % NPRESETS);
        preset_load(sys.preset);
        peak = 0;
        midi(FM1_MIDI_USB, 0x90, (uint8_t)(48 + k % 24u), 110);
        audio(60);
        midi(FM1_MIDI_USB, 0x80, (uint8_t)(48 + k % 24u), 0);
        audio(30);
        if (peak >= 8388608)
            break;
    }
    check("sound: 40 presets, every block in range", k == 40u);

    /* ---- pages */
    {
        static const uint32_t B[] = {B_HOME, B_ENV, B_ENV, B_FX, B_FX, B_LFO, B_ARP, B_ARP, B_SEQ, B_SEQ, B_GLO, B_GLO,
                                     B_GLO, B_GLO, B_EDIT, B_REC, B_SCL, B_HOME};
        uint32_t ok = 1;
        for (i = 0; i < sizeof B / sizeof B[0]; i++) {
            char nm[64];
            press(B[i]);
            enc[3] = 5;                          /* (KNOB 1 turned on each page) */
            audio(40);
            snprintf(nm, sizeof nm, "%02u-%s-page%u", 3u + i, B_NAME[B[i]], (unsigned)sys.page);
            shot(dir, nm);
            ok &= lit() > 1000u;
        }
        check("pages: every button's page drawn", ok);
    }
    check("pages: nothing drawn off the screen", off_screen == 0);

    /* ---- store */
    {
        uint8_t a[100], b[100];
        for (i = 0; i < 100u; i++)
            a[i] = (uint8_t)(i * 7u);
        check("store: a save", st_save(OBJ_USER0 + 3, a, sizeof a) == 0);
        check("store: loads what was saved", st_load(OBJ_USER0 + 3, b, sizeof b) == 100 && !memcmp(a, b, 100));
        for (i = 0; i < 100u; i++)
            a[i] = (uint8_t)(255u - i);
        check("store: a second save (the other copy)", st_save(OBJ_USER0 + 3, a, sizeof a) == 0);
        check("store: loads the newer", st_load(OBJ_USER0 + 3, b, sizeof b) == 100 && !memcmp(a, b, 100));
        memset(b, 0x55, sizeof b);
        flash_fail_after = 1;                    /* the payload written, the record not: cut off */
        check("store: a save cut off fails", st_save(OBJ_USER0 + 3, b, sizeof b) != 0);
        flash_fail_after = -1;
        check("store: the copy before it loads", st_load(OBJ_USER0 + 3, b, sizeof b) == 100 && !memcmp(a, b, 100));
        check("store: the settings save (SAVE)", settings_save() == 0);
        check("store: not in the data area (the image as uploaded)", !memcmp(flash + 4096, image + 4096, sizeof image - 4096u));
    }

    if (wav) {                                   /* the WAV header */
        uint32_t bytes = wav_frames * 4u, v;
        fseek(wav, 0, SEEK_SET);
        fwrite("RIFF", 1, 4, wav); v = 36u + bytes; fwrite(&v, 4, 1, wav);
        fwrite("WAVEfmt ", 1, 8, wav); v = 16; fwrite(&v, 4, 1, wav);
        { uint16_t f[2] = {1, 2}; fwrite(f, 2, 2, wav); }
        v = 44100; fwrite(&v, 4, 1, wav); v = 44100u * 4u; fwrite(&v, 4, 1, wav);
        { uint16_t f[2] = {4, 16}; fwrite(f, 2, 2, wav); }
        fwrite("data", 1, 4, wav); fwrite(&bytes, 4, 1, wav);
        fclose(wav);
    }
    printf(fails ? "%d FAILED\n" : "all open tests passed\n", fails);
    return fails != 0;
}
