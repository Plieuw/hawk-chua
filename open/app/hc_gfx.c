/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the drawing the UI (ui.c, ui_style.c, fx_art.c) uses, on the SDK's screen calls.
 *
 * A canvas (up to 240 x 124 pixels, native RGB565) takes the drawing: rectangles, lines (plain, thick and
 * anti-aliased in 1/16 px), blended points and text; cv_blit sends it to the screen (fm1_screen_blit). The big canvas
 * (the full width, 64 rows or more) is sent in bands of 8 rows, and only the bands that changed since it was last
 * sent to the same place. Text: the two faces of hc_fonts.h (open/tools/gen_font.py), 4-bit alpha, blended over what
 * lies on the canvas. Colours: the theme (ux), five stored colours and the rest mixed from them. */
typedef struct { uint16_t off, adv; int8_t bx, by; uint8_t bw, bh; } hc_glyph_t;   /* adv in 1/16 px */
typedef struct {
    uint8_t h, asc, first, last;     /* line height, baseline (from the line top), the ASCII range */
    const hc_glyph_t *g;             /* first .. last, then the ellipsis */
    const uint8_t *data;
} aafont_t;
typedef struct { const char *name; uint16_t bg, surf, text, theme, accent; } ui_pal_t;
#include "hc_fonts.h"
#define ELLIPSIS '\x85'

#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))

/* ------------------------------------------------------------- theme --- */
static struct {
    uint16_t bg, surf, text, theme, accent;   /* stored */
    uint16_t mid, dim, line, sel, tint, rec;  /* mixed */
    uint8_t mono;
} ux;
#define T_BG ux.bg               /* the ground */
#define T_SURF ux.surf           /* cards */
#define T_TEXT ux.text           /* text */
#define T_THEME ux.theme         /* values, curves */
#define T_ACCENT ux.accent       /* the one active thing */
#define T_MID ux.mid             /* labels */
#define T_DIM ux.dim             /* inactive */
#define T_LINE ux.line           /* dividers, tracks */
#define T_SEL ux.sel             /* a selection */
#define T_TINT ux.tint           /* a faint area */
#define T_REC ux.rec             /* REC */

/* the stored palettes: AMBER, an oscilloscope's (ui.c UI_PALETTE), and the user's own (CUSTOM: ui_style.c) */
static const ui_pal_t UI_PALETTES[] = {{"AMBER", 0x1061, 0x2902, 0xF77B, 0xFD45, 0x66BC}};
#define NPALETTES 1u
static ui_pal_t ux_cust = {"CUSTOM", 0x0841, 0xCE59, 0xEF5D, 0xFFDF, 0xFD20};

/* a + (b - a) * pct / 100, each channel, rounded */
static uint16_t ux_mix(uint16_t a, uint16_t b, int32_t pct)
{
    int32_t r = a >> 11, g = a >> 5 & 63, bl = a & 31;
    r += ((int32_t)(b >> 11) - r) * pct / 100;
    g += ((int32_t)(b >> 5 & 63) - g) * pct / 100;
    bl += ((int32_t)(b & 31) - bl) * pct / 100;
    return (uint16_t)(clamp(r, 0, 31) << 11 | clamp(g, 0, 63) << 5 | clamp(bl, 0, 31));
}
static void palette_set(uint32_t i)
{
    const ui_pal_t *p = i >= NPALETTES ? &ux_cust : &UI_PALETTES[i];
    ux.bg = p->bg; ux.surf = p->surf; ux.text = p->text; ux.theme = p->theme; ux.accent = p->accent;
    ux.mid = ux_mix(p->bg, p->text, 70);
    ux.dim = ux_mix(p->bg, p->text, 42);
    ux.line = ux_mix(p->bg, p->text, 18);
    ux.sel = ux_mix(p->bg, p->theme, 80);
    ux.tint = ux_mix(p->bg, p->theme, 12);
    ux.rec = 0xFA49u;
    ux.mono = 0;
}
static inline uint16_t swap16(uint32_t c) { return (uint16_t)c; }   /* (the canvas holds native pixels) */

/* ------------------------------------------------------------ screen --- */
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c);
static uint8_t lcd_fresh;                        /* the screen painted over: the big canvas goes out whole next time */
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    if (x >= 240u || y >= 240u || !w || !h)
        return;
    if (x + w > 240u) w = 240u - x;
    if (y + h > 240u) h = 240u - y;
    fm1_screen_fill(x, y, w, h, c);
    lcd_fresh = 1;
}
static void lcd_sync(void) {}                    /* (fm1_screen_blit is done when it returns) */

