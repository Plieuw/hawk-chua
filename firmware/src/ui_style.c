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
 *                 iris turns through the hues, its pupil opens with the sound, it blinks; the blocks round it stay plain
 *   MAGIC CATS    a lilac night: a crescent moon, stars, a fish drawn in the stars, a cat, big in the middle, with
 *                 a ball of yarn: its tail swings, it blinks, its eyes follow the notes (low left, high right) and
 *                 glow while the sound rings, each note sends sparkles up from between its ears and makes it bat the
 *                 ball; every 4 bars it pounces, after the ball of yarn or the laser dot, and back; left alone a
 *                 while it falls asleep (z z z)
 *   JEZUS         a cathedral's blue: golden rays from above, a haloed cross that shines with the sound, dust
 *                 drifting in the light, a dove gliding by now and then, candles flickering on the stained glass
 *   SAIKO         Saiko the husky in fairyland: a pastel dusk over rolling hills, a castle in the air on clouds
 *                 (floating to and fro over 4 bars) with twinkling windows, glowing toadstools, fairies flitting about with trails of light; Saiko sits in a crown of
 *                 flowers, blue eyes glowing with the sound: tail wagging (faster as it gets louder), ears perking
 *                 up at each note, tongue out while you play, sparkles from the crown; a rainbow over it all, and it
 *                 rains ketamine: little K's and crystals drifting down, more of them as the sound gets louder
 * While the ARP, the SEQ or the TAPE plays, everything grooves: holograms rise on each step (a ring of light, a
 * spinning hexagon glitching upward), a display of the eight steps across the top, a scan line down the screen, the
 * flashes pulse on the tempo's beat between the notes too, the characters hop, Jezus walks a step a beat.
 * Each style's scene changes with the oscillator (HOME: the circuit or a noise program): RAINBOW ROAD four lands
 * (purple mountains, a neon city, palm trees, pyramids), the road's colours turned; GRADIENT other skies and
 * aurora colours; PSYCHEDELIC 4 .. 8 arms, turning the other way, sometimes coarse; MAGIC CATS another cat (its fur)
 * and the moon in another phase; JEZUS another sign (the cross, a sacred heart, the fish, a chalice) in gold,
 * silver or rose light; SAIKO another season (dusk, night, snow, autumn) and another look (crown, shades, party hat).
 * The pages themselves (the plot, the cells) stay as they are.
 * All drawn here from simple shapes (nothing copied). Saved with the settings. */

/* (the styles: US_INITIAL .. US_COUNT, ui.c's top) */
static const char *const US_NAME[US_COUNT] = {"INITIAL", "RAINBOW ROAD", "GRADIENT", "PSYCHEDELIC", "MAGIC CATS",
                                              "JEZUS", "SAIKO"};
static const char *const US_WHAT[US_COUNT] = {"THE OSCILLOSCOPE'S AMBER", "A SYNTHWAVE SUNSET",
                                              "AN AURORA OVER DRIFTING SKIES", "THE EYE IN THE KALEIDOSCOPE",
                                              "A CAT UNDER THE MOON", "GOLDEN LIGHT FROM ABOVE",
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
static uint32_t us_var(void) { return sys.osc & 31u; }   /* (the scene's variant: the oscillator) */
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
/* the beat: each note started (the ARP's and the SEQ's steps too): when, how many; groove: the ARP or the SEQ plays */
static uint32_t us_bt = 0u - 100000u, us_bn, us_bseen;
static uint32_t us_since(void) { return fm1_ms - us_bt; }
static int32_t us_bf(void) { uint32_t a = us_since(); return a < 200u ? 200 - (int32_t)a : 0; }   /* a beat's flash, 200 .. 0 */
static int us_groove(void)                      /* the ARP with notes to play, the SEQ (KARMA) running, the TAPE playing */
{
    return (sys.arp_on && arp.n) || (sys.k_on && (kar.running || kar.n)) || tape.playing;
}
static uint32_t us_bclk(void)                    /* the tempo's beat (the BPM or the MIDI clock): its phase, 0 .. 255 */
{
    uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm);
    return (fm1_ms % beat) * 256u / beat;
}
static int32_t us_hop(int32_t px)                /* a hop on each step while it grooves: up px at the top, 200 ms */
{
    uint32_t a = us_since();
    return us_groove() && a < 200u ? (int32_t)(a * (200u - a)) * px / 10000 : 0;
}
static int32_t us_gf(void)                       /* the groove's flash: a step's (200 .. 0), or the tempo's beat's while
                                                  * it grooves (so it pulses between the notes too) */
{
    int32_t f = us_bf(), b = 0;
    if (us_groove()) {
        uint32_t ph = us_bclk();
        b = ph < 64u ? (int32_t)(64u - ph) * 3 : 0;
    }
    return f > b ? f : b;
}
/* the oscillator, played into the scene (on the pages that showed its picture, the style shows this instead): its
 * HOME knobs 1..4 (the circuit: CHAOS, SHAPE, OUTPUT, RING; a noise program: X, Y, LEVEL or its own, RING), and where
 * its signal is now (the trace's mean over its last points, -100 .. 100, smoothed: a slow wander), how much it moves */
static int32_t us_kn(uint32_t i)
{
    uint8_t v = !sys.osc ? (i == 0u ? sys.chaos : i == 1u ? sys.shape : i == 2u ? sys.output : sys.ring)
              : (i == 0u ? sys.np_x : i == 1u ? sys.np_y : sys.osc > NP_BIGBANG ? (i == 2u ? sys.chaos : sys.output)
                                                                                  : (i == 2u ? sys.np_level : sys.ring));
    return (int32_t)(v & 127u);
}
static int32_t us_sx, us_sy, us_se;             /* the signal: where (x, y: -100 .. 100), its movement (0 .. 100) */
static void us_signal(void)                      /* (once a frame) */
{
    static int32_t mx = 64, my = 64, me = 64, lx, ly;
    uint32_t w = chua_trace.w, i, n = w < 64u ? w : 64u;
    int32_t ax = 0, ay = 0, e = 0, tx, ty, te;
    for (i = 0; i < n; i++) {
        uint32_t j = (w - 1u - i) & (TRACE_N - 1u);
        ax += chua_trace.x[j];
        ay += chua_trace.y[j];
    }
    if (n) {
        ax /= (int32_t)n;
        ay /= (int32_t)n;
    }
    e = us_abs(ax - lx) + us_abs(ay - ly);
    lx = ax;
    ly = ay;
    mx = us_abs(ax) > mx ? us_abs(ax) : mx - (mx >> 6) - 1;
    my = us_abs(ay) > my ? us_abs(ay) : my - (my >> 6) - 1;
    me = e > me ? e : me - (me >> 6) - 1;
    mx = mx < 24 ? 24 : mx;
    my = my < 24 ? 24 : my;
    me = me < 24 ? 24 : me;
    tx = ax * 100 / mx;
    ty = ay * 100 / my;
    te = e * 100 / me;
    us_sx += (tx - us_sx) / 5;
    us_sy += (ty - us_sy) / 5;
    us_se += (te - us_se) / 4;
}

/* the envelope (the ENV's ADSR, the loudest voice's), 0 .. 100, and whether a voice is in its attack: the scenes
 * play with it (the cat's laser eyes, Saiko's howl, the kart's boost, the light's swell) */
static int32_t us_env(int *attack)
{
    uint32_t i, m = 0;
    int at = 0;
    for (i = 0; i < NVOICE; i++)
        if (voice[i].stage != ST_OFF) {
            if (voice[i].env > m)
                m = voice[i].env;
            at |= voice[i].stage == ST_ATTACK;
        }
    if (attack)
        *attack = at;
    return (int32_t)(m >> 23) * 100 / 128;      /* (ENV_MAX = 2^30: 128 in 2^23s) */
}

#define US_NN 8
static uint32_t us_nt[US_NN], us_nw;             /* the notes rising: the beats they were born at */
static void us_beat(void)                        /* (once a frame) */
{
    if (synth_ons != us_bseen) {
        us_bseen = synth_ons;
        us_bt = fm1_ms;
        us_bn++;
        if (us_groove())
            us_nt[us_nw++ % US_NN] = fm1_ms;
    }
}
/* on the steps of the ARP, the SEQ and the TAPE, a hologram from (x, y): a ring of light widening flat, a hexagon
 * glyph rising and glitching (shifting sideways, scan lines missing), cyan into magenta, a beam to it */
