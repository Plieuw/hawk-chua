/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test of the keyboard (firmware/src/keys.c) and the synth (synth.c), the same sources as the firmware,
 * against stubs of the key scan and the USB-MIDI rings. Checks: a key plays its note at the octave set and sends
 * MIDI, the octave changing under a held key, notes held by several sources, MIDI in (note on, velocity 0, CC 123,
 * a ring overflow), the boot hold, pitch, levels, voice stealing, silence at MASTER 0.
 *   tests/basis_test [OUT.wav]   (OUT.wav: a short render of the keys, to listen to) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define memset felucca_memset              /* (libc.c's own, beside the host's) */
#define memcpy felucca_memcpy
#define memcmp felucca_memcmp
#include "../firmware/src/libc.c"
#undef memset
#undef memcpy
#undef memcmp
static struct { volatile uint32_t notes, buttons; } fm1_in;

#include "../firmware/src/core.h"

/* the USB-MIDI rings of usb.c */
#define MQ 64u
static uint32_t midi_in_q[MQ];
static uint8_t midi_in_source[MQ];             /* 1 USB, 2 TRS (0: the tests' own, always taken) */
static volatile uint32_t mi_w, mi_r;
static volatile uint8_t midi_in_overflow;
static uint32_t out_log[256], out_n;
static void midi_out_event(uint32_t pkt) { if (out_n < 256u) out_log[out_n++] = pkt; }
static void midi_in(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_in_q[mi_w % MQ] = (st >> 4) | st << 8 | d1 << 16 | d2 << 24;
    mi_w++;
    midi_in_source[mi_w % MQ] = 0;               /* (the next one: the tests' own unless set) */
}

/* the second core: off (0), run at once (1), a real second thread (2: under ThreadSanitizer, a race between the two
 * halves of a block shows), or one that never answers (3: C1_WAIT gives up, this core renders its voices) */
#include <pthread.h>
static int c1_mode;
static void (*c1_fn)(uint32_t);
static uint32_t c1_arg;
static pthread_t c1_thread;
static void *c1_thread_main(void *p) { (void)p; c1_fn(c1_arg); return 0; }
static int c1_run(void (*fn)(uint32_t), uint32_t arg)
{
    c1_fn = fn;
    c1_arg = arg;
    if (c1_mode == 1)
        fn(arg);
    else if (c1_mode == 2)
        pthread_create(&c1_thread, 0, c1_thread_main, 0);
    return 1;
}
static int c1_wait(void)
{
    if (c1_mode == 2)
        pthread_join(c1_thread, 0);
    return c1_mode == 3 ? -1 : 0;
}
#define C1_ON() (c1_mode != 0)
#define C1_RUN(fn, arg) c1_run(fn, arg)
#define C1_WAIT() c1_wait()
#include "../firmware/src/synth.c"
#include "../firmware/src/keys.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static int32_t buf[2 * CTL];
static int32_t peak;
static FILE *wav;
static uint32_t wav_frames;
static void block(void)                      /* audio.c's audio_block, minus the USB tap and the 24-bit shift */
{
    uint32_t i;
    keys_block();
    synth_render(buf, CTL);
    for (i = 0; i < 2u * CTL; i++) {
        int32_t a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > peak)
            peak = a;
    }
    if (wav) {
        for (i = 0; i < CTL; i++) {
            int16_t s[2] = {(int16_t)buf[2 * i], (int16_t)buf[2 * i + 1]};
            fwrite(s, 2, 2, wav);
        }
        wav_frames += CTL;
    }
}
static void run_ms(uint32_t ms)
{
    uint32_t n = ms * FS / 1000u / CTL + 1u;
    while (n--)
        block();
}
static uint32_t sounding(void) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += voice[i].stage != ST_OFF; return n; }
static uint32_t key_of(uint32_t note) { return note - KEY_BASE_NOTE; }   /* at octave 0 */
static int sent(uint32_t i, uint32_t status, uint32_t note, uint32_t vel)
{
    uint32_t p = out_log[i];
    return i < out_n && ((p >> 8) & 0xFFu) == status && ((p >> 16) & 0x7Fu) == note && ((p >> 24) & 0x7Fu) == vel;
}

/* upward zero crossings of the output over 1 s: the pitch of a periodic regime */
static double measure_hz(uint32_t note)
{
    uint32_t frames = 0, cross = 0, i;
    int32_t prev = 0;
    synth_kill();
    sys.attack = 0;
    synth_note_on(note, 127);
    run_ms(300);                                 /* (onto the cycle, the DC blocker settled) */
    while (frames < FS) {
        synth_render(buf, CTL);
        for (i = 0; i < CTL; i++, frames++) {
            if (prev < 0 && buf[2 * i] >= 0)
                cross++;
            prev = buf[2 * i];
        }
    }
    synth_kill();
    return cross;
}

/* the same solver in double precision: the fixed-point one must follow it */
static void ref_step(double *s, double h, double a, double b)
{
    double m[3], dx, dy, dz;
#define G(x) (fabs(x) < 1 ? -8.0 / 7 * (x) : fabs(x) < 3 ? ((x) < 0 ? -1 : 1) * (-8.0 / 7 - 5.0 / 7 * (fabs(x) - 1)) \
                                                       : ((x) < 0 ? -1 : 1) * (-8.0 / 7 - 10.0 / 7 + 2 * (fabs(x) - 3)))
    dx = h * a * (s[1] - s[0] - G(s[0])); dy = h * (s[0] - s[1] + s[2]); dz = -h * b * s[1];
    m[0] = s[0] + dx / 2; m[1] = s[1] + dy / 2; m[2] = s[2] + dz / 2;
    s[0] += h * a * (m[1] - m[0] - G(m[0])); s[1] += h * (m[0] - m[1] + m[2]); s[2] += -h * b * m[1];
#undef G
}

static void wav_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    uint16_t a = 1, ch = 2, ba = 4, bits = 16;
    uint32_t sr = FS, br = FS * 4u;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 4; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 4; fwrite(&v, 4, 1, f);
}

static voice_t *voice_of(uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (voice[k].stage != ST_OFF && voice[k].note == note)
            return &voice[k];
    return 0;
}

