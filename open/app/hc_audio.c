/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: the sound, one SDK block (128 frames, stereo) at a time. The audio interrupt
 * (app/board/audio.c) calls hc_render; it mixes in USB and Bluetooth audio and sends the block out
 * (app/audio_output.c). Per sub-block of CTL frames: the keys and MIDI in (keys_block), then the synth (Q15 stereo). */
#define OUT_SHIFT 7                              /* Q15 -> 24-bit: -6 dBFS ceiling, as on the Felucca build */
extern volatile uint16_t hc_load_q8;             /* app/board/audio.c: the interrupt's share of a block */
static int32_t hc_block[2u * HALF_FRAMES] __attribute__((aligned(4)));

unsigned hc_output_level(void) { return 127u; }  /* (MASTER is applied in synth.c: sys.master_q12) */

int hc_render(int32_t *out)
{
    static uint16_t hot, cool;
    uint32_t b, load = hc_load_q8;
    for (b = 0; b < HALF_FRAMES; b += CTL) {
        keys_block();
        synth_render(hc_block + 2u * b, CTL);
    }
    for (b = 0; b < 2u * HALF_FRAMES; b++)
        out[b] = hc_block[b] * (1 << OUT_SHIFT);
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
