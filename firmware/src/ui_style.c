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
 * On the FX pages the oscillator's own picture instead, mashed up with the FX's vectors (us_mash); on HOME it
 * shines through the tunnel, softly, under the eye.
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
/* the eye, in lines, at (ex, ey), W wide: two lids open (open) px, an iris of rings turning through the hues
 * (SHIMMER: how many; DECAY: how fast), a pupil (pr) narrowing to a slit with the RING, looking where the signal goes,
 * a glint; a glow round it on the beat (gf) and, as the gate's lamp, as bright as glow (0 .. 255) */
static void us_eye(int32_t ex, int32_t ey, int32_t W, int32_t open, int32_t pr, uint32_t hue0, int32_t gf, int32_t glow)
{
    uint32_t t = us_t();
    int32_t H = W / 2, ek = 40 + us_k() * 55 / 100, m;
    int32_t slit = 100 - (int32_t)(sys.ring & 127u) * 70 / 127;
    uint16_t lid = ux_mix(T_BG, ux_mix(US_RGB(255, 255, 255), us_hue(hue0, 255u), 25), ek);
    int32_t lx = ex - W, ly = ey, tx = lx, ty = ly;
    for (m = 0; glow && m < 6; m++)              /* (the lamp's glow: rings fading out from it) */
        us_poly(ex, ey, W + 4 + m * 5, 24u, (uint32_t)m << 27, 160, ux_mix(T_BG, us_hue(hue0 + 128u, 255u), glow * (6 - m) / 12));
    if (gf)                                      /* (a glow round it on the beat) */
        us_poly(ex, ey, W + 4 + gf / 25, 16u, 0, 128, ux_mix(T_BG, us_hue(hue0 + 128u, 255u), gf / 3));
    for (m = -W + 3; m <= W; m += 3) {            /* (the lids: two arcs) */
        int32_t o = open * (W * W - m * m) / (W * W);
        cv_line(tx, ty, ex + m, ey - o, lid);
        cv_line(lx, ly, ex + m, ey + o, lid);
        tx = ex + m; ty = ey - o; lx = ex + m; ly = ey + o;
    }
    if (open > 1) {
        int32_t look = us_sx * W / 400, rings = 2 + (int32_t)(sys.sh_shimmer & 127u) / 32, ir = (open < H ? open : H) - 2, dy;
        uint32_t spin = t * (2u + (uint32_t)(sys.decay & 127u) / 16u);   /* (the iris turns as fast as the DECAY) */
        if (ir < 2)
            ir = 2;
        for (m = 0; m < rings; m++)              /* (the iris: rings turning through the hues) */
            us_poly(ex + look, ey, ir - m * ir / (rings + 1), 12u, spin << 20, 256,
                    ux_mix(T_BG, us_hue(t * 5u + (uint32_t)m * 50u + hue0, 255u), glow ? 40 + glow / 4 : ek));
        pr = pr + 2 > ir ? ir - 2 : pr;
        for (dy = -pr - 2; dy <= pr + 2; dy++) {   /* (the pupil: round, a slit with the RING) */
            int32_t w = us_isqrt((pr + 2) * (pr + 2) - dy * dy) * slit / 100;
            cv_rect(ex + look - w, ey + dy, 2 * w + 1, 1, US_RGB(0, 0, 0));
        }
        cv_rect(ex + look + pr / 2 + 2, ey - pr, 2, 2, lid);   /* (a glint) */
    }
}
/* the ENV page's lamp, as the eye (ui.c draw_env): shut when nothing sounds, opening with the gate (the newest voice's,
 * lv 0 .. 255, with the vactrol's lag), its pupil widening and its glow spreading as the lamp's did */
static void us_gate_eye(int32_t ex, int32_t ey, int32_t lv)
{
    int32_t W = 40, H = W / 2;
    uint32_t hue0 = us_t() * 3u + (uint32_t)us_kn(2) * 2u + (us_groove() ? us_bn * 24u : 0u);
    us_eye(ex, ey, W, 1 + (H + 2) * lv / 255, 2 + lv / 40, hue0, us_gf(), lv);
}