static void us_holo(int32_t x, int32_t y)
{
    uint32_t i;
    for (i = 0; i < US_NN; i++) {
        uint32_t a = fm1_ms - us_nt[i], hs = us_hash(us_nt[i] + i), j;
        int32_t gx, gy, rx, ry, fade, sh;
        uint16_t col, ring;
        if (!us_nt[i] || a > 1400u)
            continue;
        fade = 100 - (int32_t)(a * 100u / 1400u);
        col = ux_mix(T_BG, ux_mix(US_RGB(80, 240, 255), US_RGB(255, 80, 230), (int32_t)(a * 100u / 1400u)), fade);
        if (a < 500u) {                          /* the ring, widening flat */
            rx = 8 + (int32_t)a * 40 / 500;
            ry = rx / 4 + 1;
            ring = ux_mix(T_BG, US_RGB(120, 255, 255), 100 - (int32_t)a / 5);
            for (j = 0; j < 64u; j++) {
                uint32_t ang = j << 26;          /* (Q32: 64 steps a turn) */
                cv_pset(x + us_sin(ang + (1u << 30), rx), y + us_sin(ang, ry), ring);
            }
        }
        sh = (us_hash(hs + a / 70u) & 7u) == 0u ? ((hs & 1u) ? 3 : -3) : 0;   /* (a glitch now and then) */
        gx = x + (int32_t)(hs % 50u) - 25 + sh;
        gy = y - 10 - (int32_t)(a * 60u / 1400u);
        for (j = 0; j < 6u; j++) {               /* the hexagon */
            int32_t r = 5 + (int32_t)((hs >> 8) % 3u);
            uint32_t a1 = j * 715827883u + (uint32_t)(a << 22), a2 = a1 + 715827883u;   /* (a sixth of a turn; it spins) */
            int32_t x1 = gx + us_sin(a1 + (1u << 30), r), y1 = gy + us_sin(a1, r);
            int32_t x2 = gx + us_sin(a2 + (1u << 30), r), y2 = gy + us_sin(a2, r);
            us_line(x1, y1, x2, y2, col);
        }
        cv_rect(gx - 1, gy - 1, 2, 2, col);      /* (its core) */
        if (a < 300u)                            /* the beam up to it */
            for (j = (uint32_t)gy + 6u; (int32_t)j < y; j += 2u)
                cv_pset(gx, (int32_t)j, ux_mix(T_BG, US_RGB(120, 255, 255), 60 - (int32_t)a / 5));
        if ((a / 90u) & 1u)                      /* (scan lines through it, flickering) */
            cv_rect(gx - 7, gy - 1 + (int32_t)((a / 30u) % 3u), 15, 1, T_BG);
    }
}
/* the steps, as a display across the top while it grooves: eight cells, the step now lit, a scan line sweeping down
 * the screen on each step */
static void us_hud(uint32_t h)
{
    uint32_t i, cur = us_bn & 7u, a = us_since();
    int32_t k = us_k();
    if (!us_groove())
        return;
    for (i = 0; i < 8u; i++) {
        int32_t x = 120 - 4 * 13 + (int32_t)i * 13;
        uint16_t c = i == cur ? ux_mix(US_RGB(80, 240, 255), US_RGB(255, 255, 255), us_gf() / 4)
                              : ux_mix(T_BG, US_RGB(80, 240, 255), 18 + k / 6);
        cv_rect(x, 3, 10, 3, c);
        if (i == cur)
            cv_rect(x - 1, 2, 12, 1, ux_mix(T_BG, US_RGB(80, 240, 255), 50));
    }
    if (a < 260u) {
        int32_t y = (int32_t)(a * h / 260u);
        cv_rect(0, y, 240, 1, ux_mix(T_BG, US_RGB(120, 255, 255), 45 - (int32_t)a / 7));
    }
}
/* a visitor in the sky, one for each FX model (six kinds, coloured by the model): a UFO, a hot air balloon, a flock
 * of birds, butterflies, a kite, a jellyfish; each answers the beat */