/* ------------------------------------------------------------ canvas --- */
#define CV_MAX (240u * 124u)
static uint16_t cv_px[CV_MAX] __attribute__((aligned(4)));
static uint32_t cv_w, cv_h;
static int32_t cv_oy;                            /* (fx_art.c: added to y by cv_pset) */
static uint16_t cv_bg;
static void (*cv_backdrop)(uint32_t h);          /* the UI style's backdrop under the big canvas (ui_style.c) */
static uint8_t cv_style_off;                     /* (ui.c: pages that keep the plain ground) */

static void cv_begin(uint32_t w, uint32_t h, uint16_t bg)
{
    uint32_t i, n;
    if (!w || w > 240u)
        w = 240u;
    if (w * h > CV_MAX)
        h = CV_MAX / w;
    cv_w = w;
    cv_h = h;
    cv_oy = 0;
    cv_bg = bg;
    n = w * h;
    for (i = 0; i < n; i++)
        cv_px[i] = bg;
    if (!cv_style_off && bg == ux.bg && cv_backdrop && w == 240u && h >= 64u)
        cv_backdrop(h);
}

#define CV_BAND 8u
#define CV_NBAND ((124u + CV_BAND - 1u) / CV_BAND)
static uint32_t cv_hash[CV_NBAND], cv_key;
static void cv_send(uint32_t x, uint32_t y, uint32_t r0, uint32_t r1)   /* canvas rows r0 .. r1 - 1 */
{
    if (r1 > r0 && y + r0 < 240u)
        (void)fm1_screen_blit(x, y + r0, cv_w, (y + r1 > 240u ? 240u - y : r1) - r0, cv_px + r0 * cv_w,
                              (size_t)(r1 - r0) * cv_w);
}
static void cv_blit(uint32_t x, uint32_t y)
{
    uint32_t key = x | y << 8 | cv_w << 16 | cv_h << 24, b, nb, run = 0, all;
    if (x + cv_w > 240u)
        return;
    if (cv_w != 240u || cv_h < 64u) {
        cv_send(x, y, 0, cv_h);
        return;
    }
    all = lcd_fresh || key != cv_key;
    lcd_fresh = 0;
    cv_key = key;
    nb = (cv_h + CV_BAND - 1u) / CV_BAND;
    for (b = 0; b <= nb; b++) {
        int changed = 0;
        if (b < nb) {
            uint32_t r0 = b * CV_BAND, r1 = r0 + CV_BAND > cv_h ? cv_h : r0 + CV_BAND, i, h = 2166136261u;
            const uint16_t *p = cv_px + r0 * 240u;
            for (i = 0; i < (r1 - r0) * 240u; i++)
                h = (h ^ p[i]) * 16777619u;
            changed = all || h != cv_hash[b];
            cv_hash[b] = h;
        }
        if (changed && !run)
            run = b + 1u;
        else if (!changed && run) {
            cv_send(x, y, (run - 1u) * CV_BAND, b * CV_BAND > cv_h ? cv_h : b * CV_BAND);
            run = 0;
        }
    }
}