int main(int argc, char **argv)
{
    if (getenv("SIZES"))
        printf("   (sizes: rz_t %u, its union %u, np_t %u, np_buf %u, voice_t %u)\n", (unsigned)sizeof(rz_t),
               (unsigned)sizeof rz[0].u, (unsigned)sizeof(np_t), (unsigned)sizeof np_buf[0], (unsigned)sizeof(voice_t));
    uint32_t k, w;
    keys_init();
    sys.master_q12 = 4096;
    sys.spk_bass = 0;                            /* (SPEAKER BASS off: the tests bit for bit as the sound is) */

    /* a key: its note (octave 0: key 0 = F3 = 53), MIDI out on channel 1, sound, release */
    fm1_in.notes = 1u << key_of(60);
    run_ms(10);
    check("key C4 down: note 60 sounds", sounding() == 1 && voice[0].note == 60 && sys.last_note == 60);
    check("... and goes out as MIDI note on 90 3C 64", out_n == 1 && sent(0, 0x90, 60, KEY_VEL));
    peak = 0;
    run_ms(100);
    check("... at a sane level (peak 2000..32767)", peak > 2000 && peak <= 32767);
    fm1_in.notes = 0;
    run_ms(5);
    check("key up: MIDI note off 80 3C", out_n == 2 && sent(1, 0x80, 60, 0));
    run_ms(2000);
    check("... the release ends the voice", sounding() == 0);

    /* the octave changes while a key is held: the key-up ends the note it started */
    out_n = 0;
    sys.octave = 1;
    fm1_in.notes = 1u << key_of(60);
    run_ms(10);
    sys.octave = -1;
    fm1_in.notes = 0;
    run_ms(10);
    check("octave +1: the key plays 72; changed to -1 while held, 72 still ends",
          sent(0, 0x90, 72, KEY_VEL) && sent(1, 0x80, 72, 0) && voice[1].note == 72u && voice[1].stage == ST_RELEASE);   /* (the next voice of the cycle) */
    sys.octave = 0;
    run_ms(2000);

    /* the lowest and the highest key */
    out_n = 0;
    fm1_in.notes = 1u | 1u << 26;
    run_ms(10);
    fm1_in.notes = 0;
    run_ms(10);
    check("keys 0 and 26: F3 (53) and G5 (79)",
          out_n == 4 && sent(0, 0x90, 53, KEY_VEL) && sent(1, 0x90, 79, KEY_VEL));
    run_ms(2000);

    /* a note held by a key and by MIDI ends with the last of them */
    fm1_in.notes = 1u << key_of(64);
    midi_in(0x90, 64, 90);
    run_ms(10);
    fm1_in.notes = 0;
    run_ms(10);
    check("key + MIDI on the same note: the key let go, it still sounds", voice[0].note == 64 && voice[0].stage == ST_SUSTAIN);
    midi_in(0x80, 64, 0);
    run_ms(10);
    check("... MIDI note off ends it", voice[0].stage == ST_RELEASE);
    run_ms(2000);

    /* MIDI in: note on, velocity 0 as note off, CC 123 */
    midi_in(0x93, 48, 127);
    midi_in(0x95, 55, 64);
    run_ms(10);
    check("MIDI in, any channel: two notes", sounding() == 2 && sys.last_vel == 64);
    midi_in(0x93, 48, 0);
    run_ms(10);
    check("note on with velocity 0 is a note off", note_held[48] == 0 && note_held[55] == 1);
    midi_in(0xB0, 123, 0);
    run_ms(10);
    check("CC 123 releases everything", note_held[55] == 0 && voice[0].stage != ST_SUSTAIN && voice[1].stage != ST_SUSTAIN);
    run_ms(2000);

    {   /* the XG / GM CC map: HOLD, SOSTENUTO, VOLUME / EXPRESSION / PAN, the sound's controls 71 .. 74, RESET */
        int okk = 1;
        uint32_t q;
        double el = 0, er = 0, e0 = 0;
#define VST(nt) ({ uint32_t _s = ST_OFF, _q; for (_q = 0; _q < NVOICE; _q++) if (voice[_q].note == (nt) && voice[_q].stage != ST_OFF && voice[_q].stage != ST_TAIL) _s = voice[_q].stage; _s; })
        midi_in(0xB0, 64, 127);                  /* HOLD down, a note played and let go: it sounds on */
        midi_in(0x90, 60, 100);
        run_ms(20);
        midi_in(0x80, 60, 0);
        run_ms(20);
        okk &= VST(60) == ST_SUSTAIN || VST(60) == ST_ATTACK;
        midi_in(0xB0, 64, 0);                    /* ... until it comes up */
        run_ms(20);
        okk &= VST(60) == ST_RELEASE || VST(60) == ST_OFF;
        run_ms(2000);
        midi_in(0x90, 62, 100);                  /* SOSTENUTO: holds 62 (held as it went down), not 64 (after) */
        run_ms(20);
        midi_in(0xB0, 66, 127);
        midi_in(0x90, 64, 100);
        run_ms(20);
        midi_in(0x80, 62, 0);
        midi_in(0x80, 64, 0);
        run_ms(20);
        okk &= (VST(62) == ST_SUSTAIN || VST(62) == ST_ATTACK) && VST(64) != ST_SUSTAIN && VST(64) != ST_ATTACK;
        midi_in(0xB0, 66, 0);
        run_ms(20);
        okk &= VST(62) != ST_SUSTAIN && VST(62) != ST_ATTACK;
        run_ms(2000);
        printf("   (HOLD, SOSTENUTO: %s)\n", okk ? "ok" : "wrong");
        sys.osc = 0;                             /* 71 .. 74: the HOME page's knobs (the circuit; a noise program) */
        midi_in(0xB0, 71, 11); midi_in(0xB0, 72, 22); midi_in(0xB0, 73, 33); midi_in(0xB0, 74, 44);
        run_ms(10);
        okk &= sys.chaos == 11u && sys.shape == 22u && sys.output == 33u && sys.ring == 44u;
        sys.osc = 3;
        midi_in(0xB0, 71, 55); midi_in(0xB0, 72, 66); midi_in(0xB0, 73, 77);
        run_ms(10);
        okk &= sys.np_x == 55u && sys.np_y == 66u && sys.np_level == 77u && sys.chaos == 11u;
        sys.osc = 0; sys.chaos = 85; sys.shape = 76; sys.output = 0; sys.ring = 50;
        run_ms(2000);
        for (q = 0; q < 3u; q++) {               /* VOLUME 0: silence; PAN 0: all left */
            static const uint8_t vv[3] = { 100, 100, 0 }, pp[3] = { 64, 0, 64 };
            uint32_t w, b;
            double sl = 0, sr = 0;
            midi_in(0xB0, 7, vv[q]); midi_in(0xB0, 10, pp[q]);
            midi_in(0x90, 57, 110);
            run_ms(100);
            for (w = 0; w < 40u; w++) { block(); for (b = 0; b < CTL; b++) { sl += fabs((double)buf[2 * b]); sr += fabs((double)buf[2 * b + 1]); } }
            midi_in(0x80, 57, 0);
            run_ms(1500);
            if (q == 0) { e0 = sl + sr; }
            if (q == 1) { el = sl; er = sr; }
            if (q == 2) okk &= sl + sr < 0.001 * e0;
        }
        printf("   (PAN 0: left %.0f right %.0f; VOLUME 100 both %.0f)\n", el, er, e0);
        okk &= e0 > 0 && er < 0.01 * el;
        midi_in(0xB0, 7, 100); midi_in(0xB0, 10, 64);
        run_ms(10);
#undef VST
        check("MIDI CCs (the XG map): HOLD, SOSTENUTO, VOLUME, PAN, 71 .. 74 = HOME KNOB 1 .. 4", okk);
    }
    {   /* MIDI pitch bend: an octave up and down at the ends (BEND RANGE 12), half at 6, the middle back */
        int okk = 1;
        uint32_t k, base = 0;
        voice_t *vv = 0;
        synth_kill(); keys_all_off(); run_ms(50);
        sys.bend_range = 12;
        midi_in(0x90, 60, 100); run_ms(20);
        for (k = 0; k < NVOICE; k++) if (voice[k].stage != ST_OFF) vv = &voice[k];
        okk &= vv != 0;
        if (vv) {
            base = vv->inc0;
            midi_in(0xE0, 127, 127); run_ms(5);
            okk &= vv->inc > base * 2u - base / 100u && vv->inc < base * 2u + base / 100u;
            midi_in(0xE0, 0, 0); run_ms(5);
            okk &= vv->inc > base / 2u - base / 200u && vv->inc < base / 2u + base / 200u;
            sys.bend_range = 6;
            midi_in(0xE0, 127, 127); run_ms(5);
            okk &= vv->inc > base * 141u / 100u && vv->inc < base * 142u / 100u;
            midi_in(0xE0, 0, 64); run_ms(5);
            okk &= vv->inc == base;
        }
        sys.bend_range = 12;
        midi_in(0x80, 60, 0); synth_kill(); keys_all_off(); run_ms(20);
        check("MIDI pitch bend: +-12 semitones at the ends, +6 at range 6, the middle the note", okk);
    }

    /* a ring overflow: everything let go */
    midi_in(0x90, 50, 100);
    run_ms(10);
    midi_in_overflow = 1;
    run_ms(10);
    check("MIDI ring overflow: held notes released, ring emptied",
          note_held[50] == 0 && mi_r == mi_w && !midi_in_overflow);
    run_ms(2000);

    /* the boot hold: keys pressed under the splash are silent, down and up */
    out_n = 0;
    kb_boot_hold = 1;
    fm1_in.notes = 1u << 3;
    run_ms(10);
    kb_boot_hold = 0;
    run_ms(10);
    fm1_in.notes = 0;
    run_ms(10);
    check("a key held from boot: no note, no MIDI, not even on release", out_n == 0 && sounding() == 0);

    /* every key at once: NVOICE voices, no more, in every regime */
    fm1_in.notes = (1u << NKEYS) - 1u;
    peak = 0;
    for (k = 0; k < 128u; k += 16u) {
        sys.chaos = (uint8_t)k;
        run_ms(100);
    }
    check("27 keys at once: 8 voices sound, the output stays within 16 bits", sounding() == NVOICE && peak <= 32767);
    fm1_in.notes = 0;
    run_ms(3000);
    check("... all released", sounding() == 0);

    /* the circuit: fixed point against double precision, the same midpoint solver, the same start; 2 time units
     * (chaos parts the two exponentially after that: then only the statistics can agree) */
    {
        static const uint8_t KN[][2] = {{4, 64}, {40, 64}, {54, 20}, {85, 76}, {124, 40}, {100, 127}};
        double worst = 0;
        for (k = 0; k < sizeof KN / sizeof KN[0]; k++) {
            chua_t s = {C1 * 3 / 2, C1 / 10, -C1};
            double r[3] = {1.5, 0.1, -1.0}, a, b, h = 0.01;
            int32_t hs = (int32_t)(h * (1 << 24) + 0.5), ahs, bhs;
            sys.chaos = KN[k][0];
            sys.shape = KN[k][1];
            cc.valid = 0;
            chua_params();
            a = cc.alpha / 65536.0;
            b = cc.beta / 65536.0;
            ahs = (int32_t)(((int64_t)cc.alpha * hs) >> 16);
            bhs = (int32_t)(((int64_t)cc.beta * hs) >> 16);
            for (w = 0; w < 200u; w++) {
                double e;
                chua_step(&s, hs, ahs, bhs);
                ref_step(r, h, a, b);
                e = fabs(s.x / 1048576.0 - r[0]) + fabs(s.y / 1048576.0 - r[1]) + fabs(s.z / 1048576.0 - r[2]);
                worst = e > worst ? e : worst;
            }
        }
        printf("   (largest difference over 2 time units: %.2e)\n", worst);
        check("fixed-point circuit follows the double-precision one (< 1e-3)", worst < 1e-3);
    }

    /* every regime, shape and range of notes: the solver stays in range (no restarts), the output in 16 bits */
    {
        static const uint8_t NOTES[] = {24, 48, 60, 72, 96, 120};
        uint32_t c, sh, ni;
        chua_restarts = 0;
        peak = 0;
        sys.attack = 0;
        for (c = 0; c < 128u; c += 8u)
            for (sh = 0; sh < 128u; sh += 42u) {
                sys.chaos = (uint8_t)c;
                sys.shape = (uint8_t)sh;
                for (ni = 0; ni < sizeof NOTES; ni++)
                    synth_note_on(NOTES[ni], 127);
                run_ms(150);
                synth_kill();
            }
        printf("   (restarts %u, peak %d)\n", chua_restarts, peak);
        check("CHAOS x SHAPE x notes 24..120: no solver restart, output within 16 bits", chua_restarts == 0 && peak <= 32767);
    }

    /* pitch: in the CYCLE regime the circuit is periodic, and the tables' time scale puts it on the note */
    {
        double a3, a4, c2;
        sys.chaos = 4;
        sys.output = 64;                         /* y: the cleanest */
        for (k = 0; k < 128u; k += 63u) {
            char what[80];
            sys.shape = (uint8_t)k;
            a3 = measure_hz(57);
            a4 = measure_hz(69);
            c2 = measure_hz(36);
            snprintf(what, sizeof what, "CYCLE regime, SHAPE %3u: A3 %.0f, A4 %.0f, C2 %.1f Hz (within 2 %%)", k, a3, a4, c2);
            check(what, fabs(a3 / 220 - 1) < 0.02 && fabs(a4 / 440 - 1) < 0.02 && fabs(c2 / 65.41 - 1) < 0.02);
        }
        sys.output = 0;
    }

    /* the second core: the same output, bit for bit, whichever core renders which voice */
    {
        static int32_t ref[2 * CTL * 2000];
        static const uint8_t CH[] = {48, 52, 55, 59, 62, 65, 69, 72};
        int mode, same = 1;
        uint32_t blocks = 0;
        for (mode = 0; mode < 4; mode++) {
            uint32_t b, j;
            synth_kill();
            memset(voice, 0, sizeof voice);
            ghost.x = C1 / 10; ghost.y = ghost.z = 0;
            rng_state = 0x1234567u;
            voice_clock = 0;
            c1_blocks = 0;
            memset(rz, 0, sizeof rz); rz_clock = rz_view_r = 0;
            sys.fx_mix = 0; sys.fx_model = RZ_MODAL; sys.fx_struct = 70; sys.fx_damp = 100;
            c1_mode = mode;
            sys.chaos = 85; sys.shape = 76; sys.output = 32; sys.attack = 2; sys.release = 30; sys.osc = 0;
            sys.menv_height = 0; sys.basis = 100; sys.orbit = 90; voice_rr = 0;   /* (BASIS, ORBIT: panned, each core its own) */
            memset(rz_epan, 0, sizeof rz_epan); memset(rz_age, 0, sizeof rz_age);
            for (b = 0; b < 2000u; b++) {
                if (b % 250u == 0u)
                    for (j = 0; j < sizeof CH; j++)
                        (b / 250u + j) & 1u ? synth_note_on(CH[j] + b / 250u, 100) : synth_note_off(CH[j] + b / 250u - 1u);
                if (b == 1000u)
                    sys.chaos = 120;
                if (b == 300u)
                    sys.fx_mix = 100;            /* the resonators (four: half on the second core), */
                if (b == 800u)
                    sys.fx_model = RZ_SYMP;      /* the sympathetic strings */
                if (b == 1400u)
                    sys.osc = 7;                 /* noise programs under held notes: GRAIN GLITCH (its buffer), */
                if (b == 1700u)
                    sys.osc = 14;                /* SINE FM CLUST, */
                if (b == 1850u) {
                    sys.menv_height = 90;        /* the MOD ENV on its pitch */
                }
                synth_render(buf, CTL);
                for (j = 0; j < 2u * CTL; j++) {
                    if (mode == 0)
                        ref[b * 2u * CTL + j] = buf[j];
                    else
                        same &= ref[b * 2u * CTL + j] == buf[j];
                }
            }
            if (mode == 1)
                blocks = c1_blocks;
        }
        c1_mode = 0;
        sys.osc = 0;
        sys.fx_mix = 0;
        sys.menv_height = 0;
        sys.basis = 0;
        sys.orbit = 0;
        printf("   (blocks with voices on the second core: %u of 2000)\n", blocks);
        check("second core (at once, a thread, one that gives up): the same output bit for bit", same && blocks > 1000u);
    }

    /* the noise programs (noise.c): each sounds, at about the circuit's level at X = Y = 64, and stays bounded and
     * without DC in every corner of X and Y, chords included; switching under held notes is clean */
    {
        static const uint8_t XY[5][2] = {{64, 64}, {0, 0}, {127, 0}, {0, 127}, {127, 127}};
        static const uint8_t CH[4] = {48, 55, 60, 67};
        uint32_t a, c, j, b, okk = 1;
        double lo = 1e9, hi = 0;
        sys.attack = 0; sys.decay = 64; sys.sustain = 127; sys.release = 20; sys.ring = 50;
        sys.fx_mix = 0; sys.sh_mix = 0; sys.np_level = 64;
        if (getenv("NP_LEVELS")) {                       /* the circuit, the same way: the level to match */
            double e = 0;
            synth_kill(); run_ms(50); sys.osc = 0;
            for (j = 0; j < 4u; j++) synth_note_on(CH[j], 110);
            run_ms(200);
            for (b = 0; b < 400u; b++) { synth_render(buf, CTL); for (j = 0; j < CTL; j++) e += (double)buf[2 * j] * buf[2 * j]; }
            printf("   LEVEL CHUA %.0f\n", sqrt(e / (400.0 * CTL)));
        }
        for (a = 0; a < NP_COUNT; a++) {
            for (c = 0; c < 5u; c++) {
                double e = 0, dc = 0;
                int32_t pk = 0;
                synth_kill();
                run_ms(50);
                sys.osc = (uint8_t)(a + 1u);
                sys.np_x = XY[c][0];
                sys.np_y = XY[c][1];
                for (j = 0; j < 4u; j++)
                    synth_note_on(CH[j], 110);
                run_ms(200);
                for (b = 0; b < 400u; b++) {                /* ~290 ms */
                    synth_render(buf, CTL);
                    for (j = 0; j < CTL; j++) {
                        int32_t v = buf[2 * j], av = v < 0 ? -v : v;
                        e += (double)v * v;
                        dc += v;
                        pk = av > pk ? av : pk;
                    }
                }
                e = sqrt(e / (400.0 * CTL));
                dc /= 400.0 * CTL;
                if (c == 0u && getenv("NP_LEVELS"))
                    printf("   LEVEL %u %.0f\n", a, e);
                if (c == 0u) {
                    lo = e < lo ? e : lo;
                    hi = e > hi ? e : hi;
                    if (e < 2700 || e > 11000) {             /* (the circuit: ~5400) */
                        printf("   %s: RMS %.0f at X = Y = 64\n", NP_NAME[a], e);
                        okk = 0;
                    }
                }
                if (e < 50 || fabs(dc) > 3000 || pk > 32767) {
                    printf("   %s X %u Y %u: RMS %.0f DC %.0f peak %d\n", NP_NAME[a], XY[c][0], XY[c][1], e, dc, pk);
                    okk = 0;
                }
            }
        }
        printf("   (noise programs at X = Y = 64: RMS %.0f .. %.0f)\n", lo, hi);
        check("noise: all 32 programs sound, levelled, bounded, no DC in X / Y corners", okk);
        synth_kill();
        sys.osc = 0;
        sys.np_x = sys.np_y = sys.np_level = 64;
    }

    /* every oscillator and FX model over the whole MIDI range (KARMA reaches 12..120, MIDI in 0..127), X / Y and FM at
     * their ends, notes retriggered fast: nothing out of bounds (the sanitizers), bounded */
    {
        static const uint8_t NT[] = {0, 1, 12, 24, 36, 100, 108, 115, 120, 126, 127};
        uint32_t a, j, b, m, okk = 1;
        for (a = 0; a <= NP_COUNT; a++)
            for (m = 0; m < 3u; m++) {
                synth_kill();
                sys.osc = (uint8_t)a;
                sys.np_x = sys.np_y = (uint8_t)(m == 1u ? 0u : 127u);
                sys.menv_height = (uint8_t)(m == 2u ? 127u : 0u);
                sys.menv_skew = 64;
                sys.fx_model = (uint8_t)((a + m) % RZ_COUNT);
                sys.fx_mix = 100;
                for (b = 0; b < 3u * sizeof NT; b++) {
                    synth_note_on(NT[(b * 7u) % sizeof NT], 127);
                    for (j = 0; j < 4u; j++) {
                        synth_render(buf, CTL);
                        for (uint32_t k = 0; k < 2u * CTL; k++)
                            if (buf[k] > 8388607 || buf[k] < -8388608)
                                okk = 0;
                    }
                }
            }
        synth_kill();
        sys.osc = 0; sys.np_x = sys.np_y = 64; sys.menv_height = 0; sys.menv_skew = 10; sys.fx_mix = 0; sys.fx_model = 0;
        check("every oscillator and FX model over the whole MIDI range, fast retriggers: in bounds", okk);
    }

    /* every sound parameter at random, every block, over the whole MIDI range (presets and ALGORITHM turned fast):
     * in bounds (the sanitizers), bounded */
    {
        uint32_t okk = 1, b, j, r = 0x13579BDFu;
        static volatile uint8_t *const P[] = {&sys.chaos, &sys.shape, &sys.output, &sys.ring, &sys.np_x, &sys.np_y,
            &sys.np_level, &sys.menv_len, &sys.menv_skew, &sys.menv_bend,
            &sys.menv_height, &sys.fx_struct, &sys.fx_bright, &sys.fx_damp, &sys.fx_pos, &sys.fx_mix, &sys.sh_size,
            &sys.sh_decay, &sys.sh_shimmer, &sys.sh_mix, &sys.sh_tone, &sys.basis, &sys.orbit, &sys.attack,
            &sys.decay, &sys.sustain, &sys.release};
        synth_kill();
        for (b = 0; b < 20000u; b++) {
            uint32_t q;
            for (q = 0; q < 3u; q++) {
                r = r * 1664525u + 1013904223u;
                *P[(r >> 8) % (sizeof P / sizeof P[0])] = (uint8_t)((r >> 20) & 127u);
            }
            r = r * 1664525u + 1013904223u;
            if ((r >> 28) == 0u) sys.osc = (uint8_t)((r >> 8) % (NP_COUNT + 1u));
            if ((r >> 28) == 1u) sys.fx_model = (uint8_t)((r >> 8) % RZ_COUNT);
            if ((r >> 28) == 2u) sys.menv_height = (uint8_t)((r >> 8) & 127u);
            if ((r >> 28) == 3u) sys.sh_interval = (uint8_t)((r >> 8) % SI_COUNT);
            if ((r >> 28) == 7u) sys.sh_space = (uint8_t)((r >> 8) % SS_COUNT);
            if ((r >> 28) == 4u) synth_note_on((r >> 8) & 127u, 1u + ((r >> 16) & 126u));
            if ((r >> 28) == 5u) synth_note_off((r >> 8) & 127u);
            if ((r >> 28) == 6u) { sys.osc = (uint8_t)((r >> 8) % (NP_COUNT + 1u)); sys.fx_model = (uint8_t)((r >> 16) % RZ_COUNT); }
            block();
            for (j = 0; j < 2u * CTL; j++)
                if (buf[j] > 8388607 || buf[j] < -8388608)
                    okk = 0;
        }
        synth_kill();
        sys.chaos = 85; sys.shape = 76; sys.output = 0; sys.ring = 50; sys.attack = 6; sys.decay = 80; sys.sustain = 110;
        sys.release = 70; sys.fx_model = 0; sys.fx_struct = 70; sys.fx_bright = 90; sys.fx_damp = 100; sys.fx_pos = 24;
        sys.sh_size = 100; sys.sh_decay = 90; sys.sh_shimmer = 70; sys.sh_interval = 0; sys.sh_tone = 80; sys.sh_space = 0;
        sys.menv_len = 80; sys.menv_skew = 10; sys.menv_bend = 64;
        sys.fx_mix = 0; sys.sh_mix = 0; sys.basis = 0; sys.orbit = 0; sys.menv_height = 0;
        sys.np_x = sys.np_y = sys.np_level = 64; sys.osc = 0;
        check("every sound parameter at random every block, presets, 0..127 notes: in bounds", okk);
    }

    /* BASIS's seats: notes one after another cycle through the voices and, apart from them, through the twelve seats
     * (an orchestra's players in turn); a note held keeps its voice and seat */
    {
        uint32_t okk = 1, j;
        synth_kill();
        for (j = 0; j < 9u; j++) {
            synth_note_on(60u + j, 100);
            okk &= voice[j % 4u].note == 60u + j && voice[j % 4u].pan == SEAT_PAN[j % NSEAT];
            synth_note_off(60u + j);
            voice[j % 4u].stage = ST_OFF;                /* (its tail over) */
        }
        synth_note_on(48, 100);                          /* voice 1 (after voice 0, the 9th): held */
        synth_note_on(50, 100);                          /* voice 2 */
        okk &= voice[1].note == 48u && voice[2].note == 50u && voice[2].pan == SEAT_PAN[10] && voice[1].pan == SEAT_PAN[9];
        synth_kill();
        check("BASIS: the notes cycle through the voices and the seats", okk);
    }

    /* the CPU governor: a half over budget lets one voice go, the least heard (a tail before a held note) */
    {
        uint32_t okk = 1;
        synth_kill();
        sys.attack = 0; sys.sustain = 127; sys.release = 60;
        synth_note_on(48, 100); synth_note_on(52, 100); synth_note_on(55, 100); synth_note_on(59, 100);
        for (k = 0; k < 100u; k++)
            synth_render(buf, CTL);
        synth_note_off(52);
        synth_render(buf, CTL);
        synth_shed = 1;
        synth_render(buf, CTL);
        okk &= voice[1].stage == ST_OFF && voice[0].stage != ST_OFF && voice[2].stage != ST_OFF && !synth_shed;
        if (!okk)
            printf("   (governor 1: stages %u %u %u %u, notes %u %u)\n", voice[0].stage, voice[1].stage, voice[2].stage,
                   voice[3].stage, voice[0].note, voice[1].note);
        synth_shed = 1;                                  /* held notes only: three of them, the oldest goes */
        synth_render(buf, CTL);
        synth_render(buf, CTL);
        okk &= voice[0].stage == ST_OFF && voice[2].stage != ST_OFF;
        if (!okk)
            printf("   (governor: stages %u %u %u %u)\n", voice[0].stage, voice[1].stage, voice[2].stage, voice[3].stage);
        synth_kill();
        check("CPU governor: over budget, the least heard voice goes", okk);
    }

    /* the MOD ENV's BEND: 64 straight (bit for bit), ends kept (0 and full), monotonic, + below the line (exponential),
     * - above it (logarithmic), the most at the ends of the knob */
    {
        uint32_t okk = 1, bd;
        int32_t x;
        for (x = 0; x <= 32768; x += 512)
            okk &= np_bend(x, 64) == x;
        for (bd = 0; bd < 128u; bd += 9u) {
            int32_t prev = -1;
            okk &= np_bend(0, bd) == 0 && np_bend(32768, bd) == 32768;
            for (x = 0; x <= 32768; x += 256) {
                int32_t y = np_bend(x, bd);
                okk &= y >= prev && y >= 0 && y <= 32768;
                prev = y;
                if (x > 0 && x < 32768)
                    okk &= bd > 64u ? y <= x : bd < 64u ? y >= x : y == x;
            }
        }
        okk &= np_bend(16384, 127) < 5000 && np_bend(16384, 0) > 28000;   /* (x^3: 1/8; 1 - (1/2)^3: 7/8) */
        check("MOD ENV BEND: straight at 64, ends kept, monotonic, both ways", okk);
    }

    /* the MOD ENV on the pitch: at HEIGHT 0 its other knobs change nothing, bit for bit; at full HEIGHT every program
     * changes, sounds and stays bounded without DC; a falling AD's effect is gone once it has run */
    {
        static const uint8_t CH[3] = {48, 60, 67};
        static int32_t ref[2 * CTL * 150];
        uint32_t a, wv, j, b, okk = 1, same = 1, moved = 1;
        sys.attack = 0; sys.decay = 64; sys.sustain = 127; sys.release = 20; sys.ring = 50;
        sys.fx_mix = 0; sys.sh_mix = 0; sys.np_level = 64; sys.np_x = sys.np_y = 64;
        for (a = 0; a < NP_COUNT; a++) {
            for (wv = 0; wv < 4u; wv++) {
                double e = 0, dc = 0;
                int32_t pk = 0;
                uint32_t diff = 0;
                synth_kill(); memset(voice, 0, sizeof voice); memset(np_state, 0, sizeof np_state); rng_state = 0x1234567u;
                sys.osc = (uint8_t)(a + 1u);
                sys.menv_height = wv < 2u ? 0 : 127;             /* 0, 1: off (the other knobs moved); 2, 3: on */
                sys.menv_len = (uint8_t)(wv == 1u ? 10 : 90);
                sys.menv_skew = (uint8_t)(wv == 3u ? 100 : wv == 1u ? 127 : 0);
                sys.menv_bend = (uint8_t)(wv == 1u ? 0 : 64);
                for (j = 0; j < 3u; j++)
                    synth_note_on(CH[j], 110);
                for (b = 0; b < 150u; b++) {
                    synth_render(buf, CTL);
                    for (j = 0; j < CTL; j++) {
                        int32_t v = buf[2 * j], av = v < 0 ? -v : v;
                        if (wv == 0u) ref[b * 2u * CTL + j] = v;
                        else if (wv == 1u) same &= ref[b * 2u * CTL + j] == v;
                        else diff += ref[b * 2u * CTL + j] != v;
                        if (b >= 50u) { e += (double)v * v; dc += v; pk = av > pk ? av : pk; }
                    }
                }
                e = sqrt(e / (100.0 * CTL));
                dc /= 100.0 * CTL;
                if (wv >= 2u && diff < CTL * 10u)
                    moved = 0, printf("   MOD ENV: %s %u changes only %u samples\n", NP_NAME[a], wv, diff);
                if (e < 50 || fabs(dc) > 3000 || pk > 32767) {
                    printf("   MOD ENV: %s %u: RMS %.0f DC %.0f peak %d\n", NP_NAME[a], wv, e, dc, pk);
                    okk = 0;
                }
            }
        }
        {   /* a falling AD: its effect early, none late */
            double ed[2] = {0, 0};
            uint32_t ph2;
            synth_kill(); memset(voice, 0, sizeof voice);
            sys.osc = 4; sys.menv_height = 127; sys.menv_len = 60; sys.menv_skew = 0; sys.menv_bend = 64;
            synth_note_on(60, 110);
            for (ph2 = 0; ph2 < 2u; ph2++) {
                for (b = 0; b < 100u; b++) {
                    np_t keep;
                    int32_t o1[CTL], o2[CTL];
                    keep = np_state[0];
                    np_render(0, 3, voice[0].inc, o1, CTL, 0);
                    np_state[0] = keep;
                    sys.menv_height = 0;
                    np_render(0, 3, voice[0].inc, o2, CTL, 0);
                    sys.menv_height = 127;
                    np_state[0] = keep;
                    synth_render(buf, CTL);
                    for (j = 0; j < CTL; j++)
                        ed[ph2] += fabs((double)o1[j] - o2[j]);
                }
                run_ms(600);
            }
            printf("   (MOD ENV: its effect early / late: %.0f / %.0f)\n", ed[0], ed[1]);
            okk &= ed[0] > 0 && ed[1] < ed[0] / 20;
        }
        sys.menv_len = 80; sys.menv_skew = 10; sys.menv_height = 0; sys.menv_bend = 64;
        check("MOD ENV: HEIGHT 0 bit for bit off", same);
        {   /* HEIGHT both ways: up a rise of the pitch (more crossings early), down (bit 7) a fall below the note */
            uint32_t zc[3], w3, q3;
            for (w3 = 0; w3 < 3u; w3++) {
                int32_t last = 0;
                synth_kill(); memset(np_state, 0, sizeof np_state); rng_state = 0x1234567u;
                sys.osc = NP_BIGBANG + 1u; sys.np_x = 0; sys.np_y = 0; sys.chaos = 0; sys.output = 0;
                sys.menv_len = 90; sys.menv_skew = 0; sys.menv_bend = 64;
                sys.menv_height = (uint8_t)(w3 == 0u ? 0u : w3 == 1u ? 100u : 0x80u | 100u);
                synth_note_on(57, 110);
                zc[w3] = 0;
                for (b = 0; b < 100u; b++) {
                    synth_render(buf, CTL);
                    for (q3 = 0; q3 < CTL; q3++) {
                        if ((buf[2 * q3] >= 0) != (last >= 0)) zc[w3]++;
                        last = buf[2 * q3];
                    }
                }
            }
            printf("   (MOD ENV HEIGHT: crossings in 70 ms: off %u, +100 %u, -100 %u)\n", zc[0], zc[1], zc[2]);
            check("MOD ENV HEIGHT: up and down (bit 7) from the note", zc[1] > zc[0] * 3 / 2 && zc[2] < zc[0] * 3 / 4);
            sys.menv_height = 0; sys.osc = 0; synth_kill();
        }
        check("... full HEIGHT: each program changes, sounds, bounded, no DC; a falling AD runs out", okk && moved);
        synth_kill();
        sys.osc = 0;
    }

    /* BASIS: at 0 left = right, bit for bit; at 127 the notes take the seats in turn (the far side turned down by the
     * seat's place), the resonator's ring with them */
    {
        uint32_t fx, j, b, okk = 1, mono = 1;
        for (fx = 0; fx < 2u; fx++) {
            double e[4][2];
            sys.fx_mix = fx ? 110 : 0; sys.fx_model = RZ_MODAL; sys.fx_damp = 60; sys.sh_mix = 0;
            sys.osc = 0; sys.attack = 0; sys.sustain = 127; sys.release = 10; sys.chaos = 85;
            for (b = 0; b < 2u; b++) {                       /* 0: BASIS 0, 1: BASIS 127 */
                synth_kill(); run_ms(1500);
                sys.basis = b ? 127 : 0;
                voice_rr = 0;
                seat_next = 0;
                for (j = 0; j < 4u; j++) {
                    uint32_t q;
                    e[j][0] = e[j][1] = 0;
                    synth_note_on(48 + 7 * j, 110);
                    for (q = 0; q < 200u; q++) {
                        uint32_t w2;
                        synth_render(buf, CTL);
                        for (w2 = 0; w2 < CTL; w2++) {
                            if (!b) mono &= buf[2 * w2] == buf[2 * w2 + 1];
                            e[j][0] += (double)buf[2 * w2] * buf[2 * w2];
                            e[j][1] += (double)buf[2 * w2 + 1] * buf[2 * w2 + 1];
                        }
                    }
                    synth_note_off(48 + 7 * j);
                    run_ms(fx ? 3000 : 400);
                }
            }
            printf("   (BASIS 127%s: L/R energy per note %.3f %.3f %.3f %.3f)\n", fx ? ", resonator" : "",
                   e[0][0] / (e[0][1] + 1), e[1][0] / (e[1][1] + 1), e[2][0] / (e[2][1] + 1), e[3][0] / (e[3][1] + 1));
            for (j = 0; j < 4u; j++) {           /* the far side's energy: (1 - |seat|)^2 of the near side's */
                double p = SEAT_PAN[j] / 32767.0, want = (1 - fabs(p)) * (1 - fabs(p)), got;
                got = p > 0 ? e[j][0] / (e[j][1] + 1) : e[j][1] / (e[j][0] + 1);
                okk &= fabs(got - want) < 0.15 + want * 0.3;
            }
        }
        {   /* ORBIT, BASIS 0: the voice's phase moves with the double scroll (all-passes): a side signal that comes and
             * goes with the chaos, the middle the voice itself; the circuit on one scroll (SPIRAL) still has a side */
            uint32_t q, w2, side = 0;
            double smin = 1e30, smax = 0, mid = 0;
            synth_kill(); run_ms(500);
            sys.fx_mix = 0; sys.basis = 0; sys.orbit = 127; sys.chaos = 85; sys.shape = 76;
            synth_note_on(48, 110);
            run_ms(200);
            for (q = 0; q < 60u; q++) {                       /* ~2.6 s in windows of ~44 ms */
                double es = 0, em = 0;
                for (w2 = 0; w2 < 60u; w2++) {
                    uint32_t j;
                    synth_render(buf, CTL);
                    for (j = 0; j < CTL; j++) {
                        double l = buf[2 * j], r = buf[2 * j + 1];
                        es += (l - r) * (l - r);
                        em += (l + r) * (l + r);
                    }
                }
                smin = es < smin ? es : smin;
                smax = es > smax ? es : smax;
                mid += em;
            }
            printf("   (ORBIT, double scroll: side energy %.2g .. %.2g, mid %.2g)\n", smin, smax, mid / 60.0);
            okk &= smax > mid / 60.0 * 0.01 && smax > smin * 2.0;
            synth_kill(); run_ms(500);
            sys.chaos = 54;                                   /* SPIRAL: one scroll, one side */
            synth_note_on(48, 110);
            run_ms(1000);
            for (q = 0; q < 300u; q++) {
                synth_render(buf, CTL);
                for (w2 = 0; w2 < CTL; w2++)
                    side |= (uint32_t)(buf[2 * w2] != buf[2 * w2 + 1]);
            }
            okk &= side;
            synth_kill();
        }
        synth_kill();
        sys.basis = 0; sys.fx_mix = 0; sys.orbit = 0; sys.chaos = 85;
        check("BASIS: 0 mono bit for bit; 127 the seats in turn (resonator too); ORBIT: the harmonics part, moving with the scroll", okk && mono);
    }

    /* FREE EG: 1 BAR at 120 BPM (2 s) records a ramp of the knob and plays it onto CHAOS, looping; DEPTH scales it
     * around the value it had; PLAY off gives it back; NOTE records until stopped and starts again at each note;
     * under MIDI clock the cycle follows the clocks */
    {
        uint32_t okk = 1, b, L = 2u * FS, t;
        int32_t c0, c1, c2;
        synth_kill(); keys_all_off(); run_ms(20);
        sys.arp_on = 0; sys.arp_bpm = 120; sys.chaos = 85;
        sys.feg_len = 9; sys.feg_target = 0; sys.feg_depth = 127; sys.feg_on = 0; sys.feg_val = 0;
        feg_req = 1;
        for (t = 0; t < L + 20u * CTL; t += CTL) {           /* the knob turned up over the bar */
            sys.feg_val = (uint8_t)(t >= L ? 127u : t * 127u / L);
            block();
        }
        okk &= !feg.rec && sys.feg_has;                      /* (stopped by itself after one bar) */
        sys.chaos = 85;
        sys.feg_on = 1;
        run_ms(10);
        okk &= feg.base == 85u;
        for (b = 0; b < 3u; b++) {                           /* the ramp, three times */
            uint32_t q;
            while (feg.pos != 0u) block();
            c0 = sys.chaos;
            for (q = 0; q < L / 2u / CTL; q++) block();
            c1 = sys.chaos;
            for (q = 0; q < L * 9u / 20u / CTL; q++) block();
            c2 = sys.chaos;
            okk &= c0 < 12 && c1 > 50 && c1 < 78 && c2 > 105;
        }
        printf("   (FREE EG 1 BAR: CHAOS %d .. %d .. %d)\n", c0, c1, c2);
        sys.feg_depth = 0;                                   /* DEPTH 0: as it was */
        run_ms(100);
        okk &= sys.chaos == 85;
        sys.feg_depth = 127;
        sys.feg_on = 0;                                      /* off: given back */
        run_ms(20);
        okk &= sys.chaos == 85;
        /* NOTE: 0 for ~0.5 s, then 127; stopped; each note from the start */
        sys.feg_len = 0; sys.feg_val = 0;
        feg_req = 1;
        run_ms(500);
        sys.feg_val = 127;
        run_ms(500);
        feg_req = 2;
        run_ms(10);
        okk &= sys.feg_has && sys.feg_npts > 18u && sys.feg_npts < 24u;   /* (~1 s: 21 points of 46 ms) */
        sys.feg_on = 1;
        synth_note_on(60, 100);
        run_ms(100);
        okk &= sys.chaos < 10;
        run_ms(700);
        okk &= sys.chaos > 115;
        run_ms(1500);
        okk &= sys.chaos > 115;
        synth_note_on(62, 100);                              /* a note: from the start again */
        run_ms(100);
        okk &= sys.chaos < 10;
        sys.feg_on = 0;
        synth_kill();
        run_ms(20);
        /* MIDI clock: 1 BAR = 96 clocks; recorded under it, it stops after 96 */
        sys.feg_len = 9; sys.feg_val = 30;
        midi_in(0xFA, 0, 0);
        midi_in(0xF8, 0, 0);
        run_ms(20);
        feg_req = 1;
        for (b = 0; b < 100u; b++) {
            sys.feg_val = (uint8_t)(b < 48u ? 20u : 100u);
            midi_in(0xF8, 0, 0);
            run_ms(20);
        }
        okk &= !feg.rec && feg.curve[10] == 20u && feg.curve[100] == 100u;
        sys.chaos = 85;
        check("FREE EG: a bar recorded and looped, DEPTH, off, NOTE from each note, under MIDI clock", okk);
        sys.feg_has = 0; sys.chaos = 85;
        run_ms(700);                                         /* (the clock gone) */
    }

    /* the MIDI page (GLO): TRS in off ignores the jack, USB in off ignores USB; RX channel filters (OMNI: all);
     * the keys go out on the TX channel, and not at all with USB out off (their note offs always) */
    {
        uint32_t okk = 1, q;
        synth_kill(); keys_all_off(); run_ms(20);
        sys.arp_on = 0; sys.osc = 0; sys.fx_mix = 0; sys.sh_mix = 0;
        sys.midi_trs = 0;
        midi_in_source[mi_w % MQ] = 2; midi_in(0x90, 60, 100);       /* TRS, off */
        run_ms(10);
        okk &= sounding() == 0;
        sys.midi_trs = 1;
        midi_in_source[mi_w % MQ] = 2; midi_in(0x90, 60, 100);       /* TRS, on */
        run_ms(10);
        okk &= sounding() == 1;
        midi_in_source[mi_w % MQ] = 2; midi_in(0x80, 60, 0);
        run_ms(500);
        sys.midi_usb = 2;                                            /* USB in off */
        midi_in_source[mi_w % MQ] = 1; midi_in(0x90, 62, 100);
        run_ms(10);
        okk &= sounding() == 0;
        sys.midi_usb = 3; sys.midi_rx = 5;                           /* channel 5 only */
        midi_in_source[mi_w % MQ] = 1; midi_in(0x90, 62, 100);       /* channel 1: not */
        run_ms(10);
        okk &= sounding() == 0;
        midi_in_source[mi_w % MQ] = 1; midi_in(0x94, 62, 100);       /* channel 5 */
        run_ms(10);
        okk &= sounding() == 1;
        midi_in_source[mi_w % MQ] = 1; midi_in(0x84, 62, 0);
        sys.midi_rx = 0;
        run_ms(500);
        out_n = 0; sys.midi_tx = 9;                                  /* the keys out on channel 10 */
        fm1_in.notes = 1u << key_of(60);
        run_ms(10);
        okk &= sent(0, 0x99, 60, KEY_VEL);
        sys.midi_tx = 0;                                             /* (changed while held: the off on 10) */
        fm1_in.notes = 0;
        run_ms(10);
        okk &= sent(1, 0x89, 60, 0);
        out_n = 0; sys.midi_usb = 1;                                 /* USB out off: no note on goes out */
        fm1_in.notes = 1u << key_of(60);
        run_ms(10);
        q = out_n;
        fm1_in.notes = 0;
        run_ms(10);
        okk &= q == 0;
        sys.midi_usb = 3;
        run_ms(500);
        check("MIDI page: TRS / USB in on and off, RX channel, TX channel, USB out off", okk);
    }
    {   /* the mod wheel (CC 1) moves the EDIT page's parameter and gives it back; program change asks for a preset */
        int okk = 1;
        uint8_t base;
        sys.mw_target = 8; sys.mw_depth = 127; sys.fx_struct = 20;
        base = sys.fx_struct;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 127);
        run_ms(10);
        okk &= sys.fx_struct == 127;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 64);
        run_ms(10);
        okk &= sys.fx_struct > base && sys.fx_struct < 127;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 0);
        run_ms(10);
        okk &= sys.fx_struct == base;
        sys.mw_depth = 0;                                            /* depth 0: the wheel does nothing */
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 127);
        run_ms(10);
        okk &= sys.fx_struct == base;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 0);
        sys.mw_depth = 127;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xC0, 3, 0);
        run_ms(10);
        okk &= keys_prog_req == 4;
        keys_prog_req = 0;
        check("mod wheel moves FX STRUCTURE and gives it back; program change asks for preset 4", okk);
    }
    {   /* FREE EG playing the wheel's parameter: the wheel sets FREE EG's depth (down: still at the base) */
        int okk = 1, k;
        uint8_t base = 30;
        for (k = 0; k < FEG_N; k++)
            feg.curve[k] = 120;
        sys.fx_struct = base; sys.feg_target = 8; sys.feg_depth = 127; sys.feg_len = 9;
        sys.feg_has = 1; sys.feg_on = 1; sys.mw_target = 8; sys.mw_depth = 127;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 0);
        run_ms(20);
        okk &= feg.playing && sys.fx_struct == base;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 127);
        run_ms(20);
        okk &= sys.fx_struct == 120;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 64);
        run_ms(20);
        okk &= sys.fx_struct > base + 30 && sys.fx_struct < 90;
        midi_in_source[mi_w % MQ] = 1; midi_in(0xB0, 1, 0);
        sys.feg_on = 0;
        run_ms(20);
        okk &= !feg.playing && sys.fx_struct == base;
        if (!okk) printf("   (fx_struct %u playing %u)\n", sys.fx_struct, feg.playing);
        check("mod wheel on FREE EG's parameter sets FREE EG's depth", okk);
    }
    {   /* FREE EG LFO 4 HZ: a sine-like curve loops on its own (CHAOS goes up and down 4 times a second); AUDIO X2:
         * the voice's level moved at twice the note, every sample: sidebands at the note +- 2 x the note (here the
         * third harmonic grows) */
        int okk = 1, k, ups = 0, last = -1, dir = 0;
        uint32_t b2, w3;
        for (k = 0; k < FEG_N; k++)
            feg.curve[k] = (uint8_t)(64 + 60 * sin(2 * M_PI * k / FEG_N));
        sys.chaos = 64; sys.feg_target = 0; sys.feg_depth = 127; sys.feg_len = FEG_LFO0 + 5u;   /* 4 Hz */
        sys.feg_has = 1; sys.feg_on = 1;
        for (b2 = 0; b2 < FS / CTL; b2++) {
            block();
            if (last >= 0 && sys.chaos != last) {
                int d = sys.chaos > last ? 1 : -1;
                if (d > 0 && dir < 0) ups++;
                dir = d;
            }
            last = sys.chaos;
        }
        okk &= ups >= 3 && ups <= 5;
        sys.feg_on = 0; run_ms(10);
        {   /* AUDIO X2 on a pure cycle: the third harmonic against the fundamental, off and on */
            double h[2];
            uint32_t q2;
            static int32_t ob[8192];
            for (q2 = 0; q2 < 2u; q2++) {
                double r1 = 0, i1 = 0, r3 = 0, i3 = 0;
                synth_kill(); sys.osc = 0; sys.chaos = 6; sys.shape = 64; sys.output = 64; sys.attack = 0;
                sys.sustain = 127; sys.ring = 0; sys.fx_mix = 0; sys.sh_mix = 0;
                for (k = 0; k < FEG_N; k++)
                    feg.curve[k] = (uint8_t)(64 + 63 * sin(2 * M_PI * k / FEG_N));
                sys.feg_len = FEG_AUD0 + 2u; sys.feg_depth = 127; sys.feg_on = (uint8_t)q2;
                synth_note_on(57, 100);
                run_ms(200);
                for (w3 = 0; w3 < 8192u / CTL; w3++) { block(); for (b2 = 0; b2 < CTL; b2++) ob[w3 * CTL + b2] = buf[2 * b2]; }
                for (b2 = 0; b2 < 8192u; b2++) {
                    double ph = 2 * M_PI * 220.0 * b2 / FS;
                    r1 += ob[b2] * cos(ph); i1 += ob[b2] * sin(ph); r3 += ob[b2] * cos(3 * ph); i3 += ob[b2] * sin(3 * ph);
                }
                h[q2] = (r3 * r3 + i3 * i3) / (r1 * r1 + i1 * i1 + 1);
            }
            printf("   (FREE EG AUDIO X2: 3rd harmonic / fundamental %.4f off, %.4f on)\n", h[0], h[1]);
            okk &= h[1] > 10 * h[0] + 0.01;
        }
        {   /* FM X2: tonal on the note through most of DEPTH, noise only near the top */
            double sh[2];
            uint32_t q2, hk;
            static int32_t ob[8192];
            static const uint8_t dd[2] = { 96, 127 };
            for (q2 = 0; q2 < 2u; q2++) {
                double tot = 0, hs = 0;
                synth_kill(); sys.osc = 0; sys.chaos = 6; sys.shape = 64; sys.output = 64; sys.attack = 0;
                sys.sustain = 127; sys.ring = 0; sys.fx_mix = 0; sys.sh_mix = 0;
                for (k = 0; k < FEG_N; k++)
                    feg.curve[k] = (uint8_t)(64 + 63 * sin(2 * M_PI * k / FEG_N));
                sys.feg_len = FEG_FM0 + 2u; sys.feg_depth = dd[q2]; sys.feg_on = 1;
                synth_note_on(57, 100);
                run_ms(200);
                for (w3 = 0; w3 < 8192u / CTL; w3++) { block(); for (b2 = 0; b2 < CTL; b2++) ob[w3 * CTL + b2] = buf[2 * b2]; }
                for (b2 = 0; b2 < 8192u; b2++) {
                    double wv = 0.5 - 0.5 * cos(2 * M_PI * b2 / 8192.0);
                    tot += (double)ob[b2] * ob[b2] * wv * wv;
                }
                for (hk = 1; hk <= 40u; hk++) {
                    double rr = 0, ii = 0;
                    for (b2 = 0; b2 < 8192u; b2++) {
                        double wv = 0.5 - 0.5 * cos(2 * M_PI * b2 / 8192.0), ph = 2 * M_PI * 220.0 * hk * b2 / FS;
                        rr += ob[b2] * wv * cos(ph); ii += ob[b2] * wv * sin(ph);
                    }
                    hs += (rr * rr + ii * ii) * 2 / 8192.0 / 0.375;
                }
                sh[q2] = hs / (tot + 1);
            }
            printf("   (FREE EG FM X2: on the note's harmonics %.2f at DEPTH 96, %.2f at 127)\n", sh[0], sh[1]);
            okk &= sh[0] > 0.5 && sh[1] < 0.5 * sh[0];
        }
        sys.feg_on = 0; sys.feg_len = 9; synth_kill(); run_ms(10);
        check("FREE EG loops on its own (LFO) and at audio rate (AUDIO: sidebands; FM: tonal, noise at the top)", okk);
    }
    {   /* SHIMMER's SPACEs: each rings on after a note, bounded, unlike the plain HALL */
        int okk = 1;
        uint32_t sp, b, j;
        double e[SS_COUNT];
        int32_t pk = 0;
        for (sp = 0; sp < SS_COUNT; sp++) {
            synth_kill(); keys_all_off(); run_ms(200);
            sys.sh_mix = 100; sys.sh_size = 110; sys.sh_decay = 110; sys.sh_shimmer = 0; sys.sh_tone = 80;
            sys.sh_space = (uint8_t)sp; sys.fx_mix = 0;
            synth_note_on(48, 120);
            run_ms(300);
            synth_note_off(48);
            run_ms(200);
            e[sp] = 0;
            for (b = 0; b < 300u; b++) {
                block();
                for (j = 0; j < 2u * CTL; j++) {
                    e[sp] += (double)buf[j] * buf[j];
                    pk = buf[j] > pk ? buf[j] : -buf[j] > pk ? -buf[j] : pk;
                }
            }
            okk &= e[sp] > 1e6;
        }
        okk &= pk < 32767;
        for (sp = 1; sp < SS_COUNT; sp++)
            okk &= fabs(e[sp] - e[0]) > e[0] * 0.02;
        printf("   (spaces' tails: HALL %.3g TEMPLE %.3g CATHEDRAL %.3g MOSQUE %.3g, peak %d)\n", e[0], e[1], e[2], e[3], pk);
        sys.sh_mix = 0; sys.sh_space = 0; sys.sh_shimmer = 70; sys.sh_size = 100; sys.sh_decay = 90;
        synth_kill(); run_ms(200);
        check("SHIMMER SPACEs: each rings on, bounded, unlike the HALL", okk);
    }
    {   /* the bodies: an impulse through each body's modes rings its lowest (A0, the air in the box ...) well
         * above a frequency between its modes */
        static const uint16_t F0[8] = {92, 100, 275, 100, 90, 150, 233, 55}, FX[8] = {116, 150, 340, 122, 135, 250, 290, 80};
        int okk = 1;
        uint32_t sp, i;
        static int32_t wv[8192];
        for (sp = SS_BODY; sp < SS_LESLIE; sp++) {
            double r0 = 0, i0 = 0, r1 = 0, i1 = 0, a0, a1;
            int32_t bm, bs;
            memset(&sh, 0, sizeof sh);
            for (i = 0; i < 8192u; i++) {
                sh_body(sp, i == 0 ? 30000 : 0, &bm, &bs);
                wv[i] = bm;
            }
            for (i = 0; i < 8192u; i++) {
                double p0 = 2 * M_PI * F0[sp - SS_BODY] * i / FS, p1 = 2 * M_PI * FX[sp - SS_BODY] * i / FS;
                r0 += wv[i] * cos(p0); i0 += wv[i] * sin(p0); r1 += wv[i] * cos(p1); i1 += wv[i] * sin(p1);
            }
            a0 = sqrt(r0 * r0 + i0 * i0); a1 = sqrt(r1 * r1 + i1 * i1);
            printf("   (%s: %u Hz %.0f, %u Hz %.0f)\n", SH_SPACE_NAME[sp], F0[sp - SS_BODY], a0, FX[sp - SS_BODY], a1);
            okk &= a0 > 1.5 * a1;
        }
        memset(&sh, 0, sizeof sh);
        sys.sh_mix = 0; sys.sh_space = 0;
        check("SHIMMER bodies (PIANO, GUITAR, VIOLIN, CELLO, HARP, ACCORDION, BAGPIPE, PIPE ORGAN): their modes ring", okk);
    }
    {   /* the DELAYS: PING PONG 1/8 at 120 BPM: an impulse comes back left at 250 ms, right at 500 ms, left again at
         * 750 ms (fed back), softer; BOUNCE BALL: its bounces come sooner and sooner; every pattern bounded */
        static int32_t xin[CTL], ol[CTL], orr[CTL];
        uint32_t b2, i, t, best[2] = {0, 0}, okk = 1, p2;
        double el[4] = {0, 0, 0, 0}, er[4] = {0, 0, 0, 0};
        uint8_t keep[8] = {sys.arp_bpm, sys.sh_size, sys.sh_decay, sys.sh_shimmer, sys.sh_tone, sys.sh_mix, sys.sh_interval, sys.sh_space};
        memset(&sh, 0, sizeof sh);
        sys.arp_bpm = 120; sys.sh_space = SS_PINGPONG; sys.sh_interval = 0; sys.sh_size = 64; sys.sh_decay = 80;
        sys.sh_shimmer = 127; sys.sh_tone = 0; sys.sh_mix = 64;
        for (b2 = 0, t = 0; b2 < FS / CTL; b2++) {
            for (i = 0; i < CTL; i++, t++)
                xin[i] = t < 64u ? 20000 : 0;
            fx_shimmer(xin, ol, orr, CTL);
            for (i = 0; i < CTL; i++) {
                uint32_t tt = b2 * CTL + i, w = tt < FS / 8u ? 9u : (tt - FS / 8u) / (FS / 4u);   /* 125..375 375..625 .. */
                if (w < 4u) { el[w] += (double)ol[i] * ol[i]; er[w] += (double)orr[i] * orr[i]; }
                (void)best;
            }
        }
        printf("   (PING PONG L/R energy around 250, 500, 750 ms: %.2g/%.2g %.2g/%.2g %.2g/%.2g)\n", el[0], er[0], el[1], er[1], el[2], er[2]);
        okk &= el[0] > 20 * er[0] && er[1] > 20 * el[1] && el[2] > 20 * er[2] && el[2] < el[0] && el[0] > 1e6;
        for (p2 = 0; p2 < 12u; p2++) {           /* every pattern, every step: bounded, no DC left */
            int32_t pk = 0;
            memset(&sh, 0, sizeof sh);
            sys.sh_space = (uint8_t)(SS_DELAY + p2 / 4u); sys.sh_interval = (uint8_t)(p2 % 4u);
            sys.sh_size = (uint8_t)(p2 * 11u); sys.sh_decay = 127; sys.sh_shimmer = (uint8_t)(p2 * 10u); sys.sh_tone = 127;
            for (b2 = 0, t = 0; b2 < 3u * FS / CTL; b2++) {
                for (i = 0; i < CTL; i++, t++)
                    xin[i] = (int32_t)((t * 2654435761u) >> 17) - 16384;   /* (noise, loud) */
                fx_shimmer(xin, ol, orr, CTL);
                for (i = 0; i < CTL; i++) {
                    int32_t a2 = ol[i] < 0 ? -ol[i] : ol[i];
                    pk = a2 > pk ? a2 : pk;
                }
            }
            if (pk > 120000) { printf("   (delay %s %s: peak %d)\n", SH_SPACE_NAME[sys.sh_space], sh_iv_name(sys.sh_space, sys.sh_interval), pk); okk = 0; }
        }
        memset(&sh, 0, sizeof sh);
        sys.arp_bpm = keep[0]; sys.sh_size = keep[1]; sys.sh_decay = keep[2]; sys.sh_shimmer = keep[3]; sys.sh_tone = keep[4];
        sys.sh_mix = keep[5]; sys.sh_interval = keep[6]; sys.sh_space = keep[7];
        check("DELAYS: PING PONG left, right, fed back, in the tempo; every pattern bounded", okk);
    }
    {   /* PIANO's hammer: with the voices all but silent the note still sounds (the hammer strikes it), louder and
         * brighter (more above 2 kHz) the harder the key is struck */
        double pk[2], hi[2];
        uint32_t q, w, b, k;
        int okk = 1;
        static int32_t ob[8192];
        for (q = 0; q < 2u; q++) {
            double tot = 0, h = 0, p = 0;
            synth_kill(); keys_all_off(); sys.pedal = 0; sys.transpose = 0; sys.fx_mix = 0; run_ms(50);   /* (the
                                                  * resonators cleared) */
            sys.osc = 19; sys.np_x = 0; sys.np_y = 63; sys.np_level = 1; sys.attack = 0; sys.decay = 50; sys.sustain = 0;
            sys.release = 50; sys.ring = 10; sys.fx_model = RZ_PIANO; sys.fx_struct = 40; sys.fx_bright = 80;
            sys.fx_damp = 70; sys.fx_pos = 40; sys.fx_mix = 127; sys.sh_mix = 0; sys.feg_on = 0;
            synth_note_on(60, q ? 127 : 30);
            for (w = 0; w < 8192u / CTL; w++) { block(); for (b = 0; b < CTL; b++) ob[w * CTL + b] = buf[2 * b]; }
            for (b = 0; b < 8192u; b++)
                p = fabs((double)ob[b]) > p ? fabs((double)ob[b]) : p;
            for (k = 2; k < 200u; k += 2) {      /* (a coarse spectrum: 2 .. 200 x 5.4 Hz steps over 1 kHz) */
                double rr = 0, ii = 0, f = 2000.0 * k / 100.0;
                for (b = 0; b < 8192u; b += 2) { rr += ob[b] * cos(2 * M_PI * f * b / FS); ii += ob[b] * sin(2 * M_PI * f * b / FS); }
                if (f > 2000.0) h += rr * rr + ii * ii;
                tot += rr * rr + ii * ii;
            }
            pk[q] = p; hi[q] = h / (tot + 1);
        }
        printf("   (PIANO hammer: peak %.0f / %.0f, above 2 kHz %.3f / %.3f at velocity 30 / 127)\n", pk[0], pk[1], hi[0], hi[1]);
        okk &= pk[0] > 300 && pk[1] > 4 * pk[0] && hi[1] > hi[0];
        synth_kill(); sys.fx_mix = 0; sys.osc = 0; sys.np_level = 64; run_ms(50);
        {   /* struck once: a voice held on, loud, does not keep the string going (no bowing): the note falls; its
             * partials stretched sharp (the stiffness: the 6th above 6 x the fundamental) */
            double e0 = 0, e1 = 0, rr, ii, best = 0, f6 = 0, f;
            synth_kill(); keys_all_off(); sys.fx_mix = 0; run_ms(50);
            sys.osc = 19; sys.np_x = 64; sys.np_y = 64; sys.np_level = 100; sys.attack = 0; sys.decay = 60; sys.sustain = 100;
            sys.release = 50; sys.fx_model = RZ_PIANO; sys.fx_struct = 0; sys.fx_mix = 127; sys.fx_damp = 64;
            synth_note_on(48, 100);
            for (w = 0; w < 8192u / CTL; w++) { block(); for (b = 0; b < CTL; b++) { ob[w * CTL + b] = buf[2 * b]; e0 += (double)buf[2 * b] * buf[2 * b]; } }
            run_ms(1500);
            for (w = 0; w < 8192u / CTL; w++) { block(); for (b = 0; b < CTL; b++) e1 += (double)buf[2 * b] * buf[2 * b]; }
            for (f = 6 * 130.81 * 0.995; f < 6 * 130.81 * 1.03; f += 0.25) {   /* (the 6th partial: where its peak is) */
                rr = ii = 0;
                for (b = 0; b < 8192u; b++) { rr += ob[b] * cos(2 * M_PI * f * b / FS); ii += ob[b] * sin(2 * M_PI * f * b / FS); }
                if (rr * rr + ii * ii > best) { best = rr * rr + ii * ii; f6 = f; }
            }
            printf("   (PIANO under a held voice: %.1f dB after 1.5 s; 6th partial %+.1f cents)\n", 10 * log10(e1 / (e0 + 1) + 1e-12),
                   1200 * log2(f6 / (6 * 130.81)));
            okk &= e1 < e0 / 8 && f6 > 6 * 130.81 * 1.0005;
            synth_note_off(48); run_ms(200); synth_kill();
        }
        check("PIANO: the hammer strikes (louder and brighter the harder), once (a held voice not bowing), stretched", okk);
    }
    {   /* BIG BANG: WAVE gives the standard waves (saw: the 2nd harmonic 6 dB under the 1st; square: no 2nd, the 3rd
         * 9.5 dB under), ENTROPY takes the series apart, BANG starts each note in disorder and cools */
        static int32_t ob[8192];
        double hs[3], h2[2], h3[2], early, late;
        uint32_t q, w, b, k;
        int okk = 1;
        static const uint8_t XS[3] = {85, 127, 85}, YS[3] = {0, 0, 110};
        for (q = 0; q < 3u; q++) {
            double hm = 0, tot = 0, a[4] = {0};
            synth_kill(); run_ms(20);
            sys.osc = NP_BIGBANG + 1u; sys.np_x = XS[q]; sys.np_y = YS[q]; sys.chaos = 0; sys.output = 0; sys.attack = 0;
            sys.sustain = 127; sys.ring = 0; sys.fx_mix = 0; sys.sh_mix = 0; sys.feg_on = 0; sys.menv_height = 0;
            synth_note_on(57, 100);
            run_ms(200);
            for (w = 0; w < 8192u / CTL; w++) { block(); for (b = 0; b < CTL; b++) ob[w * CTL + b] = buf[2 * b]; }
            for (b = 0; b < 8192u; b++)
                tot += (double)ob[b] * ob[b];
            for (k = 1; k <= 12u; k++) {
                double rr = 0, ii = 0;
                for (b = 0; b < 8192u; b++) {
                    double wv = 0.5 - 0.5 * cos(2 * M_PI * b / 8192.0), ph = 2 * M_PI * 220.0 * k * b / FS;
                    rr += ob[b] * wv * cos(ph); ii += ob[b] * wv * sin(ph);
                }
                hm += (rr * rr + ii * ii) * 2 / 8192.0 / 0.375;
                if (k <= 3u) a[k] = rr * rr + ii * ii;
            }
            hs[q] = hm / (tot + 1);
            if (q < 2u) { h2[q] = 10 * log10(a[2] / a[1] + 1e-12); h3[q] = 10 * log10(a[3] / a[1] + 1e-12); }
        }
        {   /* BANG 64: the first 50 ms against 0.5 s later, the noise above 4 kHz */
            double e0 = 0, e1 = 0, prev = 0;
            synth_kill(); run_ms(20);
            sys.np_x = 85; sys.np_y = 0; sys.chaos = 64;
            synth_note_on(57, 100);
            for (w = 0; w < 1000u; w++) {
                block();
                for (b = 0; b < CTL; b++) {
                    double d = buf[2 * b] - prev;     /* (the difference: the highs) */
                    prev = buf[2 * b];
                    if (w < 70u) e0 += d * d; else if (w >= 700u && w < 770u) e1 += d * d;
                }
            }
            early = e0; late = e1;
        }
        printf("   (BIG BANG: saw h2 %.1f dB, square h2 %.1f h3 %.1f dB; on the series %.2f, %.2f, ENTROPY 110: %.2f; BANG highs %.1fx)\n",
               h2[0], h2[1], h3[1], hs[0], hs[1], hs[2], early / (late + 1));
        okk &= h2[0] > -7.5 && h2[0] < -4.5 && h2[1] < -30 && h3[1] > -11.5 && h3[1] < -8;
        okk &= hs[0] > 0.5 && hs[1] > 0.5 && hs[2] < 0.2 * hs[0] && early > 2 * late;
        synth_kill(); sys.osc = 0; sys.chaos = 85; sys.output = 0; run_ms(20);
        check("BIG BANG: the standard waves from the series, ENTROPY, BANG", okk);
    }
    {   /* ORBIT on the harmonic series: a saw (BIG BANG), its 1st harmonic stays in the middle, the 2nd goes to the
         * sides (the all-passes between them turn it half round) */
        static int32_t sl[8192], sm[8192];
        double r1[2] = {0, 0}, r2[2] = {0, 0};
        uint32_t w, b, q;
        int okk = 1;
        synth_kill(); run_ms(20);
        sys.osc = NP_BIGBANG + 1u; sys.np_x = 85; sys.np_y = 0; sys.chaos = 0; sys.output = 0; sys.attack = 0; sys.sustain = 127;
        sys.ring = 0; sys.fx_mix = 0; sys.sh_mix = 0; sys.basis = 0; sys.orbit = 127; sys.feg_on = 0;
        synth_note_on(57, 100);
        {   /* (the voice's drift from a start whose places stay near the middle, not from the clock: the tests before
             * don't move it) */
            uint32_t k2;
            for (k2 = 0; k2 < NVOICE; k2++)
                if (voice[k2].stage == ST_ATTACK) { voice[k2].orng = 1453382u; voice[k2].ocnt = 0; voice[k2].orb = 0; }
        }
        run_ms(300);
        for (w = 0; w < 8192u / CTL; w++) {
            block();
            for (b = 0; b < CTL; b++) { sl[w * CTL + b] = buf[2 * b] - buf[2 * b + 1]; sm[w * CTL + b] = buf[2 * b] + buf[2 * b + 1]; }
        }
        for (q = 0; q < 2u; q++) {               /* q: side, mid */
            const int32_t *x = q ? sm : sl;
            double a1 = 0, b1 = 0, a2 = 0, b2 = 0;
            for (b = 0; b < 8192u; b++) {
                double p = 2 * M_PI * 220.0 * b / FS;
                a1 += x[b] * cos(p); b1 += x[b] * sin(p); a2 += x[b] * cos(2 * p); b2 += x[b] * sin(2 * p);
            }
            r1[q] = a1 * a1 + b1 * b1; r2[q] = a2 * a2 + b2 * b2;
        }
        printf("   (ORBIT, a saw: side / mid at the 1st harmonic %.3f, at the 2nd %.3f)\n", r1[0] / (r1[1] + 1), r2[0] / (r2[1] + 1));
        okk &= r2[0] / (r2[1] + 1) > 10 * (r1[0] / (r1[1] + 1)) && r2[0] / (r2[1] + 1) > 0.05;
        synth_kill(); sys.osc = 0; sys.orbit = 0; sys.chaos = 85; run_ms(20);
        check("ORBIT: on the harmonic series (the 2nd harmonic to the sides, the 1st in the middle)", okk);
    }
    {   /* ACCORDION, BAGPIPE, KALIMBA: the accordion speaks at the note; the bagpipe's drones sound under the chanter
         * at the key POSITION sets; the kalimba's tine rings its cantilever's second mode (6.27 x), not a harmonic */
        static int32_t ob[8192];
        double r[6];
        uint32_t q, w, b;
        int okk = 1;
        static const double FQ[6] = {220.0, 330.0, 110.0, 100.0, 1379.0, 1320.0};   /* (what each measures) */
        for (q = 0; q < 3u; q++) {
            uint32_t k;
            synth_kill(); sys.fx_mix = 0; run_ms(20);
            sys.osc = q == 2u ? 19 : 17; sys.np_x = q == 2u ? 0 : 126; sys.np_y = 63; sys.np_level = q == 2u ? 127 : 30;
            sys.attack = q == 2u ? 0 : 10; sys.decay = 46; sys.sustain = q == 2u ? 0 : 120; sys.release = 46;
            sys.ring = 10; sys.sh_mix = 0; sys.feg_on = 0; sys.osc = sys.osc;
            sys.fx_model = q == 0u ? RZ_ACCORD : q == 1u ? RZ_BAGPIPE : RZ_KALIMBA;
            sys.fx_struct = q == 1u ? 30 : 0; sys.fx_bright = 80; sys.fx_damp = 80;
            sys.fx_pos = q == 1u ? 0 : 0;        /* (BAGPIPE: drones in A: A2 110 Hz tenors) */
            sys.fx_mix = 127;
            synth_note_on(57, 100);
            run_ms(q == 2u ? 5 : 300);
            for (w = 0; w < 8192u / CTL; w++) { block(); for (b = 0; b < CTL; b++) ob[w * CTL + b] = buf[2 * b]; }
            for (k = 0; k < 2u; k++) {
                double rr = 0, ii = 0, f = FQ[q * 2u + k];
                for (b = 0; b < (q == 2u ? 2048u : 8192u); b++) {
                    double wv = 0.5 - 0.5 * cos(2 * M_PI * b / (q == 2u ? 2048.0 : 8192.0));
                    rr += ob[b] * wv * cos(2 * M_PI * f * b / FS); ii += ob[b] * wv * sin(2 * M_PI * f * b / FS);
                }
                r[q * 2u + k] = rr * rr + ii * ii;
            }
        }
        printf("   (ACCORDION 220 / 330 Hz %.1f dB; BAGPIPE drone 110 / 100 Hz %.1f dB; KALIMBA 6.27 x / 6 x %.1f dB)\n",
               10 * log10(r[0] / (r[1] + 1)), 10 * log10(r[2] / (r[3] + 1)), 10 * log10(r[4] / (r[5] + 1)));
        okk &= r[0] > 30 * r[1] && r[2] > 10 * r[3] && r[4] > 2 * r[5];
        synth_kill(); sys.fx_mix = 0; sys.osc = 0; run_ms(20);
        check("ACCORDION speaks, BAGPIPE drones, KALIMBA's tine (6.27 x)", okk);
    }
    {   /* DIDGERIDOO drones at the note (its mouth's formants change the colour, not the pitch); SHAKUHACHI speaks
         * at the note (its fundamental the strongest harmonic), MERI bends it down */
        static int32_t ob[16384];
        uint32_t q, w, b, k;
        int okk = 1;
        double hm[2][4];
        for (q = 0; q < 2u; q++) {
            synth_kill(); sys.fx_mix = 0; run_ms(20);
            rng_state = 0x1234567u;              /* (the noise from the same start: the tests before don't move it) */
            sys.osc = 17; sys.np_x = 126; sys.np_y = 63; sys.np_level = 30; sys.attack = 20; sys.decay = 80;
            sys.sustain = 115; sys.release = 60; sys.ring = 20; sys.sh_mix = 0; sys.feg_on = 0;
            sys.fx_model = q ? RZ_SHAKU : RZ_DIDGE; sys.fx_struct = q ? 70 : 10; sys.fx_bright = 70;
            sys.fx_damp = q ? 80 : 60; sys.fx_pos = q ? 0 : 40; sys.fx_mix = 127;
            synth_note_on(q ? 60 : 36, 100);
            run_ms(1500);
            for (w = 0; w < 16384u / CTL; w++) { block(); for (b = 0; b < CTL; b++) ob[w * CTL + b] = buf[2 * b]; }
            for (k = 1; k <= 4u; k++) {
                double rr = 0, ii = 0, f = (q ? 261.63 : 65.41) * k;
                for (b = 0; b < 16384u; b++) {
                    double wv = 0.5 - 0.5 * cos(2 * M_PI * b / 16384.0);
                    rr += ob[b] * wv * cos(2 * M_PI * f * b / FS); ii += ob[b] * wv * sin(2 * M_PI * f * b / FS);
                }
                hm[q][k - 1u] = rr * rr + ii * ii;
            }
        }
        printf("   (DIDGERIDOO 65 Hz drone: h1..4 %.0f %.0f %.0f %.0f dB; SHAKUHACHI h1..4 %.0f %.0f %.0f %.0f dB)\n",
               0.0, 10 * log10(hm[0][1] / hm[0][0]), 10 * log10(hm[0][2] / hm[0][0]), 10 * log10(hm[0][3] / hm[0][0]),
               0.0, 10 * log10(hm[1][1] / hm[1][0]), 10 * log10(hm[1][2] / hm[1][0]), 10 * log10(hm[1][3] / hm[1][0]));
        okk &= hm[0][0] > 1e9 && hm[1][0] > hm[1][1] && hm[1][0] > hm[1][2] && hm[1][0] > hm[1][3];
        synth_kill(); sys.fx_mix = 0; sys.osc = 0; run_ms(20);
        check("DIDGERIDOO drones, SHAKUHACHI speaks at the note", okk);
    }
    {   /* MULTI: channel 1 plays part A (voices 0, 1), channel 2 part B (2, 3) with B's sound; after the render sys
         * holds A's sound again, part_b B's; A's notes steal only among A's voices */
        uint32_t k, w, b2;
        int okk = 1;
        int32_t pk = 0;
        synth_kill(); keys_all_off(); sys.fx_mix = 0; sys.sh_mix = 0; run_ms(20);
        sys.osc = 0; sys.chaos = 85; sys.sustain = 120; sys.attack = 0; sys.midi_rx = 1;
        for (k = 0; k < PART_N_SYS; k++)
            part_b[k] = *PART_SYS[k];
        part_b[20] = 19;                         /* (B: a noise program) */
        part_b_tr = 0;
        sys.multi = 1; sys.ch_b = 2;
        midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);   /* A: three notes, two voices */
        midi_in(0x91, 48, 100);                  /* B */
        run_ms(30);
        for (k = 0; k < 2u; k++)
            okk &= voice[k].stage != ST_OFF && voice[k].part == 0u && voice[k].osc == 0u;
        okk &= (voice[2].stage != ST_OFF && voice[2].part == 1u && voice[2].osc == 19u && voice[2].key == 48u) ||
               (voice[3].stage != ST_OFF && voice[3].part == 1u && voice[3].osc == 19u && voice[3].key == 48u);
        for (w = 0; w < 50u; w++) { block(); for (b2 = 0; b2 < 2u * CTL; b2++) pk = abs(buf[b2]) > pk ? abs(buf[b2]) : pk; }
        okk &= pk > 1000 && sys.osc == 0u && part_b[20] == 19u;
        midi_in(0x81, 48, 0);
        run_ms(10);
        okk &= !((voice[2].key == 48u && voice[2].stage == ST_SUSTAIN) || (voice[3].key == 48u && voice[3].stage == ST_SUSTAIN));
        if (!okk) printf("   (multi: v0 %u/%u v1 %u/%u v2 %u/%u/%u v3 %u/%u/%u pk %d)\n", voice[0].stage, voice[0].part,
                         voice[1].stage, voice[1].part, voice[2].stage, voice[2].part, voice[2].osc, voice[3].stage,
                         voice[3].part, voice[3].osc, pk);
        {   /* OMNI: B layered on A: one note on A's channel starts a voice of each part, its off ends both */
            int a0 = 0, b0 = 0;
            keys_all_off(); synth_kill(); run_ms(20);
            sys.ch_b = 0;
            midi_in(0x90, 62, 100);
            run_ms(20);
            for (k = 0; k < NVOICE; k++) {
                a0 |= voice[k].stage != ST_OFF && voice[k].part == 0u && voice[k].key == 62u;
                b0 |= voice[k].stage != ST_OFF && voice[k].part == 1u && voice[k].key == 62u && voice[k].osc == 19u;
            }
            okk &= a0 && b0;
            midi_in(0x80, 62, 0);
            run_ms(10);
            for (k = 0; k < NVOICE; k++)
                okk &= !(voice[k].key == 62u && (voice[k].stage == ST_SUSTAIN || voice[k].stage == ST_ATTACK));
            if (!(a0 && b0)) printf("   (multi OMNI: A %d B %d)\n", a0, b0);
            sys.ch_b = 2;
        }
        keys_all_off(); synth_kill(); sys.multi = 0; sys.midi_rx = 0; run_ms(20);
        check("MULTI: two parts on two channels, each its voices and sound; B on OMNI layered on A", okk);
    }
    {   /* LESLIE: a steady 2 kHz tone comes out louder and softer as the horn turns (SIZE 127: fast), the two
         * microphones apart; SIZE 0 turns slower */
        int okk = 1;
        uint32_t q, b, i, sz;
        static int32_t inb[CTL], lo[CTL], ro[CTL];
        double sw[2];
        for (sz = 0; sz < 2u; sz++) {
            double mn = 1e18, mx = 0, dlr = 0;
            uint32_t flips = 0, up = 0;
            double prev = 0;
            memset(&sh, 0, sizeof sh);
            sys.sh_mix = 127; sys.sh_size = sz ? 127 : 0; sys.sh_decay = 0; sys.sh_shimmer = 0; sys.sh_tone = 90;
            sys.sh_space = SS_LESLIE;
            double e = 0, el = 0;
            for (q = 0; q < 2000u; q++) {        /* (the level over 12 blocks: a few periods of the tone) */
                for (b = 0; b < CTL; b++)
                    inb[b] = (int32_t)(12000 * sin(2 * M_PI * 2000.0 * (q * CTL + b) / FS));
                fx_shimmer(inb, lo, ro, CTL);
                for (i = 0; i < CTL; i++) { e += (double)lo[i] * lo[i]; el += (double)(lo[i] - ro[i]) * (lo[i] - ro[i]); }
                if (q % 12u == 11u) {
                    if (q > 200u) {
                        if (e < mn) mn = e;
                        if (e > mx) mx = e;
                        dlr += el;
                        if ((e > prev) != up) { flips++; up = e > prev; }
                    }
                    prev = e;
                    e = el = 0;
                }
            }
            (void)flips;
            sw[sz] = mx / (mn + 1);
            okk &= sw[sz] > 1.5 && dlr > 0;
        }
        printf("   (LESLIE: louder / softer %.1fx at SIZE 0, %.1fx at 127)\n", sw[0], sw[1]);
        memset(&sh, 0, sizeof sh);
        sys.sh_mix = 0; sys.sh_space = 0;
        check("SHIMMER LESLIE: the horn turns (louder, softer, the microphones apart)", okk);
    }
    {   /* TAPE: a pass recorded on track 1 plays back under the synth; its level; the speed with the BPM; erased */
        int okk = 1;
        uint32_t j, b;
        int32_t pk;
#define TAPE_PEAK(ms) do { pk = 0; for (b = 0; b < (ms) * FS / 1000u / CTL; b++) { block(); \
            for (j = 0; j < 2u * CTL; j++) pk = buf[j] > pk ? buf[j] : -buf[j] > pk ? -buf[j] : pk; } } while (0)
        tape_init();
        synth_kill(); keys_all_off(); run_ms(300);
        sys.arp_bpm = 120; sys.tape_len = 2; sys.tape_arm = 0; sys.tape_lv[0] = 127;
        sys.fx_mix = 0; sys.sh_mix = 0;
        sys.tape_len = 4; sys.arp_bpm = 140;                     /* (4 bars: whole from ~133 BPM, halved below) */
        okk &= tape_cells(2646000u / 140u) == 16u * (2646000u / 140u) / TAPE_DIV && (tape.spb || tape_fit() == 4u);
        sys.arp_bpm = 120;
        okk &= tape_cells(2646000u / 120u) == 8u * (2646000u / 120u) / TAPE_DIV;
        sys.tape_len = 2;
        tape.req = 2;
        run_ms(5);
        okk &= tape.playing && tape.rec && tape.spb == 2646000u / 120u && tape.len == 22050u;
        synth_note_on(60, 110);
        run_ms(800);
        synth_note_off(60);
        run_ms(1400);                                            /* (a pass: 2 s) */
        okk &= !tape.rec && tape.has[0];
        synth_kill();
        run_ms(333);                                             /* (mid-loop: track 2 from the next block's start) */
        sys.tape_arm = 1; tape.req = 2; run_ms(1);
        okk &= tape.rec && (tape.rec_wait || tape.cell % TAPE_KB == 0u);
        run_ms(2100);
        okk &= !tape.rec && tape.has[1] && !tape.rec_wait;
        if (!okk) printf("   (tape: overdub rec %u wait %u)\n", tape.rec, tape.rec_wait);
        sys.tape_arm = 0; tape.has[1] = 0;
        TAPE_PEAK(2000);
        okk &= pk > 1000;                                        /* the note, from the tape */
        if (!okk) printf("   (tape: playing %u rec %u spb %u len %u peak %d)\n", tape.playing, tape.rec, tape.spb, tape.len, pk);
        sys.tape_lv[0] = 0;
        run_ms(10);
        TAPE_PEAK(1000);
        okk &= pk < 50;                                          /* its fader down: silent */
        sys.tape_lv[0] = 127; sys.arp_bpm = 60;
        run_ms(10);
        okk &= tape.speed >= 2040u && tape.speed <= 2056u && tape.len == 22050u;   /* half the tempo: half the speed */
        {   /* BOUNCE: track 2 recorded too, then all onto track 1: 2 blank, 1 holds both, armed 2 */
            uint32_t c, diff = 0;
            sys.arp_bpm = 120; run_ms(10);
            ima_t q = {0, 0};
            for (c = 0; c < TAPE_N; c++) {
                if (c % TAPE_KB == 0u)
                    tape_key(1, c, &q);
                tape_wnib(1, c, ima_enc(&q, (int32_t)(c % 200u) * 40 - 4000));
            }
            tape.has[1] = 1; sys.tape_lv[1] = 100; sys.tape_lv[0] = 100;
            tape.req = 4;
            run_ms(400);                             /* (256 cells a block: ~230 ms) */
            okk &= !tape.bounce && tape.has[0] && !tape.has[1] && tape_buf[1][500] == 0 && sys.tape_arm == 1u;
            for (c = 0; c < 1000u; c++)
                diff += tape_buf[0][c] != 0;
            {   /* (the saw on track 2, decoded from track 1 after: close to it) */
                ima_t d = {0, 0};
                int32_t e = 0, w;
                for (c = 30000u; c < 34000u; c++) {   /* (past the loop: track 1 blank there) */
                    if (c % TAPE_KB == 0u) { d.p = tape_kp[0][c / TAPE_KB]; d.i = tape_ki[0][c / TAPE_KB]; }
                    w = ima_dec(&d, tape_nib(0, c));
                    if (c > 30400u) {
                        int32_t x = w - (int32_t)(c % 200u) * 40 + 4000, lim = 900;
                        e += x > lim || x < -lim;
                    }
                }
                okk &= e < 400;
                if (e >= 400) printf("   (bounce: the saw decoded off in %d cells)\n", e);
            }
            okk &= diff > 500u;
            if (!okk) printf("   (bounce: %u %u %u %u %u)\n", tape.bounce, tape.has[0], tape.has[1], sys.tape_arm, diff);
            sys.tape_arm = 0; sys.arp_bpm = 60; sys.tape_lv[0] = 127; run_ms(10);
        }
        tape.req = 3;                                            /* erased: blank again, silent */
        run_ms(10);
        okk &= !tape.spb && !tape.has[0];
        TAPE_PEAK(500);
        okk &= pk < 50;
        tape.req = 1;
        run_ms(10);
        okk &= !tape.playing;
        sys.arp_bpm = 120; sys.tape_lv[0] = 100;
