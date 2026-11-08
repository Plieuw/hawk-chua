/* SPDX-License-Identifier: GPL-3.0-only */
/* UI STYLE (GLO's fourth page, KNOB 1): the screen's look. INITIAL is HAWK CHUA's own (the AMBER palette, an
 * oscilloscope's); the others are a palette of their own and a backdrop painted under the big canvas (the plot of
 * every page: gfx.c cv_backdrop), some of it moving (KNOB 2 MOTION: how fast, 0 still) and answering the sound;
 * KNOB 3 BACKDROP how strongly it shows (the curves and text over it stay readable), KNOB 4 REACT how much it
 * answers the sound (the road speeds up, the rings flare, the band brightens, the cat's eyes and the light glow).
 *   RAINBOW ROAD  a road of seven colours running to the horizon under a night of twinkling stars, the curves
 *                 cycling through the hues
 *   GRADIENT      indigo into teal, a soft band of light drifting down it
 *   PSYCHEDELIC   rings of colour spreading from the middle, the curves and the accent turning through the hues
 *   MAGIC CATS    a lilac night with a crescent moon and stars, a cat sitting in the corner: its tail swings, it
 *                 blinks, its eyes glow while the sound rings, and each note sends a few sparkles up
 *   JEZUS         a cathedral's blue: golden rays from above, a haloed cross that shines with the sound, a row of
 *                 stained glass at the foot
 * All drawn here from simple shapes (nothing copied). Saved with the settings. */

/* (the styles: US_INITIAL .. US_COUNT, ui.c's top) */
static const char *const US_NAME[US_COUNT] = {"INITIAL", "RAINBOW ROAD", "GRADIENT", "PSYCHEDELIC", "MAGIC CATS",
                                              "JEZUS"};
static const char *const US_WHAT[US_COUNT] = {"THE OSCILLOSCOPE'S AMBER", "SEVEN COLOURS TO THE HORIZON",
                                              "INDIGO INTO TEAL", "RINGS OF COLOUR", "A CAT UNDER THE MOON",
                                              "GOLDEN LIGHT FROM ABOVE"};
#define US_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* BG, SURF, TEXT, THEME, ACCENT (INITIAL: the AMBER palette instead) */
static const uint16_t US_PAL[US_COUNT][5] = {
    {0, 0, 0, 0, 0},
    {US_RGB(8, 4, 22), US_RGB(26, 16, 52), US_RGB(250, 244, 255), US_RGB(255, 92, 180), US_RGB(110, 240, 255)},
    {US_RGB(14, 8, 34), US_RGB(34, 22, 66), US_RGB(240, 236, 255), US_RGB(255, 172, 128), US_RGB(120, 255, 200)},
    {US_RGB(10, 0, 18), US_RGB(32, 8, 46), US_RGB(255, 250, 240), US_RGB(255, 60, 200), US_RGB(80, 255, 120)},
    {US_RGB(20, 11, 38), US_RGB(46, 30, 74), US_RGB(255, 240, 252), US_RGB(255, 128, 214), US_RGB(124, 255, 226)},
    {US_RGB(8, 14, 44), US_RGB(24, 34, 80), US_RGB(255, 246, 224), US_RGB(242, 193, 78), US_RGB(224, 66, 79)},
};

/* a hue (0..255 round the circle) at full saturation, its value v (0..255), as RGB565 */
static uint16_t us_hue(uint32_t h, uint32_t v)
{
    uint32_t s = (h & 255u) * 6u, seg = s >> 8, f = s & 255u, r, g, b, q = v * (255u - f) / 255u, t = v * f / 255u;
    switch (seg) {
    case 0: r = v; g = t; b = 0; break;
    case 1: r = q; g = v; b = 0; break;
    case 2: r = 0; g = v; b = t; break;
    case 3: r = 0; g = q; b = v; break;
    case 4: r = t; g = 0; b = v; break;
    default: r = v; g = 0; b = q; break;
    }
    return US_RGB(r, g, b);
}

static uint32_t us_t(void)                       /* the motion's clock: MOTION 64 is a turn of the hues in ~8 s */
{
    return (uint32_t)((uint64_t)fm1_ms * (sys.ui_motion & 127u) / 2048u);
}
static int32_t us_k(void) { return (int32_t)(sys.ui_backdrop & 127u) * 100 / 127; }   /* BACKDROP, % */
static int32_t us_react(void)                    /* the sound now, 0 .. 1000, as far as KNOB 4 REACT lets it in */
{
    uint32_t l = synth_out_lvl > 4000u ? 4000u : synth_out_lvl;
    return (int32_t)(l * (sys.ui_react & 127u) / 508u);
}
static uint32_t us_hash(uint32_t i) { i ^= i >> 15; i *= 0x2C1B3C6Du; i ^= i >> 12; i *= 0x297A2D39u; return i ^ (i >> 15); }

