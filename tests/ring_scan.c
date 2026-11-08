#define main ui_shot_main
#include <math.h>
#include "ui_shot.c"
#undef main
/* RINGING: each preset, a C4 of 0.4 s: how long it rings after the release (to -40 dB of the note, s) and how peaky
 * the tail is (its strongest spectral line over the median, dB: a resonance ringing on); the reverb's share apart */
#define NS (44100 * 5)
static float wv[NS];
static void render(uint32_t p, int sh, uint32_t *n)
{
    uint32_t b, k;
    synth_kill(); preset_load(p); sys.master_q12 = 4095; sys.lvl_match = 0; synth_pgain[0] = 4096;
    if (!sh) sys.sh_mix = 0;
    for (b = 0; b < 1500u * FS / 1000u / CTL; b++) synth_render(abuf_, CTL);
    *n = 0;
    synth_note_on(60, 100);
    for (b = 0; b < 400u * FS / 1000u / CTL; b++) { synth_render(abuf_, CTL); for (k = 0; k < CTL; k++) wv[(*n)++] = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0f; }
    synth_note_off(60);
    while (*n + CTL < NS) { synth_render(abuf_, CTL); for (k = 0; k < CTL; k++) wv[(*n)++] = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0f; }
}
static void measure(uint32_t n, double *t40, double *peak)
{
    uint32_t i, w = 2205, off = 400u * 44100u / 1000u, last = off;
    double ref = 0, e;
    for (i = 0; i + w <= off; i += w) { e = 0; for (uint32_t j = 0; j < w; j++) e += wv[i + j] * wv[i + j]; ref = e > ref ? e : ref; }
    for (i = off; i + w <= n; i += w) { e = 0; for (uint32_t j = 0; j < w; j++) e += wv[i + j] * wv[i + j]; if (e > ref * 1e-4) last = i + w; }
    *t40 = (double)(last - off) / 44100.0;
    {   /* the tail's spectrum, 0.2 .. 0.2 + 0.74 s after the release (a DFT at 2048 bins by Goertzel-ish sums) */
        uint32_t s0 = off + 8820, N = 32768, kk;
        static double mag[1024];
        double sorted[1024], mx = 0;
        if (s0 + N > n) { *peak = 0; return; }
        for (kk = 0; kk < 1024; kk++) {
            double f = (kk + 2) * 10.0, c = 0, sn = 0, wv2 = 2 * M_PI * f / 44100.0;
            for (i = 0; i < N; i += 2) { double x = wv[s0 + i] * (0.5 - 0.5 * cos(2 * M_PI * i / N)); c += x * cos(wv2 * i); sn += x * sin(wv2 * i); }
            mag[kk] = c * c + sn * sn; sorted[kk] = mag[kk]; mx = mag[kk] > mx ? mag[kk] : mx;
        }
        for (i = 0; i < 1024; i++) for (kk = i + 1; kk < 1024; kk++) if (sorted[kk] < sorted[i]) { double t = sorted[i]; sorted[i] = sorted[kk]; sorted[kk] = t; }
        *peak = 10 * log10(mx / (sorted[512] + 1e-30) + 1e-30);
    }
}
int main(int argc, char **argv)
{
    uint32_t p, n, from = 0, to = NPRESETS - 1;
    if (argc > 2) { from = (uint32_t)atoi(argv[1]); to = (uint32_t)atoi(argv[2]); }
    palette_set(UI_PALETTE);
    for (p = from; p <= to; p++) {
        double t1, k1, t0, k0;
        render(p, 1, &n); measure(n, &t1, &k1);
        render(p, 0, &n); measure(n, &t0, &k0);
        printf("%u\t%s\t%.2f\t%.1f\t%.2f\t%.1f\n", p, PRESETS[p].name, t1, k1, t0, k0);
        fflush(stdout);
    }
    return 0;
}
