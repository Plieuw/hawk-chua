/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the sound, one SDK block (128 frames, stereo) at a time. The audio interrupt
 * (app/board/audio.c) calls hc_render; it mixes in USB and Bluetooth audio and sends the block out
 * (app/audio_output.c). Per sub-block of CTL frames: the keys and MIDI in (keys_block), then the synth (Q15 stereo).
 * The sound's code lives in HAWK CHUA's data (hc_data.c), as the UI's: without it, or while it is written, silence. */
#define OUT_SHIFT 7                              /* Q15 -> 24-bit: -6 dBFS ceiling, as on the Felucca build */
extern volatile uint16_t hc_load_q8;             /* app/board/audio.c: the interrupt's share of a block */
static int32_t hc_block[2u * HALF_FRAMES] __attribute__((aligned(4)));

unsigned hc_output_level(void) { return 127u; }  /* (MASTER is applied in synth.c: sys.master_q12) */

/* the sound, in HAWK CHUA's data (hc_data.c): called only through hc_ovl_entry_audio, and only while the data is in
 * place (hc_ovl_audio); per sub-block of CTL frames the keys and MIDI in, then the synth */
static void hc_render_ovl(int32_t *out)
{
    uint32_t b;
    for (b = 0; b < HALF_FRAMES; b += CTL) {
        keys_block();
        synth_render(hc_block + 2u * b, CTL);
    }
    for (b = 0; b < 2u * HALF_FRAMES; b++)
        out[b] = hc_block[b] * (1 << OUT_SHIFT);
}
static void (*volatile hc_ovl_entry_audio)(int32_t *) __attribute__((section(".data.hc_ovl_entry"))) = hc_render_ovl;
static volatile uint8_t hc_ovl_audio;            /* the control task: 1 the data is in place, the sound may run */
static volatile uint32_t hc_audio_blocks;        /* blocks rendered or skipped (the control task waits on it) */

int hc_render(int32_t *out)
{
    static uint16_t hot, cool;
    uint32_t b, load = hc_load_q8;
    if (!hc_ovl_audio) {                         /* (no data, or it is being written: silence) */
        for (b = 0; b < 2u * HALF_FRAMES; b++)
            out[b] = 0;
        hc_audio_blocks++;
        return 0;
    }
    hc_ovl_entry_audio(out);
    hc_audio_blocks++;
    /* the load (the screen's CPU) and LIGHT: over ~95 % a step lighter (synth.c), 10 s under 50 %: back */
    sys.cpu_q8 = load;
    if (load > 243u) {
        cool = 0;
        if (++hot >= 3u) {
            hot = 0;
            if (synth_light < 3u)
                synth_light++;
        }
    } else {
        hot = 0;
        if (synth_light > synth_light_min && load < 128u && ++cool >= 3450u) {
            cool = 0;
            synth_light--;
        }
    }
    if (load > 205u)
        synth_shed = 1;                          /* (over 80 %: the synth lets a voice go) */
    return 0;
}
/* the control task: the sound off the data, and wait (bounded) till the audio interrupt is out of it */
static void hc_audio_off(void)
{
    uint32_t n, t0 = fm1_millis();
    hc_ovl_audio = 0;
    RING_PUBLISH();
    n = hc_audio_blocks;
#ifndef FM1_HOST_TEST                            /* (the host test renders between control rounds: nothing to wait for) */
    while (hc_audio_blocks - n < 2u && fm1_millis() - t0 < 30u)
        ;
#endif
    (void)n; (void)t0;
}
