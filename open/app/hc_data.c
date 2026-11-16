/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA's data in the user flash (step 2 of the install; control task).
 *
 * The firmware holds the platform layer, the store and a plain screen; the sound and the UI (synth.c and all it
 * includes, keys.c, ui.c, ui_style.c, fx_art.c: their code, the presets, their tables) are linked to run from the user
 * flash (tools/ovl_split.py, build_open.py: .hc_ovl), where this data puts them. The image (tools/build_open.py: hawk-chua-data.bin): a header sector, then .hc_ovl as linked.
 * It is used only when its header is whole, its commit word written, its CRC matches, it was built with this firmware
 * (the build id) and the SDK maps it where it was linked to run. Until then the firmware is silent, with a plain
 * screen that asks for step 2. The store (hc_store.c) keeps the end of the partition; an upload never reaches it.
 *
 * Upload, SysEx (manufacturer 7D, non-commercial), one message at a time, each answered (as Orkest's bank):
 *   F0 7D 48 43 10 F7                                  INFO  -> 11 state, total(5), room(5), version...
 *   F0 7D 48 43 20 total(5) F7                         BEGIN -> 7E 20 0(5)       (the UI stops)
 *   F0 7D 48 43 21 offset(5) count(2) data sum F7      DATA  -> 7E 21 next(5)   or 7D 21 error expected(5)
 *   F0 7D 48 43 22 F7                                  END   -> 7E 22 total(5)  or 7D 22 error state(5)
 * numbers 7 bits a byte, low first; data: 8 bytes per 7 (the first holds the others' top bits), count <= 512,
 * sum = the data bytes XOR, & 7F. Sectors are erased as the data reaches them; the host sends DATA in order. */
#define HC_DATA_MAGIC 0x41444348u                /* "HCDA" */
#define HC_DATA_COMMIT 0x54494D43u               /* "CMIT" */
#define HC_DATA_HEAD 4096u                       /* the header's sector; .hc_ovl from here */
struct hc_data_header {
    uint32_t magic;
    uint16_t version, header_bytes;
    uint32_t total, crc;                         /* bytes in all; CRC-32 of [HC_DATA_HEAD, total) */
    uint32_t commit;                             /* HC_DATA_COMMIT once completely written (0xFFFFFFFF in the file) */
    uint32_t vma, size;                          /* where .hc_ovl runs, its bytes */
    uint8_t build_id[32];                        /* the firmware it belongs to (fm1_build_id) */
    char name[4];
};
_Static_assert(sizeof(struct hc_data_header) == 64, "data header");
enum { HC_DATA_OK, HC_DATA_NONE, HC_DATA_BAD, HC_DATA_CRC, HC_DATA_OLD, HC_DATA_ADDR, HC_DATA_LOADING };
static const char *const HC_DATA_SAY[] = {"OK", "NOT INSTALLED", "DAMAGED", "DAMAGED (CRC)", "FOR ANOTHER BUILD",
                                          "ADDRESS", "LOADING"};
static uint8_t hc_data_state = HC_DATA_NONE;
static uint32_t hc_data_mapped;                  /* (HC_DATA_ADDR: where the SDK maps it) */

#ifdef FM1_HOST_TEST
static const unsigned char *hc_test_build_id;    /* (the host test's) */
static uint32_t hc_ovl_test_vma, hc_ovl_test_size;
#define HC_BUILD_ID hc_test_build_id
#define HC_OVL_VMA hc_ovl_test_vma
#define HC_OVL_SIZE hc_ovl_test_size
#else
extern const unsigned char fm1_build_id[32];     /* (build_identity.c, tools/build_open.py) */
extern char hc_ovl_start[], hc_ovl_end[];        /* (the linker: .hc_ovl) */
#define HC_BUILD_ID fm1_build_id
#define HC_OVL_VMA ((uint32_t)(uintptr_t)hc_ovl_start)
#define HC_OVL_SIZE ((uint32_t)(hc_ovl_end - hc_ovl_start))
#endif

static uint32_t hc_data_room(void)               /* the partition less the store */
{
    uint32_t b = st_base();
    return b > HC_DATA_HEAD ? b : 0u;
}

static int hc_data_check(int committed)          /* HC_DATA_* of the image in the flash */
{
    struct hc_data_header h;
    const uint8_t *map;
    uint32_t room = hc_data_room();
    if (!room || fm1_flash_read(0, &h, sizeof h) != FM1_FLASH_OK || h.magic != HC_DATA_MAGIC)
        return HC_DATA_NONE;
    if (h.version != 1u || h.header_bytes != sizeof h || h.total > room || h.total < HC_DATA_HEAD ||
        h.size != h.total - HC_DATA_HEAD || h.commit != (committed ? HC_DATA_COMMIT : 0xFFFFFFFFu))
        return HC_DATA_BAD;
    if (memcmp(h.build_id, HC_BUILD_ID, 32) || h.vma != HC_OVL_VMA || h.size != HC_OVL_SIZE)
        return HC_DATA_OLD;
    if (!(map = fm1_flash_map(0, h.total)))
        return HC_DATA_BAD;
#ifndef FM1_HOST_TEST
    if ((uint32_t)(uintptr_t)(map + HC_DATA_HEAD) != h.vma) {
        hc_data_mapped = (uint32_t)(uintptr_t)(map + HC_DATA_HEAD);
        return HC_DATA_ADDR;
    }
#endif
    if (st_crc32(map + HC_DATA_HEAD, h.size) != h.crc)
        return HC_DATA_CRC;
    return HC_DATA_OK;
}
static void hc_data_boot(void)
{
    hc_data_state = (uint8_t)hc_data_check(1);
    hc_ovl_audio = hc_data_state == HC_DATA_OK;  /* (hc_audio.c: the sound may run) */
}
static int hc_data_ok(void) { return hc_data_state == HC_DATA_OK; }

/* ------------------------------------------------------------ upload --- */
enum { MSG_MAX = 1024u, CHUNK_MAX = 512u };
static uint8_t mbox[MSG_MAX];
static volatile uint32_t mbox_n;
static volatile uint8_t mbox_full, mbox_src;
static uint32_t up_total, up_next, up_erased;
static uint8_t up_active, up_done;               /* up_done: an upload checked out (hc_sdk.c starts the UI) */
static uint8_t chunk[CHUNK_MAX];

void fm1_app_sysex(const uint8_t *m, size_t n)   /* the SDK's routing task: queue it */
{
    if (!m || n < 6u || n > MSG_MAX || m[1] != 0x7Du || m[2] != 0x48u || m[3] != 0x43u || mbox_full)
        return;                                  /* not ours, or one still waiting: the host retries */
    memcpy(mbox, m, n);
    mbox_n = (uint32_t)n;
    mbox_src = (uint8_t)fm1_midi_input_source();
    RING_PUBLISH();
    mbox_full = 1;
}
static uint32_t hc_data_progress(void) { return up_active ? up_next : 0u; }

static uint32_t get7(const uint8_t *p, unsigned bytes)
{
    uint32_t v = 0;
    while (bytes--)
        v = v << 7 | (p[bytes] & 0x7Fu);
    return v;
}
static unsigned put7(uint8_t *p, uint32_t v, unsigned bytes)
{
    unsigned i;
    for (i = 0; i < bytes; i++, v >>= 7)
        p[i] = (uint8_t)(v & 0x7Fu);
    return bytes;
}
static void reply(const uint8_t *body, unsigned n)
{
    uint8_t m[72];
    uint32_t t0 = fm1_millis();
    unsigned port = mbox_src <= FM1_MIDI_BLE ? mbox_src : FM1_MIDI_USB;
    m[0] = 0xF0;
    m[1] = 0x7D;
    m[2] = 0x48;
    m[3] = 0x43;
    memcpy(m + 4, body, n);
    m[4 + n] = 0xF7;
    while (fm1_midi_send_port(port, m, n + 5u) == -2 && fm1_millis() - t0 < 50u)
        ;
}
static void ack(uint8_t cmd, uint32_t v)
{
    uint8_t b[7] = {0x7E, cmd};
    reply(b, 2u + put7(b + 2, v, 5));
}
static void nak(uint8_t cmd, uint8_t err, uint32_t v)
{
    uint8_t b[8] = {0x7D, cmd, err};
    reply(b, 3u + put7(b + 3, v, 5));
}
static void info(void)
{
    uint8_t b[64];
    unsigned n = 0;
    const char *v = HC_VERSION;
    struct hc_data_header h;
    b[n++] = 0x11;
    b[n++] = hc_data_state;
    if (fm1_flash_read(0, &h, sizeof h) != FM1_FLASH_OK || h.magic != HC_DATA_MAGIC)
        h.total = 0;
    n += put7(b + n, hc_data_state == HC_DATA_OK ? h.total : 0u, 5);
    n += put7(b + n, hc_data_room(), 5);
    while (*v && n < sizeof b)
        b[n++] = (uint8_t)(*v++ & 0x7F);
    reply(b, n);
}

static void handle(const uint8_t *m, uint32_t n)
{
    uint8_t cmd = m[4];
    if (m[n - 1] != 0xF7)
        return;
    switch (cmd) {
    case 0x10:
        info();
        break;
    case 0x20: {                                 /* BEGIN: the UI stops (its code is what gets written) */
        uint32_t total;
        if (n != 11u) {
            nak(cmd, 1, 0);
            break;
        }
        total = get7(m + 5, 5);
        if (total < HC_DATA_HEAD || total > hc_data_room()) {
            nak(cmd, 2, hc_data_room());
            break;
        }
        hc_audio_off();                          /* (the sound's code is in it too) */
        hc_data_state = HC_DATA_LOADING;
        up_total = total;
        up_next = up_erased = 0;
        up_active = 1;
        ack(cmd, 0);
        break;
    }
    case 0x21: {                                 /* DATA */
        uint32_t off, cnt, i, groups, sum = 0;
        const uint8_t *p;
        if (!up_active || n < 14u) {
            nak(cmd, 3, up_next);
            break;
        }
        off = get7(m + 5, 5);
        cnt = get7(m + 10, 2);
        groups = (cnt + 6u) / 7u;
        if (!cnt || cnt > CHUNK_MAX || n != 12u + groups * 8u + 2u || off != up_next || off + cnt > up_total) {
            nak(cmd, 4, up_next);
            break;
        }
        p = m + 12;
        for (i = 0; i < cnt; i++) {              /* 8 bytes per 7: the first holds the others' top bits */
            uint32_t g = i / 7u, j = i % 7u;
            chunk[i] = (uint8_t)(p[g * 8u + 1u + j] | ((p[g * 8u] >> j) & 1u) << 7);
            sum ^= chunk[i];
        }
        if ((sum & 0x7Fu) != m[n - 2]) {
            nak(cmd, 5, up_next);
            break;
        }
        while (up_erased < off + cnt) {          /* the sectors this chunk reaches */
            if (fm1_flash_erase(up_erased) != FM1_FLASH_OK) {
                nak(cmd, 6, up_next);
                return;
            }
            up_erased += FM1_FLASH_SECTOR_BYTES;
        }
        if (fm1_flash_write(off, chunk, cnt) != FM1_FLASH_OK) {
            nak(cmd, 7, up_next);
            break;
        }
        up_next = off + cnt;
        ack(cmd, up_next);
        break;
    }
    case 0x22: {                                 /* END: check it all, then commit */
        static const uint32_t commit = HC_DATA_COMMIT;
        int r;
        if (!up_active || up_next != up_total) {
            nak(cmd, 8, up_next);
            break;
        }
        up_active = 0;
        (void)fm1_flash_invalidate(0, up_total);
        r = hc_data_check(0);
        if (r != HC_DATA_OK) {
            hc_data_state = (uint8_t)r;
            nak(cmd, (uint8_t)(16 + r), hc_data_mapped);
            break;
        }
        if (fm1_flash_write(offsetof(struct hc_data_header, commit), &commit, sizeof commit) != FM1_FLASH_OK) {
            hc_data_state = HC_DATA_BAD;
            nak(cmd, 10, 0);
            break;
        }
        (void)fm1_flash_invalidate(0, HC_DATA_HEAD);
        hc_data_boot();
        if (!hc_data_ok()) {
            nak(cmd, (uint8_t)(16 + hc_data_state), hc_data_mapped);
            break;
        }
        up_done = 1;
        ack(cmd, up_total);
        break;
    }
    default:
        break;
    }
}
static void hc_data_poll(void)                   /* the control task: a queued upload message */
{
    if (!mbox_full)
        return;
    RING_PUBLISH();
    handle(mbox, mbox_n);
    mbox_full = 0;
}
