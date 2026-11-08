#define main ui_shot_main
#include <math.h>
#include "ui_shot.c"
#undef main
/* FX models: each model's wet loudness against the dry (mix full vs off), over a grid of the knobs, from two sources
 * (a sustained saw-ish, a pluck); and its noise (the hiss: energy over 6 kHz) */
typedef struct { double b0,b1,b2,a1,a2,z1,z2; } bq_t;
static double bq(bq_t *f, double x) { double y = f->b0*x + f->z1; f->z1 = f->b1*x - f->a1*y + f->z2; f->z2 = f->b2*x - f->a2*y; return y; }
static bq_t kw0 = {1.5308412300503478, -2.6509799951547297, 1.169079079921587, -1.6636551132560204, 0.7125954280732254};
static double e_all, e_hi; static int en;
static bq_t kw, hp6;
static void meas(uint32_t ms)
{
    uint32_t b, k, nb = ms * FS / 1000 / CTL;
    for (b = 0; b < nb; b++) {
        synth_render(abuf_, CTL);
        for (k = 0; k < CTL; k++) {
            double m = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0, kv = bq(&kw, m), h = bq(&hp6, m);
            e_all += kv * kv; e_hi += h * h; en++;
        }
    }
}
static void idle(uint32_t ms) { uint32_t b; for (b = 0; b < ms * FS / 1000 / CTL; b++) synth_render(abuf_, CTL); }
static double run(int src, int model, int st, int br, int dm, int pos, int mix, double *hi)
{
    static const uint8_t ns[4] = {48, 55, 60, 67};
    int i;
    synth_kill();
    preset_load(src ? 300u : 0u);   /* (replaced below) */
    sys.attack = src ? 0 : 20; sys.decay = src ? 50 : 90; sys.sustain = src ? 0 : 110; sys.release = 30;
    sys.sh_mix = 0; sys.master_q12 = 4095; sys.spk_bass = 0; sys.lvl_match = 0;
    synth_pgain[0] = synth_pgain[1] = 4096;
    sys.fx_model = (uint8_t)model; sys.fx_struct = (uint8_t)st; sys.fx_bright = (uint8_t)br;
    sys.fx_damp = (uint8_t)dm; sys.fx_pos = (uint8_t)pos; sys.fx_mix = (uint8_t)mix;
    idle(300);
    kw = kw0; kw.z1 = kw.z2 = 0;
    { double K = tan(M_PI * 6000.0 / FS), Q = 0.7071, a0 = 1 + K / Q + K * K;
      hp6.b0 = 1 / a0; hp6.b1 = -2 / a0; hp6.b2 = 1 / a0; hp6.a1 = 2 * (K * K - 1) / a0; hp6.a2 = (1 - K / Q + K * K) / a0; hp6.z1 = hp6.z2 = 0; }
    e_all = e_hi = 0; en = 0;
    for (i = 0; i < 4; i++) { synth_note_on(ns[i], 100); meas(450); synth_note_off(ns[i]); meas(150); }
    if (hi) *hi = 10 * log10(e_hi / (e_all + 1e-20) + 1e-12);
    return 10 * log10(e_all / en + 1e-20);
}
int main(int argc, char **argv)
{
    static const int ST[3] = {20, 64, 110}, BR[2] = {40, 100}, DM[2] = {30, 100}, PO[3] = {0, 64, 127};
    int md, src, a, b, c, d;
    palette_set(UI_PALETTE);
    (void)argc; (void)argv;
    if (argc > 1 && argv[1][0] == 'k') {          /* the KALIMBA's noise over its knobs */
        for (a = 0; a < 3; a++) for (b = 0; b < 2; b++) for (c = 0; c < 2; c++) for (d = 0; d < 3; d++) {
            double hi, l = run(0, RZ_KALIMBA, ST[a], BR[b], DM[c], PO[d], 127, &hi), hm, lm = run(0, RZ_MODAL, ST[a], BR[b], DM[c], PO[d], 127, &hm);
            printf("st %3d br %3d dm %3d pos %3d  kal %6.1f dB hi %6.1f   modal %6.1f hi %6.1f\n", ST[a], BR[b], DM[c], PO[d], l, hi, lm, hm);
        }
        return 0;
    }
    for (md = 0; md < RZ_COUNT; md++) {
        double sum = 0; int n = 0;
        for (src = 0; src < 2; src++) {
            double dry = run(src, md, 64, 64, 64, 64, 0, 0);
            for (a = 0; a < 3; a++) for (b = 0; b < 2; b++) for (c = 0; c < 2; c++) for (d = 0; d < 3; d += 2) {
                double w = run(src, md, ST[a], BR[b], DM[c], PO[d], 127, 0);
                sum += w - dry; n++;
            }
        }
        printf("%d\t%s\t%.1f\n", md, RZ_NAME[md], sum / n);
        fflush(stdout);
    }
    return 0;
}