/* a twinkling star field over rows y0 .. y1 */
static void us_stars(uint32_t n, int32_t y0, int32_t y1, uint16_t c)
{
    uint32_t i, t = us_t();
    for (i = 0; i < n; i++) {
        uint32_t r = us_hash(i * 7919u + 13u);
        int32_t x = (int32_t)(r % 240u), y = y0 + (int32_t)((r >> 8) % (uint32_t)(y1 - y0 > 0 ? y1 - y0 : 1));
        int32_t tw = (np_sin((t * 40u + (r >> 16)) << 20) >> 9) + 64;   /* 0 .. 128 */
        uint16_t sc = ux_mix(T_BG, c, (tw < 0 ? 0 : tw > 100 ? 100 : tw) * us_k() / 100);
        cv_pset(x, y, sc);
        if ((r & 7u) == 0u && tw > 80) {         /* (a few brighter ones: a little cross) */
            cv_pset(x - 1, y, ux_mix(T_BG, sc, 50));
            cv_pset(x + 1, y, ux_mix(T_BG, sc, 50));
            cv_pset(x, y - 1, ux_mix(T_BG, sc, 50));
            cv_pset(x, y + 1, ux_mix(T_BG, sc, 50));
        }
    }
}

static void us_gradient(uint32_t h, uint16_t top, uint16_t bot, int32_t band)
{
    uint32_t y, t = us_t();
    int32_t k = us_k();
    for (y = 0; y < h; y++) {
        uint16_t c = ux_mix(top, bot, (int32_t)(y * 100u / (h - 1u)));
        if (band) {                              /* a soft band of light drifting down */
            int32_t d = (int32_t)((y * 256u / h + 256u - (t * 3u & 255u)) & 255u) - 128;
            int32_t w = 40 - (d < 0 ? -d : d);
            if (w > 0)
                c = ux_mix(c, T_THEME, w * (band + us_react() / 25) / 40);
        }
        cv_rect(0, (int32_t)y, 240, 1, ux_mix(T_BG, c, k));
    }
}

static void us_rainbow_road(uint32_t h)
{
    int32_t hz = (int32_t)h * 30 / 100, y, k = us_k(), L;
    uint32_t t = us_t();
    us_gradient((uint32_t)hz, T_BG, US_RGB(48, 14, 92), 0);
    us_stars(28, 2, hz - 2, US_RGB(255, 255, 255));
    for (y = hz; y < (int32_t)h; y++) {
        int32_t d = y - hz + 1, span = (int32_t)h - hz, hw = 3 + d * 160 / span, near = 25 + 75 * d / span;
        int32_t z = 6000 / d, stripe = ((z + (int32_t)t * 4 + (int32_t)(fm1_ms * (uint32_t)us_react() / 4000u)) >> 5) & 1, x0 = 120 - hw;
        cv_rect(0, y, 240, 1, ux_mix(T_BG, US_RGB(24, 6, 40), k * near / 100));   /* (the ground) */
        for (L = 0; L < 7; L++) {
            int32_t xa = x0 + 2 * hw * L / 7, xb = x0 + 2 * hw * (L + 1) / 7;
            uint16_t c = us_hue((uint32_t)L * 36u, stripe ? 255u : 175u);
            cv_rect(xa, y, xb - xa, 1, ux_mix(T_BG, c, k * near / 100));
        }
    }
}

static void us_psyche(uint32_t h)
{
    int32_t bx, by, k = us_k() * 55 / 100;
    uint32_t t = us_t();
    for (by = 0; by < (int32_t)h; by += 4)
        for (bx = 0; bx < 240; bx += 8) {
            int32_t dx = bx + 4 - 120, dy = by + 2 - (int32_t)h / 2;
            uint32_t r = (uint32_t)(dx * dx + 4 * dy * dy) >> 7;
            uint32_t hue = r * 3u - t * 2u + (uint32_t)(np_sin((uint32_t)(bx * 3 + (int32_t)t) << 22) >> 11);
            cv_rect(bx, by, 8, 4, ux_mix(T_BG, us_hue(hue + (uint32_t)us_react() / 8u, (r & 8u) ? 200u : 130u),
                                         k + us_react() * (100 - k) / 2000));
        }
}

