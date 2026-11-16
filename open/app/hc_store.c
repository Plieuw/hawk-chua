/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the settings and the saved presets, kept in the SDK's user flash (fm1_flash_*).
 *
 * The objects (OBJ_SETTINGS; OBJ_USER0 + h: presets 64 h .. 64 h + 63, ui.c) each own two sectors at the end of the
 * user partition, A and B. A save goes into the copy that is not the newest: erase, the payload, then its record
 * (magic, object, sequence, length, CRC-32) last, so a save cut off by the power leaves the other copy as it was.
 * A load takes the valid copy with the newer sequence. The start of the partition is HAWK CHUA's data (hc_data.c). */
enum { OBJ_SETTINGS, OBJ_USER0, OBJ_BOOT = OBJ_USER0 + 8, OBJ_COUNT };   /* (OBJ_BOOT: hc_sdk.c's boot guard) */
#define ST_SECTOR FM1_FLASH_SECTOR_BYTES
#define ST_REC 16u                               /* the record, at the start of the sector */
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_REC)
#define ST_MAGIC 0x54534348u                     /* "HCST" */
#define ST_SECTORS (2u * OBJ_COUNT)
typedef struct { uint32_t magic; uint16_t obj, len; uint32_t seq, crc; } st_rec_t;
_Static_assert(sizeof(st_rec_t) == ST_REC, "store record");

static uint8_t flash_ok;                         /* the user partition answered at boot */
static uint8_t st_buf[ST_PAYLOAD_MAX] __attribute__((aligned(4)));

static uint32_t st_crc32(const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *b++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}
static uint32_t st_base(void)                    /* the store's first sector in the partition (0: no room) */
{
    uint32_t size = fm1_flash_size();
    return size >= (ST_SECTORS + 1u) * ST_SECTOR ? size - ST_SECTORS * ST_SECTOR : 0u;
}
static uint32_t st_at(uint32_t obj, uint32_t copy) { return st_base() + (2u * obj + copy) * ST_SECTOR; }

static int st_rec(uint32_t obj, uint32_t copy, st_rec_t *r)   /* 0: the copy holds a whole object */
{
    if (fm1_flash_read(st_at(obj, copy), r, sizeof *r) != FM1_FLASH_OK || r->magic != ST_MAGIC || r->obj != obj ||
        r->len > ST_PAYLOAD_MAX)
        return -1;
    if (fm1_flash_read(st_at(obj, copy) + ST_REC, st_buf, r->len) != FM1_FLASH_OK || st_crc32(st_buf, r->len) != r->crc)
        return -1;
    return 0;
}
static int st_newest(uint32_t obj, st_rec_t *r)  /* the newer whole copy (0 A, 1 B) into st_buf, or -1 */
{
    st_rec_t a, b;
    int ha, hb;
    if (!flash_ok || obj >= OBJ_COUNT)
        return -1;
    ha = st_rec(obj, 0, &a) == 0;
    hb = st_rec(obj, 1, &b) == 0;
    if (!ha && !hb)
        return -1;
    if (ha && (!hb || (int32_t)(a.seq - b.seq) > 0))
        return st_rec(obj, 0, r) == 0 ? 0 : -1;  /* (read again: st_buf held the last one read) */
    return st_rec(obj, 1, r) == 0 ? 1 : -1;
}
static int st_load(uint32_t obj, void *dst, uint32_t max)   /* bytes loaded (up to max), or -1 */
{
    st_rec_t r;
    uint32_t n;
    if (st_newest(obj, &r) < 0)
        return -1;
    n = r.len < max ? r.len : max;
    memcpy(dst, st_buf, n);
    return (int)r.len;
}
static int st_save(uint32_t obj, const void *src, uint32_t len)   /* 0: saved */
{
    st_rec_t r, now;
    int cur;
    uint32_t to, at;
    if (!flash_ok || obj >= OBJ_COUNT || len > ST_PAYLOAD_MAX || !st_base())
        return -1;
    cur = st_newest(obj, &now);
    to = cur == 0 ? 1u : 0u;
    at = st_at(obj, to);
    memcpy(st_buf, src, len);                    /* (the source may be the mapped flash: never write from it) */
    r.magic = ST_MAGIC;
    r.obj = (uint16_t)obj;
    r.len = (uint16_t)len;
    r.seq = cur < 0 ? 1u : now.seq + 1u;
    r.crc = st_crc32(st_buf, len);
    if (fm1_flash_erase(at) != FM1_FLASH_OK ||
        (len && fm1_flash_write(at + ST_REC, st_buf, len) != FM1_FLASH_OK) ||
        fm1_flash_write(at, &r, sizeof r) != FM1_FLASH_OK)
        return -1;
    return 0;
}
static void st_boot(void)                        /* the control task's start */
{
    flash_ok = st_base() != 0u;
}