#undef TAPE_PEAK
        check("TAPE: recorded, played back, its fader, varispeed with the BPM, BOUNCE, erased, stopped", okk);
    }
    {   /* velocity on the sound: BRIGHT closes a soft note's gate, FM makes its depth shallower; OFF changes nothing */
        int okk = 1;
        synth_kill();
        sys.vel_to = 0;
        synth_note_on(60, 20);
        okk &= voice_of(60) && voice_of(60)->vbr == 0u && voice_of(60)->vfm == 0u;
        synth_kill();
        sys.vel_to = 3; sys.vel_amt = 127;
        synth_note_on(60, 20);
        okk &= voice_of(60) && voice_of(60)->vbr > 150u && voice_of(60)->vfm > 150u;
        synth_note_on(64, 127);
        okk &= voice_of(64) && voice_of(64)->vbr == 0u;
        synth_kill();
        sys.vel_to = 0;
        check("velocity: soft notes darker and less FM when on, unchanged when OFF", okk);
    }

    /* the arpeggiator: on its own clock (120 BPM, 1/16: 8 steps a second) UP plays the held chord low to high over
     * the octaves; MIDI clock (24 a quarter) takes over and sets the tempo; without it again (0.5 s) its own; the
     * gate ends each note; turning it off lets the held notes sound as they are */
    {
        uint32_t okk = 1, w0, j, b, ons;
        okk &= ARP_NTYPE == 48u && !strcmp(arp_type_name(15), "TECHNO-A") && !strcmp(arp_type_name(29), "BASSLINE D") &&
               !strcmp(arp_type_name(32), "GLISSANDO") && !strcmp(arp_type_name(47), "BOUND BALL");   /* (the AN1x's 30 + 2, the MC-303's 16) */
        static const uint8_t UP2[6] = {60, 64, 67, 72, 76, 79};
        synth_kill(); keys_all_off(); run_ms(50);
        sys.osc = 0; sys.fx_mix = 0; sys.sh_mix = 0; sys.attack = 0; sys.sustain = 100; sys.release = 20;
        sys.arp_type = AT_UP; sys.arp_bpm = 120; sys.arp_div = 7; sys.arp_gate = 60; sys.arp_swing = 0;
        sys.arp_hold = 0; sys.arp_on = 1;
        run_ms(10);
        w0 = arp_view.w;
        midi_in(0x90, 64, 100); midi_in(0x90, 60, 100); midi_in(0x90, 67, 100);
        run_ms(1000);
        printf("   (ARP 120 BPM 1/16: %u steps in 1 s; notes", arp_view.w - w0);
        for (j = 0; j < 8u; j++)
            printf(" %u", arp_view.note[(w0 + j) & 31u]);
        printf(")\n");
        okk &= arp_view.w - w0 >= 8u && arp_view.w - w0 <= 9u;
        for (j = 0; j < 8u; j++)
            okk &= arp_view.note[(w0 + j) & 31u] == UP2[j % 6u];
        ons = 0;
        for (j = 0; j < NVOICE; j++)
            ons += voice[j].stage == ST_ATTACK || voice[j].stage == ST_SUSTAIN;
        okk &= ons <= 1u;                                    /* (one note at a time) */
        /* MIDI clock at 150 BPM: 24 a quarter -> 1/16 every 6 clocks; 2 s: 20 steps */
        w0 = arp_view.w;
        for (b = 0; b < 48u * 2u; b++) {                     /* 48 clocks a second at 120 .. 60 at 150 */
            midi_in(0xF8, 0, 0);
            run_ms(1000 / 60);                               /* (16.6 ms: ~150 BPM) */
        }
        printf("   (ARP under MIDI clock: %u steps for 96 clocks, %u BPM seen)\n", arp_view.w - w0, arp_bpm_now());
        okk &= arp_view.w - w0 >= 15u && arp_view.w - w0 <= 17u && arp_ext();
        okk &= arp_bpm_now() >= 140u && arp_bpm_now() <= 160u;
        run_ms(700);                                         /* no clock: its own again */
        okk &= !arp_ext();
        sys.arp_on = 0;                                      /* off: the chord held sounds */
        run_ms(20);
        ons = 0;
        for (j = 0; j < NVOICE; j++)
            ons += voice[j].stage == ST_ATTACK || voice[j].stage == ST_SUSTAIN;
        okk &= ons == 3u;
        midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
        run_ms(20);
        {   /* HOLD: the chord let go plays on; each pattern type plays something, bounded */
            uint32_t t2;
            sys.arp_on = 1; sys.arp_hold = 1;
            run_ms(10);
            midi_in(0x90, 62, 90); midi_in(0x90, 65, 90);
            run_ms(100);
            midi_in(0x80, 62, 0); midi_in(0x80, 65, 0);
            for (t2 = 0; t2 < ARP_NTYPE; t2++) {
                sys.arp_type = (uint8_t)t2;
                w0 = arp_view.w;
                peak = 0;
                run_ms(600);
                okk &= arp_view.w - w0 >= 4u && peak <= 32767;
            }
            sys.arp_hold = 0;
            run_ms(50);
            okk &= arp.n == 0u;
        }
        {   /* the patterns' steps: UP&DOWN B turns with its ends twice; TECHNO-A opens on the root an octave down;
             * ECHO repeats its note; SWING 83 %: the second of each pair two thirds of a step late */
            static const uint8_t UDB[8] = {60, 64, 67, 67, 64, 60, 60, 64};
            uint32_t t3, k3, at[4] = {0, 0, 0, 0}, w3, smp = 0;
            sys.arp_hold = 0;
            run_ms(20);
            sys.arp_hold = 1;
            sys.arp_type = AT_UDB;
            midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);
            run_ms(5);
            midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
            w0 = arp_view.w - 1u;                    /* (the first step went at once) */
            run_ms(1000);
            for (k3 = 0; k3 < 8u; k3++)
                okk &= arp_view.note[(w0 + k3) & 31u] == UDB[k3];
            for (t3 = AT_BASIC; t3 < ARP_NTYPE; t3++) {
                sys.arp_type = (uint8_t)t3;
                arp.running = 1;                     /* (from its first step) */
                w0 = arp_view.w;
                run_ms(300);
                if (!strcmp(ARP_STEPS[t3 - AT_BASIC].name, "TECHNO-A"))
                    okk &= arp_view.note[w0 & 31u] == 60u;   /* (its octave down folded into the chord's) */
                if (!strcmp(ARP_STEPS[t3 - AT_BASIC].name, "ECHO (DJX)"))
                    okk &= arp_view.note[(w0 + 1u) & 31u] == arp_view.note[w0 & 31u];
                {   /* every step pattern within the chord's octave (60 .. 71) */
                    uint32_t q;
                    for (q = 0; q < 8u; q++) {
                        uint8_t v8 = arp_view.note[(w0 + q) & 31u];
                        if (v8 && (v8 < 60u || v8 > 71u)) { okk = 0; printf("   (%s: %u out of the octave)\n", ARP_STEPS[t3 - AT_BASIC].name, v8); break; }
                    }
                }
                if (!strcmp(ARP_STEPS[t3 - AT_BASIC].name, "DA HOUSE"))   /* the bass, then the right hand */
                    okk &= arp_view.note[w0 & 31u] == 60u && arp_view.note[(w0 + 2u) & 31u] == 64u;
                if (!strcmp(ARP_STEPS[t3 - AT_BASIC].name, "GLISSANDO"))  /* semitones up from the lowest */
                    okk &= arp_view.note[w0 & 31u] == 60u && arp_view.note[(w0 + 1u) & 31u] == 61u &&
                           arp_view.note[(w0 + 2u) & 31u] == 62u;
                if (!strcmp(ARP_STEPS[t3 - AT_BASIC].name, "3 FINGER"))   /* the thumb low, the melody on top */
                    okk &= arp_view.note[w0 & 31u] == 60u && arp_view.note[(w0 + 1u) & 31u] == 67u;
            }
            {   /* STRUM GTR: the chord strummed down, a string each 12 ms; RHYTHM GTR's muted chop: a quarter step */
                uint32_t t4, n1 = 0, n2 = 0, mut = 0;
                for (t4 = AT_BASIC; t4 < ARP_NTYPE; t4++)
                    if (!strcmp(ARP_STEPS[t4 - AT_BASIC].name, "STRUM GTR"))
                        sys.arp_type = (uint8_t)t4;
                arp.running = 1;
                block(); block();
                n1 = arp.non;
                run_ms(30);
                n2 = arp.non;
                okk &= n1 == 1u && n2 == 3u && arp.on[0] == 67u && arp.on[2] == 60u;
                for (t4 = AT_BASIC; t4 < ARP_NTYPE; t4++)
                    if (!strcmp(ARP_STEPS[t4 - AT_BASIC].name, "RHYTHM GTR"))
                        sys.arp_type = (uint8_t)t4;
                arp.running = 1;
                w3 = arp_view.w;
                for (t4 = 0; t4 < 2000u && arp_view.w - w3 < 2u; t4++)
                    block();
                mut = arp.gate <= arp.len / 4u;
                okk &= mut;
                if (!(n1 == 1u && n2 == 3u) || !mut)
                    printf("   (strum: %u then %u notes; muted gate %u of %u)\n", n1, n2, arp.gate, arp.len);
            }
            sys.arp_type = AT_UP1; sys.arp_swing = 127; arp.running = 1;
            w3 = arp_view.w;
            for (k3 = 0; k3 < 4u && smp < 2u * FS; ) {   /* when the steps come (in samples) */
                block();
                smp += CTL;
                if (arp_view.w != w3) {
                    w3 = arp_view.w;
                    at[k3++] = smp;
                }
            }
            printf("   (SWING 83 %%: steps at %u %u %u %u samples)\n", at[0], at[1], at[2], at[3]);
            /* 120 BPM 1/16: 5512 a step; swung: 0, +3675 (2/3 late), 11025, +3675 */
            okk &= at[1] - at[0] > 8500u && at[1] - at[0] < 9900u && at[2] - at[0] > 10900u && at[2] - at[0] < 11200u;
            sys.arp_swing = 0;
            sys.arp_hold = 0;
            run_ms(20);
        }
        sys.arp_on = 0;
        run_ms(20);
        synth_kill();
        {   /* GROOVE: ARP VELOCITY SEQ SKIP (9 0): a rest every second step (the pattern goes on: UP's notes 60 67 76 ...),
             * VELOCITY ACCENT 4 louder on every fourth; LEGATO ties the notes on; every pattern a step or more */
            uint32_t g0, k, rests = 0, notes = 0;
            synth_kill(); keys_all_off(); run_ms(50);
            sys.arp_type = AT_UP; sys.arp_div = 7; sys.arp_gate = 60; sys.arp_swing = 0; sys.arp_hold = 0; sys.arp_on = 1;
            sys.arp_vp = 14; sys.arp_vd = 127;
            okk &= !strcmp(ARP_VPAT[14].name, "SKIP") && !strcmp(ARP_VPAT[1].name, "ACCENT 4");
            run_ms(10);
            w0 = arp_view.w; g0 = gen_n;
            midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);
            run_ms(1000);
            for (k = w0; k != arp_view.w; k++) {
                rests += arp_view.note[k & 31u] == 0;
                notes += arp_view.note[k & 31u] != 0;
            }
            okk &= rests >= 3u && notes >= 3u && rests <= notes + 1u && gen_n - g0 == notes;
            okk &= arp_view.note[w0 & 31u] == 60u && arp_view.note[(w0 + 2u) & 31u] == 67u;
            if (!okk) printf("   (GROOVE: %u notes, %u rests, %u made)\n", notes, rests, gen_n - g0);
            for (k = 0; k < ARP_NVP; k++) okk &= ARP_VPAT[k].s[0] || !k;
            midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
            sys.arp_vp = 0;
            run_ms(100);
            {   /* SEQ LENGTH 3: UP over four notes held loops its first three: 60 64 67 60 64 67 ... */
                uint32_t j2;
                synth_kill(); keys_all_off(); run_ms(50);
                sys.arp_slen = 3;
                w0 = arp_view.w;
                midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100); midi_in(0x90, 72, 100);
                run_ms(1000);
                for (j2 = 0; j2 < 6u; j2++) {
                    static const uint8_t L3[3] = {60, 64, 67};
                    okk &= arp_view.note[(w0 + j2) & 31u] == L3[j2 % 3u];
                }
                if (arp_view.note[(w0 + 3u) & 31u] != 60u) printf("   (SEQ LENGTH 3: %u %u %u %u)\n", arp_view.note[w0 & 31u],
                    arp_view.note[(w0 + 1u) & 31u], arp_view.note[(w0 + 2u) & 31u], arp_view.note[(w0 + 3u) & 31u]);
                midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0); midi_in(0x80, 72, 0);
                sys.arp_slen = 0;
            }
            sys.arp_on = 0;
            run_ms(100);
        }
        check("ARP: own clock, MIDI clock in and out, gate, off, HOLD, every pattern, UP&DOWN B, echo, swing, GROOVE, SEQ LENGTH", okk);
    }

    /* KARMA (SEQ): the chord held makes a phrase. PULSE ARP at 120 BPM (sixteenths) starts at once and plays its hits
     * ('1', '2': 12 of 16 steps) and not its ghosts; DENSITY 0 nothing, 127 more; every GE plays (and stops); LATCH
     * keeps it going, a new chord replaces the old; MIDI clock drives it (a step each 6 clocks, START from the top);
     * ARP and KARMA never at once, the notes held move over; what it makes goes out on USB MIDI (the keys' own not);
     * a GE's envelope moves its parameter and gives it back; FREE EG PHRASE loops with the phrase, KARMA K1 driven */
    {
#define KOK(x) do { if (!(x)) { printf("   (KARMA: line %d)\n", __LINE__); okk = 0; } } while (0)
        uint32_t okk = 1, w0, g, j, b, q, n0;
        uint8_t ring0;
        synth_kill(); keys_all_off(); run_ms(50);
        sys.k_logic = 0;                         /* (one or the other: the handover below) */
        sys.arp_on = 0; sys.osc = 0; sys.fx_mix = 0; sys.sh_mix = 0; sys.attack = 0; sys.sustain = 100;
        sys.release = 20; sys.arp_bpm = 120; sys.arp_swing = 0; sys.feg_on = 0; sys.feg_has = 0;
        KOK(KGE_N == 46u);
        for (g = 0; g < KGE_N; g++) {                        /* every GE's strings as long as its rhythm */
            uint32_t L = str_len(KGE[g].rhy);
            if ((KGE[g].vel && str_len(KGE[g].vel) != L) || (KGE[g].clu && str_len(KGE[g].clu) != L) ||
                (KGE[g].idx && str_len(KGE[g].idx) != L) || (KGE[g].oct && str_len(KGE[g].oct) != L)) {
                printf("   (KARMA %s: a string not %u long)\n", KGE[g].name, L);
                okk = 0;
            }
        }
        sys.k_ge = 0; sys.k_latch = 0; sys.k_m[0] = sys.k_m[1] = sys.k_m[2] = sys.k_m[3] = 64;
        sys.k_on = 1;
        run_ms(10);
        w0 = kar_view.w;
        midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);
        run_ms(3);
        okk &= kar_view.w - w0 == 1u && kar_view.note[w0 & 31u] == 60u;   /* (at once, the lowest first) */
        run_ms(1997);                                        /* 2 s: 16 sixteenths, 12 of them hits */
        printf("   (KARMA PULSE ARP 120 BPM: %u notes in 2 s:", kar_view.w - w0);
        for (j = 0; j < 12u; j++)
            printf(" %u", kar_view.note[(w0 + j) & 31u]);
        printf(")\n");
        KOK(kar_view.w - w0 >= 12u && kar_view.w - w0 <= 13u);
        sys.k_m[0] = 0;                                      /* DENSITY 0: nothing */
        w0 = kar_view.w;
        run_ms(1000);
        KOK(kar_view.w == w0);
        sys.k_m[0] = 127;                                    /* DENSITY 127: ghosts and fills too */
        run_ms(1000);
        KOK(kar_view.w - w0 >= 8u);                          /* (a second: 8 steps, all played) */
        sys.k_m[0] = 64;
        for (g = 0; g < KGE_N; g++) {                        /* every GE: notes, in range, and stopping */
            uint32_t lo = 127, hi = 0;
            sys.k_ge = (uint8_t)g;
            w0 = kar_view.w;
            run_ms(2500);
            for (j = w0; j != kar_view.w && j - w0 < 32u; j++) {
                uint32_t nt = kar_view.note[j & 31u];
                lo = nt < lo ? nt : lo;
                hi = nt > hi ? nt : hi;
            }
            if (kar_view.w - w0 < 2u || lo < 12u || hi > 120u) {
                printf("   (GE %s: %u notes, %u..%u)\n", KGE[g].name, kar_view.w - w0, lo, hi);
                okk = 0;
            }
        }
        midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
        run_ms(1000);
        w0 = kar_view.w;
        run_ms(1000);
        KOK(kar_view.w == w0 && !kar.running && sounding() == 0);
        /* an envelope: TECHNO STAB moves RING over the phrase, gives it back */
        sys.k_ge = 5; sys.ring = 50; ring0 = 50;
        midi_in(0x90, 60, 100); midi_in(0x90, 63, 100); midi_in(0x90, 67, 100);
        q = 0;
        for (b = 0; b < 40u; b++) {
            run_ms(50);
            q |= sys.ring != ring0;
        }
        KOK(q && kar.env_on);
        midi_in(0x80, 60, 0); midi_in(0x80, 63, 0); midi_in(0x80, 67, 0);
        run_ms(20);
        KOK(sys.ring == ring0 && !kar.env_on);
        /* LATCH: let go, it goes on; a new chord replaces the old */
        sys.k_ge = 0; sys.k_latch = 1;
        midi_in(0x90, 60, 100); midi_in(0x90, 64, 100);
        run_ms(100);
        midi_in(0x80, 60, 0); midi_in(0x80, 64, 0);
        w0 = kar_view.w;
        run_ms(1000);
        KOK(kar_view.w - w0 >= 5u && kar.n == 2u);
        midi_in(0x90, 50, 100);
        run_ms(300);
        midi_in(0x80, 50, 0);
        KOK(kar.n == 1u && kar.note[0] == 50u);
        sys.k_latch = 0;
        run_ms(20);
        KOK(kar.n == 0u && !kar.running);
        run_ms(500);
        /* MIDI clock: 96 clocks = 16 sixteenths; START: the phrase from its top */
        midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);
        midi_in(0xFA, 0, 0);
        run_ms(5);
        w0 = kar_view.w; n0 = kar.triggers;
        for (b = 0; b < 96u; b++) {
            midi_in(0xF8, 0, 0);
            run_ms(20);
        }
        KOK(kar_view.w - w0 >= 12u && kar_view.w - w0 <= 13u && kar.triggers - n0 >= 1u);
        /* ARP on: KARMA off, the notes held into the ARP; KARMA on again: the ARP off */
        sys.arp_on = 1;
        run_ms(10);
        KOK(!sys.k_on && arp.n == 3u && !kar.n);
        sys.k_on = 1;
        run_ms(10);
        KOK(!sys.arp_on && kar.n == 3u && !arp.n);
        sys.k_on = 0;                                        /* off: the chord sounds as held */
        run_ms(10);
        KOK(sounding() == 3u);
        sys.k_on = 1;
        run_ms(700);                                         /* (the clock gone: its own again) */
        /* USB MIDI out: KARMA's notes on the TX channel, the keys' own not */
        out_n = 0; sys.midi_tx = 4;
        fm1_in.notes = 1u << key_of(72);
        run_ms(400);
        q = 0;
        for (j = 0; j < out_n; j++)
            q += ((out_log[j] >> 8) & 0xFFu) == 0x94u;
        KOK(q >= 2u && !sent(0, 0x94, 72, KEY_VEL));
        fm1_in.notes = 0;
        sys.midi_tx = 0;
        /* FREE EG: PHRASE (a phrase of PULSE ARP: 16 sixteenths, 2 s) recorded in step with it, KARMA K1 its target */
        sys.k_ge = 0; sys.feg_len = 13; sys.feg_target = 23; sys.feg_depth = 127; sys.feg_val = 0;
        KOK(!strcmp(FEG_T_NAME[23], "KARMA K1") && !strcmp(FEG_LEN_NAME[13], "PHRASE"));
        n0 = kar.triggers;
        for (b = 0; b < 3000u && kar.triggers == n0; b++)   /* (from a phrase's start) */
            run_ms(1);
        feg_req = 1;
        run_ms(1000);
        sys.feg_val = 100;
        for (b = 0; b < 300u && feg.rec; b++)
            run_ms(10);
        KOK(!feg.rec && sys.feg_has && feg.curve[10] == 0u && feg.curve[120] == 100u);
        if (feg.rec || feg.curve[10] || feg.curve[120] != 100u)
            printf("   (FEG PHRASE: rec %u, %u %u %u)\n", feg.rec, feg.curve[10], feg.curve[64], feg.curve[120]);
        sys.feg_on = 1;
        sys.k_m[0] = 64;
        q = 0;
        for (b = 0; b < 200u; b++) {                         /* driven: 0 in its first half, 100 in its second */
            run_ms(20);
            q |= (sys.k_m[0] == 0u) << 0 | (sys.k_m[0] == 100u) << 1;
        }
        KOK(q == 3u);
        sys.feg_on = 0;
        run_ms(10);
        KOK(sys.k_m[0] == 64u);
        sys.feg_has = 0; sys.feg_len = 9; sys.feg_target = 0;
        midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
        sys.k_on = 0;
        run_ms(500);
        check("KARMA: a phrase from the chord, DENSITY, every GE, envelope, LATCH, MIDI clock, ARP handover, MIDI out, "
              "FREE EG PHRASE", okk);
