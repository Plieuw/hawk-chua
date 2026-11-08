/* SPDX-License-Identifier: GPL-3.0-only */
/* UI STYLE (GLO's fourth page, KNOB 1): the screen's look. INITIAL is HAWK CHUA's own (the AMBER palette, an
 * oscilloscope's); the others are a palette of their own and a backdrop painted under the big canvas (the plot of
 * every page: gfx.c cv_backdrop), some of it moving (KNOB 2 MOTION: how fast, 0 still) and answering the sound;
 * KNOB 3 BACKDROP how strongly it shows (the curves and text over it stay readable), KNOB 4 REACT how much it
 * answers the sound (the road speeds up, the rings flare, the band brightens, the cat's eyes and the light glow).
 *   RAINBOW ROAD  a synthwave night: a striped sun sinking behind purple mountains, a road of seven colours curving
 *                 away to it over a glowing grid, twinkling stars and now and then a shooting one; the road runs
 *                 faster and the sun swells with the sound, the curves cycle through the hues
 *   GRADIENT      the sky's colours drifting from one pair to the next (indigo into teal, dusk, deep sea, mint),
 *                 an aurora's curtains waving over it and soft bokeh rising; the curtains flare with the sound
 *   PSYCHEDELIC   a six-fold kaleidoscope spiralling out of the middle, and in it an eye that looks at you: its
 *                 iris turns through the hues, its pupil opens with the sound, it blinks; the curves and the accent
 *                 turn through the hues too
 *   MAGIC CATS    a lilac night: a crescent moon, stars, a fish drawn in the stars, a wizard cat in the corner with
 *                 a ball of yarn: its tail swings, it blinks, its eyes follow the notes (low left, high right) and
 *                 glow while the sound rings, each note sends sparkles from its hat's star and makes it bat the
 *                 ball; left alone a while it falls asleep (z z z)
 *   JEZUS         a cathedral's blue: golden rays from above, a haloed cross that shines with the sound, dust
 *                 drifting in the light, a dove gliding by now and then, candles flickering on the stained glass
 *   SAIKO         Saiko the husky in fairyland: a pastel dusk over rolling hills, a castle far off with twinkling
 *                 windows, glowing toadstools, fairies flitting about with trails of light; Saiko sits in a crown of
 *                 flowers, blue eyes glowing with the sound: tail wagging (faster as it gets louder), ears perking
 *                 up at each note, tongue out while you play, sparkles from the crown; a rainbow over it all, and it
 *                 rains ketamine: little K's and crystals drifting down, more of them as the sound gets louder
 * All drawn here from simple shapes (nothing copied). Saved with the settings. */

/* (the styles: US_INITIAL .. US_COUNT, ui.c's top) */
static const char *const US_NAME[US_COUNT] = {"INITIAL", "RAINBOW ROAD", "GRADIENT", "PSYCHEDELIC", "MAGIC CATS",
                                              "JEZUS", "SAIKO"};
static const char *const US_WHAT[US_COUNT] = {"THE OSCILLOSCOPE'S AMBER", "A SYNTHWAVE SUNSET",
                                              "AN AURORA OVER DRIFTING SKIES", "THE EYE IN THE KALEIDOSCOPE",
                                              "A WIZARD CAT UNDER THE MOON", "GOLDEN LIGHT FROM ABOVE",
                                              "THE HUSKY IN FAIRYLAND"};