static inline void cv_pset(int32_t x, int32_t y, uint16_t c)
{
    y += cv_oy;
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h)
        cv_px[(uint32_t)y * cv_w + (uint32_t)x] = c;
}
static inline uint16_t mix565(uint16_t d, uint16_t c, uint32_t a)   /* c over d, a 0 .. 256 */
{
    uint32_t s = ((uint32_t)d | (uint32_t)d << 16) & 0x07E0F81Fu, t = ((uint32_t)c | (uint32_t)c << 16) & 0x07E0F81Fu;
    a >>= 3;                                     /* 0 .. 32 */
    s = (s + (((t - s) * a) >> 5)) & 0x07E0F81Fu;
    return (uint16_t)(s | s >> 16);
}
static inline void cv_pblend(int32_t x, int32_t y, uint16_t c, uint32_t a)   /* a 0 .. 256 */
{
    y += cv_oy;
    if ((uint32_t)x < cv_w && (uint32_t)y < cv_h && a) {
        uint16_t *p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
        *p = a >= 256u ? c : mix565(*p, c, a);
    }
}
static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    int32_t x1 = x + w, y1 = y + h + cv_oy, i, j;
    y += cv_oy;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > (int32_t)cv_w) x1 = (int32_t)cv_w;
    if (y1 > (int32_t)cv_h) y1 = (int32_t)cv_h;
    for (j = y; j < y1; j++)
        for (i = x; i < x1; i++)
            cv_px[(uint32_t)j * cv_w + (uint32_t)i] = c;
}
static void cv_frame(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)
{
    cv_rect(x, y, w, 1, c);
    cv_rect(x, y + h - 1, w, 1, c);
    cv_rect(x, y, 1, h, c);
    cv_rect(x + w - 1, y, 1, h, c);
}
static void cv_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y0 - y1 : y1 - y0, sx = x0 < x1 ? 1 : -1,
            sy = y0 < y1 ? 1 : -1, e = dx + dy, n = 0;
    for (;;) {
        cv_pset(x0, y0, c);
        if ((x0 == x1 && y0 == y1) || ++n > 1024)
            return;
        if (2 * e >= dy) { e += dy; x0 += sx; }
        if (2 * e <= dx) { e += dx; y0 += sy; }
    }
}
static void cv_line_t(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t thick)
{
    int32_t i, j, lo = -(thick - 1) / 2, hi = thick / 2;
    for (i = lo; i <= hi; i++)
        for (j = lo; j <= hi; j++)
            cv_line(x0 + i, y0 + j, x1 + i, y1 + j, c);
}
/* anti-aliased line, ends in 1/16 px: a point a pixel along the long axis, spread over the two pixels across */
static void cv_aline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    int32_t dx = x1 - x0, dy = y1 - y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, n, i;
    int steep = ady > adx;
    n = (steep ? ady : adx) >> 4;
    if (n < 1)
        n = 1;
    if (n > 512)
        n = 512;
    for (i = 0; i <= n; i++) {
        int32_t x = x0 + dx * i / n, y = y0 + dy * i / n;
        if (steep) {
            int32_t f = x & 15;
            cv_pblend(x >> 4, (y + 8) >> 4, c, (uint32_t)(16 - f) << 4);
            cv_pblend((x >> 4) + 1, (y + 8) >> 4, c, (uint32_t)f << 4);
        } else {
            int32_t f = y & 15;
            cv_pblend((x + 8) >> 4, y >> 4, c, (uint32_t)(16 - f) << 4);
            cv_pblend((x + 8) >> 4, (y >> 4) + 1, c, (uint32_t)f << 4);
        }
    }
}

/* -------------------------------------------------------------- text --- */
static const hc_glyph_t *glyph_of(const aafont_t *f, uint32_t ch)
{
    if (ch == (uint8_t)ELLIPSIS)
        return &f->g[f->last - f->first + 1u];
    if (ch < f->first || ch > f->last)
        ch = '?';
    return &f->g[ch - f->first];
}
static int32_t text_w(const aafont_t *f, const char *s)   /* the advance, px */
{
    int32_t pen = 0;
    for (; *s; s++)
        pen += glyph_of(f, (uint8_t)*s)->adv;
    return (pen + 8) >> 4;
}
/* y: the top of the line box; returns the pen x at the end. bg: (the drawing blends over the canvas itself) */
static int32_t cv_text_on(int32_t x, int32_t y, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    int32_t pen = 0;
    (void)bg;
    for (; *s; s++) {
        const hc_glyph_t *g = glyph_of(f, (uint8_t)*s);
        const uint8_t *d = f->data + g->off;
        int32_t gx = x + ((pen + 8) >> 4) + g->bx, gy = y + g->by, i, j, stride = (g->bw + 1) / 2;
        for (j = 0; j < g->bh; j++)
            for (i = 0; i < g->bw; i++) {
                uint32_t a = (d[j * stride + i / 2] >> (i & 1 ? 4 : 0)) & 15u;
                if (a)
                    cv_pblend(gx + i, gy + j, fg, a == 15u ? 256u : a * 17u);
            }
        pen += g->adv;
    }
    return x + ((pen + 8) >> 4);
}
static int32_t cv_text(int32_t x, int32_t y, const aafont_t *f, const char *s, uint16_t fg)
{
    return cv_text_on(x, y, f, s, fg, cv_bg);
}
static int32_t cv_text_r(int32_t xr, int32_t y, const aafont_t *f, const char *s, uint16_t fg, uint16_t bg)
{
    int32_t w = text_w(f, s);
    cv_text_on(xr - w, y, f, s, fg, bg);
    return xr - w;
}
/* a line of text in a box of colour bg, sent at once (align: 0 left, 1 centred, 2 right) */
static void draw_text_line(uint32_t x, uint32_t y, uint32_t w, const aafont_t *f, const char *s, uint16_t c,
                           uint16_t bg, int align)
{
    int32_t tw = text_w(f, s);
    cv_begin(w, f->h, bg);
    cv_text(align == 1 ? ((int32_t)w - tw) / 2 : align == 2 ? (int32_t)w - tw : 0, 0, f, s, c);
    cv_blit(x, y);
}
static void draw_text_box(uint32_t x, uint32_t y, uint32_t w, const aafont_t *f, const char *s, uint16_t c, int align)
{
    draw_text_line(x, y, w, f, s, c, T_BG, align);
}