#undef KOK
    }
    {   /* LOGIC: the ARP (UP, every sixteenth or eighth) with KARMA (PULSE ARP: 12 hits in 16 sixteenths, its ghosts
         * rests) on one grid: OR both, AND only where both, XOR only where one; the notes made as many as let through;
         * on its own clock and on MIDI clock */
        uint32_t okk = 1, m, d, ext;
        for (ext = 0; ext < 2u; ext++)
            for (d = 0; d < 3u; d++)             /* (1/16, 1/8, 1/12: SUBDIVIDE its own on the one clock) */
                for (m = 1; m <= 7u; m++) {      /* (7: OR INVERSE) */
                    uint32_t w0, g0, i, na = 0, nk = 0, pa = 0, pk = 0, both = 0, b;
                    synth_kill(); keys_all_off(); run_ms(300);
                    sys.arp_type = 0; sys.arp_div = d == 2u ? 6 : d ? 4 : 7; sys.arp_swing = 0; sys.arp_gate = 60; sys.arp_hold = 0;
                    sys.k_ge = 0; sys.k_latch = 0; sys.k_m[0] = sys.k_m[1] = sys.k_m[2] = sys.k_m[3] = 64;
                    sys.arp_bpm = 120; sys.k_logic = (uint8_t)(m == 7u ? 1u : m); sys.k_inv = (uint8_t)(m == 7u); sys.k_on = 1; sys.arp_on = 1;
                    run_ms(20);
                    if (ext) midi_in(0xFA, 0, 0);
                    midi_in(0x90, 60, 100); midi_in(0x90, 64, 100); midi_in(0x90, 67, 100);
                    if (ext) for (b = 0; b < 12u; b++) { midi_in(0xF8, 0, 0); run_ms(20); }
                    else run_ms(300);
                    w0 = logic_view.w; g0 = gen_n;
                    for (b = 0; b < (d == 2u ? 96u : 192u); b++) {   /* (two bars at 120, a clock 21 ms; triplets one) */
                        if (ext) midi_in(0xF8, 0, 0);
                        run_ms(ext ? 20 : 21);
                    }
                    for (i = w0; i != logic_view.w; i++) {
                        uint8_t v = logic_view.v[i & 31u];
                        na += v & 1u; nk += (v >> 1) & 1u; pa += (v >> 2) & 1u; pk += (v >> 3) & 1u;
                        both += (v & 3u) == 3u;
                        if (m == 2u && ((v >> 2 & 1u) != (v & 3u) / 3u || (v >> 3 & 1u) != (v & 3u) / 3u)) okk = 0;
                        if (m == 3u && ((v >> 2 & 1u) != ((v & 3u) == 1u) || (v >> 3 & 1u) != ((v & 3u) == 2u))) okk = 0;
                        if (m == 1u && (v >> 2) != (v & 3u)) okk = 0;
                        if ((m == 4u || m == 7u) && (v >> 2)) okk = 0;   /* (NOR: UP hits every step, no gap; OR INVERSE: none) */
                        if ((m == 5u || m == 6u) && (v >> 2) == 3u) okk = 0;   /* (ALT, S/R: one at a time) */
                    }
                    if (logic_view.w - w0 > 32u) okk = 0;   /* (the view keeps 32) */
                    /* (the grid: the eighths' ARP hits on every 2nd sixteenth, on KARMA's own; all its hits met) */
                    if (na < (d == 2u ? 11u : d ? 7u : 14u) || nk < (d == 2u ? 5u : 10u) || both < (d == 2u ? 3u : d ? 5u : 10u)) okk = 0;
                    if (d == 2u && both > 5u) okk = 0;   /* (triplets meet the sixteenths once a beat) */
                    if (gen_n - g0 + 2u < pa + pk || gen_n - g0 > pa + pk + 2u) okk = 0;
                    if ((m == 5u || m == 6u) && (!pa || !pk)) okk = 0;   /* (each gets its turns) */
                    if (!okk) {
                        printf("   (LOGIC %u %s ext %u: entries %u arp %u karma %u both %u through %u + %u notes %u)\n", m,
                               d == 2u ? "1/12" : d ? "1/8" : "1/16", ext, logic_view.w - w0, na, nk, both, pa, pk, gen_n - g0);
                        break;
                    }
                    midi_in(0x80, 60, 0); midi_in(0x80, 64, 0); midi_in(0x80, 67, 0);
                    run_ms(300);
                }
        {   /* NOTE QUANT: C major held: C# to C, F# to F (the lower of two), in SCALE; D to C, F to E in CHORD; A minor:
             * G# to G; off: as it is; the note made ends as the one it sounded as */
            synth_kill(); keys_all_off(); run_ms(50);
            sys.k_logic = 1; sys.arp_on = sys.k_on = 1; run_ms(5);
            arp.n = 3; arp.note[0] = 60; arp.note[1] = 64; arp.note[2] = 67;
            sys.k_root = 0;
            sys.k_lq = LQ_AUTO;
            okk &= logic_q(61) == 60u && logic_q(66) == 65u && logic_q(71) == 71u && logic_q(74) == 74u;
            sys.k_lq = LQ_CHORD;
            okk &= logic_q(62) == 60u && logic_q(65) == 64u && logic_q(76) == 76u && logic_q(70) == 72u;
            arp.note[0] = 57; arp.note[1] = 60; arp.note[2] = 64; sys.k_lq = LQ_AUTO;
            okk &= logic_q(56) == 55u && logic_q(68) == 67u && logic_q(62) == 62u;
            sys.k_lq = LQ_SCALES + 2u; sys.k_root = 3;   /* (D DORIAN: F# to F, B stays, C# to C) */
            okk &= !strcmp(logic_q_name(sys.k_lq), "DORIAN") && logic_q(66) == 65u && logic_q(71) == 71u && logic_q(61) == 60u;
            sys.k_lq = LQ_SCALES + 9u; sys.k_root = 0;   /* (PENTA MAJOR from the lowest, A: B stays, D to C#) */
            okk &= logic_q(71) == 71u && logic_q(62) == 61u;
            sys.k_root = 0;
            sys.k_lq = 0;
            okk &= logic_q(61) == 61u;
            sys.k_lq = LQ_AUTO;
            gen_on(58, 100);                     /* (A#: sounds as A) */
            run_ms(5);
            {   uint32_t k, on57 = 0;
                for (k = 0; k < NVOICE; k++) on57 |= voice[k].stage != ST_OFF && voice[k].stage != ST_RELEASE && voice[k].note == 57u;
                gen_off(58); run_ms(5);
                for (k = 0; k < NVOICE; k++) okk &= !(voice[k].stage != ST_OFF && voice[k].stage != ST_RELEASE && voice[k].note == 57u);
                okk &= on57 != 0;
            }
            arp.n = 0;
            if (!okk) printf("   (NOTE QUANT)\n");
        }
        sys.k_on = sys.arp_on = 0; sys.k_logic = 0; sys.k_inv = 0;
        run_ms(100);
        check("LOGIC: the ARP and KARMA on one grid, OR / AND / XOR / NOR / ALT / S/R, INVERSE, own clock and MIDI clock; NOTE QUANT", okk);
    }

    /* KARMA running every GE while the oscillator, X / Y, FM, the FX model and the GE's knobs are turned at random
     * (the crash report of 2026-10-29: KARMA and the oscillator's ALGORITHM): in bounds (the sanitizers), bounded */
    {
        uint32_t okk = 1, b, j, r = 0x2468ACE1u;
        synth_kill(); keys_all_off(); run_ms(20);
        sys.arp_on = 0; sys.k_on = 1; sys.k_latch = 1; sys.fx_mix = 90; sys.sh_mix = 40; sys.basis = 100; sys.orbit = 60;
        midi_in(0x90, 48, 100); midi_in(0x90, 55, 100); midi_in(0x90, 60, 100); midi_in(0x90, 64, 100);
        run_ms(20);
        midi_in(0x80, 48, 0); midi_in(0x80, 55, 0); midi_in(0x80, 60, 0); midi_in(0x80, 64, 0);
        for (b = 0; b < 12000u; b++) {
            r = r * 1664525u + 1013904223u;
            switch ((r >> 24) % 13u) {
            case 8: sys.k_on = (uint8_t)!sys.k_on; break;
            case 9: sys.arp_on = (uint8_t)!sys.arp_on; break;
            case 10: sys.k_latch = (uint8_t)((r >> 8) & 1u); sys.arp_hold = (uint8_t)((r >> 9) & 1u); break;
            case 11: midi_in((r >> 8) & 1u ? 0x90 : 0x80, 36u + (r >> 10) % 48u, 100); break;
            case 12: sys.feg_on = (uint8_t)((r >> 8) & 1u); sys.feg_has = 1; sys.feg_len = (uint8_t)((r >> 9) % FEG_NLEN);
                     sys.feg_target = (uint8_t)((r >> 13) % FEG_NT); break;
            case 0: sys.osc = (uint8_t)((r >> 8) % (NP_COUNT + 1u)); break;
            case 1: sys.k_ge = (uint8_t)((r >> 8) % KGE_N); break;
            case 2: sys.k_m[(r >> 8) & 3u] = (uint8_t)((r >> 12) & 127u); break;
            case 3: sys.np_x = (uint8_t)((r >> 8) & 127u); sys.np_y = (uint8_t)((r >> 16) & 127u); break;
            case 4: sys.menv_height = (uint8_t)((r >> 8) & 127u); sys.menv_len = (uint8_t)((r >> 16) & 127u); break;
            case 5: sys.fx_model = (uint8_t)((r >> 8) % RZ_COUNT); break;
            case 6: sys.chaos = (uint8_t)((r >> 8) & 127u); sys.arp_bpm = (uint8_t)(40u + (r >> 16) % 211u); break;
            default: sys.octave = (int8_t)((int32_t)((r >> 8) % 7u) - 3); break;
            }
            run_ms(5);
            for (j = 0; j < 2u * CTL; j++)
                if (buf[j] > 8388607 || buf[j] < -8388608)
                    okk = 0;
        }
        sys.k_on = 0; sys.k_latch = 0; sys.octave = 0; sys.arp_on = 0; sys.arp_hold = 0; sys.feg_on = 0; sys.feg_has = 0;
        sys.feg_len = 9; sys.feg_target = 0;
        keys_all_off();
        run_ms(500);
        synth_kill();
        sys.osc = 0; sys.np_x = sys.np_y = 64; sys.menv_height = 0; sys.fx_mix = 0; sys.sh_mix = 0;
        sys.fx_model = 0; sys.chaos = 85; sys.basis = 0; sys.orbit = 0; sys.arp_bpm = 120; sys.k_ge = 0;
        for (j = 0; j < 4u; j++) sys.k_m[j] = 64;
        check("KARMA under turned knobs: every GE, oscillator, FM, FX model at random, 15 s", okk);
    }

    /* the ADSR: attack time, the decay settling on the sustain level, the release into the tail and off */
    {
        voice_t v;
        uint32_t f = 0, ok = 1;
        memset(&v, 0, sizeof v);
        sys.attack = 40; sys.decay = 50; sys.sustain = 64; sys.release = 50; sys.ring = 100;
        env_params();
        v.stage = ST_ATTACK;
        while (v.stage == ST_ATTACK && f < 100000u) { env_frame(&v); f++; }
        ok &= f >= ENV_FRAMES[40] - 1u && f <= ENV_FRAMES[40] + 1u;
        for (f = 0; f < 8u * ENV_FRAMES[50]; f++) env_frame(&v);
        ok &= v.stage == ST_SUSTAIN && v.env > eg.sus - ENV_MAX / 100u && v.env < eg.sus + ENV_MAX / 100u;
        v.stage = ST_RELEASE;
        for (f = 0; f < 20u * ENV_FRAMES[50] && v.stage == ST_RELEASE; f++) env_frame(&v);
        ok &= v.stage == ST_TAIL && f > 8u * ENV_FRAMES[50];
        for (f = 0; f < FS && v.stage == ST_TAIL; f++) env_frame(&v);
        ok &= v.stage == ST_OFF;
        check("ADSR: attack on time, decay settles on sustain, release -> tail -> off", ok);
    }

    /* the low pass gate's ring: an impulse into the filter held at ~1 kHz; RING 0 dies at once, RING 127 rings on */
    {
        uint32_t r, len[2];
        for (r = 0; r < 2u; r++) {
            voice_t v;
            uint32_t f, last = 0;
            memset(&v, 0, sizeof v);
            sys.ring = (uint8_t)(r ? 127 : 0);
            env_params();
            v.ctl = (uint32_t)(ENV_MAX * 0.666);          /* index ~170: 25 Hz x 600^(170/255) ~ 1.8 kHz */
            v.amp = 32767;
            for (f = 0; f < 20000u; f++) {
                int32_t y = lpg_frame(&v, f == 0 ? 32767 : 0);
                if (y > 33 || y < -33)                    /* above -60 dB of the impulse */
                    last = f;
            }
            len[r] = last;
        }
        printf("   (impulse at ~1.8 kHz rings for %u frames with RING 0, %u with RING 127)\n", len[0], len[1]);
        check("RING 127 rings (Q 6): an impulse lasts 4..20x longer than with RING 0, under 15 ms",
              len[1] > 4u * (len[0] + 1u) && len[1] < 20u * (len[0] + 1u) && len[1] < FS * 15u / 1000u);
    }

    /* the gate in every corner: every RING, plucks and holds, every regime, low to high notes: bounded, quiet after */
    {
        static const uint8_t NOTES[] = {24, 60, 96, 120};
        uint32_t r, c, ni, b, okk = 1;
        int32_t big = 0;
        peak = 0;
        for (r = 0; r < 128u; r += 21u)
            for (c = 0; c < 128u; c += 32u) {
                sys.ring = (uint8_t)r; sys.chaos = (uint8_t)c; sys.shape = 64; sys.output = 64;
                sys.attack = 0; sys.decay = 40; sys.sustain = (uint8_t)(c & 64u ? 127 : 0); sys.release = 30;
                for (ni = 0; ni < sizeof NOTES; ni++)
                    synth_note_on(NOTES[ni], 127);
                for (b = 0; b < 200u; b++) {
                    block();
                    for (ni = 0; ni < NVOICE; ni++) {
                        int32_t a = voice[ni].ic1 < 0 ? -voice[ni].ic1 : voice[ni].ic1;
                        big = a > big ? a : big;
                    }
                }
                synth_all_off();
                for (b = 0; b < 2000u && sounding(); b++)
                    block();
                okk &= sounding() == 0;
                synth_kill();
            }
        printf("   (largest filter state %d of 2^31, peak %d)\n", big, peak);
        check("low pass gate: every RING x regime x note range bounded, every voice ends", okk && big < (1 << 29) && peak <= 32767);
    }

    /* the FX resonators (Rings' models): every model x STRUCTURE x DAMPING x note range bounded, no NaN-like jumps;
     * MIX 0 changes nothing; MODAL tuned: a lone mode at the note (STRUCTURE 0.25..0.3: harmonic) rings at its pitch */
    {
        static const uint8_t NOTES[] = {24, 48, 72, 96};
        uint32_t md, st, dmp, ni, b2, okk = 1;
        int32_t pk2 = 0;
        sys.chaos = 85; sys.shape = 76; sys.output = 32; sys.attack = 0; sys.decay = 60; sys.sustain = 90;
        sys.release = 60; sys.ring = 50; sys.fx_bright = 100; sys.fx_pos = 40; sys.fx_mix = 127;
        for (md = 0; md < RZ_COUNT; md++)
            for (st = 0; st < 128u; st += 42u)
                for (dmp = 0; dmp < 128u; dmp += 63u) {
                    sys.fx_model = (uint8_t)md; sys.fx_struct = (uint8_t)st; sys.fx_damp = (uint8_t)dmp;
                    for (ni = 0; ni < sizeof NOTES; ni++) {
                        synth_note_on(NOTES[ni], 127);
                        for (b2 = 0; b2 < 120u; b2++) {
                            synth_render(buf, CTL);
                            for (w = 0; w < CTL; w++) {
                                int32_t a2 = buf[2 * w] < 0 ? -buf[2 * w] : buf[2 * w];
                                pk2 = a2 > pk2 ? a2 : pk2;
                            }
                        }
                        synth_kill();
                    }
                }
        printf("   (resonators: peak %d)\n", pk2);
        okk &= pk2 <= 32767;
        {   /* MIX 0: bit for bit the same as never having had a resonator */
            static int32_t ref2[2 * CTL * 300];
            int pass, same = 1;
            for (pass = 0; pass < 2; pass++) {
                synth_kill(); memset(voice, 0, sizeof voice);
                ghost.x = C1 / 10; ghost.y = ghost.z = 0; rng_state = 0x1234567u; voice_clock = 0;
                sys.fx_mix = 0;
                if (pass)
                    sys.fx_model = RZ_STRING;
                synth_note_on(55, 100);
                for (b2 = 0; b2 < 300u; b2++) {
                    synth_render(buf, CTL);
                    for (w = 0; w < 2u * CTL; w++) {
                        if (!pass) ref2[b2 * 2u * CTL + w] = buf[w];
                        else same &= ref2[b2 * 2u * CTL + w] == buf[w];
                    }
                }
            }
            okk &= same;
        }
        synth_kill();
        sys.fx_mix = 0;
        check("FX resonators: every model x STRUCTURE x DAMPING x note bounded; MIX 0 bit for bit dry", okk);
    }

    /* polyphonic resonators: a chord's notes each take their own resonator, tuned to it; the wet output (MIX 127)
     * holds every note of the chord; a fifth note takes the oldest; all bounded (four ringing at once); once quiet
     * none runs */
    {
        static const uint8_t CH[4] = {48, 52, 55, 59};
        uint32_t md, j, b2, okk = 1, dist = 1, tuned = 1, heard = 1;
        int32_t pk2 = 0;
        sys.chaos = 85; sys.shape = 76; sys.output = 32; sys.attack = 0; sys.decay = 70; sys.sustain = 100;
        sys.release = 60; sys.ring = 50; sys.fx_bright = 90; sys.fx_pos = 20; sys.fx_mix = 127; sys.fx_damp = 110;
        sys.osc = 0;
        for (md = 0; md < RZ_COUNT; md++) {
            double re[4] = {0}, im[4] = {0}, tot = 0;
            uint32_t t = 0;
            synth_kill();
            sys.fx_model = (uint8_t)md; sys.fx_struct = md == RZ_MODAL ? 34 : 20;
            sys.fx_mix = 0; run_ms(10); sys.fx_mix = 127;     /* (state anew) */
            for (j = 0; j < 4u; j++)
                synth_note_on(CH[j], 110);
            for (j = 0; j < 4u; j++) {
                uint32_t q;
                for (q = 0; q < j; q++)
                    dist &= voice[j].rz != voice[q].rz;
                tuned &= rz[voice[j].rz].note == CH[j];
            }
            run_ms(100);
            for (b2 = 0; b2 < 600u; b2++) {
                synth_render(buf, CTL);
                for (w = 0; w < CTL; w++, t++) {
                    double v = buf[2 * w];
                    int32_t a2 = buf[2 * w] < 0 ? -buf[2 * w] : buf[2 * w];
                    pk2 = a2 > pk2 ? a2 : pk2;
                    tot += v * v;
                    for (j = 0; j < 4u; j++) {                 /* each note's fundamental */
                        double ph = 2 * M_PI * 440.0 * pow(2, (CH[j] - 69) / 12.0) * t / FS;
                        re[j] += v * cos(ph); im[j] += v * sin(ph);
                    }
                }
            }
            for (j = 0; j < 4u && md < RZ_VOICE; j++) {       /* its share of the energy: > 0.5 % (Rings' models: */
                double e = 2 * (re[j] * re[j] + im[j] * im[j]) / t / tot;   /* the waveguides: tested below) */
                if (e < 0.005) {
                    printf("   %s: note %u only %.4f of the energy\n", RZ_NAME[md], CH[j], e);
                    heard = 0;
                }
            }
        }
        {   /* a fifth note, all four held: it steals a voice (NVOICE), and takes the oldest resonator (48's) */
            uint32_t k3, r48 = RZ_POLY, found = 0;
            for (k3 = 0; k3 < NVOICE; k3++)
                if (voice[k3].note == 48u && voice[k3].stage != ST_OFF)
                    r48 = voice[k3].rz;
            synth_note_on(62, 110);
            for (k3 = 0; k3 < NVOICE; k3++)
                if (voice[k3].note == 62u && voice[k3].stage == ST_ATTACK) {
                    okk &= voice[k3].rz == r48;
                    found = 1;
                }
            okk &= found && r48 < RZ_POLY && rz[r48].note == 62u;
        }
        sys.fx_damp = 0;                                         /* (the shortest decay: they go quiet) */
        synth_kill();
        run_ms(4000);
        for (j = 0; j < RZ_POLY; j++)
            okk &= rz[j].quiet >= RZ_QUIET;
        printf("   (polyphonic resonators: peak %d)\n", pk2);
        sys.fx_mix = 0;
        check("FX resonators polyphonic: a chord, one tuned resonator a note, all heard", dist && tuned && heard);
        check("... a fifth note takes the oldest; bounded; quiet ones stop running", okk && pk2 <= 32767);
    }

    /* the waveguides: the flute speaks at the note (STRUCTURE low), overblows to its octave and twelfth; the syrinx
     * whistles two octaves up and its pitch rises with CHIRP; the throat's vowels move its spectrum (I brighter than
     * U); each sounds with the voices and goes quiet without them */
    {
        static int32_t o[44100 / 2];
        uint32_t okk = 1, q, w2;
        struct { uint32_t md; uint8_t st, pos; double lo, hi; } T[] = {
            {RZ_FLUTE, 0, 30, 0.96, 1.04}, {RZ_FLUTE, 100, 30, 1.9, 2.1}, {RZ_FLUTE, 127, 30, 2.85, 4.15},
            {RZ_SYRINX, 0, 0, 3.5, 9}, {RZ_SYRINX, 127, 0, 5.0, 25},
            {RZ_BRASS, 0, 0, 1.96, 2.04}, {RZ_BOW, 120, 60, 0.96, 1.04}, {RZ_REED, 40, 0, 0.96, 1.04},
            {RZ_REED, 60, 70, 1.96, 2.04},   /* (brass and the cone: their second harmonic the strongest, in tune) */
        };
        double fsy[2] = {0, 0};
        sys.osc = 11; sys.np_x = sys.np_y = 64; sys.np_level = 64;   /* (CLUSTER SAW: a steady breath) */
        sys.attack = 0; sys.sustain = 127; sys.release = 20; sys.fx_bright = 70; sys.fx_damp = 90; sys.sh_mix = 0;
        synth_kill(); memset(voice, 0, sizeof voice); rng_state = 0x1234567u; voice_clock = 0;   /* (the same
                                                  * voices and noise whatever ran before) */
        sys.basis = 0;
        for (q = 0; q < sizeof T / sizeof T[0]; q++) {
            double best = 0, f = 0;
            uint32_t k2;
            synth_kill(); sys.fx_mix = 0; run_ms(20);
            sys.fx_model = (uint8_t)T[q].md; sys.fx_struct = T[q].st; sys.fx_pos = T[q].pos; sys.fx_mix = 127;
            synth_note_on(57, 110);
            run_ms(300);
            for (w2 = 0; w2 < sizeof o / sizeof o[0] / CTL; w2++) {
                synth_render(buf, CTL);
                for (k2 = 0; k2 < CTL; k2++)
                    o[w2 * CTL + k2] = buf[2 * k2];
            }
            for (k2 = 100; k2 < 4000; k2 += 5) {           /* the strongest of a comb of frequencies: 100 Hz .. 4 kHz */
                double re = 0, im = 0, e;
                uint32_t i2;
                for (i2 = 0; i2 < sizeof o / sizeof o[0]; i2 += 2) {
                    double ph = 2 * M_PI * k2 * i2 / FS;
                    re += o[i2] * cos(ph);
                    im += o[i2] * sin(ph);
                }
                e = re * re + im * im;
                if (e > best) { best = e; f = k2; }
            }
            if (T[q].md == RZ_SYRINX) {                    /* (the whistle: its period, by autocorrelation; the
                                                            * strongest peak is whichever harmonic the beak lifts) */
                uint32_t L, bl = 5;
                double bc = -1e300;
                for (L = 5; L < 70u; L++) {
                    double c = 0, e0 = 1;
                    uint32_t i2;
                    for (i2 = 0; i2 + L < sizeof o / sizeof o[0]; i2++) {
                        c += (double)o[i2] * o[i2 + L];
                        e0 += (double)o[i2] * o[i2];
                    }
                    if (c / e0 > bc) { bc = c / e0; bl = L; }
                }
                f = (double)FS / bl;
            }
            printf("   (%s STRUCTURE %u: strongest %.0f Hz, %.2f x the note)\n", RZ_NAME[T[q].md], T[q].st, f, f / 220.0);
            if (T[q].md == RZ_SYRINX)
                fsy[T[q].st ? 1 : 0] = f;
            okk &= f / 220.0 >= T[q].lo && f / 220.0 <= T[q].hi;
        }
        okk &= fsy[1] > 1.3 * fsy[0];                     /* (the chirp: up) */
        {   /* the voice: U darker than I (the energy above 1.5 kHz) */
            double hi2[2];
            uint32_t v2;
            for (v2 = 0; v2 < 2u; v2++) {
                double eh = 0, et = 0;
                synth_kill(); sys.fx_mix = 0; run_ms(20);
                sys.fx_model = RZ_VOICE; sys.fx_struct = 40; sys.fx_pos = v2 ? 127 : 0; sys.fx_mix = 127;
                synth_note_on(57, 110);
                run_ms(300);
                for (w2 = 0; w2 < 400u; w2++) {       /* the energy at 2..3 kHz (an I's second formant) against
                                                       * 500..1000 Hz (where an U's first two sit) */
                    synth_render(buf, CTL);
                    for (q = 0; q < CTL; q++)
                        o[w2 * CTL + q] = buf[2 * q];
                }
                for (q = 500; q <= 3000; q += 100) {
                    double re = 0, im = 0;
                    uint32_t i2;
                    if (q > 1000 && q < 2000)
                        continue;
                    for (i2 = 0; i2 < 400u * CTL; i2++) {
                        double ph = 2 * M_PI * q * i2 / FS;
                        re += o[i2] * cos(ph);
                        im += o[i2] * sin(ph);
                    }
                    if (q >= 2000) eh += re * re + im * im; else et += re * re + im * im;
                }
                hi2[v2] = eh / (et + 1);
            }
            printf("   (VOICE, SOPRANO: 2..3 kHz against 0.5..1 kHz: U %.3f, I %.3f)\n", hi2[0], hi2[1]);
            okk &= hi2[1] > 4 * hi2[0];
        }
        {   /* silence in, silence out (no self-oscillation without breath) */
            uint32_t md2;
            for (md2 = RZ_VOICE; md2 < RZ_COUNT; md2++) {
                int32_t pk = 0;
                synth_kill(); sys.fx_mix = 0; run_ms(20);
                sys.fx_model = (uint8_t)md2; sys.fx_damp = 127; sys.fx_mix = 127;
                synth_note_on(57, 110); run_ms(200); synth_kill(); run_ms(md2 == RZ_DRUM ? 9000 : md2 == RZ_KALIMBA ? 14000 : 3000);   /* (the drum rings 4 s, a low tine ~9) */
                for (w2 = 0; w2 < 100u; w2++) {
                    synth_render(buf, CTL);
                    for (q = 0; q < CTL; q++) pk = abs(buf[2 * q]) > pk ? abs(buf[2 * q]) : pk;
                }
                if (pk > 30) { printf("   (%s still %d 3 s after)\n", RZ_NAME[md2], pk); okk = 0; }
            }
        }
        synth_kill();
        sys.osc = 0; sys.fx_mix = 0;
        check("FX waveguides: flute registers, syrinx chirp, throat vowels, quiet without breath", okk);
    }

    /* aftertouch is the breath (once one has come): a BRASS note pressed down is louder and brighter than one
     * resting; polyphonic aftertouch the same for its note; SCREAM (the voices driving the breath) bounded, and it
     * changes the sound */
    {
        uint32_t q, okk = 1;
        double rms[3], cen[3];
        int32_t pk = 0;
        sys.osc = 0; sys.chaos = 6; sys.shape = 64; sys.output = 64; sys.attack = 20; sys.decay = 70; sys.sustain = 115;
        sys.release = 40; sys.ring = 20; sys.fx_model = RZ_BRASS; sys.fx_struct = 100; sys.fx_bright = 100;
        sys.fx_damp = 80; sys.fx_pos = 0; sys.sh_mix = 0;
        for (q = 0; q < 3u; q++) {
            static int32_t o[44100 / 4];
            uint32_t w2, k2;
            double e = 0, num = 0, den = 0;
            synth_kill(); sys.fx_mix = 0; run_ms(20); sys.fx_mix = 127;
            midi_in_source[mi_w % MQ] = 1;
            if (q < 2u)
                midi_in(0xD0, q ? 127 : 0, 0);
            else {
                midi_in(0xD0, 0, 0);
                midi_in_source[mi_w % MQ] = 1; midi_in(0xA0, 57, 127);   /* (poly: after the note on) */
            }
            synth_note_on(57, 100);
            run_ms(400);
            for (w2 = 0; w2 < sizeof o / sizeof o[0] / CTL; w2++) {
                synth_render(buf, CTL);
                for (k2 = 0; k2 < CTL; k2++) o[w2 * CTL + k2] = buf[2 * k2];
            }
            for (k2 = 0; k2 < sizeof o / sizeof o[0]; k2++) e += (double)o[k2] * o[k2];
            for (k2 = 200; k2 < 5000; k2 += 100) {
                double re = 0, im = 0, en;
                uint32_t i2;
                for (i2 = 0; i2 < sizeof o / sizeof o[0]; i2 += 2) {
                    double ph = 2 * M_PI * k2 * i2 / FS;
                    re += o[i2] * cos(ph); im += o[i2] * sin(ph);
                }
                en = re * re + im * im; num += en * k2; den += en;
            }
            rms[q] = sqrt(e / (sizeof o / sizeof o[0]));
            cen[q] = num / (den + 1);
        }
        printf("   (BRASS: aftertouch 0: rms %.0f, centroid %.0f Hz; 127: %.0f, %.0f Hz; poly 127: %.0f, %.0f Hz)\n",
               rms[0], cen[0], rms[1], cen[1], rms[2], cen[2]);
        okk &= rms[1] > 1.6 * rms[0] && cen[1] > 0.98 * cen[0] && rms[2] > 1.6 * rms[0];   /* (not pressed: as without it) */
        {   /* SCREAM: the chaotic circuit on the breath, every waveguide, bounded */
            uint32_t md, b2;
            sys.chaos = 110; sys.output = 32; sys.scream = 127;
            midi_in_source[mi_w % MQ] = 1; midi_in(0xD0, 127, 0);
            for (md = RZ_VOICE; md < RZ_COUNT; md++) {
                synth_kill(); sys.fx_model = (uint8_t)md;
                synth_note_on(45, 127); synth_note_on(57, 127); synth_note_on(64, 127);
                for (b2 = 0; b2 < 300u; b2++) {
                    synth_render(buf, CTL);
                    for (q = 0; q < 2u * CTL; q++) pk = abs(buf[q]) > pk ? abs(buf[q]) : pk;
                }
            }
            okk &= pk <= 8388607;
            sys.scream = 0;
        }
        synth_kill();
        sys.at = 0; sys.at_seen = 0; sys.fx_mix = 0; sys.chaos = 85; sys.output = 0;
        check("aftertouch is more breath (channel and poly): up to twice as loud; SCREAM bounded", okk);
    }

    /* the shimmer: bounded in every corner (SIZE x DECAY x SHIMMER x INTERVAL, held chords), no DC; it rises (the
     * tail's spectral weight climbs); MIX 0 bit for bit dry, and mono (left = right) */
    {
        uint32_t sz, dc, sm, iv, b2, okk = 1;
        int32_t pk2 = 0;
        int64_t dcs = 0;
        sys.chaos = 85; sys.shape = 76; sys.output = 32; sys.attack = 0; sys.decay = 60; sys.sustain = 100;
        sys.release = 60; sys.ring = 50; sys.fx_mix = 0; sys.sh_tone = 127; sys.sh_mix = 127;
        for (sz = 0; sz < 128u; sz += 127u)
            for (dc = 0; dc < 128u; dc += 127u)
                for (sm = 0; sm < 128u; sm += 63u)
                    for (iv = 0; iv < SI_COUNT; iv++) {
                        sys.sh_size = (uint8_t)sz; sys.sh_decay = (uint8_t)dc; sys.sh_shimmer = (uint8_t)sm;
                        sys.sh_interval = (uint8_t)iv;
                        synth_note_on(36, 127); synth_note_on(48, 127); synth_note_on(55, 127); synth_note_on(64, 127);
                        for (b2 = 0; b2 < 400u; b2++) {
                            if (b2 == 200u)
                                synth_all_off();
                            synth_render(buf, CTL);
                            for (w = 0; w < 2u * CTL; w++) {
                                int32_t a2 = buf[w] < 0 ? -buf[w] : buf[w];
                                pk2 = a2 > pk2 ? a2 : pk2;
                                if (b2 >= 300u)
                                    dcs += buf[w];
                            }
                        }
                        synth_kill();
                    }
        printf("   (shimmer: peak %d, mean of the tails %.1f)\n", pk2, (double)dcs / (64.0 * 100 * 2 * CTL));
        okk &= pk2 <= 32767 && dcs / (64 * 100 * 2 * CTL) < 64 && dcs / (64 * 100 * 2 * CTL) > -64;
        {   /* it sinks: a high pure note's tail with SHIMMER 127, -2 OCT: its spectrum's centroid (the energy-weighted
             * frequency) falls by a fifth and more from just after the note to 0.7 s later (the plain tail rings on) */
            static int32_t w0[200u * CTL], w1[200u * CTL];
            double c[2];
            uint32_t j, q2;
            synth_kill(); memset(voice, 0, sizeof voice); rng_state = 0x1234567u; voice_clock = 0;   /* (the same
                                                  * voices whatever ran before) */
            sys.sh_mix = 0; sys.fx_mix = 0; sys.osc = 0; sys.sh_space = 0;
            fx_shimmer(buf, buf, buf, 0);          /* (clear) */
            sys.chaos = 6; sys.output = 64; sys.sustain = 0; sys.decay = 70; sys.release = 60; sys.ring = 30;
            sys.sh_mix = 127; sys.sh_size = 110; sys.sh_decay = 110; sys.sh_shimmer = 127; sys.sh_interval = SI_2OCT;
            sys.sh_tone = 100;
            synth_note_on(84, 110);
            for (b2 = 0; b2 < 1200u; b2++) {
                if (b2 == 20u)
                    synth_note_off(84);
                synth_render(buf, CTL);
                for (j = 0; j < CTL; j++) {
                    if (b2 >= 20u && b2 < 220u) w0[(b2 - 20u) * CTL + j] = buf[2 * j];
                    if (b2 >= 1000u) w1[(b2 - 1000u) * CTL + j] = buf[2 * j];
                }
            }
            for (q2 = 0; q2 < 2u; q2++) {
                const int32_t *w3 = q2 ? w1 : w0;
                double num = 0, den = 0;
                uint32_t f;
                for (f = 60; f < 4000u; f += 40u) {
                    double re = 0, im = 0, e;
                    for (j = 0; j < 200u * CTL; j += 2u) {
                        double ph = 2 * M_PI * f * j / FS;
                        re += w3[j] * cos(ph); im += w3[j] * sin(ph);
                    }
                    e = re * re + im * im; num += e * f; den += e;
                }
                c[q2] = num / (den + 1);
            }
            printf("   (the tail's centroid: %.0f Hz at 0.0..0.15 s after the note, %.0f Hz at 0.7..0.85 s)\n", c[0], c[1]);
            okk &= c[1] < 0.8 * c[0];
            synth_kill();
        }
        {   /* MIX 0: the same output as with the shimmer never on, left = right */
            static int32_t ref3[2 * CTL * 300];
            int pass, same = 1;
            for (pass = 0; pass < 2; pass++) {
                synth_kill(); memset(voice, 0, sizeof voice);
                ghost.x = C1 / 10; ghost.y = ghost.z = 0; rng_state = 0x1234567u; voice_clock = 0;
                sys.sh_mix = 0;
                if (pass)
                    sys.sh_interval = SI_2OCT;
                synth_note_on(55, 100);
                for (b2 = 0; b2 < 300u; b2++) {
                    synth_render(buf, CTL);
                    for (w = 0; w < 2u * CTL; w++) {
                        if (!pass) ref3[b2 * 2u * CTL + w] = buf[w];
                        else same &= ref3[b2 * 2u * CTL + w] == buf[w];
                    }
                    for (w = 0; w < CTL; w++)
                        same &= buf[2 * w] == buf[2 * w + 1];
                }
            }
            okk &= same;
        }
        synth_kill();
        check("shimmer: every corner bounded, no DC, the tail sinks; MIX 0 bit for bit dry and mono", okk);
    }

    /* MASTER at 0: silence */
    sys.master_q12 = 0;
    peak = 0;
    synth_note_on(60, 127);
    run_ms(50);
    check("MASTER 0: silence", peak == 0);
    synth_kill();
    sys.master_q12 = 4096;

    /* a render to listen to: per preset of ui.c, an arpeggio then a held chord */
    if (argc > 1 && (wav = fopen(argv[1], "wb")) != 0) {
        static const uint8_t ARP[] = {48, 55, 60, 63, 67, 72, 67, 63};
        static const struct {
            uint8_t chaos, shape, output, attack, decay, sustain, release, ring;
            uint8_t fx_model, fx_struct, fx_bright, fx_damp, fx_pos, fx_mix;
            uint8_t sh_size, sh_decay, sh_shimmer, sh_mix, sh_interval, sh_tone;
            uint8_t osc, np_x, np_y;
        } P[] = {   /* ui.c PRESETS */
            {85, 76, 0, 6, 80, 110, 70, 50}, {85, 90, 64, 0, 62, 0, 66, 110}, {6, 64, 64, 10, 70, 120, 64, 30},
            {38, 64, 32, 4, 60, 90, 66, 60}, {54, 56, 64, 2, 64, 80, 72, 70}, {30, 100, 32, 0, 50, 0, 52, 100},
            {110, 64, 96, 2, 76, 100, 74, 60}, {124, 40, 64, 0, 56, 60, 60, 40}, {70, 110, 127, 92, 96, 110, 100, 70},
            {96, 16, 0, 0, 54, 70, 50, 40},
            {54, 90, 64, 0, 60, 0, 70, 40, RZ_MODAL, 70, 100, 110, 12, 100},
            {85, 64, 32, 20, 80, 110, 90, 30, RZ_SYMP, 16, 80, 105, 70, 90},
            {30, 76, 64, 0, 64, 40, 74, 50, RZ_STRING, 100, 70, 96, 20, 85},
            {110, 40, 0, 4, 76, 90, 80, 60, RZ_MODAL, 116, 40, 80, 24, 70},
            {6, 64, 64, 30, 80, 100, 96, 30, 0, 0, 0, 0, 0, 0, 110, 110, 100, 96, SI_OCT, 100},
            {85, 76, 32, 60, 90, 110, 100, 50, 0, 0, 0, 0, 0, 0, 127, 120, 50, 80, SI_OCT, 80},
            {54, 90, 64, 0, 62, 0, 70, 40, RZ_MODAL, 70, 100, 110, 12, 70, 90, 110, 120, 100, SI_FIFTH, 110},
            {85, 76, 0, 0, 70, 100, 70, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 40, 90},
            {85, 76, 0, 20, 80, 90, 96, 30, 0, 0, 0, 0, 0, 0, 100, 100, 60, 70, SI_OCT, 90, 7, 70, 50},
            {85, 76, 0, 0, 40, 0, 44, 70, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 96, 64},
            {85, 76, 0, 80, 96, 110, 100, 50, 0, 0, 0, 0, 0, 0, 110, 110, 90, 80, SI_FIFTH, 100, 18, 50, 40},
            {85, 76, 0, 0, 62, 0, 70, 40, RZ_MODAL, 70, 100, 110, 12, 80, 0, 0, 0, 0, 0, 0, 11, 30, 80},
            {85, 76, 0, 2, 76, 100, 74, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 80, 80},
            {85, 76, 0, 0, 64, 60, 60, 50, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 29, 64, 100},
        };
        wav_hdr(wav, 0);
        for (w = 0; w < sizeof P / sizeof P[0]; w++) {
            sys.chaos = P[w].chaos; sys.shape = P[w].shape; sys.output = P[w].output;
            sys.attack = P[w].attack; sys.decay = P[w].decay; sys.sustain = P[w].sustain;
            sys.release = P[w].release; sys.ring = P[w].ring;
            sys.fx_model = P[w].fx_model; sys.fx_struct = P[w].fx_struct; sys.fx_bright = P[w].fx_bright;
            sys.fx_damp = P[w].fx_damp; sys.fx_pos = P[w].fx_pos; sys.fx_mix = P[w].fx_mix;
            sys.sh_size = P[w].sh_size; sys.sh_decay = P[w].sh_decay; sys.sh_shimmer = P[w].sh_shimmer;
            sys.sh_mix = P[w].sh_mix; sys.sh_interval = P[w].sh_interval; sys.sh_tone = P[w].sh_tone;
            sys.osc = P[w].osc; sys.np_x = P[w].np_x; sys.np_y = P[w].np_y;
            for (k = 0; k < sizeof ARP; k++) {
                fm1_in.notes = 1u << key_of(ARP[k]);
                run_ms(170);
                fm1_in.notes = 0;
                run_ms(30);
            }
            fm1_in.notes = 1u << key_of(60) | 1u << key_of(63) | 1u << key_of(67);
            run_ms(1200);
            fm1_in.notes = 0;
            run_ms(900);
        }
        fseek(wav, 0, SEEK_SET);
        wav_hdr(wav, wav_frames);
        fclose(wav);
        printf("wrote %s (%u frames)\n", argv[1], wav_frames);
    }

    printf(bad ? "basis test: %d FAILED\n" : "basis test passed\n", bad);
    return bad != 0;
}
