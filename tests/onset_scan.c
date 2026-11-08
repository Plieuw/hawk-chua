#define main ui_shot_main
#include <math.h>
#include "ui_shot.c"
#undef main
/* ONSET: each built-in preset's held C4: the time to -6 dB of its loudest 10 ms (ms), as the sequencers need it fast */
static double rms[400]; static int nr;
static void meas(uint32_t ms)
{
    uint32_t b, k, nb = ms * FS / 1000 / CTL; static double acc; static int accn;
    for (b = 0; b < nb; b++) {
        synth_render(abuf_, CTL);
        for (k = 0; k < CTL; k++) {
            double m = (abuf_[2*k] + abuf_[2*k+1]) / 65536.0;
            acc += m * m;
            if (++accn == 441) { if (nr < 400) rms[nr++] = sqrt(acc / accn); acc = 0; accn = 0; }
        }
    }
}
int main(int argc, char **argv)
{
    int p, from = 0, to = (int)NPRESETS - 1;
    if (argc > 2) { from = atoi(argv[1]); to = atoi(argv[2]); }
    palette_set(UI_PALETTE);
    for (p = from; p <= to; p++) {
        int i, t = -1; double mx = 0;
        synth_kill();
        preset_load((uint32_t)p);
        sys.master_q12 = 4095; sys.spk_bass = 0; sys.lvl_match = 0;
        sys.sh_mix = 0; sys.fx_mix = sys.fx_mix;   /* (the space off: its tail no onset) */
        { uint32_t b; for (b = 0; b < 2000u * FS / 1000 / CTL; b++) synth_render(abuf_, CTL); }
        nr = 0;
        env_cap = getenv("CAP") ? (uint32_t)atoi(getenv("CAP")) : 0u;
        synth_note_on(60, 100); meas(1500); synth_note_off(60); meas(50);
        for (i = 0; i < nr && i < 150; i++) mx = rms[i] > mx ? rms[i] : mx;
        for (i = 0; i < nr; i++) if (rms[i] >= mx * 0.5) { t = i * 10; break; }
        printf("%d\t%d\t%s\t%u", p, t, PRESETS[p].name, PRESETS[p].attack);
        if (getenv("CURVE")) for (i = 0; i < 40; i++) printf(" %.0f", 100.0 * rms[i * 2] / (mx + 1e-12));
        printf("\n");
    }
    return 0;
}