/* the cat: 16 x 19, drawn twice the size, a wizard's hat on: H the hat, S its star, B fur, P the ears' insides, E an
 * eye, W the muzzle, N the nose, T the tail (it swings) */
#define US_CAT_H 19
static const char *const US_CAT[US_CAT_H] = {
    ".....H..........",
    ".....HH.........",
    "....HHH.........",
    "....HSHH........",
    "...HHHHHH.......",
    ".HHHHHHHHHH.....",
    "..B.......B.....",
    "..BB.....BB.....",
    "..BPB...BPB.....",
    "..BBBBBBBBB.....",
    ".BBEEBBBEEBB....",
    ".BBBBWNWBBBB....",
    ".BBBBWWWBBBB..T.",
    "..BBBBBBBBB...T.",
    "...BBBBBBB....T.",
    "..BBBBBBBBB..TT.",
    ".BBBBBBBBBBBTT..",
    ".BBBBBBBBBBBB...",
    "..BB.BB.BB.BB...",
};
static void us_cats(uint32_t h)
{
    static uint32_t ons_seen, spark_t;
    int32_t k = us_k(), x0 = 194, y0 = (int32_t)h - 2 * US_CAT_H - 2, r, c;
    uint32_t t = us_t(), now = fm1_ms;
    int blink = (now % 4200u) < 140u, glow = us_react() > 120;
    int32_t sway = (int32_t)(np_sin(t << 23) >> 13);   /* -2 .. 2: the tail's swing */
    uint16_t fur = ux_mix(T_BG, US_RGB(250, 220, 255), 20 + k * 60 / 100);
    us_gradient(h, US_RGB(52, 26, 92), US_RGB(12, 6, 30), 0);
    us_stars(36, 2, (int32_t)h - 44, US_RGB(255, 236, 255));
    {   /* the moon: a disc with a darker one over it, a crescent */
        int32_t mx = 30, my = 22, dx, dy;
        for (dy = -10; dy <= 10; dy++)
            for (dx = -10; dx <= 10; dx++)
                if (dx * dx + dy * dy <= 100 && (dx - 5) * (dx - 5) + (dy + 2) * (dy + 2) > 64)
                    cv_pset(mx + dx, my + dy, ux_mix(T_BG, US_RGB(255, 246, 200), 30 + k * 60 / 100));
    }
    for (r = 0; r < US_CAT_H; r++)
        for (c = 0; c < 16; c++) {
            char p = US_CAT[r][c];
            uint16_t col;
            int32_t xs = c, ys = r;
            if (p == '.')
                continue;
            if (p == 'T') {                      /* (the tail swings) */
                xs = c + sway * (US_CAT_H - 1 - r) / 6;
                col = fur;
            } else if (p == 'H')
                col = ux_mix(T_BG, US_RGB(110, 70, 200), 40 + k * 55 / 100);
            else if (p == 'S')
                col = ux_mix(T_BG, US_RGB(255, 230, 120), 50 + (int32_t)((np_sin(t << 24) >> 10) + 32) * k / 128);
            else if (p == 'P')
                col = ux_mix(T_BG, US_RGB(255, 150, 200), 30 + k * 60 / 100);
            else if (p == 'W')
                col = ux_mix(T_BG, US_RGB(255, 255, 255), 30 + k * 65 / 100);
            else if (p == 'E')
                col = blink ? fur : glow ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 70);
            else if (p == 'N')
                col = T_THEME;
            else
                col = fur;
            cv_rect(x0 + xs * 2, y0 + ys * 2, 2, 2, col);
        }
    if (synth_ons != ons_seen) {                 /* a note: sparkles rise from the cat */
        ons_seen = synth_ons;
        spark_t = now;
    }
    if (now - spark_t < 700u) {
        uint32_t a = now - spark_t, i;
        for (i = 0; i < 6u + (uint32_t)us_react() / 150u; i++) {
            uint32_t rr = us_hash(i + spark_t);
            int32_t sx = x0 + 6 + (int32_t)(rr % 10u), sy = y0 + 2 - (int32_t)(a * (16u + (rr >> 8) % 20u) / 700u);   /* (from the hat's star) */
            uint16_t sc = ux_mix(T_BG, i & 1u ? T_ACCENT : T_THEME, 100 - (int32_t)(a * 100u / 700u));
            cv_pset(sx, sy, sc);
            cv_pset(sx - 1, sy, sc); cv_pset(sx + 1, sy, sc); cv_pset(sx, sy - 1, sc); cv_pset(sx, sy + 1, sc);
        }
    }
}

