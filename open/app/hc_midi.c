/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* HAWK CHUA on FM1 Open: MIDI. The SDK owns every port (USB, the TRS jack, Bluetooth LE); this is the hand-over.
 *
 * In:  fm1_app_midi_received (the SDK's routing task) -> midi_in_q, a ring of 4-byte USB-MIDI style packets
 *      (CIN | status << 8 | d1 << 16 | d2 << 24) with their source, read by keys.c in the audio interrupt.
 * Out: midi_out_event (keys.c, arp.c: the audio interrupt) -> midi_out_q -> hc_midi_poll (the control task) sends
 *      each message on USB (GLO: USB OUT on), Bluetooth LE and the TRS jack, where the SDK has them. */
#define MQ 64u
static uint32_t midi_in_q[MQ], midi_out_q[MQ];
static uint8_t midi_in_source[MQ];               /* 1 USB, 2 TRS, 3 Bluetooth LE (keys.c: GLO turns 1 and 2 off) */
static volatile uint32_t mi_w, mi_r, mo_w, mo_r;
static volatile uint8_t midi_in_overflow;        /* the audio interrupt drops the ring and releases every note */
static struct { uint8_t config, suspended; } usb; /* (ui.c's status: USB connected) */

static uint32_t midi_len(uint32_t st)            /* a message's bytes from its status */
{
    if (st >= 0xF8u || st == 0xF6u)
        return 1;
    if (st == 0xF1u || st == 0xF3u || (st & 0xF0u) == 0xC0u || (st & 0xF0u) == 0xD0u)
        return 2;
    return st == 0xF2u || (st >= 0x80u && st < 0xF0u) ? 3u : 0u;
}

void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n)
{
    uint32_t at, st, pkt;
    if (!m || !n || n > 3u || n != midi_len(m[0]))
        return;
    st = m[0];
    pkt = (st >= 0xF0u ? 0x0Fu : st >> 4) | st << 8 | (n > 1u ? (uint32_t)m[1] << 16 : 0u) |
          (n > 2u ? (uint32_t)m[2] << 24 : 0u);
    if (midi_in_overflow)
        return;
    if (mi_w - mi_r >= MQ) {
        midi_in_overflow = 1;
        return;
    }
    at = mi_w % MQ;
    midi_in_q[at] = pkt;
    midi_in_source[at] = (uint8_t)(source == FM1_MIDI_USB ? 1u : source == FM1_MIDI_DIN ? 2u : 3u);
    RING_PUBLISH();
    mi_w++;
}

static void midi_out_event(uint32_t pkt)         /* from the audio interrupt */
{
    if (mo_w - mo_r < MQ) {
        midi_out_q[mo_w % MQ] = pkt;
        RING_PUBLISH();
        mo_w++;
    }
}

static void hc_midi_poll(void)                   /* the control task: the queued messages out */
{
    unsigned ready = fm1_midi_ready_ports();
    usb.config = (uint8_t)(fm1_device_usb_connected() > 0);
    usb.suspended = 0;
    while (mo_r != mo_w) {
        uint32_t pkt = midi_out_q[mo_r % MQ], n = midi_len(pkt >> 8 & 0xFFu);
        uint8_t m[3] = {(uint8_t)(pkt >> 8), (uint8_t)(pkt >> 16), (uint8_t)(pkt >> 24)};
        if (n) {
            if ((sys.midi_usb & 2u) && (ready >> FM1_MIDI_USB & 1u) && fm1_midi_send_port(FM1_MIDI_USB, m, n) == -2)
                break;                           /* (USB full: the rest next time) */
            if (ready >> FM1_MIDI_BLE & 1u)
                (void)fm1_midi_send_port(FM1_MIDI_BLE, m, n);
            if (ready >> FM1_MIDI_DIN & 1u)
                (void)fm1_midi_send_port(FM1_MIDI_DIN, m, n);
        }
        mo_r++;
    }
}