/* the tunnel's figure: a cymatic one, a ring of folds round the middle, as an oscillator's wave wrapped round a
 * circle: its folds (KNOB 1: 2 .. 16, the oscillator's own count to start from), their shape (KNOB 2: sine, triangle,
 * square, a pulse), the rings' density (KNOB 3: 4 .. 16 rings), its foldback (KNOB 4: the wave folded back on itself,
 * up to 7 times, as a wavefolder does: inner lobes, sharp ridges). Per oscillator its own start: the circuit's double
 * scroll, ring and cross mod two counts in a ratio beating against each other (a Lissajous ratio), clusters smooth
 * petals, the glitches and walks square teeth, the crushers triangles */
enum { UF_LISS, UF_ROSE, UF_HYPO, UF_EPI, UF_POLY, UF_COG };
static const uint8_t US_FIG[33][3] = {             /* osc 0 (the circuit) .. 32: family, a, b */
    {UF_LISS, 1, 2},                                                       /* CHUA */
    {UF_COG, 6, 0}, {UF_EPI, 5, 0}, {UF_LISS, 3, 2}, {UF_LISS, 1, 3}, {UF_LISS, 3, 4},   /* RADIO .. CROSS MOD */
    {UF_ROSE, 5, 0}, {UF_COG, 8, 0}, {UF_COG, 10, 0}, {UF_COG, 12, 0}, {UF_HYPO, 3, 0},   /* RESONOISE .. BASURILLA */
    {UF_ROSE, 3, 0}, {UF_POLY, 4, 0}, {UF_ROSE, 4, 0}, {UF_EPI, 3, 0}, {UF_HYPO, 3, 0},   /* the clusters */
    {UF_HYPO, 5, 0}, {UF_HYPO, 7, 0}, {UF_EPI, 8, 0}, {UF_ROSE, 6, 0}, {UF_LISS, 4, 5},   /* PRIME .. PHASING */
    {UF_COG, 5, 0}, {UF_POLY, 4, 0}, {UF_ROSE, 9, 0}, {UF_COG, 9, 0}, {UF_POLY, 6, 0},    /* BASURA .. ARRAY ROCKS */
    {UF_HYPO, 4, 0}, {UF_LISS, 5, 6}, {UF_EPI, 6, 0}, {UF_POLY, 3, 0}, {UF_COG, 7, 0},    /* EXIST. PAIN .. LFREE */
    {UF_EPI, 11, 0}, {UF_ROSE, 2, 0},                                      /* BIG BANG, PIANO */
};
static const uint8_t US_SHAPE0[6] = {0, 0, 40, 40, 100, 170};   /* (each family's shape to start from) */
/* a wave of shape s (0 sine .. 85 triangle .. 170 square .. 255 a narrow pulse) at phase p (a turn 2^32): +-256 */
static int32_t us_wave(uint32_t p, int32_t s)
{
    int32_t sn = us_sin(p, 256), f = (int32_t)(p >> 23), tr;   /* (f: 0 .. 511 a turn) */
    tr = f < 128 ? f * 2 : f < 384 ? 512 - f * 2 : f * 2 - 1024;   /* (the triangle, in phase with the sine) */
    if (s < 85)
        return sn + (tr - sn) * s / 85;
    if (s < 170) {
        int32_t sq = sn >= 0 ? 256 : -256;
        return tr + (sq - tr) * (s - 85) / 85;
    }
    {                                            /* (the square narrowing to a pulse: up only near the crest) */
        int32_t w = 256 - (s - 170) * 2;        /* (the crest's width) */
        return sn > 256 - w ? 256 : -256;
    }
}
static int32_t us_fold(int32_t v, int32_t g)     /* foldback: v (+-256) times g/64, folded back into +-256 */
{
    v = v * g / 64;
    while (v > 256 || v < -256)
        v = v > 256 ? 512 - v : -512 - v;
    return v;
}

