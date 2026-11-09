/* SPDX-License-Identifier: GPL-3.0-only */
/* UI STYLE (GLO's fourth page, KNOB 1): the screen's look. INITIAL is HAWK CHUA's own (the AMBER palette, an
 * oscilloscope's). PSYCHEDELIC draws, under the big canvas of every page (gfx.c cv_backdrop) and instead of the
 * oscillator's picture, a tunnel in vectors with an eye at its end, both played by the sound's settings:
 *   the tunnel   turning polygons rushing at you, a ring a beat of the tempo (the BPM or the MIDI clock): its sides
 *                the oscillator and HOME KNOB 1, its twist KNOB 2 and the FX's STRUCTURE, its hues KNOB 3 and the FX
 *                model, the lines along it KNOB 4, its depth the reverb's SIZE, echoes of each ring the reverb's
 *                DECAY, its points (a star) the FX's BRIGHTNESS, its width the stereo BASIS, its far end the
 *                oscillator's signal, its swell the envelope and the sound, its brightness the FX MIX; on the
 *                ARP's, SEQ's and TAPE's steps the nearest ring flashes and the hues step on
 *   the eye      at the tunnel's end, in lines: its size the SUSTAIN, its iris rings the SHIMMER, turning as fast as
 *                the DECAY lets them, its pupil opening with the envelope and the sound and narrowing to a slit
 *                with the RING (the resonance), it looks where the signal goes, blinks every 2 bars and wide on
 *                each note's ATTACK
 * On the FX pages the oscillator's own picture instead, mashed up with the FX's vectors (us_mash).
 * KNOB 2 MOTION how fast it moves (0 still), KNOB 3 BACKDROP how strongly it shows, KNOB 4 REACT how much it answers
 * the sound. The rest of the screen stays plain. Saved with the settings. */

