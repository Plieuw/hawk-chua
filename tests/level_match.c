#define main ui_shot_main
#include <math.h>
#include "ui_shot.c"
#undef main
/* LEVEL MATCH's table (tests/level_match.sh writes firmware/src/preset_gain.h): every built-in preset played the
 * same way (C4 E4 G4 C5 G5 C6 one by one, then C E G held 2 s, at the keys' velocity, master full, SPEAKER BASS off), its
 * loudness measured as BS.1770 does (K-weighting, gated: here 100 ms blocks; the mean of the integrated and the
 * loudest 400 ms, so a struck note's attack counts) with a speaker's low cut at
 * 120 Hz, and the gain to LM_TARGET LU found: never over its peak's room (to -1 dBFS), -20 .. +24 dB */
static void wr32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void wr16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }
static FILE *wf; static uint32_t frames_;
static void play(uint32_t ms)
{
    uint32_t b, k, nb = ms * FS / 1000 / CTL;
    for (b = 0; b < nb; b++) {
        synth_render(abuf_, CTL);
        for (k = 0; k < 2 * CTL; k++) { int32_t v = abuf_[k]; v = v > 32767 ? 32767 : v < -32768 ? -32768 : v; wr16(wf, (uint16_t)v); }
        frames_ += CTL;
    }
}

typedef struct { double b0,b1,b2,a1,a2,z1,z2; } bq_t;
static double bq(bq_t *f, double x) { double y = f->b0*x + f->z1; f->z1 = f->b1*x - f->a1*y + f->z2; f->z2 = f->b2*x - f->a2*y; return y; }
static bq_t kw = {1.5308412300503478, -2.6509799951547297, 1.169079079921587, -1.6636551132560204, 0.7125954280732254};
static bq_t hp;
static double blk[4000]; static int nblk; static double acc; static int accn; static double peak;
static void meas(uint32_t ms)
{
    uint32_t b, k, nb = ms * FS / 1000 / CTL;
    for (b = 0; b < nb; b++) {
        synth_render(abuf_, CTL);
        for (k = 0; k < CTL; k++) {
            double l = abuf_[2*k] / 32768.0, r = abuf_[2*k+1] / 32768.0, m;
            double al = l < 0 ? -l : l, ar = r < 0 ? -r : r;
            peak = al > peak ? al : peak; peak = ar > peak ? ar : peak;
            m = bq(&hp, bq(&kw, (l + r) / 2));
            acc += m * m; accn++;
            if (accn == 4410) { if (nblk < 4000) blk[nblk++] = acc / accn; acc = 0; accn = 0; }   /* 100 ms blocks */
        }
    }
}
#define LM_TARGET (-22.0)
static unsigned gq[NPRESETS];
int main(int argc, char **argv)
{
    int p, from = 0, to = (int)NPRESETS - 1;
    (void)argc; (void)argv;
    double K = tan(M_PI * 120.0 / FS), Q = 0.7071, a0 = 1 + K / Q + K * K;
    palette_set(UI_PALETTE);
    for (p = from; p <= to; p++) {
        int i, n = 0; double s = 0, g, s2 = 0; int n2 = 0;
        hp.b0 = 1 / a0; hp.b1 = -2 / a0; hp.b2 = 1 / a0; hp.a1 = 2 * (K * K - 1) / a0; hp.a2 = (1 - K / Q + K * K) / a0;
        hp.z1 = hp.z2 = kw.z1 = kw.z2 = 0;
        synth_kill();
        preset_load((uint32_t)p);
        sys.master_q12 = 4095; sys.spk_bass = 0; sys.lvl_match = 0;
        synth_pgain[0] = synth_pgain[1] = 4096;
        { uint32_t b; for (b = 0; b < 3000u * FS / 1000 / CTL; b++) synth_render(abuf_, CTL); }   /* (tails gone) */
        nblk = 0; acc = 0; accn = 0; peak = 0;
        { static const uint8_t ns[6] = {60, 64, 67, 72, 79, 84};
          for (i = 0; i < 6; i++) { synth_note_on(ns[i], 100); meas(500); synth_note_off(ns[i]); meas(100); }
          synth_note_on(60, 100); synth_note_on(64, 100); synth_note_on(67, 100); meas(2000);
          synth_note_off(60); synth_note_off(64); synth_note_off(67); meas(800); }
        for (i = 0; i < nblk; i++) if (blk[i] > 1e-7) { s += blk[i]; n++; }          /* (-70 absolute) */
        g = n ? s / n : 1e-12;
        for (i = 0; i < nblk; i++) if (blk[i] > g * 0.1) { s2 += blk[i]; n2++; }    /* (-10 relative) */
        {
            double mm = 1e-12, lu, pk;
            for (i = 0; i + 4 <= nblk; i++) {                 /* (the loudest 400 ms: a strike's attack counts) */
                double q = (blk[i] + blk[i + 1] + blk[i + 2] + blk[i + 3]) / 4;
                mm = q > mm ? q : mm;
            }
            lu = n2 ? (10 * log10(s2 / n2) + 10 * log10(mm)) / 2 : -99.0;   /* (integrated and momentary max, halved) */
            pk = 20 * log10(peak + 1e-9);
            double gd = LM_TARGET - lu;
            if (sys.osc == NP_PIANO + 1u)                    /* (the PIANO: its strikes heard louder than measured) */
                gd -= 4.0;
            if (gd > -1.0 - pk) gd = -1.0 - pk;
            gd = gd > 24 ? 24 : gd < -20 ? -20 : gd;   /* (Q12 in 16 bits: x 15.8 at most) */
            if (lu < -90) gd = 0;
            gq[p] = (unsigned)(4096.0 * pow(10, gd / 20) + 0.5);
            fprintf(stderr, "%3d %6.1f LU  peak %6.1f  gain %+5.1f dB  %s\n", p, lu, pk, gd, PRESETS[p].name);
        }
        fflush(stdout);
    }
    printf("/* SPDX-License-Identifier: GPL-3.0-only */\n/* LEVEL MATCH: each built-in preset's gain to %.0f LU (Q12), written by tests/level_match.sh: do not edit */\n", (double)LM_TARGET);
    printf("static const uint16_t PRESET_GAIN[%u] = {", (unsigned)NPRESETS);
    for (p = 0; p < (int)NPRESETS; p++)
        printf("%s%u,", p % 16 ? " " : "\n    ", gq[p] ? gq[p] : 4096u);
    printf("\n};\n");
    return 0;
}