#define US_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* BG, SURF, TEXT, THEME, ACCENT (INITIAL: the AMBER palette instead) */
static const uint16_t US_PAL[US_COUNT][5] = {
    {0, 0, 0, 0, 0},
    {US_RGB(8, 4, 22), US_RGB(26, 16, 52), US_RGB(250, 244, 255), US_RGB(255, 92, 180), US_RGB(110, 240, 255)},
    {US_RGB(14, 8, 34), US_RGB(34, 22, 66), US_RGB(240, 236, 255), US_RGB(255, 172, 128), US_RGB(120, 255, 200)},
    {US_RGB(10, 0, 18), US_RGB(32, 8, 46), US_RGB(255, 250, 240), US_RGB(255, 60, 200), US_RGB(80, 255, 120)},
    {US_RGB(20, 11, 38), US_RGB(46, 30, 74), US_RGB(255, 240, 252), US_RGB(255, 128, 214), US_RGB(124, 255, 226)},
    {US_RGB(8, 14, 44), US_RGB(24, 34, 80), US_RGB(255, 246, 224), US_RGB(242, 193, 78), US_RGB(224, 66, 79)},
    {US_RGB(26, 14, 44), US_RGB(54, 34, 84), US_RGB(255, 246, 252), US_RGB(255, 150, 220), US_RGB(130, 220, 255)},
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
static int32_t us_abs(int32_t v) { return v < 0 ? -v : v; }
static int32_t us_isqrt(int32_t v)              /* floor(sqrt(v)), bit by bit (v < 2^30) */
{
    uint32_t x = v > 0 ? (uint32_t)v : 0u, r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
        b >>= 2;
    }
    return (int32_t)r;
}
static int32_t us_sin(uint32_t a, int32_t amp) { return (int32_t)(((int64_t)np_sin(a) * amp) >> 15); }   /* a: Q32 turn */
static void us_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* (thin, any direction) */
{
    int32_t dx = us_abs(x1 - x0), dy = us_abs(y1 - y0), n = dx > dy ? dx : dy, i;
    for (i = 0; i <= n; i++)
        cv_pset(x0 + (n ? (x1 - x0) * i / n : 0), y0 + (n ? (y1 - y0) * i / n : 0), c);
}
static void us_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    int32_t dy;
    for (dy = -r; dy <= r; dy++) {
        int32_t w = us_isqrt(r * r - dy * dy);
        cv_rect(cx - w, cy + dy, 2 * w + 1, 1, c);
    }
}
static uint32_t us_ang(int32_t x, int32_t y)     /* the angle of (x, y), 0..255 round (roughly: fine for colour) */
{
    int32_t ax = us_abs(x), ay = us_abs(y);
    uint32_t a;
    if (!ax && !ay)
        return 0;
    a = ax >= ay ? (uint32_t)(ay * 32 / ax) : 64u - (uint32_t)(ax * 32 / ay);
    if (x < 0)
        a = 128u - a;
    if (y < 0)
        a = 256u - a;
    return a & 255u;
}

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
    int32_t hz = (int32_t)h * 34 / 100, y, x, k = us_k(), L, span = (int32_t)h - hz, rc = us_react();
    uint32_t t = us_t(), run = t * 4u + fm1_ms * (uint32_t)rc / 3000u;   /* (the road's run: faster with the sound) */
    int32_t curve = us_sin(t << 22, 34);         /* (the far end swings: the road curves) */
    us_gradient((uint32_t)hz, T_BG, US_RGB(96, 18, 112), 0);
    us_stars(30, 2, hz - 16, US_RGB(255, 255, 255));
    {   /* a shooting star, every ~7 s */
        uint32_t ph = fm1_ms % 7000u, seed = us_hash(fm1_ms / 7000u);
        if (ph < 500u && hz > 20) {
            int32_t sx = 20 + (int32_t)(seed % 140u) + (int32_t)ph / 4, sy = 4 + (int32_t)((seed >> 8) % 8u) + (int32_t)ph / 14, i;
            for (i = 0; i < 14; i++)
                cv_pset(sx - i, sy - i / 3, ux_mix(T_BG, US_RGB(255, 255, 255), (100 - i * 7) * k / 100));
        }
    }
    {   /* the sun: a disc going yellow to pink, cut by bands that drift down and widen below */
        int32_t r = 24 + rc / 90, dy;
        for (dy = -r; dy < 0; dy++) {
            int32_t row = dy + r, band = (row + (int32_t)(t / 2u)) % 7, gap = row * 4 / r;
            if (row > r / 3 && band < gap)
                continue;
            {
                int32_t w = us_isqrt(r * r - dy * dy);
                uint16_t c = ux_mix(US_RGB(255, 226, 92), US_RGB(255, 52, 156), row * 100 / r);
                cv_rect(120 - w, hz + dy, 2 * w + 1, 1, ux_mix(T_BG, c, 20 + k * 75 / 100));
            }
        }
    }
    for (x = 0; x < 240; x++) {                  /* two ranges of mountains before it */
        int32_t m1 = 8 + us_sin((uint32_t)x << 24, 4) + us_sin(((uint32_t)x * 3u + 70u) << 23, 3);
        int32_t m2 = 4 + us_abs(us_sin(((uint32_t)x * 5u + 11u) << 22, 6));
        m1 = m1 < 1 ? 1 : m1;
        cv_rect(x, hz - m1, 1, m1, ux_mix(T_BG, US_RGB(58, 16, 88), k));
        cv_rect(x, hz - m2, 1, m2, ux_mix(T_BG, US_RGB(30, 8, 52), k));
        if (us_abs(x - 120) > 30)                /* (their ridge lit pink by the sun) */
            cv_pset(x, hz - m1, ux_mix(T_BG, US_RGB(255, 80, 170), k * 45 / 100));
    }
    for (y = hz; y < (int32_t)h; y++) {
        int32_t d = y - hz + 1, hw = 3 + d * 160 / span, near = 25 + 75 * d / span;
        int32_t far = span - d, cx = 120 + curve * far * far / (span * span);
        int32_t z = 6000 / d, zr = z + (int32_t)run, stripe = (zr >> 5) & 1, x0 = cx - hw;
        uint16_t ground = ux_mix(US_RGB(10, 2, 24), US_RGB(255, 40, 170), (zr & 31) < 2 ? 70 : 0);   /* (the grid) */
        cv_rect(0, y, 240, 1, ux_mix(T_BG, ground, k * near / 100));
        for (x = (cx % 24) - 24; x < 240; x += 24) {   /* (the grid's lines to the horizon) */
            int32_t gx = cx + (x - cx) * d / span * 3;
            cv_pset(gx, y, ux_mix(T_BG, US_RGB(200, 40, 160), k * near / 160));
        }
        for (L = 0; L < 7; L++) {
            int32_t xa = x0 + 2 * hw * L / 7, xb = x0 + 2 * hw * (L + 1) / 7;
            uint16_t c = us_hue((uint32_t)L * 36u, stripe ? 255u : 175u);
            cv_rect(xa, y, xb - xa, 1, ux_mix(T_BG, c, k * near / 100));
        }
        if (stripe) {                            /* (white posts at its edges) */
            cv_rect(x0 - 2, y, 2, 1, ux_mix(T_BG, US_RGB(255, 255, 255), k * near / 100));
            cv_rect(x0 + 2 * hw, y, 2, 1, ux_mix(T_BG, US_RGB(255, 255, 255), k * near / 100));
        }
    }
}