static void us_jezus(uint32_t h)
{
    int32_t k = us_k(), i, cx = 120, cy = 20, lvl = us_react() * 4;
    uint32_t t = us_t();
    int32_t shine = 30 + lvl * 50 / 4000;        /* (the light grows with the sound) */
    uint16_t gold = US_RGB(242, 193, 78);
    us_gradient(h, US_RGB(34, 46, 112), T_BG, 0);
    for (i = 0; i < 18; i++) {                   /* golden rays from above the cross, turning slowly */
        uint32_t a0 = ART_DEG(20 + i * 8) + t * 600000u, a1 = a0 + ART_DEG(3);
        int32_t L = 420;
        art_tri(Q8(cx), Q8(cy), Q8(cx) + L * art_cos(a0) / 128, Q8(cy) + L * art_sin(a0) / 128,
                Q8(cx) + L * art_cos(a1) / 128, Q8(cy) + L * art_sin(a1) / 128, gold,
                (i & 1 ? 55 : 95) * shine * k / 10000);
    }
    {   /* the halo and the cross, a glow around them */
        int32_t dx, dy;
        for (dy = -17; dy <= 17; dy++)
            for (dx = -17; dx <= 17; dx++) {
                int32_t q = dx * dx + dy * dy;
                if (q >= 13 * 13 && q <= 15 * 15)
                    cv_pset(cx + dx, cy + dy, ux_mix(T_BG, gold, 40 + shine * k / 200));
            }
        cv_rect(cx - 3, cy - 13, 7, 30, ux_mix(T_BG, gold, 20 * k / 100));   /* (the glow) */
        cv_rect(cx - 10, cy - 5, 21, 7, ux_mix(T_BG, gold, 20 * k / 100));
        cv_rect(cx - 1, cy - 11, 3, 26, ux_mix(T_BG, gold, 50 + shine * k / 200));
        cv_rect(cx - 8, cy - 3, 17, 3, ux_mix(T_BG, gold, 50 + shine * k / 200));
    }
    {   /* stained glass at the foot: panes of ruby, emerald, sapphire, gold between lead */
        static const uint16_t PANE[4] = {US_RGB(200, 40, 60), US_RGB(40, 160, 90), US_RGB(50, 90, 220), US_RGB(230, 180, 60)};
        int32_t x;
        for (x = 0; x < 240; x += 12) {
            uint16_t c = ux_mix(T_BG, PANE[(x / 12) % 4], 25 + k * 35 / 100);
            cv_rect(x + 1, (int32_t)h - 7, 10, 6, c);
        }
    }
}

static void us_backdrop(uint32_t h)
{
    switch (sys.ui_style % US_COUNT) {
    case US_RAINBOW: us_rainbow_road(h); break;
    case US_GRADIENT: us_gradient(h, US_RGB(76, 24, 128), US_RGB(0, 92, 110), 18); break;
    case US_PSYCHE: us_psyche(h); break;
    case US_CATS: us_cats(h); break;
    case US_JEZUS: us_jezus(h); break;
    default: break;
    }
}

/* the style set: its palette, its backdrop (each change, and at power-on) */
static void style_apply(void)
{
    uint32_t st = sys.ui_style % US_COUNT;
    if (st == US_INITIAL) {
        palette_set(UI_PALETTE);
        cv_backdrop = 0;
    } else {
        ux_cust.bg = US_PAL[st][0];
        ux_cust.surf = US_PAL[st][1];
        ux_cust.text = US_PAL[st][2];
        ux_cust.theme = US_PAL[st][3];
        ux_cust.accent = US_PAL[st][4];
        palette_set(NPALETTES);
        cv_backdrop = us_backdrop;
    }
    ui_force = 1;
}

/* each frame: the colours that move (PSYCHEDELIC: the curves and the accent round the hues; RAINBOW ROAD: the curves) */
static void style_frame(void)
{
    uint32_t st = sys.ui_style % US_COUNT, t = us_t();
    if (st == US_PSYCHE || st == US_RAINBOW) {
        ux.theme = us_hue(t * 2u, 255u);
        if (st == US_PSYCHE)
            ux.accent = us_hue(t * 2u + 128u, 255u);
        ux.sel = ux_mix(ux.bg, ux.theme, UI_SEL_PCT);
        ux.tint = ux_mix(ux.bg, ux.theme, UI_TINT_PCT);
        ux.lane = ux_mix(ux.surf, ux.theme, 10);
    }
}