#define US_TR 16
#define US_NP 96                                 /* (the points of a figure) */
static uint8_t us_splash;                        /* (the boot splash draws the tunnel alone: its own eye over it) */
static void us_mash(uint32_t h, int32_t dim, int32_t ex, int32_t ey);
static void us_psyche(uint32_t h)
{
    int32_t k = us_k(), rc = us_react(), cy = (int32_t)h / 2, at, ev = us_env(&at), gf = us_gf(), i;
    uint32_t t = us_t(), var = us_var(), beat = us_beatms();
    int32_t frac = (int32_t)((fm1_ms % beat) * 256u / beat);   /* (the rings' advance through a beat) */
    int32_t vx = 120, vy = cy;                   /* (in balance: all round the middle, as sand on a plate) */
    int32_t k1 = us_kn(0), k2 = us_kn(1), k3 = us_kn(2), k4 = us_kn(3);   /* (the oscillator's KNOBs 1 .. 4) */
    const uint8_t *fg = US_FIG[sys.osc % 33u];
    int32_t folds = (fg[0] == UF_LISS ? 2 : (int32_t)fg[1] / 2) + k1 / 8;   /* KNOB 1: the folds, one more each 8 */
    int32_t folds2, shape, gain, nr, depth;
    folds = folds < 2 ? 2 : folds > 16 ? 16 : folds;
    folds2 = fg[0] == UF_LISS ? folds * fg[2] / fg[1] : 0;   /* (a second count, beating: the ratio) */
    folds2 = folds2 > 24 ? 24 : folds2;
    shape = US_SHAPE0[fg[0]] + k2 * 2;           /* KNOB 2: the shape */
    shape = shape > 255 ? 255 : shape;
    gain = 64 + k4 * 3;                          /* KNOB 4: the foldback, x1 .. x7 */
    nr = 4 + k3 * 12 / 127;                      /* KNOB 3: the density, 4 .. 16 rings */
    depth = 70 + ev / 8 + (int32_t)(sys.fx_bright & 127u) / 4;   /* (the folds' depth: the envelope swells it) */
    /* the standing wave: the folds swinging deep and shallow, each ring against its neighbours, a swing a beat */
    int32_t sw = us_sin((uint32_t)((uint64_t)(fm1_ms % (beat * 2u)) * 0xFFFFFFFFu / (beat * 2u)), 256);
    int32_t echo = (int32_t)(sys.sh_decay & 127u) / 64;      /* (each ring's echo: the reverb's DECAY) */
    int32_t sq = 180 + (int32_t)(sys.basis & 127u) / 2;      /* (the width: the stereo BASIS) */
    int32_t lit = 30 + k * 40 / 100 + (int32_t)(sys.fx_mix & 127u) * 15 / 127 + k3 * 15 / 127;
    int16_t px[US_TR][US_NP], py[US_TR][US_NP];
    int32_t gm[2][US_NP];
    uint32_t hue0 = t * 3u + (uint32_t)k1 * 3u + (uint32_t)(sys.fx_model % RZ_COUNT) * 37u + var * 29u +
                    (us_groove() ? us_bn * 24u : 0u);
    uint32_t step = 0xFFFFFFFFu / US_NP + 1u, j, sect = 0xFFFFFFFFu / (uint32_t)folds + 1u;
    uint32_t rot0 = (var & 1u ? 0u - t : t) * 12u * 65536u;   /* (the whole turning, slowly) */
    int32_t op = us_splash ? 256 : wave_open(), f = op + 4;   /* (the silent collapse, as INITIAL's: all into the eye) */
    for (j = 0; j < US_NP; j++) {                /* (the figure's radius, twice: its wave up and down) */
        uint32_t th = j * step;
        int32_t v = us_wave(th * (uint32_t)folds + (1u << 30), shape);
        if (folds2)
            v = (v + us_wave(th * (uint32_t)folds2, shape)) / 2;
        v = us_fold(v, gain);
        gm[0][j] = 256 + v * depth * (160 + sw * 3 / 8) / 65536;
        gm[1][j] = 256 + v * depth * (160 - sw * 3 / 8) / 65536;
    }
    for (i = 0; i < nr; i++) {                   /* far (0) .. near */
        int32_t z = (nr - i) * 256 - frac;       /* (Q8 depth: 1 .. nr) */
        int32_t R = (34 + ev / 4 + rc / 80) * 512 / (z < 128 ? 128 : z), e;
        uint32_t rot = rot0 + (((uint32_t)(nr - i) & 1u) ? sect / 2u : 0u);   /* (every other ring half a fold on: a
                                                  * lattice, the pattern's nodes) */
        int32_t w = (nr - i) & 1;
        int32_t fade = 35 + 65 * (nr * 256 - z) / (nr * 256);
        uint16_t col = ux_mix(T_BG, us_hue(hue0 + (uint32_t)(i * 40), 255u), fade * lit / 100);
        R = R * f / 260;
        if (i == nr - 1 && gf)                   /* (the nearest flashes on the beat) */
            col = ux_mix(col, US_RGB(255, 255, 255), gf / 3);
        for (j = 0; j < US_NP; j++) {
            int32_t r = R * gm[w][j] / 256;
            uint32_t a = rot + j * step;
            px[i][j] = (int16_t)(vx + us_sin(a + (1u << 30), r));
            py[i][j] = (int16_t)(vy + us_sin(a, r) * sq / 256);
        }
        for (e = echo; e >= 0; e--) {            /* (its echo first, smaller and dimmer, then the ring) */
            uint16_t ce = e ? ux_mix(T_BG, col, 40) : col;
            for (j = 0; j < US_NP; j++) {
                uint32_t n1 = (j + 1u) % US_NP;
                int32_t x0 = vx + (px[i][j] - vx) * (8 - e) / 8, y0 = vy + (py[i][j] - vy) * (8 - e) / 8;
                int32_t x1 = vx + (px[i][n1] - vx) * (8 - e) / 8, y1 = vy + (py[i][n1] - vy) * (8 - e) / 8;
                cv_line(x0, y0, x1, y1, ce);
            }
        }
        if (i)                                   /* (along the tunnel, to the ring before: one spoke a fold) */
            for (j = 0; j < (uint32_t)folds; j++) {
                uint32_t q = (j * US_NP / (uint32_t)folds) % US_NP;
                cv_line(px[i - 1][q], py[i - 1][q], px[i][q], py[i][q], ux_mix(T_BG, col, 30));
            }
    }
    if (us_splash)
        return;
    if (sys.page == 0u) {                        /* (HOME: sand on the plate, the oscillator's own signal scattered
                                                  * over the figure: as many grains as KNOB 3's density, as far
                                                  * thrown as KNOB 4's foldback; they settle on the figure's lines) */
        uint32_t w = chua_trace.w, ng = 48u + (uint32_t)k3 * 2u, g;
        int32_t sc = 2 + k4 / 12;
        for (g = 0; g < ng; g++) {
            uint32_t at = (w - g * 5u) & (TRACE_N - 1u);
            uint32_t hs = ((uint32_t)(uint16_t)chua_trace.x[at] * 2654435761u) ^ ((uint32_t)(uint16_t)chua_trace.y[at] * 40503u) ^
                          (g * 2246822519u);
            int32_t gi = (int32_t)(hs % (uint32_t)nr), gj = (int32_t)((hs >> 8) % US_NP);
            int32_t gx = px[gi][gj] + (int32_t)((hs >> 16) & 15u) * sc / 8 - sc;
            int32_t gy = py[gi][gj] + (int32_t)((hs >> 20) & 15u) * sc / 8 - sc;
            cv_rect(gx, gy, 2, 1, ux_mix(T_BG, us_hue(hue0 + (hs >> 24), 255u), 45 + (int32_t)((hs >> 12) & 63u)));
        }
    }
    if (sys.page != 1u) {                        /* the eye at the tunnel's end (ENV: the gate's own eye instead, ui.c) */
        uint32_t bp = fm1_ms % (beat * 8u);
        int32_t W = 18 + (int32_t)(sys.sustain & 127u) / 6, H = W / 2;   /* (its size: the SUSTAIN) */
        W = W * (64 + k3) / 128 * f / 260;       /* (KNOB 3 too) */
        H = W / 2;
        if (W >= 4)
            us_eye(vx, vy, W, bp < 140u ? 1 : at ? H + 3 : H, (3 + rc / 150 + ev / 25 + gf / 60) * f / 260, hue0, gf, 0);   /* (it blinks
                                                  * every 2 bars; wide in an attack) */
    }
    if (op < 32)                                 /* (folded: a point left glowing in the eye, fading) */
        cv_rect(vx - 1, vy - 1, 3, 3, ux_mix(T_BG, us_hue(hue0, 255u), 40 + op * 2));
}

