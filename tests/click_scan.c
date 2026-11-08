#define main ui_shot_main
#include <math.h>
#include "ui_shot.c"
#undef main
/* CLICKS: each preset, a note on and off: the high band (over 6 kHz, the click's) in the 4 ms at the start and at the
 * release against that band while the note holds (dB); a click: much more there than the sound itself has */
static double hp_e(int32_t *b, uint32_t n, double *z) { double e = 0; uint32_t k; for (k = 0; k < n; k++) { double x = (b[2*k] + b[2*k+1]) / 65536.0, y = 0.6 * (x - z[0]) + 0.4 * z[1]; z[0] = x; z[1] = y; e += y * y; } return e; }
/* a click: a corner in the wave sharper than any the sound itself has: the largest second difference over the largest
 * level, in the first 8 ms of a note (and the 8 ms after its release), against the same while it holds (100 .. 300 ms),
 * dB (tests/click_scan.c; not in the run: a tool) */
int main(int argc, char **argv)
{
    static float wv[30000];
    uint32_t p, from = 0, to = NPRESETS - 1;
    if (argc > 2) { from = (uint32_t)atoi(argv[1]); to = (uint32_t)atoi(argv[2]); }
    palette_set(UI_PALETTE);
    for (p = from; p <= to; p++) {
        uint32_t b, k, n = 0, i, off = 0;
        double po = 0, ph = 0, pr = 0, ao = 0, ah = 0, ar = 0;
        synth_kill(); preset_load(p); sys.master_q12 = 4095; sys.lvl_match = 0; synth_pgain[0] = 4096; sys.sh_mix = 0;
        for (b = 0; b < 1500u * FS / 1000u / CTL; b++) synth_render(abuf_, CTL);
        synth_note_on(48, 100);
        for (b = 0; b < 300u * FS / 1000u / CTL; b++) { synth_render(abuf_, CTL); for (k = 0; k < CTL && n < 30000; k++) wv[n++] = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0f; }
        synth_note_off(48); off = n;
        for (b = 0; b < 30u * FS / 1000u / CTL; b++) { synth_render(abuf_, CTL); for (k = 0; k < CTL && n < 30000; k++) wv[n++] = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0f; }
        for (i = 2; i < n; i++) {
            double d = fabs(wv[i] - 2 * wv[i - 1] + wv[i - 2]), a = fabs(wv[i]);
            if (i < 353) { po = d > po ? d : po; ao = a > ao ? a : ao; }
            else if (i >= 4410 && i < off) { ph = d > ph ? d : ph; ah = a > ah ? a : ah; }
            else if (i >= off && i < off + 353) { pr = d > pr ? d : pr; ar = a > ar ? a : ar; }
        }
        if (ao < ah * 0.1) po = 0;               /* (too soft there to click) */
        if (ar < ah * 0.1) pr = 0;
        printf("%u\t%s\t%.1f\t%.1f\t%u\t%u\n", p, PRESETS[p].name, 20 * log10((po / (ao + 1e-6) + 1e-9) / (ph / (ah + 1e-6) + 1e-9)),
               20 * log10((pr / (ar + 1e-6) + 1e-9) / (ph / (ah + 1e-6) + 1e-9)),
               PRESETS[p].attack, PRESETS[p].release);
    }
    return 0;
}
