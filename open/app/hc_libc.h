/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the few string helpers the UI uses (memset / memcpy / memcmp: the SDK's libc). */
#ifndef HC_LIBC_H
#define HC_LIBC_H
#include <stdint.h>

static int str_eq(const char *a, const char *b)
{
    for (; *a == *b; a++, b++)
        if (!*a)
            return 1;
    return 0;
}
static unsigned str_len(const char *s)
{
    const char *p = s;
    while (*p)
        p++;
    return (unsigned)(p - s);
}
static void str_cpy(char *d, const char *s, unsigned max)   /* at most max - 1 characters, always ended */
{
    if (!max)
        return;
    while (*s && --max)
        *d++ = *s++;
    *d = 0;
}
/* v with `decimals` digits after a point: (-63, 1) -> "-6.3", (5, 2) -> "0.05" */
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[16];
    unsigned n = 0, i;
    uint32_t u = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;
    for (;;) {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if ((int)n == decimals)
            t[n++] = '.';
        if (!u && (int)n > decimals)
            break;
    }
    if (v < 0)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1u - i];
    b[n] = 0;
}
static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

static uint32_t rng_state = 0x2545F491u;
static uint32_t rng(void)                        /* xorshift32 */
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}
#endif
