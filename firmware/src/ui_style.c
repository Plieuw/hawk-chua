/* SPDX-License-Identifier: GPL-3.0-only */
/* UI STYLE (GLO's fourth page, KNOB 1): the screen's look. INITIAL is HAWK CHUA's own (the AMBER palette, an
 * oscilloscope's); the others are a palette of their own and a backdrop painted under the big canvas (the plot of
 * every page: gfx.c cv_backdrop), some of it moving (KNOB 2 MOTION: how fast, 0 still) and answering the sound;
 * KNOB 3 BACKDROP how strongly it shows (the curves and text over it stay readable), KNOB 4 REACT how much it
 * answers the sound (the rings flare, the eyes glow).
 *   PSYCHEDELIC   a kaleidoscope spiralling out of the middle, and in it an eye that looks at you: its iris turns
 *                 through the hues, its pupil opens with the sound, it blinks; the blocks round it stay plain
 *   SAIKO         Saiko the husky in fairyland: a pastel dusk over rolling hills, a castle in the air on clouds
 *                 (floating to and fro over 4 bars) with twinkling windows, glowing toadstools, fairies flitting
 *                 about with trails of light; Saiko sits in a crown of flowers, blue eyes glowing with the sound:
 *                 tail wagging (faster as it gets louder), ears perking up at each note, tongue out while you play,
 *                 sparkles from the crown; a rainbow over it all, and it rains ketamine: little K's and crystals
 *                 drifting down, more of them as the sound gets louder
 * While the ARP, the SEQ or the TAPE plays, everything grooves: holograms rise on each step (a ring of light, a
 * spinning hexagon glitching upward), a display of the eight steps across the top, a scan line down the screen, the
 * flashes pulse on the tempo's beat between the notes too, Saiko hops. The envelope plays them too (the rings crowd
 * in; Saiko's wag widens, his ears go up in the attack, he howls when it is loud). Each style's scene changes with
 * the oscillator (HOME: the circuit or a noise program): PSYCHEDELIC 3 .. 8 arms, turning the other way, sometimes
 * coarse; SAIKO another season (dusk, night, snow, autumn) and another look (crown, shades, party hat). The pages
 * that drew the oscillator's picture show the scene instead, the HOME knobs and the signal playing it. A visitor
 * in the sky for each FX model.
 * All drawn here from simple shapes (nothing copied). Saved with the settings. */

/* (the styles: US_INITIAL .. US_COUNT, ui.c's top) */
static const char *const US_NAME[US_COUNT] = {"INITIAL", "PSYCHEDELIC", "SAIKO"};
static const char *const US_WHAT[US_COUNT] = {"THE OSCILLOSCOPE'S AMBER", "THE EYE IN THE KALEIDOSCOPE",
                                              "THE HUSKY IN FAIRYLAND"};
#define US_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* BG, SURF, TEXT, THEME, ACCENT (INITIAL: the AMBER palette instead) */
static const uint16_t US_PAL[US_COUNT][5] = {
    {0, 0, 0, 0, 0},
    {US_RGB(10, 0, 18), US_RGB(32, 8, 46), US_RGB(255, 250, 240), US_RGB(255, 60, 200), US_RGB(80, 255, 120)},
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
 * play with it (the rings crowd in, Saiko's wag, his ears, his howl) */
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

static uint32_t us_bars4(uint32_t *cyc)          /* where in a run of 4 bars (16 beats of the tempo) we are, ms; which run */
{
    uint32_t bpm = arp_bpm_now(), beat = 60000u / (bpm < 20u ? 20u : bpm), per = 16u * beat;
    if (cyc)
        *cyc = fm1_ms / per;
    return fm1_ms % per;
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
    case US_PSYCHE:                              /* plain: the colour stays in the kaleidoscope */
        return T_BG;
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
    case US_PSYCHE: us_psyche(h); break;
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

/* each frame: the beat and the signal the scenes play with */
static void style_frame(void)
{
    us_beat();
    us_signal();
}
