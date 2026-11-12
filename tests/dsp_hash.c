#define main ui_shot_main
#include <math.h>
#include <time.h>
#include "ui_shot.c"
#undef main
/* DSP HASH: every preset rendered the same way (and again with MOD FX, BASIS, ORBIT, A 432), a hash of its output:
 * an optimisation must leave them all as they were.
 *   gcc -O2 -w -Itests -Ibuild/gen -o build/dsp_hash tests/dsp_hash.c -lm && ./build/dsp_hash > a.txt  (before, after: diff) */
static uint64_t h_ = 1469598103934665603ull;
static void run(uint32_t ms)
{
    uint32_t b, k, nb = ms * FS / 1000 / CTL;
    for (b = 0; b < nb; b++) {
        synth_render(abuf_, CTL);
        for (k = 0; k < 2 * CTL; k++) { h_ ^= (uint16_t)abuf_[k]; h_ *= 1099511628211ull; }
    }
}
int main(int argc, char **argv)
{
    int p, from = 0, to = (int)NPRESETS - 1, v;
    clock_t t0 = clock();
    if (argc > 2) { from = atoi(argv[1]); to = atoi(argv[2]); }
    palette_set(UI_PALETTE);
    for (v = 0; v < 2; v++)
    for (p = from; p <= to; p++) {
        uint64_t h0;
        synth_kill();
        preset_load((uint32_t)p);
        sys.master_q12 = 4095; sys.spk_bass = v ? 64 : 0; sys.lvl_match = 1;
        sys.basis = v ? 90 : sys.basis; sys.orbit = v ? 40 : sys.orbit;
        if (v) { mfx_set(1u + (uint32_t)p % MFX_NPRE); sys.tune432 = (uint8_t)(p & 1); }
        h_ = 1469598103934665603ull;
        run(50);
        synth_note_on(60, 100); synth_note_on(64, 90); synth_note_on(67, 110); run(600);
        synth_note_off(64); synth_note_on(71, 70); run(300);
        synth_note_off(60); synth_note_off(67); synth_note_off(71); run(400);
        h0 = h_;
        printf("%d %d %016llx %s\n", v, p, (unsigned long long)h0, PRESETS[p].name);
        sys.tune432 = 0;
    }
    fprintf(stderr, "cpu %.2f s\n", (double)(clock() - t0) / CLOCKS_PER_SEC);
    return 0;
}
