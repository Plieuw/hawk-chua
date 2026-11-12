/* SPDX-License-Identifier: GPL-3.0-only */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../firmware/src/fx_chuamod.c"

#define FS 48000
#define N FS
#define PI 3.14159265358979323846
static int32_t in[N], mod[N], out[N];

static double tone_amp(const int32_t *s, uint32_t start, uint32_t hz)
{
    double re = 0.0, im = 0.0;
    uint32_t i, count = N - start;
    for (i = start; i < N; ++i) {
        double a = (2.0 * PI * hz * i) / FS;
        re += s[i] * cos(a);
        im -= s[i] * sin(a);
    }
    return 2.0 * sqrt(re * re + im * im) / count;
}

static int test_shift(uint32_t mode, uint32_t wanted, uint32_t unwanted)
{
    uint32_t i;
    double a, b;
    for (i = 0; i < N; ++i) {
        in[i] = (int32_t)(20000.0 * sin(2.0 * PI * 3000.0 * i / FS));
        mod[i] = (int32_t)(20000.0 * sin(2.0 * PI * 500.0 * i / FS));
    }
    fx_chuamod_reset();
    fx_chuamod_process(in, mod, out, N, mode, 0, 32768, 32768);
    a = tone_amp(out, 5000, wanted);
    b = tone_amp(out, 5000, unwanted);
    printf("shift %s: wanted %.0f, image %.0f\n",
           mode == 1u ? "up" : "down", a, b);
    return a > 8000.0 && a > b * 5.0;
}

static int test_tremolo(void)
{
    uint32_t i;
    double early = 0.0, late = 0.0;
    for (i = 0; i < N; ++i) {
        in[i] = (int32_t)(16000.0 * sin(2.0 * PI * 440.0 * i / FS));
        mod[i] = (int32_t)(30000.0 * sin(2.0 * PI * 2.0 * i / FS));
    }
    fx_chuamod_reset();
    fx_chuamod_process(in, mod, out, N, 3, 0, 32768, 32768);
    for (i = FS / 16; i < FS / 8; ++i) early += (double)out[i] * out[i];
    for (i = FS * 5 / 16; i < FS * 3 / 8; ++i) late += (double)out[i] * out[i];
    early = sqrt(early / (FS / 16));
    late = sqrt(late / (FS / 16));
    printf("tremolo: high %.0f, low %.0f\n", early, late);
    return early > late * 1.5;
}

int main(void)
{
    int ok = 1;
    ok &= test_shift(1, 3500, 2500);
    ok &= test_shift(2, 2500, 3500);
    ok &= test_tremolo();
    puts(ok ? "Chua modulation DSP: ok" : "Chua modulation DSP: FAIL");
    return ok ? 0 : 1;
}