/* on the FX pages: the oscillator's picture (its trace turning in 3D, as HOME's INITIAL shows it) mashed up with
 * the FX's own vectors drawn over it: the trace copied round the middle in mirrored turns (2 .. 5: the FX's
 * POSITION, the reverb's SHIMMER), each copy its own hue running along the trace (the FX model's colours), as
 * big as the FX's BRIGHTNESS or the reverb's SIZE with the envelope's swell, as long as the DAMPING or the reverb's
 * DECAY lets it trail, turning as fast as the STRUCTURE (the reverb: its TONE); it flashes on the beat */
static void us_mash(uint32_t h, int32_t dim, int32_t ex, int32_t ey)   /* dim: how bright, %; (ex, ey): where it folds to */
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
    uint32_t rot0 = t << 21;
    int32_t f = wave_open() + 4, ox = ex + (120 - ex) * f / 260, oy = ey + (cy - ey) * f / 260;
    S = S * f / 260;                             /* (the silent collapse, ui.c: folded into (ex, ey)) */
    int32_t lit = (25 + k * 35 / 100 + (int32_t)((verb ? sys.sh_mix : sys.fx_mix) & 127u) * 20 / 127) * dim / 100;   /* (under the FX's own) */
    uint32_t ang = (uint32_t)((uint64_t)fm1_ms * (20u + spin) / 160u) & 255u;   /* (the 3D turn) */
    int32_t ca = plot_sin(ang + 64u), sa = plot_sin(ang);
    if (n < 2u)
        return;
    plot_scale(AX, w, n);
    n = n * (30u + tl * 70u / 127u) / 100u;      /* (the trail's length) */
    if (n < 2u)
        n = 2u;
    for (c = 0; c < copies; c++) {
        uint32_t a = c * (0xFFFFFFFFu / copies) + rot0;   /* (turned, every other one mirrored: in balance) */
        int32_t cs = us_sin(a + (1u << 30), 256), sn = us_sin(a, 256), mir = (c & 1u) ? -1 : 1, px = 0, py = 0;
        for (i = 0; i < n; i++) {
            uint32_t at = (w - n + i) & (TRACE_N - 1u);
            int32_t x = plot_norm(chua_trace.x[at], 0), y = plot_norm(chua_trace.y[at], 1), z = plot_norm(chua_trace.z[at], 2);
            int32_t d = (-x * sa + z * ca) >> 15, u = (x * ca + z * sa) >> 15, v = (y * 29696 + d * 13848) >> 15;
            int32_t ru = (u * mir * cs - v * sn) / 256, rv = (u * mir * sn + v * cs) / 256;
            int32_t X = ox + ru * S / 4096, Y = oy - rv * S / 4096;
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

/* PSYCHEDELIC's FX pictures: each model as a simple line drawing, big in the middle, as plain as the eye (the
 * pictograms of fx_icons.h, and a few more for the reverb's rooms), breathing with the sound and flashing on the
 * beat, a halo round it like the eye's lids; the oscillator's mash turning behind (us_mash) */
static const int8_t US_IC_HALL[] = {2, 2, 8, 44, 8, 20, 1, 5, 8, 20, 12, 10, 24, 6, 36, 10, 40, 20, 2, 2, 40, 20, 40, 44,
                                    2, 2, 2, 44, 46, 44, 1, 3, 18, 44, 24, 32, 30, 44, 0};
static const int8_t US_IC_TEMPLE[] = {2, 4, 4, 16, 24, 4, 44, 16, 4, 16, 2, 2, 10, 16, 10, 40, 2, 2, 20, 16, 20, 40, 2, 2, 28,
                                      16, 28, 40, 2, 2, 38, 16, 38, 40, 2, 2, 4, 42, 44, 42, 0};
static const int8_t US_IC_CATHEDRAL[] = {2, 3, 18, 14, 24, 2, 30, 14, 2, 4, 14, 44, 14, 14, 34, 14, 34, 44, 3, 24, 24, 10,
                                         1, 3, 20, 44, 24, 36, 28, 44, 2, 2, 8, 44, 40, 44, 0};
static const int8_t US_IC_MOSQUE[] = {1, 5, 12, 24, 14, 14, 24, 8, 34, 14, 36, 24, 2, 2, 12, 24, 36, 24, 2, 4, 10, 44, 10, 24,
                                      38, 24, 38, 44, 2, 2, 4, 44, 4, 12, 2, 2, 44, 44, 44, 12, 4, 4, 8, 4, 44, 8, 2, 2, 2, 44,
                                      46, 44, 0};
static const int8_t US_IC_ORGAN[] = {2, 2, 6, 44, 6, 22, 2, 2, 12, 44, 12, 14, 2, 2, 18, 44, 18, 8, 2, 2, 24, 44, 24, 4, 2, 2,
                                     30, 44, 30, 8, 2, 2, 36, 44, 36, 14, 2, 2, 42, 44, 42, 22, 2, 2, 2, 44, 46, 44, 0};
static const int8_t US_IC_LESLIE[] = {3, 24, 14, 20, 3, 24, 34, 20, 2, 2, 24, 4, 24, 44, 2, 2, 14, 14, 34, 14, 2, 2, 14, 34,
                                      34, 34, 0};
static const int8_t US_IC_DELAY[] = {1, 3, 4, 40, 12, 10, 20, 40, 1, 3, 20, 40, 27, 22, 34, 40, 1, 3, 34, 40, 38, 32, 42,
                                     40, 2, 2, 2, 42, 46, 42, 3, 12, 8, 6, 0};
/* a pictogram (fx_icons.h's format: half pixels on 48) at (cx, cy), sc/256 px a unit */
static void us_icon(const int8_t *p, int32_t cx, int32_t cy, int32_t sc, uint16_t c)
{
    while (*p) {
        int32_t op = *p++, n, i, x0 = 0, y0 = 0;
        if (op == 1 || op == 2) {                /* (a curve or a polyline: lines between its points) */
            n = *p++;
            for (i = 0; i < n; i++, p += 2) {
                int32_t x = cx + (p[0] - 24) * sc / 256, y = cy + (p[1] - 24) * sc / 256;
                if (i)
                    cv_line(x0, y0, x, y, c);
                x0 = x;
                y0 = y;
            }
        } else if (op == 3) {                    /* (a ring) */
            us_poly(cx + (p[0] - 24) * sc / 256, cy + (p[1] - 24) * sc / 256, p[2] * sc / 512 + 1, 16u, 0, 256, c);
            p += 3;
        } else {                                 /* (a dot) */
            cv_rect(cx + (p[0] - 24) * sc / 256 - 1, cy + (p[1] - 24) * sc / 256 - 1, 3, 3, c);
            p += 2;
        }
    }
}
static void us_fx_pic(int verb)
{
    static const int8_t *const RZ_IC[RZ_COUNT] = {ICON_BELL, ICON_SITAR, ICON_STRING, ICON_HUMAN, ICON_FLUTE,
        ICON_SONGBIRD, ICON_HAWK, ICON_TRUMPET, ICON_VIOLIN, ICON_SAX, ICON_DRUM, ICON_PIANO, ICON_PIANO, ICON_SAX,
        ICON_PIANO, ICON_SAX, ICON_FLUTE};
    static const int8_t *const SS_IC[SS_COUNT] = {US_IC_HALL, US_IC_TEMPLE, US_IC_CATHEDRAL, US_IC_MOSQUE, ICON_PIANO,
        ICON_STRING, ICON_VIOLIN, ICON_VIOLIN, ICON_SITAR, ICON_PIANO, ICON_SAX, US_IC_ORGAN, US_IC_LESLIE, US_IC_DELAY,
        US_IC_DELAY, US_IC_DELAY, ICON_HUMAN};
    uint32_t m = verb ? sys.sh_space % SS_COUNT : sys.fx_model % RZ_COUNT, t = us_t();
    int on = verb ? sys.sh_mix != 0 : sys.fx_mix != 0;
    const int8_t *ic = verb ? SS_IC[m] : RZ_IC[m];
    int32_t cy = PLOT_H / 2, gf = us_gf(), ev = us_env(0), rc = us_react(), k = us_k();
    int32_t sc = 400 + ev + rc / 6 + gf / 2 + us_sin(t << 22, 12);   /* (it breathes with the sound) */
    uint32_t hue = t * 3u + m * 37u + (verb ? 128u : 0u) + (us_groove() ? us_bn * 24u : 0u);
    uint16_t line = ux_mix(US_RGB(255, 255, 255), us_hue(hue, 255u), 25);
    uint16_t glow = ux_mix(T_BG, us_hue(hue + 40u, 255u), (on ? 45 : 20) + gf / 6);
    cv_begin(240, PLOT_H, T_BG);                 /* (the mash behind: the backdrop) */
    if (!on)
        line = ux_mix(T_BG, line, 45);
    us_poly(120, cy, 24 * sc / 256 + 10, 32u, t << 21, 200, glow);   /* (the halo, as the eye's lids) */
    us_icon(ic, 120, cy + 2, sc, glow);          /* (a glow under the lines) */
    us_icon(ic, 122, cy, sc, glow);
    us_icon(ic, 120, cy, sc, ux_mix(T_BG, line, 50 + k / 2));
    us_icon(ic, 121, cy, sc, ux_mix(T_BG, line, 50 + k / 2));   /* (the lines two wide) */
    cv_text(8, 4, &AF_S, verb ? SH_SPACE_NAME[m] : RZ_NAME[m], ux_mix(T_BG, us_hue(hue, 255u), 80));
    if (!on)
        cv_text(160, 4, &AF_S, "MIX OFF", T_MID);
    cv_blit(0, PLOT_Y);
}

static void us_backdrop(uint32_t h)
{
    if (sys.ui_style % US_COUNT != US_PSYCHE)
        return;
    if (sys.page == 2u || sys.page == 3u)        /* (the FX pages: the oscillator's vectors with the FX's) */
        us_mash(h, 40, 120, (int32_t)h / 2);                          /* (softly: the model's drawing over it stays plain) */
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
    ui_force = 1;
}

/* each frame: the beat and the signal it plays with */
static void style_frame(void)
{
    us_beat();
    us_signal();
}