/* GRADIENT: pairs of sky colours it drifts through, an aurora's curtains and bokeh over it */
static void us_aurora(uint32_t h)
{
    static const uint16_t SKY[4][2] = {
        {US_RGB(76, 24, 128), US_RGB(0, 92, 110)},    /* indigo into teal */
        {US_RGB(120, 30, 90), US_RGB(255, 130, 70)},  /* dusk */
        {US_RGB(8, 30, 90), US_RGB(0, 150, 160)},     /* deep sea */
        {US_RGB(30, 90, 110), US_RGB(140, 230, 170)}, /* mint */
    };
    uint32_t t = us_t(), ph = t * 2u, i = (ph >> 8) & 3u, j = (i + 1u) & 3u, x;
    int32_t f = (int32_t)(ph & 255u) * 100 / 255, k = us_k(), rc = us_react(), r;
    uint16_t top, bot;
    f = f < 70 ? 0 : (f - 70) * 100 / 30;        /* (each pair held a while, then a slow change) */
    top = ux_mix(SKY[i][0], SKY[j][0], f);
    bot = ux_mix(SKY[i][1], SKY[j][1], f);
    us_gradient(h, top, bot, 0);
#define US_SKY(y) ux_mix(T_BG, ux_mix(top, bot, (int32_t)((y) < 0 ? 0 : (uint32_t)(y) >= h ? 100u : (uint32_t)(y) * 100u / (h - 1u))), k)
    for (r = 0; r < 2; r++)                      /* the curtains: a bright top edge, fading down */
        for (x = 0; x < 240u; x += 2u) {
            int32_t yc = (int32_t)h * (28 + r * 18) / 100 + us_sin((x * 3u + t * (5u + (uint32_t)r * 3u)) << 22, 10) +
                         us_sin((x * 7u - t * 4u + (uint32_t)r * 90u) << 21, 5);
            int32_t len = 12 + us_sin((x * 5u + t * 6u) << 21, 5) + rc / 60, yy;
            uint16_t c = us_hue(96u + (uint32_t)r * 70u + (x >> 2) + us_sin((x + t * 3u) << 23, 20), 255u);
            for (yy = 0; yy < len; yy++)
                cv_rect((int32_t)x, yc + yy, 2, 1, ux_mix(US_SKY(yc + yy), c, (len - yy) * (75 + rc / 20) * k / (len * 100)));
        }
    for (r = 0; r < 7; r++) {                    /* bokeh, rising */
        uint32_t hs = us_hash((uint32_t)r * 31u + 7u);
        int32_t bx = (int32_t)(hs % 240u) + us_sin((t * 2u + hs) << 22, 6);
        int32_t by = (int32_t)h - (int32_t)((t * (2u + (hs >> 9) % 3u) + (hs >> 4)) % (h + 20u)) + 10;
        us_disc(bx, by, 2 + (int32_t)((hs >> 12) % 4u), ux_mix(US_SKY(by), US_RGB(255, 255, 255), 35 * k / 100));
    }
#undef US_SKY
}