/* (the styles: US_INITIAL .. US_COUNT, ui.c's top) */
static const char *const US_NAME[US_COUNT] = {"INITIAL", "PSYCHEDELIC"};
static const char *const US_WHAT[US_COUNT] = {"THE OSCILLOSCOPE'S AMBER", "THE EYE IN THE TUNNEL"};
#define US_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* BG, SURF, TEXT, THEME, ACCENT (INITIAL: the AMBER palette instead) */
static const uint16_t US_PAL[US_COUNT][5] = {
    {0, 0, 0, 0, 0},
    {US_RGB(10, 0, 18), US_RGB(32, 8, 46), US_RGB(255, 250, 240), US_RGB(255, 60, 200), US_RGB(80, 255, 120)},
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
/* the beat: each note started (the ARP's, the SEQ's, the TAPE's steps too): when, how many */
static uint32_t us_bt = 0u - 100000u, us_bn, us_bseen;
static int32_t us_bf(void) { uint32_t a = fm1_ms - us_bt; return a < 200u ? 200 - (int32_t)a : 0; }   /* a note's flash, 200 .. 0 */
static int us_groove(void)                      /* the ARP with notes to play, the SEQ (KARMA) running, the TAPE playing */
{
    return (sys.arp_on && arp.n) || (sys.k_on && (kar.running || kar.n)) || tape.playing;
}
static uint32_t us_beatms(void)                  /* the tempo's beat (the BPM or the MIDI clock), ms */
{
    uint32_t bpm = arp_bpm_now();
    return 60000u / (bpm < 20u ? 20u : bpm);
}
static int32_t us_gf(void)                       /* the flash: a note's (200 .. 0), or the tempo's beat's while it grooves */
{
    int32_t f = us_bf(), b = 0;
    if (us_groove()) {
        uint32_t ph = (fm1_ms % us_beatms()) * 256u / us_beatms();
        b = ph < 64u ? (int32_t)(64u - ph) * 3 : 0;
    }
    return f > b ? f : b;
}
static void us_beat(void)                        /* (once a frame) */
{
    if (synth_ons != us_bseen) {
        us_bseen = synth_ons;
        us_bt = fm1_ms;
        us_bn++;
    }
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

/* a polygon's outline (n sides, radius r, turned by a: Q32 of a turn), squashed vertically by sq/256 */
static void us_poly(int32_t cx, int32_t cy, int32_t r, uint32_t n, uint32_t a, int32_t sq, uint16_t c)
{
    uint32_t j, step = 0xFFFFFFFFu / n + 1u;
    int32_t x0 = cx + us_sin(a + (1u << 30), r), y0 = cy + us_sin(a, r) * sq / 256;
    for (j = 1; j <= n; j++) {
        int32_t x1 = cx + us_sin(a + j * step + (1u << 30), r), y1 = cy + us_sin(a + j * step, r) * sq / 256;
        cv_line(x0, y0, x1, y1, c);
        x0 = x1;
        y0 = y1;
    }
}
#define US_TR 14
static void us_psyche(uint32_t h)
{
    int32_t k = us_k(), rc = us_react(), cy = (int32_t)h / 2, at, ev = us_env(&at), gf = us_gf(), i;
    uint32_t t = us_t(), var = us_var(), n = 3u + (var + (uint32_t)us_kn(0) / 14u) % 9u, beat = us_beatms();
    uint32_t tt = var & 1u ? 0u - t : t;
    int32_t frac = (int32_t)((fm1_ms % beat) * 256u / beat);   /* (the rings' advance through a beat) */
    int32_t vx = 120 + us_sx * 50 / 100, vy = cy + us_sy * 22 / 100;   /* (the far end: where the signal is) */
    int32_t nr = 6 + (int32_t)(sys.sh_size & 127u) / 16;    /* (the depth: the reverb's SIZE, 6 .. 13 rings) */
    int32_t echo = (int32_t)(sys.sh_decay & 127u) / 43;      /* (each ring's echoes: the reverb's DECAY, 0 .. 2) */
    int32_t spike = (int32_t)(sys.fx_bright & 127u) * 45 / 127;   /* (a star: every other point drawn in, % ) */
    int32_t sq = 120 + (int32_t)(sys.basis & 127u);          /* (the width: the stereo BASIS) */
    int32_t lit = 35 + k * 45 / 100 + (int32_t)(sys.fx_mix & 127u) * 20 / 127;   /* (the brightness: FX MIX too) */
    int32_t twist = us_kn(1) + (int32_t)(sys.fx_struct & 127u) / 2 + 8;
    int32_t px[US_TR][24], py[US_TR][24];
    uint32_t hue0 = t * 3u + (uint32_t)us_kn(2) * 2u + (uint32_t)(sys.fx_model % RZ_COUNT) * 37u + var * 29u +
                    (us_groove() ? us_bn * 24u : 0u);
    uint32_t m2 = n * 2u, step = 0xFFFFFFFFu / m2 + 1u, j;
    for (i = 0; i < nr; i++) {                   /* far (0) .. near */
        int32_t z = (nr - i) * 256 - frac;       /* (Q8 depth: 1 .. nr) */
        int32_t R = (60 + ev / 2 + rc / 40) * 256 / (z < 64 ? 64 : z), e;
        int32_t ccx = 120 + (vx - 120) * z / (nr * 256), ccy = cy + (vy - cy) * z / (nr * 256);
        uint32_t rot = (tt << 21) + (uint32_t)z * (uint32_t)twist * 4096u;
        int32_t fade = 15 + 85 * (nr * 256 - z) / (nr * 256);
        uint16_t col = ux_mix(T_BG, us_hue(hue0 + (uint32_t)(i * 40), 255u), fade * lit / 100);
        if (i == nr - 1 && gf)                   /* (the nearest flashes on the beat) */
            col = ux_mix(col, US_RGB(255, 255, 255), gf / 3);
        for (j = 0; j < m2; j++) {               /* (its points; every other one drawn in for a star) */
            int32_t r = (j & 1u) ? R * (100 - spike) / 100 : R;
            uint32_t a = rot + j * step;
            if (!spike && (j & 1u))               /* (no star: the midpoint of the side) */
                r = R * us_sin(step / 2u + (1u << 30), 256) / 256;
            px[i][j] = ccx + us_sin(a + (1u << 30), r);
            py[i][j] = ccy + us_sin(a, r) * sq / 256;
        }
        for (e = echo; e >= 0; e--) {            /* (its echoes first, smaller and dimmer, then the ring) */
            uint16_t ce = e ? ux_mix(T_BG, col, 55 - e * 15) : col;
            for (j = 0; j < m2; j++) {
                int32_t x0 = ccx + (px[i][j] - ccx) * (8 - e) / 8, y0 = ccy + (py[i][j] - ccy) * (8 - e) / 8;
                int32_t x1 = ccx + (px[i][(j + 1u) % m2] - ccx) * (8 - e) / 8, y1 = ccy + (py[i][(j + 1u) % m2] - ccy) * (8 - e) / 8;
                cv_line(x0, y0, x1, y1, ce);
            }
        }
        if (i)                                   /* (along the tunnel, to the ring before) */
            for (j = 0; j < m2; j += 2u)
                cv_line(px[i - 1][j], py[i - 1][j], px[i][j], py[i][j], ux_mix(T_BG, col, 20 + us_kn(3) / 2));
    }
    {   /* the eye at the tunnel's end, in lines */
        uint32_t bar2 = beat * 8u, bp = fm1_ms % bar2;
        int32_t W = 18 + (int32_t)(sys.sustain & 127u) / 6, H = W / 2;   /* (its size: the SUSTAIN) */
        int32_t open = bp < 140u ? 1 : at ? H + 3 : H, m;                 /* (it blinks every 2 bars; wide in an attack) */
        int32_t ex = vx, ey = vy, ek = 40 + us_k() * 55 / 100;
        int32_t pr = 3 + rc / 150 + ev / 25 + gf / 60, slit = 100 - (int32_t)(sys.ring & 127u) * 70 / 127;
        uint16_t lid = ux_mix(T_BG, ux_mix(US_RGB(255, 255, 255), us_hue(hue0, 255u), 25), ek);
        int32_t lx = ex - W, ly = ey, tx = lx, ty = ly;
        if (gf)                                  /* (a glow round it on the beat) */
            us_poly(ex, ey, W + 4 + gf / 25, 16u, 0, 128, ux_mix(T_BG, us_hue(hue0 + 128u, 255u), gf / 3));
        for (m = -W + 3; m <= W; m += 3) {        /* (the lids: two arcs) */
            int32_t o = open * (W * W - m * m) / (W * W);
            cv_line(tx, ty, ex + m, ey - o, lid);
            cv_line(lx, ly, ex + m, ey + o, lid);
            tx = ex + m; ty = ey - o; lx = ex + m; ly = ey + o;
        }
        if (open > 1) {
            int32_t look = us_sx * W / 400, rings = 2 + (int32_t)(sys.sh_shimmer & 127u) / 32, ir = H - 2, dy;
            uint32_t spin = t * (2u + (uint32_t)(sys.decay & 127u) / 16u);   /* (the iris turns as fast as the DECAY) */
            for (m = 0; m < rings; m++)          /* (the iris: rings turning through the hues) */
                us_poly(ex + look, ey, ir - m * ir / (rings + 1), 12u, spin << 20, 256,
                        ux_mix(T_BG, us_hue(t * 5u + (uint32_t)m * 50u + hue0, 255u), ek));
            for (dy = -pr - 2; dy <= pr + 2; dy++) {   /* (the pupil: round, a slit with the RING) */
                int32_t w = us_isqrt((pr + 2) * (pr + 2) - dy * dy) * slit / 100;
                cv_rect(ex + look - w, ey + dy, 2 * w + 1, 1, US_RGB(0, 0, 0));
            }
            cv_rect(ex + look + pr / 2 + 2, ey - pr, 2, 2, lid);   /* (a glint) */
        }
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

/* on the FX pages: the oscillator's picture (its trace turning in 3D, as HOME's INITIAL shows it) mashed up with
 * the FX's own vectors drawn over it: the trace copied round the middle in mirrored turns (2 .. 5: the FX's
 * POSITION, the reverb's SHIMMER), each copy its own hue running along the trace (the FX model's colours), as
 * big as the FX's BRIGHTNESS or the reverb's SIZE with the envelope's swell, as long as the DAMPING or the reverb's
 * DECAY lets it trail, turning as fast as the STRUCTURE (the reverb: its TONE); it flashes on the beat */
static void us_mash(uint32_t h)
{
    static const int16_t *const AX[3] = {chua_trace.x, chua_trace.y, chua_trace.z};
    int verb = sys.page == 3u;
    uint32_t w = chua_trace.w, n = w < TRACE_DRAW ? w : TRACE_DRAW, i, c, t = us_t();
    uint32_t copies = 2u + (uint32_t)((verb ? sys.sh_shimmer : sys.fx_pos) & 127u) / 32u;
    uint32_t fx = sys.fx_model % RZ_COUNT, hue0 = t * 3u + fx * 37u + (us_groove() ? us_bn * 24u : 0u);
    uint32_t tl = (uint32_t)((verb ? sys.sh_decay : sys.fx_damp) & 127u);
    uint32_t spin = (uint32_t)((verb ? sys.sh_tone : sys.fx_struct) & 127u);
    int32_t k = us_k(), cy = (int32_t)h / 2, gf = us_gf(), ev = us_env(0);
    int32_t S = 34 + (int32_t)((verb ? sys.sh_size : sys.fx_bright) & 127u) * 40 / 127 + ev / 5 + us_react() / 60;
    int32_t lit = 25 + k * 35 / 100 + (int32_t)((verb ? sys.sh_mix : sys.fx_mix) & 127u) * 20 / 127;   /* (under the FX's own) */
    uint32_t ang = (uint32_t)((uint64_t)fm1_ms * (20u + spin) / 160u) & 255u;   /* (the 3D turn) */
    int32_t ca = plot_sin(ang + 64u), sa = plot_sin(ang);
    if (n < 2u)
        return;
    plot_scale(AX, w, n);
    n = n * (30u + tl * 70u / 127u) / 100u;      /* (the trail's length) */
    if (n < 2u)
        n = 2u;
    for (c = 0; c < copies; c++) {
        uint32_t a = c * (0xFFFFFFFFu / copies) + (t << 21);
        int32_t cs = us_sin(a + (1u << 30), 256), sn = us_sin(a, 256), mir = (c & 1u) ? -1 : 1, px = 0, py = 0;
        for (i = 0; i < n; i++) {
            uint32_t at = (w - n + i) & (TRACE_N - 1u);
            int32_t x = plot_norm(chua_trace.x[at], 0), y = plot_norm(chua_trace.y[at], 1), z = plot_norm(chua_trace.z[at], 2);
            int32_t d = (-x * sa + z * ca) >> 15, u = (x * ca + z * sa) >> 15, v = (y * 29696 + d * 13848) >> 15;
            int32_t ru = (u * cs - v * sn) / 256 * mir, rv = (u * sn + v * cs) / 256;
            int32_t X = 120 + ru * S / 4096, Y = cy - rv * S / 4096;
            if (i) {
                uint16_t col = ux_mix(T_BG, us_hue(hue0 + c * 256u / copies + i * 96u / n, 255u),
                                      (20 + 80 * (int32_t)i / (int32_t)n) * lit / 100);
                if (gf && i + 8u > n)            /* (its head flashes on the beat) */
                    col = ux_mix(col, US_RGB(255, 255, 255), gf / 3);
                cv_line(px, py, X, Y, col);
            }
            px = X;
            py = Y;
        }
    }
}

static void us_backdrop(uint32_t h)
{
    if (sys.ui_style % US_COUNT != US_PSYCHE)
        return;
    if (sys.page == 2u || sys.page == 3u)        /* (the FX pages: the oscillator's vectors with the FX's) */
        us_mash(h);
    else
        us_psyche(h);
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
    cv_panel = 0;                                /* (the rest of the screen: plain) */
    ui_force = 1;
}

/* each frame: the beat and the signal it plays with */
static void style_frame(void)
{
    us_beat();
    us_signal();
}