static void us_visitor(uint32_t h, int32_t ytop, int32_t ybot)
{
    uint32_t fx = sys.fx_model % RZ_COUNT, kind = fx % 6u, t = us_t(), now = fm1_ms;
    int32_t k = us_k(), bf = us_gf(), ymid = (ytop + ybot) / 2, amp = (ybot - ytop) / 2, i;
    uint16_t tint = us_hue(fx * 47u, 255u);
    (void)h;
    switch (kind) {
    case 0: {   /* a UFO, hovering, its lights running round, a beam on each beat */
        int32_t ox = 120 + us_sin(t << 21, 90), oy = ymid + us_sin(t << 22, amp / 2);
        if (bf)
            art_tri(Q8(ox), Q8(oy + 2), Q8(ox - 14), Q8(oy + 40), Q8(ox + 14), Q8(oy + 40), US_RGB(200, 255, 200), bf * k / 300);
        cv_rect(ox - 13, oy, 27, 3, ux_mix(T_BG, US_RGB(170, 175, 200), 50 + k / 2));
        cv_rect(ox - 9, oy + 3, 19, 2, ux_mix(T_BG, US_RGB(110, 115, 140), 50 + k / 2));
        cv_rect(ox - 5, oy - 4, 11, 4, ux_mix(T_BG, ux_mix(tint, US_RGB(200, 255, 255), 50), 50 + k / 2));
        for (i = 0; i < 6; i++)
            cv_pset(ox - 11 + i * 4, oy + 1, ((now / 120u + (uint32_t)i) % 6u) == 0u ? US_RGB(255, 255, 255) : tint);
        break; }
    case 1: {   /* a hot air balloon drifting across, striped, bobbing */
        int32_t bx = (int32_t)((now / 40u) % 300u) - 30, by = ymid + us_sin(t << 22, amp / 2) - bf / 50, dy;
        for (dy = -8; dy <= 8; dy++) {
            int32_t w = us_isqrt(64 - dy * dy) + (dy < 0 ? 1 : 0), x;
            for (x = -w; x <= w; x++)
                cv_pset(bx + x, by + dy, ux_mix(T_BG, ((x + 8) / 3) & 1 ? tint : US_RGB(255, 245, 220), 50 + k / 2));
        }
        us_line(bx - 5, by + 6, bx - 2, by + 13, ux_mix(T_BG, US_RGB(120, 90, 60), 60));
        us_line(bx + 5, by + 6, bx + 2, by + 13, ux_mix(T_BG, US_RGB(120, 90, 60), 60));
        cv_rect(bx - 2, by + 13, 5, 3, ux_mix(T_BG, US_RGB(160, 110, 60), 60 + k / 3));
        break; }
    case 2: {   /* a flock of birds in a V, their wings beating (faster on the beat) */
        int32_t fx0 = 260 - (int32_t)((now / 30u) % 320u), fy0 = ymid + us_sin(t << 21, amp / 3);
        for (i = 0; i < 7; i++) {
            int32_t row = (i + 1) / 2, side = (i & 1) ? 1 : -1, bx = fx0 + row * 8, by = fy0 + side * row * 5;
            int32_t up = (int32_t)(((now + (uint32_t)i * 70u) / (bf ? 60u : 140u)) & 1u) ? -2 : 1;
            uint16_t bc = ux_mix(T_BG, ux_mix(US_RGB(30, 20, 40), tint, 30), 50 + k / 2);
            us_line(bx - 4, by + up, bx, by + 1, bc);
            us_line(bx, by + 1, bx + 4, by + up, bc);
        }
        break; }
    case 3:     /* two butterflies, fluttering about */
        for (i = 0; i < 2; i++) {
            int32_t bx = 120 + us_sin((t * (3u + (uint32_t)i) + (uint32_t)i * 99u) << 21, 100);
            int32_t by = ymid + us_sin((t * (5u + (uint32_t)i) + (uint32_t)i * 40u) << 21, amp);
            int32_t w = (int32_t)(((now + (uint32_t)i * 50u) / (bf ? 50u : 110u)) & 1u) ? 4 : 1;
            uint16_t wc = ux_mix(T_BG, i ? tint : US_RGB(255, 200, 80), 50 + k / 2);
            cv_rect(bx - w - 1, by - 3, w, 3, wc); cv_rect(bx + 2, by - 3, w, 3, wc);
            cv_rect(bx - w, by, w - (w > 1), 2, wc); cv_rect(bx + 2, by, w - (w > 1), 2, wc);
            cv_rect(bx, by - 3, 2, 6, ux_mix(T_BG, US_RGB(40, 30, 40), 60));
        }
        break;
    case 4: {   /* a kite on the wind, its tail waving, a tug on the beat */
        int32_t kx = 60 + us_sin(t << 21, 20), ky = ytop + 10 + us_sin(t << 22, 5) - bf / 40, j;
        art_tri(Q8(kx), Q8(ky - 9), Q8(kx - 7), Q8(ky), Q8(kx), Q8(ky), tint, 60 + k / 3);
        art_tri(Q8(kx), Q8(ky - 9), Q8(kx + 7), Q8(ky), Q8(kx), Q8(ky), US_RGB(255, 240, 120), 60 + k / 3);
        art_tri(Q8(kx - 7), Q8(ky), Q8(kx), Q8(ky + 11), Q8(kx), Q8(ky), US_RGB(255, 120, 160), 60 + k / 3);
        art_tri(Q8(kx + 7), Q8(ky), Q8(kx), Q8(ky + 11), Q8(kx), Q8(ky), US_RGB(120, 220, 255), 60 + k / 3);
        for (j = 1; j < 24; j++) {
            int32_t tx = kx - j, ty = ky + 11 + j + us_sin((t * 6u + (uint32_t)j * 12u) << 22, 3);
            cv_pset(tx, ty, ux_mix(T_BG, US_RGB(255, 255, 255), 40));
            if (j % 6 == 0)
                cv_rect(tx - 1, ty - 1, 3, 2, ux_mix(T_BG, us_hue((uint32_t)j * 40u, 255u), 60 + k / 3));
        }
        break; }
    default: {  /* a jellyfish, pulsing upward through the air, glowing on the beat */
        int32_t jx = 190 + us_sin(t << 21, 30), jy = ybot - (int32_t)((now / 50u) % (uint32_t)(ybot - ytop + 20)) + 10;
        int32_t pulse = (int32_t)((now / 300u) & 1u), dy, j;
        uint16_t jc = ux_mix(T_BG, ux_mix(tint, US_RGB(255, 200, 255), 50), 40 + k / 2 + bf / 6);
        for (dy = -6; dy <= 0; dy++) {
            int32_t w = us_isqrt(36 - dy * dy) + pulse;
            cv_rect(jx - w, jy + dy, 2 * w + 1, 1, jc);
        }
        for (j = -2; j <= 2; j++) {
            int32_t m;
            for (m = 1; m < 12; m++)
                cv_pset(jx + j * 2 + us_sin((t * 8u + (uint32_t)(m * 20 + j * 30)) << 22, 2), jy + m, ux_mix(T_BG, jc, 100 - m * 7));
        }
        break; }
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
    int32_t curve = us_sin(t << 22, 10 + us_kn(0) * 40 / 127);   /* (the far end swings: KNOB 1 how wildly) */
    static const uint16_t HZC[4] = {US_RGB(96, 18, 112), US_RGB(150, 34, 40), US_RGB(10, 96, 112), US_RGB(170, 90, 30)};
    static const uint16_t SUN[4][2] = {{US_RGB(255, 226, 92), US_RGB(255, 52, 156)}, {US_RGB(255, 170, 60), US_RGB(230, 30, 60)},
                                       {US_RGB(170, 255, 220), US_RGB(40, 170, 255)}, {US_RGB(255, 250, 200), US_RGB(255, 120, 40)}};
    uint32_t v = us_var(), land = v & 3u, rot = v * 37u + (uint32_t)us_kn(2) * 2u;   /* (the land; the road's colours
                                                  * turned, KNOB 3 turning them further) */
    us_gradient((uint32_t)hz, T_BG, HZC[land], 0);
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
        int32_t r = 18 + us_kn(1) / 10 + rc / 90 + us_gf() / 40, dy;   /* (KNOB 2: the sun's size) */
        for (dy = -r; dy < 0; dy++) {
            int32_t row = dy + r, band = (row + (int32_t)(t / 2u)) % 7, gap = row * 4 / r;
            if (row > r / 3 && band < gap)
                continue;
            {
                int32_t w = us_isqrt(r * r - dy * dy);
                uint16_t c = ux_mix(SUN[land][0], SUN[land][1], row * 100 / r);
                cv_rect(120 - w, hz + dy, 2 * w + 1, 1, ux_mix(T_BG, c, 20 + k * 75 / 100));
            }
        }
    }
    if (land == 1u) {                            /* a neon city: towers, lit windows twinkling */
        for (x = 0; x < 240; ) {
            uint32_t hs = us_hash((uint32_t)x * 17u + 5u);
            int32_t bw = 6 + (int32_t)(hs % 9u), bh = 6 + (int32_t)((hs >> 4) % 22u), wy, wx;
            if (us_abs(x + bw / 2 - 120) < 22)
                bh = bh / 3 + 2;                 /* (low before the sun) */
            cv_rect(x, hz - bh, bw - 1, bh, ux_mix(T_BG, US_RGB(24, 8, 40), k));
            for (wy = hz - bh + 2; wy < hz - 1; wy += 3)
                for (wx = x + 1; wx < x + bw - 2; wx += 2)
                    if ((us_hash((uint32_t)(wx * 7 + wy * 131) + (uint32_t)(fm1_ms / 1500u)) & 7u) == 0u)
                        cv_pset(wx, wy, ux_mix(T_BG, (hs >> 9) & 1u ? US_RGB(80, 240, 255) : US_RGB(255, 80, 200), k * 85 / 100));
            x += bw;
        }
    } else if (land == 2u) {                     /* palm trees against the sun */
        static const int16_t PX[4] = {22, 54, 186, 218};
        uint16_t pc = ux_mix(T_BG, US_RGB(16, 6, 30), k);
        int32_t i, j;
        for (x = 0; x < 240; x++)                /* (a low shore) */
            cv_rect(x, hz - 2 - us_abs(us_sin((uint32_t)x << 23, 2)), 1, 3, pc);
        for (i = 0; i < 4; i++) {
            int32_t px = PX[i], ph = 22 + (i & 1) * 6, tx = px + ((i & 1) ? 5 : -5), ty = hz - ph;
            for (j = 0; j <= ph; j++)            /* (the trunk, leaning) */
                cv_rect(px + (tx - px) * j * j / (ph * ph), hz - j, 2, 1, pc);
            for (j = 0; j < 6; j++) {            /* (the fronds, drooping) */
                int32_t dx = (j - 3) * 4 + (j >= 3 ? 4 : 0), m;
                for (m = 0; m <= 8; m++)
                    cv_rect(tx + dx * m / 8, ty + (m * m) / 10 - (6 - us_abs(dx)) / 3, 2, 1, pc);
            }
        }
    } else if (land == 3u) {                     /* pyramids in the desert dusk */
        static const int16_t QX[3] = {44, 82, 198}, QH[3] = {22, 14, 18};
        int32_t i, y2;
        for (i = 0; i < 3; i++)
            for (y2 = 0; y2 < QH[i]; y2++) {
                int32_t w = QH[i] - y2;
                cv_rect(QX[i] - w, hz - y2 - 1, w, 1, ux_mix(T_BG, US_RGB(120, 60, 40), k));            /* (the lit side) */
                cv_rect(QX[i], hz - y2 - 1, w, 1, ux_mix(T_BG, US_RGB(50, 20, 30), k));                 /* (the shade) */
            }
    } else
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
        uint16_t ground = ux_mix(US_RGB(10, 2, 24), US_RGB(255, 40, 170), (zr & 31) < 2 ? 70 + us_gf() / 8 : us_gf() / 14);   /* (the grid, flashing on the beat) */
        cv_rect(0, y, 240, 1, ux_mix(T_BG, ground, k * near / 100));
        for (x = (cx % 24) - 24; x < 240; x += 24) {   /* (the grid's lines to the horizon) */
            int32_t gx = cx + (x - cx) * d / span * 3;
            cv_pset(gx, y, ux_mix(T_BG, US_RGB(200, 40, 160), k * near / 160));
        }
        for (L = 0; L < 7; L++) {
            int32_t xa = x0 + 2 * hw * L / 7, xb = x0 + 2 * hw * (L + 1) / 7;
            uint16_t c = us_hue((uint32_t)L * 36u + rot, stripe ? 255u : 175u + (uint32_t)us_gf() * 3u / 10u);
            cv_rect(xa, y, xb - xa, 1, ux_mix(T_BG, c, k * near / 100));
        }
        if (stripe) {                            /* (white posts at its edges) */
            cv_rect(x0 - 2, y, 2, 1, ux_mix(T_BG, US_RGB(255, 255, 255), k * near / 100));
            cv_rect(x0 + 2 * hw, y, 2, 1, ux_mix(T_BG, US_RGB(255, 255, 255), k * near / 100));
        }
    }
    us_visitor(h, 4, hz - 14);
    us_holo(120, hz - 4);
    {   /* the kart: a toad racing down the road, steered by the oscillator's signal, bumping on the beat, the exhaust
         * flaring with the sound, sparks when it drifts (KNOB 4: how wild). G the toad, E its goggles, S the scarf (it
         * flaps), R the kart, W the wheels, Y the tail lights, P the pipes */
        static const char *const KART[12] = {
            "....GG....GG....", "...GEEG..GEEG...", "...GGGGGGGGGG...", "..GGGGGGGGGGGG..", "..GGGGGGGGGGGG..",
            "...SSSSSSSSSS...", "................", "..RRRRRRRRRRRR..", "WWRRRRRRRRRRRRWW", "WWRRYYRRRRYYRRWW",
            "WWRRRRRRRRRRRRWW", "WW.PP......PP.WW",
        };
        int32_t KS = 3, yb = (int32_t)h - 8, d = yb - hz + 1, hw = 3 + d * 160 / span, far = span - d;
        int32_t cxn = 120 + curve * far * far / (span * span), kx = cxn + us_sx * hw / 150 - 8 * KS;
        int at, env = us_env(&at);               /* (the envelope: the boost; a wheelie in the attack) */
        int32_t ky = yb - 12 * KS - us_hop(5) - (int32_t)((fm1_ms / 70u) & 1u) - (at ? 4 : 0), r2, c2, lean = us_sx / 40;
        int32_t flap = (int32_t)((fm1_ms / 90u) % 3u), wild = us_kn(3);
        for (r2 = 0; r2 < 12; r2++)
            for (c2 = 0; c2 < 16; c2++) {
                char p = KART[r2][c2];
                uint16_t col;
                int32_t xo = r2 < 6 ? lean : 0;  /* (the toad leans into the turn) */
                if (r2 == 6 && c2 >= 9 && c2 < 12 + flap)   /* (the scarf's end, flapping) */
                    p = 'S';
                if (p == '.')
                    continue;
                col = p == 'G' ? ux_mix(T_BG, US_RGB(90, 170, 70), 55 + k / 2) :
                      p == 'E' ? ux_mix(T_BG, US_RGB(255, 220, 90), 70 + k / 3) :
                      p == 'S' ? ux_mix(T_BG, US_RGB(255, 80, 120), 60 + k / 2) :
                      p == 'R' ? ux_mix(T_BG, us_hue(rot + 200u, 230u), 60 + k / 2) :
                      p == 'Y' ? ux_mix(T_BG, US_RGB(255, 60, 60), 60 + k / 2 + us_gf() / 6) :
                      p == 'P' ? ux_mix(T_BG, US_RGB(200, 200, 210), 60 + k / 2) :
                                 ux_mix(T_BG, US_RGB(20, 20, 26), 80);
                cv_rect(kx + (c2 + xo) * KS, ky + r2 * KS, KS, KS, col);
            }
        {   /* the exhaust: flames out of the pipes with the sound and the beat */
            int32_t fl = 2 + rc / 150 + us_gf() / 40 + env / 7, j;
            for (j = 0; j < fl; j++) {
                uint16_t fc = ux_mix(T_BG, j < 2 ? US_RGB(255, 240, 120) : US_RGB(255, 120, 40), 80 - j * 8);
                cv_rect(kx + 3 * KS + us_sin((fm1_ms * 7u + (uint32_t)j * 50u) << 22, 1), ky + 12 * KS + j, 2 * KS, 1, fc);
                cv_rect(kx + 11 * KS + us_sin((fm1_ms * 5u + (uint32_t)j * 70u) << 22, 1), ky + 12 * KS + j, 2 * KS, 1, fc);
            }
        }
        if (us_abs(us_sx) > 60 - wild / 4) {    /* drift sparks off the wheels */
            int32_t j;
            for (j = 0; j < 4 + wild / 20; j++) {
                uint32_t hs = us_hash((uint32_t)j + fm1_ms / 40u);
                int32_t sx2 = kx + (us_sx > 0 ? 0 : 15 * KS) + (int32_t)(hs % 5u) - 2, sy2 = ky + 11 * KS - (int32_t)((hs >> 4) % 6u);
                cv_pset(sx2, sy2, us_hue(hs >> 8, 255u));
            }
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
    uint32_t t = us_t(), ph = t * 2u, i = ((ph >> 8) + us_var()) & 3u, j = (i + 1u) & 3u, x, ah = us_var() * 45u;
    int32_t f = (int32_t)(ph & 255u) * 100 / 255, k = us_k(), rc = us_react() + us_gf() * 2, r;
    uint16_t top, bot;
    f = f < 70 ? 0 : (f - 70) * 100 / 30;        /* (each pair held a while, then a slow change) */
    top = ux_mix(SKY[i][0], SKY[j][0], f);
    bot = ux_mix(SKY[i][1], SKY[j][1], f);
    us_gradient(h, top, bot, 0);
#define US_SKY(y) ux_mix(T_BG, ux_mix(top, bot, (int32_t)((y) < 0 ? 0 : (uint32_t)(y) >= h ? 100u : (uint32_t)(y) * 100u / (h - 1u))), k)
    for (r = 0; r < 2; r++)                      /* the curtains: a bright top edge, fading down */
        for (x = 0; x < 240u; x += 2u) {
            int32_t yc = (int32_t)h * (28 + r * 18) / 100 + us_sy / 5 +
                         us_sin((x * 3u + t * (3u + (uint32_t)us_kn(1) / 16u + (uint32_t)r * 3u)) << 22, 3 + us_kn(0) / 8) +
                         us_sin((x * 7u - t * 4u + (uint32_t)r * 90u) << 21, 5);
            int32_t len = 10 + us_sin((x * 5u + t * 6u) << 21, 5) + rc / 60 + us_env(0) / 6, yy;
            uint16_t c = us_hue(96u + ah + (uint32_t)us_kn(2) * 2u + (uint32_t)r * 70u + (x >> 2) + us_sin((x + t * 3u) << 23, 20), 255u);
            for (yy = 0; yy < len; yy++)
                cv_rect((int32_t)x, yc + yy, 2, 1, ux_mix(US_SKY(yc + yy), c, (len - yy) * (75 + rc / 20) * k / (len * 100)));
        }
    for (r = 0; r < 3 + us_kn(3) / 12; r++) {   /* bokeh, rising (KNOB 4: how many) */
        uint32_t hs = us_hash((uint32_t)r * 31u + 7u);
        int32_t bx = (int32_t)(hs % 240u) + us_sin((t * 2u + hs) << 22, 6);
        int32_t by = (int32_t)h - (int32_t)((t * (2u + (hs >> 9) % 3u) + (hs >> 4)) % (h + 20u)) + 10;
        us_disc(bx, by, 2 + (int32_t)((hs >> 12) % 4u), ux_mix(US_SKY(by), US_RGB(255, 255, 255), 35 * k / 100));
    }
#undef US_SKY
    us_visitor(h, 6, (int32_t)h / 3);
    us_holo(120, (int32_t)h - 10);
}

static void us_psyche(uint32_t h)
{
    int32_t bx, by, k = us_k() * 55 / 100, rc = us_react(), cy = (int32_t)h / 2;
    uint32_t t = us_t(), var = us_var(), arms = 3u + var % 3u + (uint32_t)us_kn(0) / 21u, bs = var % 3u == 2u ? 8u : 4u;
    uint32_t tt = var & 1u ? 0u - t : t;         /* (some turn the other way) */
    int32_t ev = us_env(0);                      /* (the envelope: the rings crowd in, a zoom) */
    for (by = 0; by < (int32_t)h; by += (int32_t)bs)   /* the kaleidoscope: mirrored arms (4 .. 8), spiralling */
        for (bx = 0; bx < 240; bx += (int32_t)bs) {
            int32_t dx = bx + (int32_t)bs / 2 - 120, dy = (by + (int32_t)bs / 2 - cy) * 2;
            uint32_t r = (uint32_t)us_isqrt((dx * dx + dy * dy) >> 4) * (4u + (uint32_t)ev / 25u), a = us_ang(dx, dy) * arms & 255u;
            uint32_t fold = a > 127u ? 255u - a : a, hue = r * (1u + (uint32_t)us_kn(1) / 24u) + fold - tt * 3u + (uint32_t)rc / 8u +
                                                       var * 29u + (uint32_t)us_kn(2) * 2u + (us_groove() ? us_bn * 24u : 0u);
            uint32_t v = ((r + fold / 2u - tt * 2u) & 16u) ? 210u : 200u - (uint32_t)us_kn(3) * 3u / 4u;
            cv_rect(bx, by, (int32_t)bs, (int32_t)bs, ux_mix(T_BG, us_hue(hue, v), k + rc * (100 - k) / 2000));
        }
    {   /* the eye: an almond, its iris turning through the hues, the pupil opening with the sound; it blinks */
        int32_t dx, dy, W = 38, H = 18, open = (fm1_ms % 5200u) < 160u ? 2 : H;
        int32_t look = us_sx * 7 / 100, pr = 4 + rc / 160 + us_gf() / 50, ek = 35 + us_k() * 55 / 100;
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
    us_visitor(h, 6, (int32_t)h / 3);
    us_holo(120, cy - 20);
}

/* the cat: 16 x 13: B fur, P the ears' insides, E an eye, W the muzzle, N the nose, T the tail (it swings) */
#define US_CAT_H 13
static const char *const US_CAT[US_CAT_H] = {
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
/* the cat leaping (facing right; turned for the left): 18 x 9, the letters as US_CAT's */
static const char *const US_LEAP[9] = {
    "............B..B..",
    "............BBBB..",
    "TT.........BBEBBN.",
    ".TT......BBBBBBW..",
    "..TTBBBBBBBBBBB...",
    "...BBBBBBBBBBBB...",
    "...BBBBBBBBBBB.BB.",
    "..BB........BB..BB",
    ".BB...............",
};
static uint32_t us_bars4(uint32_t *cyc)          /* where in a run of 4 bars (16 beats of the tempo) we are, ms; which run */
{
    uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm), per = 16u * beat;
    if (cyc)
        *cyc = fm1_ms / per;
    return fm1_ms % per;
}
static void us_cats(uint32_t h)
{
    static uint32_t ons_seen, spark_t = 0u - 100000u;
    int32_t k = us_k(), S = 7, x0 = 120 - 13 * S / 2, y0 = (int32_t)h - S * US_CAT_H - 1 - us_hop(12), r, c;   /* (the main
                                                  * character: in the middle, seven times the size; it hops on the ARP's
                                                  * and the SEQ's steps) */
    uint32_t t = us_t(), now = fm1_ms;
    int asleep = now - spark_t > 9000u, blink = asleep || (now % 4200u) < 140u, glow = us_react() > 120;
    int look = us_sx > 0;                        /* (the eyes: on the laser dot, the oscillator's signal) */
    int32_t sway = (int32_t)(np_sin((t * (1u + (uint32_t)us_kn(0) / 24u)) << 23) >> 13);   /* the tail's swing (KNOB 1: how fast) */
    static const uint16_t FUR[5] = {US_RGB(250, 220, 255), US_RGB(70, 60, 96), US_RGB(255, 170, 90), US_RGB(170, 170, 190),
                                    US_RGB(255, 236, 200)};   /* (lilac white, black, ginger, grey, cream) */
    static const int8_t MOON[4] = {5, 2, -5, 99};   /* (the shadow's offset: crescent, slim, waning, full) */
    uint32_t var = us_var();
    uint16_t fur = ux_mix(T_BG, FUR[var % 5u], 20 + k * 60 / 100);
    us_gradient(h, US_RGB(52, 26, 92), US_RGB(12, 6, 30), 0);
    us_stars(12u + (uint32_t)us_kn(3) / 3u, 2, (int32_t)h - 64, US_RGB(255, 236, 255));   /* (KNOB 4: how many) */
    {   /* the moon: a disc with a darker one over it, a crescent */
        int32_t mx = 30, my = 22, dx, dy;
        for (dy = -10; dy <= 10; dy++)
            for (dx = -10; dx <= 10; dx++)
                if (dx * dx + dy * dy <= 100 && (dx - MOON[var & 3u]) * (dx - MOON[var & 3u]) + (dy + 2) * (dy + 2) > 64)
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
    us_visitor(h, 30, (int32_t)h / 2);
    /* every 4 bars the cat pounces: after the ball of yarn, the next time after the laser dot, and back (1 s) */
    uint32_t cyc, ph = us_bars4(&cyc);
    int pounce = ph < 1000u, tgt = (int)(cyc & 1u);   /* (0: the yarn, 1: the laser) */
    int32_t lx0 = 100 + us_sx * 80 / 100, ly0 = (int32_t)h / 2 + us_sy * ((int32_t)h / 3) / 100;
    int32_t flee = 0;                            /* (the yarn knocked away / the dot escaping: out, then slowly back) */
    if (ph >= 480u && ph < 3000u)
        flee = ph < 1000u ? (int32_t)(ph - 480u) * 40 / 520 : 40 - (int32_t)(ph - 1000u) * 40 / 2000;
    int32_t lx = lx0 + (tgt ? (lx0 < 120 ? -2 : 2) * flee : 0), ly = ly0 - (tgt ? flee / 2 : 0);
    {   /* a laser dot, wandering where the oscillator's signal goes; the cat watches it (KNOB 3: its colour) */
        int32_t gr = 3 + us_gf() / 60;
        uint16_t lc = us_hue((uint32_t)us_kn(2) * 2u, 255u);
        us_disc(lx, ly, gr + 2, ux_mix(T_BG, lc, 25 * k / 100));
        us_disc(lx, ly, gr, ux_mix(T_BG, lc, 60 + k / 3));
        cv_rect(lx - 1, ly - 1, 2, 2, US_RGB(255, 255, 255));
    }
    if (synth_ons != ons_seen) {                 /* a note: sparkles rise from the cat, it bats the ball */
        ons_seen = synth_ons;
        spark_t = now;
    }
    {   /* the ball of yarn, its thread to the paw: knocked on a note, it rolls back */
        uint32_t a = now - spark_t;
        int32_t off = a < 900u ? us_sin((a * 3u) << 22, 14 - (int32_t)a * 14 / 900) : 0;
        int32_t bxc = x0 - 22 - us_abs(off) - (tgt ? 0 : flee), byc = (int32_t)h - 9, i;
        uint16_t yarn = ux_mix(T_BG, US_RGB(255, 110, 170), 35 + k * 60 / 100);
        uint16_t dark = ux_mix(T_BG, US_RGB(170, 50, 110), 35 + k * 60 / 100);
        us_line(bxc + 5, byc + 2, x0 + 8, byc + 4, ux_mix(T_BG, US_RGB(255, 110, 170), 20 + k * 40 / 100));
        us_disc(bxc, byc, 8, yarn);
        for (i = -1; i <= 1; i++)                /* (its winds, turning as it rolls) */
            us_line(bxc - 6, byc + i * 3 + (off & 1), bxc + 6, byc - i * 3 - (off & 1), dark);
    }
    if (pounce) {   /* the leap: out along an arc to the yarn or the dot, then back, turned round */
        int32_t L = 5, sx0 = 120, sy0 = y0 + 6 * S, tx = tgt ? lx0 : x0 - 22, ty = tgt ? ly0 : (int32_t)h - 9;
        int32_t u = ph < 500u ? (int32_t)ph : 1000 - (int32_t)ph, dir = tx > sx0 ? 1 : -1, face = ph < 500u ? dir : -dir;
        int32_t cx = sx0 + (tx - sx0) * u / 500, cy = sy0 + (ty - sy0) * u / 500 - u * (500 - u) * 60 / 62500;
        int32_t lx1 = cx - 9 * L, ly1 = cy - 9 * L / 2, pass;
        for (pass = 0; pass < 2; pass++)         /* (a rim of moonlight first, then the cat) */
            for (r = 0; r < 9; r++)
                for (c = 0; c < 18; c++) {
                    char p = US_LEAP[r][face > 0 ? c : 17 - c];
                    uint16_t col;
                    if (p == '.')
                        continue;
                    if (!pass) {
                        cv_rect(lx1 + c * L - 1, ly1 + r * L - 1, L + 2, L + 2, ux_mix(T_BG, US_RGB(255, 236, 255), 30 + k / 4));
                        continue;
                    }
                    col = p == 'E' ? (glow ? T_ACCENT : ux_mix(T_BG, T_ACCENT, 80)) : p == 'N' ? T_THEME :
                          p == 'W' ? ux_mix(T_BG, US_RGB(255, 255, 255), 30 + k * 65 / 100) : fur;
                    cv_rect(lx1 + c * L, ly1 + r * L, L, L, col);
                }
        us_holo(cx, ly1 - 2);
        return;
    }
    {   /* a rim of moonlight round it first (it stands out, whatever its fur) */
        uint16_t rim = ux_mix(T_BG, US_RGB(255, 236, 255), 30 + k / 4);
        for (r = 0; r < US_CAT_H; r++)
            for (c = 0; c < 16; c++)
                if (US_CAT[r][c] != '.') {
                    int32_t xs = US_CAT[r][c] == 'T' ? c + sway * (US_CAT_H - 1 - r) / 6 : c;
                    cv_rect(x0 + xs * S - 1, y0 + r * S - 1, S + 2, S + 2, rim);
                }
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
            } else if (p == 'P')
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
            cv_rect(x0 + xs * S, y0 + ys * S, S, S, col);
        }
    {   /* laser eyes: with the envelope up the cat fires a beam from each eye at the dot, as wide and bright as the
         * envelope, flashing white in the attack; sparks where they hit */
        int at, e = us_env(&at);
        if (e > 12 && !blink) {
            uint16_t lc = us_hue((uint32_t)us_kn(2) * 2u, 255u), core = at ? US_RGB(255, 255, 255) : ux_mix(lc, US_RGB(255, 255, 255), 60);
            int32_t ey = y0 + 4 * S + S / 2, w = 1 + e / 30, eye, j;
            for (eye = 0; eye < 2; eye++) {
                int32_t ex = x0 + (eye ? 9 : 4) * S;
                for (j = -w; j <= w; j++)        /* (the beam: a glow round a hot core) */
                    us_line(ex, ey + j, lx, ly + j / 2, j == 0 ? core : ux_mix(T_BG, lc, 60 + e / 3 - us_abs(j) * 10));
                us_disc(ex, ey, 2 + e / 25, ux_mix(T_BG, lc, 50 + e / 2));   /* (the eye blazing) */
            }
            for (j = 0; j < 4 + e / 15; j++) {   /* (sparks at the dot) */
                uint32_t hs = us_hash((uint32_t)j * 7u + fm1_ms / 50u);
                int32_t a2 = (int32_t)(hs % 360u), rr = 3 + (int32_t)((hs >> 9) % (uint32_t)(4 + e / 8));
                cv_rect(lx + us_sin((uint32_t)a2 * 11930465u, rr), ly + us_sin((uint32_t)a2 * 11930465u + (1u << 30), rr), 2, 2,
                        (hs >> 20) & 1u ? US_RGB(255, 255, 255) : lc);
            }
            us_disc(lx, ly, 3 + e / 20, ux_mix(T_BG, US_RGB(255, 255, 255), 50 + e / 2));
        }
    }
    us_holo(x0 + 5 * S, y0 - 2);
    if (asleep) {                                /* z z z, rising from its head */
        uint32_t i;
        for (i = 0; i < 3u; i++) {
            uint32_t a = (now / 8u + i * 120u) % 360u;
            int32_t zx = x0 - 6 - (int32_t)i * 4 + us_sin(a << 24, 2), zy = y0 + 3 * S - (int32_t)a / 9, sz = 3 + (int32_t)i;
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
            int32_t sx = x0 + 2 * S + (int32_t)(rr % (uint32_t)(9 * S)), sy = y0 + S - (int32_t)(a * (16u + (rr >> 8) % 20u) / 700u);   /* (from between its ears) */
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
    int32_t shine = 30 + lvl * 50 / 4000 + us_gf() / 5 + us_env(0) / 3;   /* (the light grows with the sound and the
                                                  * envelope, flashes on the beat) */
    static const uint16_t LIGHT[3] = {US_RGB(242, 193, 78), US_RGB(225, 232, 255), US_RGB(255, 160, 150)};   /* gold, silver, rose */
    uint32_t var = us_var(), sign = var & 3u;
    uint16_t gold = ux_mix(LIGHT[(var >> 2) % 3u], US_RGB(255, 255, 255), us_kn(2) / 3);   /* (KNOB 3: whiter) */
    int32_t nr = 10 + us_kn(0) / 8;              /* (KNOB 1: how many rays; KNOB 2: how fast they turn) */
    us_gradient(h, US_RGB(34, 46, 112), T_BG, 0);
    for (i = 0; i < nr; i++) {                   /* golden rays from above the cross, turning slowly */
        uint32_t a0 = ART_DEG(20 + i * 140 / nr) + t * (200000u + (uint32_t)us_kn(1) * 9000u), a1 = a0 + ART_DEG(3);
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
        if (sign == 0u) {                        /* the cross */
            cv_rect(cx - 3, cy - 13, 7, 30, ux_mix(T_BG, gold, 20 * k / 100));   /* (the glow) */
            cv_rect(cx - 10, cy - 5, 21, 7, ux_mix(T_BG, gold, 20 * k / 100));
            cv_rect(cx - 1, cy - 11, 3, 26, ux_mix(T_BG, gold, 50 + shine * k / 200));
            cv_rect(cx - 8, cy - 3, 17, 3, ux_mix(T_BG, gold, 50 + shine * k / 200));
        } else {                                 /* or another sign, drawn twice the size: H a heart, X the cross on it,
                                                  * F its flame; O the fish's outline; C a chalice, W the wine */
            static const char *const SIGN[3][11] = {
                {"....F....", "...FXF...", "....X....", ".HH.X.HH.", "HHHHXHHHH", "HHHHHHHHH", ".HHHHHHH.", "..HHHHH..",
                 "...HHH...", "....H....", "........."},
                {".........", ".........", "..OOOO..O", ".O....OOO", "O..O...OO", ".O....OOO", "..OOOO..O", ".........",
                 ".........", ".........", "........."},
                {".........", "CCCCCCCCC", "CWWWWWWWC", ".CWWWWWC.", "..CCCCC..", "...CCC...", "....C....", "....C....",
                 "....C....", "..CCCCC..", ".CCCCCCC."},
            };
            int32_t r2, c2, fl = (int32_t)(us_hash(fm1_ms / 90u) & 1u);
            for (r2 = 0; r2 < 11; r2++)
                for (c2 = 0; c2 < 9; c2++) {
                    char p = SIGN[sign - 1u][r2][c2];
                    uint16_t col;
                    if (p == '.')
                        continue;
                    col = p == 'H' ? ux_mix(T_BG, US_RGB(220, 40, 60), 50 + shine * k / 200) :
                          p == 'F' ? ux_mix(T_BG, fl ? US_RGB(255, 200, 60) : US_RGB(255, 120, 40), 60 + k / 3) :
                          p == 'W' ? ux_mix(T_BG, US_RGB(150, 20, 50), 50 + shine * k / 200) :
                                     ux_mix(T_BG, gold, 50 + shine * k / 200);
                    cv_rect(cx - 9 + c2 * 2, cy - 11 + r2 * 2, 2, 2, col);
                }
        }
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
            int32_t ch = 8 + (i % 3) * 3, fh = us_kn(3) / 32;   /* (KNOB 4: the flames' height) */
            cv_rect(x - 1, yb - ch, 3, ch, ux_mix(T_BG, US_RGB(250, 236, 210), 30 + k / 2));
            us_disc(x, yb - ch - 4, 4 + shine / 30, ux_mix(T_BG, gold, (12 + shine / 4) * k / 100));   /* (the glow) */
            cv_rect(x - 1 + (fl == 2), yb - ch - 5 - fl / 2 - fh, 2, 4 + fh, ux_mix(T_BG, US_RGB(255, 150, 40), 50 + k / 2));
            cv_rect(x, yb - ch - 3, 1, 2, ux_mix(T_BG, US_RGB(255, 245, 180), 60 + k * 40 / 100));
        }
    }
    us_visitor(h, 40, (int32_t)h / 2);
    {   /* a light, wandering where the oscillator's signal goes, a trail behind it */
        static int32_t px[10], py[10];
        static uint32_t pn;
        int32_t ox = 120 + us_sx * 90 / 100, oy = 56 + us_sy * 30 / 100, j;
        px[pn % 10u] = ox; py[pn % 10u] = oy; pn++;
        for (j = 1; j < 10; j++) {
            uint32_t q = (pn - 1u - (uint32_t)j) % 10u;
            cv_rect(px[q], py[q], 2, 2, ux_mix(T_BG, gold, (10 - j) * 6 * k / 100));
        }
        us_disc(ox, oy, 4 + us_se / 30, ux_mix(T_BG, gold, 25 * k / 100));
        us_disc(ox, oy, 2, ux_mix(T_BG, US_RGB(255, 255, 240), 70 + k / 4));
    }
    {   /* Jezus walks to and fro along the sill, carrying his cross: in step with the ARP and the SEQ while they
         * play (a step a beat), else at a slow pace; a halo over him. R the robe, D the sash, H the hair and beard,
         * S the skin, Y the halo, F the feet (two frames: walking) */
        static const char *const JZ[17] = {
            "..YYYY....", ".Y....Y...", "..HHHH....", ".HHSSSS...", ".HHSSSS...", ".HH.SSS...", "..HHHH....",
            "..RRRR....", ".RRRRRRS..", ".RRRRRRS..", ".RRDDRR...", ".RRRRRR...", ".RRRRRR...", ".RRRRRRR..",
            "RRRRRRRR..", "RRRRRRRRR.", "..........",
        };
        static const char *const FEET[2] = {".F....F...", "..F..F...."};
        static int32_t jx = 30 << 8, jdir = 1, jgo;      /* (where, Q8; which way; the steps still to take, Q8) */
        static uint32_t jlast, jbeat;
        uint32_t now = fm1_ms, dt = now - jlast;
        int32_t S = 3, x, yb = (int32_t)h - 8, top, r2, c2, fr;
        jlast = now;
        if (dt > 200u)
            dt = 200u;
        if (us_groove()) {
            if (us_bn != jbeat) {
                jbeat = us_bn;
                jgo += 9 << 8;                   /* (a step: 9 px) */
            }
            if (jgo > 0) {
                int32_t mv = (int32_t)dt * (9 << 8) / 150;   /* (each step in ~150 ms) */
                mv = mv > jgo ? jgo : mv;
                jgo -= mv;
                jx += jdir * mv;
            }
        } else {
            jgo = 0;
            jbeat = us_bn;
            jx += jdir * (int32_t)dt * 256 / 90;   /* (~11 px a second) */
        }
        if (jx > (200 << 8)) { jx = 200 << 8; jdir = -1; }
        if (jx < (48 << 8)) { jx = 48 << 8; jdir = 1; }
        x = jx >> 8;
        fr = (x / 5) & 1;
        top = yb - 17 * S;
        for (r2 = 0; r2 < 17; r2++)
            for (c2 = 0; c2 < 10; c2++) {
                char p = r2 == 16 ? FEET[fr][c2] : JZ[r2][c2];
                int32_t cx3 = jdir > 0 ? c2 : 9 - c2;   /* (turned round when walking back) */
                uint16_t col;
                if (p == '.')
                    continue;
                col = p == 'R' ? ux_mix(T_BG, US_RGB(245, 240, 230), 50 + k / 2) :
                      p == 'D' ? ux_mix(T_BG, US_RGB(200, 40, 50), 50 + k / 2) :
                      p == 'H' ? ux_mix(T_BG, US_RGB(110, 70, 40), 50 + k / 2) :
                      p == 'S' ? ux_mix(T_BG, US_RGB(230, 180, 140), 50 + k / 2) :
                      p == 'Y' ? ux_mix(T_BG, gold, 50 + shine * k / 200) :
                                 ux_mix(T_BG, US_RGB(120, 80, 50), 50 + k / 2);
                cv_rect(x + cx3 * S, top + r2 * S, S, S, col);
            }
        {   /* the cross: its beam over his shoulder, its foot dragging behind on the ground, the crossbar near the top */
            int32_t sx = x + (jdir > 0 ? 6 : 4) * S, sy = top + 8 * S, gx = sx - jdir * 44, gy = yb;
            int32_t tx = sx + jdir * 12, ty = sy - 14, dx = tx - gx, dy = ty - gy, len = us_isqrt(dx * dx + dy * dy), j;
            int32_t mx = gx + dx * 3 / 4, my = gy + dy * 3 / 4, px = -dy * 9 / (len ? len : 1), py = dx * 9 / (len ? len : 1);
            uint16_t wood = ux_mix(T_BG, US_RGB(190, 130, 70), 55 + k / 2), dark = ux_mix(T_BG, US_RGB(110, 70, 40), 55 + k / 2);
            for (j = -1; j <= 1; j++) {
                us_line(gx, gy + j, tx, ty + j, j ? dark : wood);
                us_line(mx - px + j, my - py, mx + px + j, my + py, j ? dark : wood);
            }
        }
        us_holo(x + 14, top - 4);
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
    int32_t k = us_k(), rc = us_react(), i, S = 5, x0 = 120 - 15 * S / 2, y0 = (int32_t)h - S * US_DOG_H - 1 - us_hop(10), r, c;   /* (the main
                                                  * character: in the middle, five times the size) */
    uint32_t t = us_t(), now = fm1_ms, since;
    int32_t hz = (int32_t)h * 62 / 100;
    if (synth_ons != ons_seen) {
        ons_seen = synth_ons;
        note_t = now;
    }
    since = now - note_t;
    /* the season (the oscillator): dusk, night, snow, autumn: the sky, the hills, what falls */
    static const uint16_t SKY[4][2] = {{US_RGB(110, 70, 170), US_RGB(255, 168, 210)}, {US_RGB(14, 14, 60), US_RGB(70, 50, 140)},
                                       {US_RGB(120, 150, 200), US_RGB(225, 235, 250)}, {US_RGB(150, 70, 110), US_RGB(255, 170, 90)}};
    static const uint16_t HILL[4][2] = {{US_RGB(150, 230, 190), US_RGB(70, 170, 120)}, {US_RGB(40, 90, 90), US_RGB(20, 60, 60)},
                                        {US_RGB(240, 245, 255), US_RGB(190, 210, 240)}, {US_RGB(220, 140, 60), US_RGB(160, 80, 40)}};
    uint32_t sea = us_var() & 3u, look = (us_var() >> 2) % 3u;
    us_gradient((uint32_t)hz + 6u, SKY[sea][0], SKY[sea][1], 0);
    us_stars(sea == 1u ? 60u : 22u, 2, sea == 1u ? hz : hz / 2, US_RGB(255, 250, 230));
    if (sea == 1u) {                             /* (the night: a full moon) */
        us_disc(214, 16, 9, ux_mix(T_BG, US_RGB(255, 250, 220), 40 + k / 2));
        cv_pset(211, 14, ux_mix(T_BG, US_RGB(210, 205, 190), 40 + k / 2));
        cv_pset(216, 19, ux_mix(T_BG, US_RGB(210, 205, 190), 40 + k / 2));
    }
    {   /* a rainbow, over the hills: seven bands, the sky showing through a little */
        int32_t cx = 120, cy = hz + 26, R0 = 46 + us_kn(1) / 5, bw = 3, dy, b;   /* (KNOB 2: how big) */
        for (dy = -(R0 + 7 * bw); dy < hz + 6 - cy && dy <= 0; dy++) {
            int32_t y = cy + dy, sky = y * 100 / (hz + 5);
            uint16_t base = ux_mix(T_BG, ux_mix(SKY[sea][0], SKY[sea][1], sky < 0 ? 0 : sky > 100 ? 100 : sky), k);
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
    {   /* the castle in the air, on clouds: it floats to and fro over 4 bars (the tempo's), bobbing; towers with
         * pointed roofs, windows that twinkle, the clouds puffing under it */
        static const int8_t TX[5] = {4, 16, 28, 40, 52}, TH[5] = {18, 28, 36, 26, 16};
        uint32_t ph4 = us_bars4(0), bpm = arp_bpm_now(), per = 16u * (60000u / (bpm < 20u ? 20u : bpm));
        uint32_t turn = (uint32_t)((uint64_t)ph4 * 0xFFFFFFFFu / per);   /* (the 4 bars: one turn) */
        int32_t ox = 12 + us_sin(turn, 12), base = hz - 20 + us_sin(turn * 2u, 4);
        uint16_t wall = ux_mix(T_BG, US_RGB(196, 150, 220), k * 80 / 100);
        cv_rect(ox, base - 12, 58, 12, wall);
        for (i = 0; i < 5; i++) {
            int32_t tx = ox + TX[i], th = TH[i], y;
            cv_rect(tx - 4, base - th, 9, th, wall);
            for (y = 0; y < 8; y++)              /* (its roof) */
                cv_rect(tx - 4 + y / 2, base - th - 8 + y, 9 - y, 1, ux_mix(T_BG, US_RGB(230, 120, 190), k * 85 / 100));
            if ((us_hash((uint32_t)i * 5u + now / 700u) & 3u) != 0u)
                cv_rect(tx - 1, base - th + 4, 2, 3, ux_mix(T_BG, US_RGB(255, 236, 140), 40 + k / 2));
        }
        cv_rect(ox + 25, base - 7, 6, 7, ux_mix(T_BG, US_RGB(120, 70, 140), k));   /* (the gate) */
        for (i = 0; i < 7; i++) {                /* the clouds: puffs under it, breathing, a pink shade below */
            int32_t px = ox - 6 + i * 11, py = base + 2 + (i & 1) * 3, pr = 6 + (i % 3) * 2 + us_sin((turn * 4u) + (uint32_t)i * 0x24924924u, 2);
            us_disc(px, py + 2, pr, ux_mix(T_BG, US_RGB(255, 190, 220), 40 + k / 2));
            us_disc(px, py, pr, ux_mix(T_BG, US_RGB(255, 252, 255), 50 + k / 2));
        }
        for (i = 0; i < 3; i++) {                /* (a few wisps drifting off behind) */
            int32_t wx = ox - 14 - i * 12 - (int32_t)((ph4 / 40u + (uint32_t)i * 30u) % 30u), wy = base + 6 + i * 2;
            us_disc(wx, wy, 3 - i / 2, ux_mix(T_BG, US_RGB(255, 252, 255), 35 + k / 3 - i * 8));
        }
    }
    {   /* two rolling hills, mint and deeper green */
        int32_t x;
        for (x = 0; x < 240; x++) {
            int32_t y1 = hz + us_sin(((uint32_t)x * 2u + 40u) << 23, 6), y2 = hz + 12 + us_sin(((uint32_t)x * 3u + 170u) << 22, 7);
            cv_rect(x, y1, 1, (int32_t)h - y1, ux_mix(T_BG, HILL[sea][0], k * 75 / 100));
            cv_rect(x, y2, 1, (int32_t)h - y2, ux_mix(T_BG, HILL[sea][1], k * 80 / 100));
            if (((uint32_t)x * 7u + (uint32_t)y2) % 11u == 0u)   /* (little flowers in the grass) */
                cv_pset(x, y2 + 4 + (x % 5), ux_mix(T_BG, us_hue((uint32_t)x * 37u, 255u), 40 + k / 2));
        }
    }
    {   /* glowing toadstools: red caps with white dots, the glow breathing with the sound */
        static const int16_t MX[4] = {20, 44, 200, 224};
        static const int8_t MS[4] = {6, 4, 5, 4};
        for (i = 0; i < 4; i++) {
            int32_t mx = MX[i], sz = MS[i], my = (int32_t)h - 6 - (i & 1) * 4, gl = 15 + rc / 25 + us_sin((t * 3u + (uint32_t)i * 60u) << 22, 8);
            us_disc(mx, my - sz, sz + 4, ux_mix(ux_mix(T_BG, HILL[sea][1], k * 80 / 100), US_RGB(255, 170, 220), gl * k / 100));
            cv_rect(mx - 1, my - sz, 3, sz + 2, ux_mix(T_BG, US_RGB(255, 240, 220), 40 + k / 2));
            for (r = 0; r <= sz; r++)            /* (the cap: a half disc) */
                cv_rect(mx - us_isqrt(sz * sz - r * r) - 1, my - sz - r, 2 * us_isqrt(sz * sz - r * r) + 3, 1,
                        ux_mix(T_BG, US_RGB(235, 50, 70), 40 + k / 2));
            cv_pset(mx - sz / 2, my - sz - sz / 2, US_RGB(255, 255, 255));
            cv_pset(mx + sz / 2, my - sz - 1, US_RGB(255, 255, 255));
            cv_pset(mx, my - 2 * sz + 1, US_RGB(255, 255, 255));
        }
    }
    for (i = 0; i < 2 + us_kn(0) / 18; i++) {   /* fairies, flitting, a trail of light behind each (KNOB 1: how many) */
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
        uint32_t n = 6u + (uint32_t)us_kn(3) / 4u + (uint32_t)rc / 40u;   /* (KNOB 4: how hard it rains) */
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
    if (sea == 2u || sea == 3u)                  /* snow falling; or autumn's leaves */
        for (i = 0; i < 24; i++) {
            uint32_t hs = us_hash((uint32_t)i * 389u + 4u);
            int32_t y = (int32_t)((t * (2u + (hs >> 6) % 3u) + (hs >> 3)) % (h + 8u)) - 4;
            int32_t x = (int32_t)(hs % 240u) + us_sin((t * 3u + hs) << 21, 6);
            if (sea == 2u) {
                uint16_t sc = ux_mix(T_BG, US_RGB(255, 255, 255), 50 + k / 2);
                cv_pset(x, y, sc);
                if (hs & 4u) { cv_pset(x - 1, y, sc); cv_pset(x + 1, y, sc); cv_pset(x, y - 1, sc); cv_pset(x, y + 1, sc); }
            } else
                cv_rect(x, y, 2 + (int32_t)(hs & 1u), 2, ux_mix(T_BG, (hs >> 8) & 1u ? US_RGB(230, 110, 40) : US_RGB(200, 60, 40), 50 + k / 2));
        }
    us_visitor(h, 6, hz - 10);
    {   /* his own fairy, flying where the oscillator's signal goes, a sparkling trail */
        static int32_t px[12], py[12];
        static uint32_t pn;
        int32_t fx = 110 + us_sx * 90 / 100, fy = hz / 2 + 8 + us_sy * (hz / 2) / 100, j;
        px[pn % 12u] = fx; py[pn % 12u] = fy; pn++;
        for (j = 1; j < 12; j++) {
            uint32_t q = (pn - 1u - (uint32_t)j) % 12u;
            cv_pset(px[q], py[q], ux_mix(T_BG, us_hue((uint32_t)j * 20u + (uint32_t)us_kn(2) * 2u, 255u), (12 - j) * 8));
        }
        us_disc(fx, fy, 3, ux_mix(T_BG, US_RGB(255, 250, 200), 30 + us_se / 3));
        cv_rect(fx - 1, fy - 1, 3, 3, US_RGB(255, 255, 255));
        cv_pset(fx - 3, fy - 2 - (int32_t)((fm1_ms / 50u) & 1u), US_RGB(200, 240, 255));
        cv_pset(fx + 3, fy - 2 - (int32_t)((fm1_ms / 50u) & 1u), US_RGB(200, 240, 255));
    }
    {   /* Saiko */
        int at, env = us_env(&at);               /* (the envelope: the wag's width, the ears up in the attack, a howl) */
        int perk = since < 350u || at, tongue = since < 1500u || rc > 80, blink = (now % 3700u) < 130u, howl = env > 70;
        int32_t wag = us_sin(now * (4u + (uint32_t)rc / 40u) << 20, 2 + env / 30);
        uint16_t grey = ux_mix(T_BG, US_RGB(120, 128, 150), 45 + k * 55 / 100);
        uint16_t white = ux_mix(T_BG, US_RGB(250, 250, 255), 45 + k * 55 / 100);
        static const uint16_t FL[4] = {US_RGB(255, 120, 190), US_RGB(255, 230, 90), US_RGB(140, 210, 255), US_RGB(255, 255, 255)};
        us_disc(x0 + 8 * S, (int32_t)h + 3, 9 * S, ux_mix(T_BG, HILL[sea][1], k * 60 / 100));   /* (his shadow on the hill) */
        {   /* a soft rim of light round him first */
            uint16_t rim = ux_mix(T_BG, US_RGB(255, 245, 230), 30 + k / 4);
            for (r = 0; r < US_DOG_H; r++)
                for (c = 0; c < 20; c++)
                    if (US_DOG[r][c] != '.') {
                        int32_t xs = US_DOG[r][c] == 'T' ? c + wag * (15 - r) / 3 : c, ys = r <= 1 && perk ? r - 1 : r;
                        cv_rect(x0 + xs * S - 1, y0 + ys * S - 1, S + 2, S + 2, rim);
                    }
        }
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
                case 'F': col = look == 2u ? grey : ux_mix(T_BG, FL[(c + (int32_t)(t / 8u) + us_kn(2) / 32) & 3], 50 + k / 2); break;
                case 'E': col = blink ? grey : ux_mix(US_RGB(70, 160, 255), US_RGB(190, 245, 255), rc / 10); break;
                case 'K': col = r == 6 && blink ? grey : US_RGB(20, 20, 30); break;
                case 'M': col = howl ? US_RGB(40, 20, 40) : ux_mix(T_BG, US_RGB(60, 50, 70), 70); break;
                case 'L': col = tongue ? US_RGB(255, 110, 150) : white; break;
                case 'T': xs = c + wag * (15 - r) / 3; col = r <= 11 ? white : grey; break;
                default: col = white; break;
                }
                cv_rect(x0 + xs * S, y0 + ys * S, S, S, col);
            }
        if (howl) {                              /* the howl: the mouth open, rings of sound rising from it (as loud as the envelope) */
            int32_t j, mx = x0 + 7 * S + S / 2, my = y0 + 9 * S;
            cv_rect(x0 + 6 * S, y0 + 9 * S, 3 * S, 2 * S, US_RGB(40, 20, 40));
            for (j = 0; j < 3; j++) {
                int32_t rr = 6 + j * 7 + (int32_t)((now / 40u) % 7u), m;
                uint16_t wc = ux_mix(T_BG, US_RGB(255, 240, 255), (env - 50) * 2 - j * 20);
                for (m = -10; m <= 10; m++)      /* (an arc over the head, opening upward) */
                    cv_pset(mx + us_sin((uint32_t)m * (1u << 25), rr), my - 4 * S - us_sin((uint32_t)m * (1u << 25) + (1u << 30), rr) / 2, wc);
            }
        } else if (tongue)                       /* (the tongue hangs a little further) */
            cv_rect(x0 + 7 * S, y0 + 11 * S, S, S, US_RGB(255, 110, 150));
        if (look == 1u) {                        /* shades: two dark lenses on a bar, a glint */
            cv_rect(x0 + 2 * S, y0 + 6 * S - 1, 11 * S, 2, US_RGB(20, 20, 30));
            cv_rect(x0 + 2 * S, y0 + 6 * S - 1, 4 * S, 2 * S + 1, US_RGB(20, 20, 30));
            cv_rect(x0 + 9 * S, y0 + 6 * S - 1, 4 * S, 2 * S + 1, US_RGB(20, 20, 30));
            cv_rect(x0 + 3 * S, y0 + 6 * S, 2, 1, ux_mix(US_RGB(20, 20, 30), US_RGB(140, 220, 255), 70 + rc / 20));
            cv_rect(x0 + 10 * S, y0 + 6 * S, 2, 1, ux_mix(US_RGB(20, 20, 30), US_RGB(140, 220, 255), 70 + rc / 20));
        } else if (look == 2u) {                 /* a party hat, striped, a pompom on top */
            int32_t j, hh = 7 * S, hx = x0 + 7 * S + S / 2;
            for (j = 0; j < hh; j++) {
                int32_t w = j * S / 6 + 1;
                cv_rect(hx - w, y0 + 4 * S - hh + j, 2 * w, 1,
                        ux_mix(T_BG, (j / S) & 1 ? US_RGB(255, 230, 90) : US_RGB(90, 200, 255), 50 + k / 2));
            }
            us_disc(hx, y0 + 4 * S - hh - 2, S, ux_mix(T_BG, US_RGB(255, 120, 190), 60 + k / 2));
        }
        us_holo(x0 + 7 * S, y0 - 4);
        if (since < 800u) {                      /* sparkles from the crown */
            uint32_t a = since;
            for (i = 0; i < 8; i++) {
                uint32_t rr = us_hash((uint32_t)i + note_t);
                int32_t sx = x0 + 4 * S + (int32_t)(rr % 7u) * S + us_sin((rr + a * 4u) << 22, 3);
                int32_t sy = y0 + 3 * S - (int32_t)(a * (14u + (rr >> 8) % 20u) / 800u);
                uint16_t sc = ux_mix(T_BG, FL[i & 3], 100 - (int32_t)(a * 100u / 800u));
                cv_pset(sx, sy, sc); cv_pset(sx - 1, sy, sc); cv_pset(sx + 1, sy, sc); cv_pset(sx, sy - 1, sc); cv_pset(sx, sy + 1, sc);
            }
        }
    }
}

/* the panel: the rest of the screen in the style's colours, still (drawn only as the text over it changes): quiet,
 * so the text stays readable */
static uint16_t us_panel(int32_t x, int32_t y)
{
    uint32_t st = sys.ui_style % US_COUNT, hs;
    int32_t yy = y < 0 ? 0 : y > 239 ? 239 : y;
    uint16_t c;
    switch (st) {
    case US_RAINBOW:                             /* a purple dusk, scanlines; a rainbow at the screen's edges */
        if (y < 2 || y > 236)
            return us_hue((uint32_t)x * 255u / 240u, 220u);
        c = ux_mix(T_BG, US_RGB(52, 12, 74), 20 + yy * 40 / 239);
        return (y & 3) == 0 ? ux_mix(c, US_RGB(255, 60, 170), 8) : c;
    case US_GRADIENT:                            /* indigo into teal, top to bottom */
        return ux_mix(T_BG, ux_mix(US_RGB(76, 24, 128), US_RGB(0, 92, 110), yy * 100 / 239), 32);
    case US_PSYCHE:                              /* plain: the colour stays in the kaleidoscope */
        return T_BG;
    case US_CATS:                                /* the lilac night, a star here and there */
        hs = us_hash((uint32_t)(y * 240 + x));
        c = ux_mix(T_BG, US_RGB(60, 34, 100), 25 + yy * 20 / 239);
        return (hs & 511u) == 0u ? ux_mix(c, US_RGB(255, 236, 255), 45) : c;
    case US_JEZUS:                               /* the cathedral's blue, a faint lattice of gold */
        c = ux_mix(T_BG, US_RGB(30, 44, 120), 30);
        return ((x + y) % 24 == 0 || (x - y + 480) % 24 == 0) ? ux_mix(c, US_RGB(242, 193, 78), 14) : c;
    case US_SAIKO:                               /* lavender into the meadow, tiny flowers */
        hs = us_hash((uint32_t)(y * 240 + x) + 77u);
        c = ux_mix(T_BG, ux_mix(US_RGB(110, 70, 170), US_RGB(60, 150, 110), yy * 100 / 239), 22);
        return (hs & 1023u) == 0u ? ux_mix(c, us_hue(hs >> 12, 255u), 55) : c;
    default:
        return T_BG;
    }
}

/* the whole screen's ground (a page drawn anew): the panel, in strips; plain where the style is off */
static void style_ground(void)
{
    uint32_t y;
    if (!cv_panel || cv_style_off) {
        lcd_fill(0, 0, 240, 240, T_BG);
        return;
    }
    for (y = 0; y < 240u; y += 40u) {            /* (strips under 64 rows: not the big canvas, no backdrop) */
        cv_at(0, (int32_t)y);
        cv_begin(240, 40, T_BG);
        cv_blit(0, y);
    }
    lcd_sync();
    lcd_fresh = 1;                               /* (painted over: the big canvas goes out whole again) */
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
    us_hud(h);
}

/* the style set: its palette, its backdrop (each change, and at power-on) */
static void style_apply(void)
{
    uint32_t st = sys.ui_style % US_COUNT;
    if (st == US_INITIAL) {
        palette_set(UI_PALETTE);
        cv_backdrop = 0;
        cv_panel = 0;
    } else {
        ux_cust.bg = US_PAL[st][0];
        ux_cust.surf = US_PAL[st][1];
        ux_cust.text = US_PAL[st][2];
        ux_cust.theme = US_PAL[st][3];
        ux_cust.accent = US_PAL[st][4];
        palette_set(NPALETTES);
        cv_backdrop = us_backdrop;
        cv_panel = us_panel;
    }
    ui_force = 1;
}

/* each frame: the colours that move (PSYCHEDELIC: the curves and the accent round the hues; RAINBOW ROAD: the curves) */
static void style_frame(void)
{
    uint32_t st = sys.ui_style % US_COUNT, t = us_t();
    us_beat();
    us_signal();
    if (st == US_RAINBOW) {                      /* (PSYCHEDELIC: its colour stays in the kaleidoscope, the blocks plain) */
        ux.theme = us_hue(t * 2u, 255u);
        ux.sel = ux_mix(ux.bg, ux.theme, UI_SEL_PCT);
        ux.tint = ux_mix(ux.bg, ux.theme, UI_TINT_PCT);
        ux.lane = ux_mix(ux.surf, ux.theme, 10);
    }
}