static void us_psyche(uint32_t h)
{
    int32_t bx, by, k = us_k() * 55 / 100, rc = us_react(), cy = (int32_t)h / 2;
    uint32_t t = us_t();
    for (by = 0; by < (int32_t)h; by += 4)       /* the kaleidoscope: six mirrored arms, spiralling */
        for (bx = 0; bx < 240; bx += 4) {
            int32_t dx = bx + 2 - 120, dy = (by + 2 - cy) * 2;
            uint32_t r = (uint32_t)us_isqrt((dx * dx + dy * dy) >> 4) * 4u, a = us_ang(dx, dy) * 6u & 255u;
            uint32_t fold = a > 127u ? 255u - a : a, hue = r * 3u + fold - t * 3u + (uint32_t)rc / 8u;
            uint32_t v = ((r + fold / 2u - t * 2u) & 16u) ? 210u : 120u;
            cv_rect(bx, by, 4, 4, ux_mix(T_BG, us_hue(hue, v), k + rc * (100 - k) / 2000));
        }
    {   /* the eye: an almond, its iris turning through the hues, the pupil opening with the sound; it blinks */
        int32_t dx, dy, W = 38, H = 18, open = (fm1_ms % 5200u) < 160u ? 2 : H;
        int32_t look = us_sin(t << 21, 6), pr = 4 + rc / 160, ek = 35 + us_k() * 55 / 100;
        for (dy = -H; dy <= H; dy++)
            for (dx = -W; dx <= W; dx++) {
                int32_t lid = open * (W * W - dx * dx) / (W * W), ix = dx - look, q = ix * ix + dy * dy;
                uint16_t c;
                if (us_abs(dy) > lid)
                    continue;
                if (us_abs(dy) >= lid - 1)
                    c = ux_mix(T_BG, US_RGB(255, 255, 255), ek);           /* (the lids' edge) */
                else if (q <= pr * pr)
                    c = US_RGB(0, 0, 0);                                    /* (the pupil) */
                else if (q <= 14 * 14)
                    c = ux_mix(T_BG, us_hue(t * 5u + (uint32_t)us_isqrt(q) * 20u, 255u), ek);
                else
                    c = ux_mix(T_BG, US_RGB(250, 240, 255), ek * 70 / 100);  /* (the white) */
                cv_pset(120 + dx, cy + dy, c);
            }
        if (open > 2)
            cv_rect(120 + look + 3, cy - 5, 2, 2, ux_mix(T_BG, US_RGB(255, 255, 255), ek));   /* (a glint) */
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
    static uint32_t ons_seen, spark_t = 0u - 100000u;
    int32_t k = us_k(), x0 = 194, y0 = (int32_t)h - 2 * US_CAT_H - 2, r, c;
    uint32_t t = us_t(), now = fm1_ms;
    int asleep = now - spark_t > 9000u, blink = asleep || (now % 4200u) < 140u, glow = us_react() > 120;
    int look = sys.last_note != 0xFFu && sys.last_note >= 60u;   /* (the eyes: to the last note, low left, high right) */
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
    {   /* a fish drawn in the stars, faint lines between them */
        static const int8_t FX[7] = {70, 84, 100, 112, 100, 84, 70}, FY[7] = {20, 12, 10, 18, 26, 24, 20};
        static const int8_t TX[3] = {62, 56, 56}, TY[3] = {20, 14, 26};
        uint16_t ln = ux_mix(T_BG, US_RGB(200, 170, 255), 25 * k / 100), st = ux_mix(T_BG, US_RGB(255, 250, 255), 50 + k / 2);
        int32_t i;
        for (i = 0; i < 6; i++)
            us_line(FX[i], FY[i], FX[i + 1], FY[i + 1], ln);
        us_line(TX[0], TY[0], TX[1], TY[1], ln); us_line(TX[1], TY[1], TX[2], TY[2], ln); us_line(TX[2], TY[2], TX[0], TY[0], ln);
        us_line(FX[0], FY[0], TX[0], TY[0], ln);
        for (i = 0; i < 7; i++)
            cv_rect(FX[i], FY[i], 2, 2, st);
        cv_pset(104, 16, st);                    /* (its eye) */
    }
    if (synth_ons != ons_seen) {                 /* a note: sparkles rise from the cat, it bats the ball */
        ons_seen = synth_ons;
        spark_t = now;
    }
    {   /* the ball of yarn, its thread to the paw: knocked on a note, it rolls back */
        uint32_t a = now - spark_t;
        int32_t off = a < 900u ? us_sin((a * 3u) << 22, 14 - (int32_t)a * 14 / 900) : 0;
        int32_t bxc = x0 - 16 - us_abs(off), byc = (int32_t)h - 8, i;
        uint16_t yarn = ux_mix(T_BG, US_RGB(255, 110, 170), 35 + k * 60 / 100);
        uint16_t dark = ux_mix(T_BG, US_RGB(170, 50, 110), 35 + k * 60 / 100);
        us_line(bxc + 4, byc + 2, x0 + 6, byc + 4, ux_mix(T_BG, US_RGB(255, 110, 170), 20 + k * 40 / 100));
        us_disc(bxc, byc, 5, yarn);
        for (i = -1; i <= 1; i++)                /* (its winds, turning as it rolls) */
            us_line(bxc - 4, byc + i * 2 + (off & 1), bxc + 4, byc - i * 2 - (off & 1), dark);
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
            else if (p == 'E') {
                int pupil = (US_CAT[r][c - 1] == 'E') == look;   /* (the eye's left or right half) */
                col = blink ? (asleep ? ux_mix(T_BG, US_RGB(60, 40, 90), 70) : fur)
                            : pupil ? US_RGB(20, 10, 30) : glow ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 70);
            }
            else if (p == 'N')
                col = T_THEME;
            else
                col = fur;
            cv_rect(x0 + xs * 2, y0 + ys * 2, 2, 2, col);
        }
    if (asleep) {                                /* z z z, rising from its head */
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            uint32_t a = (now / 8u + i * 120u) % 360u;
            int32_t zx = x0 - 4 - (int32_t)i * 3 + us_sin(a << 24, 2), zy = y0 + 10 - (int32_t)a / 9, sz = 2 + (int32_t)i;
            uint16_t zc = ux_mix(T_BG, US_RGB(230, 220, 255), (360 - (int32_t)a) * (40 + k / 2) / 360);
            cv_rect(zx, zy, sz + 1, 1, zc);      /* (a little z: top, diagonal, bottom) */
            us_line(zx + sz, zy, zx, zy + sz, zc);
            cv_rect(zx, zy + sz, sz + 1, 1, zc);
        }
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
    for (i = 0; i < 26; i++) {                   /* dust drifting in the light, glinting */
        uint32_t hs = us_hash((uint32_t)i * 977u + 3u);
        int32_t dx = (int32_t)(hs % 200u) + 20 + us_sin((t + (hs >> 6)) << 21, 8);
        int32_t dy = (int32_t)h - 10 - (int32_t)((t / 2u + (hs >> 10)) % (h - 12u));
        int32_t tw = us_sin((t * 9u + hs) << 21, 50) + 50;
        cv_pset(dx, dy, ux_mix(T_BG, gold, tw * (20 + shine) * k / 10000));
    }
    {   /* a dove gliding by now and then, its wings beating */
        static const char *const DOVE[2][5] = {
            {"..X......", "XXXX.....", ".XXXXXXX.", "...XXXXXX", "........X"},   /* wings up */
            {".........", ".XXXXXXX.", "XXXXXXXXX", "..XX.....", ".X......."},   /* wings down */
        };
        uint32_t ph = fm1_ms % 14000u;
        if (ph < 6000u) {
            int32_t px = (int32_t)ph * 280 / 6000 - 20, py = 44 + us_sin(ph << 20, 6), r2, c2;
            uint32_t f = (ph / 180u) & 1u;
            for (r2 = 0; r2 < 5; r2++)
                for (c2 = 0; c2 < 9; c2++)
                    if (DOVE[f][r2][c2] == 'X')
                        cv_rect(px + c2 * 2, py + r2 * 2, 2, 2, ux_mix(T_BG, US_RGB(255, 255, 255), 40 + k / 2));
        }
    }
    {   /* candles on the sill: their flames flicker, brighter with the sound */
        static const int16_t CX[6] = {14, 40, 66, 174, 200, 226};
        for (i = 0; i < 6; i++) {
            int32_t x = CX[i], yb = (int32_t)h - 8, fl = (int32_t)(us_hash((uint32_t)i * 11u + fm1_ms / 90u) % 3u);
            int32_t ch = 8 + (i % 3) * 3;
            cv_rect(x - 1, yb - ch, 3, ch, ux_mix(T_BG, US_RGB(250, 236, 210), 30 + k / 2));
            us_disc(x, yb - ch - 4, 4 + shine / 30, ux_mix(T_BG, gold, (12 + shine / 4) * k / 100));   /* (the glow) */
            cv_rect(x - 1 + (fl == 2), yb - ch - 5 - fl / 2, 2, 4, ux_mix(T_BG, US_RGB(255, 150, 40), 50 + k / 2));
            cv_rect(x, yb - ch - 3, 1, 2, ux_mix(T_BG, US_RGB(255, 245, 180), 60 + k * 40 / 100));
        }
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

/* Saiko, sitting: 20 x 19, drawn twice the size. G the grey coat, W white, P the ears' insides, F the crown of
 * flowers, E an eye (husky blue), K the pupils and the nose, M the mouth, L the tongue (out while you play), T the
 * tail (it wags) */
#define US_DOG_H 19
static const char *const US_DOG[US_DOG_H] = {
    "..G.........G.......",
    "..GG.......GG.......",
    "..GPG.....GPG.......",
    "..GPFFFFFFFPG.......",
    "..GGGGGGGGGGG.......",
    ".GGWWGGGGGWWGG......",
    ".GWEKWGGGWKEWG......",
    ".GWWWWWGWWWWWG......",
    "..WWWWWKWWWWW.......",
    "..WWWWMMMWWWW.......",
    "...WWWWLWWWW....TT..",
    "....WWWWWWW....T..T.",
    "...GWWWWWWWG...T....",
    "..GGWWWWWWWGG..T....",
    "..GGWWWWWWWGGTT.....",
    ".GGGWWWWWWWGGG......",
    ".GGWWWWGWWWWGG......",
    ".GGWW..G..WWGG......",
    "..WW.......WW.......",
};
static void us_saiko(uint32_t h)
{
    static uint32_t ons_seen, note_t = 0u - 100000u;
    int32_t k = us_k(), rc = us_react(), i, x0 = 168, y0 = (int32_t)h - 2 * US_DOG_H - 1, r, c;
    uint32_t t = us_t(), now = fm1_ms, since;
    int32_t hz = (int32_t)h * 62 / 100;
    if (synth_ons != ons_seen) {
        ons_seen = synth_ons;
        note_t = now;
    }
    since = now - note_t;
    us_gradient((uint32_t)hz + 6u, US_RGB(110, 70, 170), US_RGB(255, 168, 210), 0);
    us_stars(22, 2, hz / 2, US_RGB(255, 250, 230));
    {   /* a rainbow, over the hills: seven bands, the sky showing through a little */
        int32_t cx = 120, cy = hz + 26, R0 = 66, bw = 3, dy, b;
        for (dy = -(R0 + 7 * bw); dy < hz + 6 - cy && dy <= 0; dy++) {
            int32_t y = cy + dy, sky = y * 100 / (hz + 5);
            uint16_t base = ux_mix(T_BG, ux_mix(US_RGB(110, 70, 170), US_RGB(255, 168, 210), sky < 0 ? 0 : sky > 100 ? 100 : sky), k);
            if (y < 0)
                continue;
            for (b = 0; b < 7; b++) {
                int32_t ro = R0 + (7 - b) * bw, ri = ro - bw, xo, xi;
                if (dy * dy > ro * ro)
                    continue;
                xo = us_isqrt(ro * ro - dy * dy);
                xi = dy * dy < ri * ri ? us_isqrt(ri * ri - dy * dy) : 0;
                {
                    uint16_t c = ux_mix(base, us_hue((uint32_t)b * 36u, 255u), 30 + k * 40 / 100);
                    cv_rect(cx - xo, y, xo - xi, 1, c);
                    cv_rect(cx + xi + 1, y, xo - xi, 1, c);
                }
            }
        }
    }
    {   /* the castle, far off: towers with pointed roofs, windows that twinkle */
        static const int8_t TX[5] = {18, 30, 42, 54, 66}, TH[5] = {18, 28, 36, 26, 16};
        uint16_t wall = ux_mix(T_BG, US_RGB(196, 150, 220), k * 80 / 100);
        int32_t base = hz + 2;
        cv_rect(14, base - 12, 58, 12, wall);
        for (i = 0; i < 5; i++) {
            int32_t tx = TX[i], th = TH[i], y;
            cv_rect(tx - 4, base - th, 9, th, wall);
            for (y = 0; y < 8; y++)              /* (its roof) */
                cv_rect(tx - 4 + y / 2, base - th - 8 + y, 9 - y, 1, ux_mix(T_BG, US_RGB(230, 120, 190), k * 85 / 100));
            if ((us_hash((uint32_t)i * 5u + now / 700u) & 3u) != 0u)
                cv_rect(tx - 1, base - th + 4, 2, 3, ux_mix(T_BG, US_RGB(255, 236, 140), 40 + k / 2));
        }
        cv_rect(39, base - 7, 6, 7, ux_mix(T_BG, US_RGB(120, 70, 140), k));   /* (the gate) */
    }
    {   /* two rolling hills, mint and deeper green */
        int32_t x;
        for (x = 0; x < 240; x++) {
            int32_t y1 = hz + us_sin(((uint32_t)x * 2u + 40u) << 23, 6), y2 = hz + 12 + us_sin(((uint32_t)x * 3u + 170u) << 22, 7);
            cv_rect(x, y1, 1, (int32_t)h - y1, ux_mix(T_BG, US_RGB(150, 230, 190), k * 75 / 100));
            cv_rect(x, y2, 1, (int32_t)h - y2, ux_mix(T_BG, US_RGB(70, 170, 120), k * 80 / 100));
            if (((uint32_t)x * 7u + (uint32_t)y2) % 11u == 0u)   /* (little flowers in the grass) */
                cv_pset(x, y2 + 4 + (x % 5), ux_mix(T_BG, us_hue((uint32_t)x * 37u, 255u), 40 + k / 2));
        }
    }
    {   /* glowing toadstools: red caps with white dots, the glow breathing with the sound */
        static const int16_t MX[4] = {96, 122, 142, 228};
        static const int8_t MS[4] = {6, 4, 5, 4};
        for (i = 0; i < 4; i++) {
            int32_t mx = MX[i], sz = MS[i], my = (int32_t)h - 6 - (i & 1) * 4, gl = 15 + rc / 25 + us_sin((t * 3u + (uint32_t)i * 60u) << 22, 8);
            us_disc(mx, my - sz, sz + 4, ux_mix(ux_mix(T_BG, US_RGB(70, 170, 120), k * 80 / 100), US_RGB(255, 170, 220), gl * k / 100));
            cv_rect(mx - 1, my - sz, 3, sz + 2, ux_mix(T_BG, US_RGB(255, 240, 220), 40 + k / 2));
            for (r = 0; r <= sz; r++)            /* (the cap: a half disc) */
                cv_rect(mx - us_isqrt(sz * sz - r * r) - 1, my - sz - r, 2 * us_isqrt(sz * sz - r * r) + 3, 1,
                        ux_mix(T_BG, US_RGB(235, 50, 70), 40 + k / 2));
            cv_pset(mx - sz / 2, my - sz - sz / 2, US_RGB(255, 255, 255));
            cv_pset(mx + sz / 2, my - sz - 1, US_RGB(255, 255, 255));
            cv_pset(mx, my - 2 * sz + 1, US_RGB(255, 255, 255));
        }
    }
    for (i = 0; i < 6; i++) {                    /* fairies, flitting, a trail of light behind each */
        uint32_t hs = us_hash((uint32_t)i * 131u + 9u), sp = 3u + (uint32_t)rc / 200u, j;
        uint16_t col = us_hue(hs, 255u);
        for (j = 0; j < 6u; j++) {
            uint32_t tt = t * sp - j * 3u;
            int32_t fx = 120 + us_sin((tt * (2u + hs % 3u) + hs) << 21, 108), fy = hz / 2 + 10 + us_sin((tt * (3u + (hs >> 4) % 3u) + (hs >> 8)) << 21, hz / 2 - 4);
            if (j == 0) {
                int32_t up = (int32_t)((now / 60u + (uint32_t)i) & 1u);
                cv_rect(fx - 1, fy - 1, 3, 3, ux_mix(T_BG, US_RGB(255, 255, 240), 50 + k / 2));
                cv_pset(fx - 2, fy - 1 - up, ux_mix(T_BG, US_RGB(220, 240, 255), 50 + k / 2));   /* (wings) */
                cv_pset(fx + 2, fy - 1 - up, ux_mix(T_BG, US_RGB(220, 240, 255), 50 + k / 2));
                cv_pset(fx, fy, col);
            } else
                cv_pset(fx, fy, ux_mix(T_BG, col, (60 - (int32_t)j * 9) * k / 100));
        }
    }
    {   /* it rains ketamine: little K's and crystals drifting down (more with the sound) */
        static const char *const KG[5] = {"X.X", "XX.", "X..", "XX.", "X.X"};
        uint32_t n = 18u + (uint32_t)rc / 40u;
        for (i = 0; i < (int32_t)n; i++) {
            uint32_t hs = us_hash((uint32_t)i * 613u + 21u), fall = (uint32_t)h + 12u;
            int32_t y = (int32_t)((t * (3u + (hs >> 7) % 4u) + (hs >> 3)) % fall) - 6;
            int32_t x = (int32_t)(hs % 248u) - 4 + y / 6 + us_sin((t * 2u + hs) << 22, 3);
            uint16_t col = ux_mix(T_BG, (hs >> 20) & 1u ? US_RGB(255, 255, 255) : US_RGB(225, 205, 255), 45 + k / 2);
            if (hs & 1u) {                       /* a K */
                int32_t a, b2;
                for (a = 0; a < 5; a++)
                    for (b2 = 0; b2 < 3; b2++)
                        if (KG[a][b2] == 'X')
                            cv_pset(x + b2, y + a, col);
            } else {                             /* a crystal, glinting */
                cv_pset(x, y, col);
                if (((hs >> 9) + t) % 6u < 2u) {
                    cv_pset(x - 1, y, col); cv_pset(x + 1, y, col); cv_pset(x, y - 1, col); cv_pset(x, y + 1, col);
                }
            }
        }
    }
    {   /* Saiko */
        int perk = since < 350u, tongue = since < 1500u || rc > 80, blink = (now % 3700u) < 130u;
        int32_t wag = us_sin(now * (4u + (uint32_t)rc / 40u) << 20, 3);
        uint16_t grey = ux_mix(T_BG, US_RGB(120, 128, 150), 45 + k * 55 / 100);
        uint16_t white = ux_mix(T_BG, US_RGB(250, 250, 255), 45 + k * 55 / 100);
        static const uint16_t FL[4] = {US_RGB(255, 120, 190), US_RGB(255, 230, 90), US_RGB(140, 210, 255), US_RGB(255, 255, 255)};
        us_disc(x0 + 16, (int32_t)h + 2, 18, ux_mix(T_BG, US_RGB(50, 130, 90), k * 80 / 100));   /* (his shadow on the hill) */
        for (r = 0; r < US_DOG_H; r++)
            for (c = 0; c < 20; c++) {
                char p = US_DOG[r][c];
                uint16_t col;
                int32_t xs = c, ys = r;
                if (p == '.')
                    continue;
                if (r <= 1 && perk)              /* (the ears up at a note) */
                    ys = r - 1;
                switch (p) {
                case 'G': col = grey; break;
                case 'P': col = ux_mix(T_BG, US_RGB(255, 170, 200), 45 + k / 2); break;
                case 'F': col = ux_mix(T_BG, FL[(c + (int32_t)(t / 8u)) & 3], 50 + k / 2); break;
                case 'E': col = blink ? grey : ux_mix(US_RGB(70, 160, 255), US_RGB(190, 245, 255), rc / 10); break;
                case 'K': col = r == 6 && blink ? grey : US_RGB(20, 20, 30); break;
                case 'M': col = ux_mix(T_BG, US_RGB(60, 50, 70), 70); break;
                case 'L': col = tongue ? US_RGB(255, 110, 150) : white; break;
                case 'T': xs = c + wag * (15 - r) / 3; col = r <= 11 ? white : grey; break;
                default: col = white; break;
                }
                cv_rect(x0 + xs * 2, y0 + ys * 2, 2, 2, col);
            }
        if (tongue)                              /* (the tongue hangs a little further) */
            cv_rect(x0 + 14, y0 + 22, 2, 2, US_RGB(255, 110, 150));
        if (since < 800u) {                      /* sparkles from the crown */
            uint32_t a = since;
            for (i = 0; i < 8; i++) {
                uint32_t rr = us_hash((uint32_t)i + note_t);
                int32_t sx = x0 + 8 + (int32_t)(rr % 12u) * 2 + us_sin((rr + a * 4u) << 22, 3);
                int32_t sy = y0 + 6 - (int32_t)(a * (14u + (rr >> 8) % 20u) / 800u);
                uint16_t sc = ux_mix(T_BG, FL[i & 3], 100 - (int32_t)(a * 100u / 800u));
                cv_pset(sx, sy, sc); cv_pset(sx - 1, sy, sc); cv_pset(sx + 1, sy, sc); cv_pset(sx, sy - 1, sc); cv_pset(sx, sy + 1, sc);
            }
        }
    }
}

static void us_backdrop(uint32_t h)
{
    switch (sys.ui_style % US_COUNT) {
    case US_RAINBOW: us_rainbow_road(h); break;
    case US_GRADIENT: us_aurora(h); break;
    case US_PSYCHE: us_psyche(h); break;
    case US_CATS: us_cats(h); break;
    case US_JEZUS: us_jezus(h); break;
    case US_SAIKO: us_saiko(h); break;
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
