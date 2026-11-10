/* SPDX-License-Identifier: GPL-3.0-only */
/* The keyboard and MIDI in, once per audio block (audio ISR: lowest latency, no locking needed).
 *
 * Keys: the 27 note keys (fm1_in.notes, debounced by the TIMER5 scan in hal/fm1_input.h) play the synth at the
 * octave OCT- / OCT+ set, and go out as MIDI (USB, channel 1). A key remembers the note it started, so changing
 * the octave while a key is down never leaves a note hanging.
 * MIDI in (USB and the TRS jack, usb.c / midi_uart.c rings, each on or off, the GLO page): note on / off on the RX
 * channel (or any: OMNI) plays the synth; the keys go out (USB, if on) on the TX channel;
 * CC 123 (all notes off) releases everything.
 * A note held by several sources (two keys at different octaves, a key and MIDI) ends when the last lets go.
 * ARP on (arp.c) or KARMA on (karma.c, the SEQ page): the notes held go to the one on instead (never both: one
 * turned on turns the other off, the notes held move over); MIDI clock in drives them. Their notes go out on USB MIDI,
 * the keys' own then do not.
 *
 *   keys_block()     audio ISR, before the block renders
 *   keys_panic_req   main loop: set to release everything at the next block */
#define KB_SILENT 0xFFu
#define KEY_VEL 100u                             /* the keys have no velocity: a fixed one */

static volatile uint8_t kb_boot_hold;            /* set from power-on to the end of the splash (main.c): keys silent */
static volatile uint8_t keys_panic_req;
static volatile uint16_t keys_prog_req;          /* a MIDI program change: bank x 128 + its number + 1 (ui.c loads it) */
static uint8_t bank_msb, bank_lsb;               /* BANK SELECT (CC 0, 32): taken by the next program change */
static uint8_t bank_msb_b, bank_lsb_b;           /* (MULTI: part B's) */
static volatile uint16_t keys_prog_req_b;        /* MULTI: a program change on part B's channel (ui.c loads it) */
static uint32_t kb_prev;                         /* fm1_in.notes at the last block */
static uint8_t kb_note[NKEYS];                   /* the note key k started (KB_SILENT: none) */
static uint8_t kb_ch[NKEYS];                     /* the MIDI channel its note on went out on (its off: the same) */
static void kb_out(uint32_t pkt)                 /* a note on to USB MIDI out, if it is on (the GLO page; the offs
                                                  * go always: none left hanging when it is turned off) */
{
    if ((sys.midi_usb & 2u) && !sys.arp_on && !sys.k_on)   /* (ARP, KARMA: what they make goes out instead) */
        midi_out_event(pkt);
}
static uint8_t note_held[128];                   /* sources holding each note */
static uint8_t note_sus[128];                    /* bit 0: let go under a pedal (sounds on), bit 1: held when SOSTENUTO
                                                  * went down (sostenuto holds it) */
static uint8_t sost_on;                          /* SOSTENUTO (CC 66) down */

enum { OWN_SYNTH, OWN_ARP, OWN_KARMA, OWN_BOTH };   /* who the notes held go to (BOTH: LOGIC) */
static uint8_t own_was, arp_was_hold, k_was_latch, k_was_on, arp_was_on;
static volatile uint8_t rt_auto;                 /* MIDI START / CONTINUE (1) or STOP (2) under CLOCK EXTERNAL: keys_block */
static uint32_t owner(void) { return logic_on() ? OWN_BOTH : sys.k_on ? OWN_KARMA : sys.arp_on ? OWN_ARP : OWN_SYNTH; }

static void note_start(uint32_t note, uint32_t vel)
{
    if (mcc.soft)                                /* (SOFT PEDAL, CC 67: softer, x 3/4) */
        vel = vel * 3u / 4u ? vel * 3u / 4u : 1u;
    if (note_sus[note] & 1u) {                   /* (played again while the pedal holds it: no longer pending) */
        note_sus[note] &= 2u;
        play_off(note);
    }
    if (note_held[note] < 255u)
        note_held[note]++;
    switch (own_was) {
    case OWN_ARP: arp_add(note, vel); break;
    case OWN_KARMA: karma_add(note, vel); break;
    case OWN_BOTH: arp_add(note, vel); karma_add(note, vel); break;
    default: play_on(note, vel); break;
    }
}

static void note_end(uint32_t note)
{
    at_poly[note & 127u] = 0;
    if (!note_held[note])
        return;
    if (--note_held[note] == 0u) {
        if (own_was == OWN_SYNTH && (sys.pedal || (note_sus[note] & 2u))) {
            note_sus[note] |= 1u;                /* (HOLD / SOSTENUTO: it sounds on until the pedal comes up) */
            return;
        }
        switch (own_was) {
        case OWN_ARP: arp_remove(note); break;
        case OWN_KARMA: karma_remove(note); break;
        case OWN_BOTH: arp_remove(note); karma_remove(note); break;
        default: play_off(note); break;
        }
    }
}

/* ARP or KARMA turned on or off (or HOLD, LATCH off) under held notes: they move over. The two at once only with
 * LOGIC (the LOGIC page): else the one just turned on wins, the other goes off */
static void arp_follow(void)
{
    uint32_t k, o;
    if (!sys.k_logic && sys.k_on && !k_was_on && sys.arp_on)
        sys.arp_on = 0;
    else if (!sys.k_logic && sys.arp_on && !arp_was_on && sys.k_on)
        sys.k_on = 0;
    k_was_on = sys.k_on;
    arp_was_on = sys.arp_on;
    o = owner();
    if (o != own_was) {
        uint32_t was = own_was;
        own_was = (uint8_t)o;
        if (o == OWN_BOTH && (was == OWN_KARMA || was == OWN_ARP)) {
            /* LOGIC: the one playing goes on, the one added joins at its bar with the same notes */
            if (was == OWN_KARMA) {
                arp_reset();
                for (k = 0; k < kar.n; k++)
                    arp_add(kar.note[k], kar.vel[k]);
            } else {
                for (k = 0; k < arp.n; k++)
                    karma_add(arp.note[k], arp.vel[k]);
            }
            goto latch;
        }
        arp_reset();
        karma_reset();
        synth_all_off();
        for (k = 0; k < 128u; k++)               /* the notes held now: to the one on, or sounding again */
            if (note_held[k]) {
                if (o == OWN_ARP || o == OWN_BOTH)
                    arp_add(k, KEY_VEL);
                if (o == OWN_KARMA || o == OWN_BOTH)
                    karma_add(k, KEY_VEL);
                if (o == OWN_SYNTH)
                    play_on(k, KEY_VEL);
            }
    }
latch:
    if (sys.arp_hold != arp_was_hold) {
        arp_was_hold = sys.arp_hold;
        if (!sys.arp_hold)
            arp_hold_released();
    }
    if (sys.k_latch != k_was_latch) {
        k_was_latch = sys.k_latch;
        if (!sys.k_latch)
            karma_latch_released();
    }
}

static void keys_all_off(void)
{
    uint32_t k;
    for (k = 0; k < 128u; k++)
        note_held[k] = note_sus[k] = 0;
    for (k = 0; k < NKEYS; k++)
        if (kb_note[k] != KB_SILENT) {
            midi_out_event(0x08u | (0x80u | kb_ch[k]) << 8 | (uint32_t)kb_note[k] << 16);
            kb_note[k] = KB_SILENT;
        }
    synth_all_off();
    arp_reset();
    karma_reset();
}

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes, ch = cur ^ kb_prev, k;
    kb_prev = cur;
    if (!ch)
        return;
    for (k = 0; k < NKEYS; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                   /* down */
            int32_t n = KEY_BASE_NOTE + (int32_t)k + 12 * sys.octave;
            if (kb_boot_hold || n < 0 || n > 127) {
                kb_note[k] = KB_SILENT;
                continue;
            }
            kb_note[k] = (uint8_t)n;
            note_start((uint32_t)n, KEY_VEL);
            kb_ch[k] = (uint8_t)(sys.midi_tx & 15u);
            kb_out(0x09u | (0x90u | kb_ch[k]) << 8 | (uint32_t)n << 16 | KEY_VEL << 24);
        } else if (kb_note[k] != KB_SILENT) {    /* up: the note it started */
            note_end(kb_note[k]);
            midi_out_event(0x08u | (0x80u | kb_ch[k]) << 8 | (uint32_t)kb_note[k] << 16);
            kb_note[k] = KB_SILENT;
        }
    }
}

/* the notes the pedals let go (HOLD up, SOSTENUTO up): those that no pedal holds any more end */
static void pedal_release(void)
{
    uint32_t k;
    for (k = 0; k < 128u; k++)
        if ((note_sus[k] & 1u) && !sys.pedal && !(note_sus[k] & 2u)) {
            note_sus[k] = 0;
            play_off(k);
        }
}

/* MIDI control changes: the XG / GM map (as on a Yamaha MU90) where the sound has the thing (bank select: 4 banks of
 * 128 presets, LSB or MSB 0 .. 3; 71 .. 74: the HOME page's knobs). Not here: portamento (5, 65, 84: no glide), RPN / NRPN (the bend's range: the SOUND page), omni / mono */
static void midi_cc(uint32_t cc, uint32_t v)
{
    uint32_t k;
    switch (cc) {
    case 0: bank_msb = (uint8_t)v; break;        /* BANK SELECT MSB, LSB: the bank for the next program change */
    case 32: bank_lsb = (uint8_t)v; break;
    case 1:                                      /* MODULATION: the mod wheel (feg.c mw_block, the EDIT page) */
        sys.mw = (uint8_t)v;
        sys.mw_seen = 1;
        break;
    case 7: mcc.vol = (uint8_t)v; break;         /* MAIN VOLUME (100: as it was; synth_render) */
    case 10: mcc.pan = (uint8_t)v; break;        /* PAN: the balance */
    case 11: mcc.expr = (uint8_t)v; break;       /* EXPRESSION */
    case 64:                                     /* HOLD 1: the notes sound on (and the PIANO's dampers stay up) */
        sys.pedal = v >= 64u;
        if (!sys.pedal)
            pedal_release();
        break;
    case 66:                                     /* SOSTENUTO: holds the notes held as it goes down */
        if (v >= 64u && !sost_on)
            for (k = 0; k < 128u; k++)
                if (note_held[k] || (note_sus[k] & 1u))
                    note_sus[k] |= 2u;
        if (v < 64u && sost_on) {
            for (k = 0; k < 128u; k++)
                note_sus[k] &= 1u;
            pedal_release();
        }
        sost_on = v >= 64u;
        break;
    case 67: mcc.soft = v >= 64u; break;         /* SOFT PEDAL: new notes softer */
    case 71:                                     /* 71 .. 74: the oscillator's KNOB 1 .. 4 as on the HOME page (the */
        if (sys.osc)                             /* circuit: CHAOS, SHAPE, OUTPUT, RING; a noise program: X, Y, */
            sys.np_x = (uint8_t)v;               /* LEVEL, RING) */
        else
            sys.chaos = (uint8_t)v;
        break;
    case 72:
        if (sys.osc)
            sys.np_y = (uint8_t)v;
        else
            sys.shape = (uint8_t)v;
        break;
    case 73:                                     /* (BIG BANG: BANG, EXPAND on CHAOS's and OUTPUT's; PIANO: DECAY,
                                                  * STIFF) */
        if (sys.osc > NP_BIGBANG)
            sys.chaos = (uint8_t)v;
        else if (sys.osc)
            sys.np_level = (uint8_t)v;
        else
            sys.output = (uint8_t)v;
        break;
    case 74:
        if (sys.osc > NP_BIGBANG)
            sys.output = (uint8_t)v;
        else
            sys.ring = (uint8_t)v;
        break;
    case 91: sys.sh_mix = (uint8_t)v; break;     /* EFFECT 1 (reverb send): the SHIMMER's mix */
    case 93: sys.fx_mix = (uint8_t)v; break;     /* EFFECT 3 (chorus send): the FX resonator's mix */
    case 94: sys.sh_shimmer = (uint8_t)v; break; /* EFFECT 4 (variation): the SHIMMER's shimmer */
    case 120:                                    /* ALL SOUND OFF, ALL NOTES OFF */
    case 123:
        keys_all_off();
        break;
    case 121:                                    /* RESET ALL CONTROLLERS (as XG: not volume, pan, the effects) */
        sys.mw = 0;
        synth_bend[0] = synth_bend[1] = 65536;
        mcc.expr = 127;
        mcc.soft = 0;
        sys.at = 0;
        for (k = 0; k < 128u; k++)
            note_sus[k] &= 1u, at_poly[k] = 0;
        sost_on = 0;
        sys.pedal = 0;
        pedal_release();
        break;
    default:
        break;
    }
}

/* MIDI pitch bend (14 bits, 8192 the middle): the part's notes up or down by BEND RANGE semitones (the SOUND page,
 * 0 .. 12), their FX with them */
static void keys_bend(uint32_t part, uint32_t d1, uint32_t d2)
{
    int32_t b = (int32_t)(d1 | d2 << 7) - 8192;  /* -8192 .. 8191 */
    synth_bend[part & 1u] = px_exp2(b * (int32_t)(sys.bend_range > 12u ? 12u : sys.bend_range) * 2 / 3);   /* Q16 */
}
static void midi_in_block(void)
{
    if (midi_in_overflow) {                      /* the ring overflowed: a note-off may be lost, release all */
        mi_r = mi_w;
        keys_all_off();
        RING_PUBLISH();
        midi_in_overflow = 0;
        return;
    }
    while (mi_r != mi_w) {
        uint32_t pkt = midi_in_q[mi_r % MQ], st = (pkt >> 8) & 0xF0u, src = midi_in_source[mi_r % MQ];
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu, rx = sys.midi_rx;
        mi_r++;
        if ((src == 2u && !sys.midi_trs) || (src == 1u && !(sys.midi_usb & 1u)))
            continue;                            /* (the GLO page: that input off) */
        if (sys.multi && sys.ch_b && st < 0xF0u && ((pkt >> 8) & 0x0Fu) == (sys.ch_b + 15u) % 16u) {   /* MULTI: B's channel
                                                  * (OMNI: B layered on A, synth.c's play_on) */
            if (st == 0x90u && d2)
                synth_note_on_b(d1, d2);
            else if (st == 0x80u || st == 0x90u)
                synth_note_off_b(d1);
            else if (st == 0xE0u)
                keys_bend(1, d1, d2);
            else if (st == 0xB0u && d1 == 0u)
                bank_msb_b = (uint8_t)d2;
            else if (st == 0xB0u && d1 == 32u)
                bank_lsb_b = (uint8_t)d2;
            else if (st == 0xB0u && (d1 == 123u || d1 == 120u))
                for (uint32_t k2 = 0; k2 < 128u; k2++)
                    synth_note_off_b(k2);
            else if (st == 0xC0u) {
                uint32_t bk = bank_lsb_b ? bank_lsb_b : bank_msb_b;
                keys_prog_req_b = (uint16_t)((bk < 4u ? bk : 0u) * 128u + d1 + 1u);
            }
            continue;
        }
        if (st < 0xF0u && rx && ((pkt >> 8) & 0x0Fu) != (rx - 1u) % 16u)
            continue;                            /* (another channel than RX) */
        if (st == 0x90u && d2)
            note_start(d1, d2);
        else if (st == 0x80u || st == 0x90u)     /* (note on with velocity 0 is a note off) */
            note_end(d1);
        else if (st == 0xB0u)
            midi_cc(d1, d2);
        else if (st == 0xD0u) {                  /* aftertouch (channel pressure): the breath (resonator.c) */
            sys.at = (uint8_t)d1;
            sys.at_seen = 1;
        }
        else if (st == 0xA0u) {                  /* polyphonic aftertouch: that note's breath */
            at_poly[d1] = (uint8_t)d2;
            sys.at_seen = 1;
        }
        else if (st == 0xC0u)                    /* program change: the preset (ui.c, the main loop) */
        {   /* (the bank: LSB as XG has it, else MSB as GS; 1 .. 3 = banks 2 .. 4, others bank 1: XG's drums 127 ...) */
            uint32_t bk = bank_lsb ? bank_lsb : bank_msb;
            keys_prog_req = (uint16_t)((bk < 4u ? bk : 0u) * 128u + d1 + 1u);
        }
        else if (((pkt >> 8) & 0xFFu) >= 0xF8u) {   /* realtime: the clock for the arpeggiator */
            uint32_t rt = (pkt >> 8) & 0xFFu;
            if (!sys.clk_int && sys.autoplay && (rt == 0xFAu || rt == 0xFBu))   /* (CLOCK EXTERNAL, AUTOPLAY: PLAY over
                                                  * there plays here) */
                rt_auto = 1;
            else if (!sys.clk_int && sys.autoplay && rt == 0xFCu)
                rt_auto = 2;
            arp_midi_rt(rt);
        }
        else if (st == 0xE0u)                    /* pitch bend */
            keys_bend(0, d1, d2);
    }
}

static void keys_init(void)
{
    uint32_t k;
    for (k = 0; k < NKEYS; k++)
        kb_note[k] = KB_SILENT;
}

/* the MOD ENV on a parameter (sys.menv_target, not the pitch): one envelope for the instrument, from 0 again at each
 * note (as noise.c's per voice: LENGTH, SKEW, BEND), HEIGHT the most it moves the parameter (up, or down: bit 7), on
 * top of where its knob is; given back as it falls (only what it moved: the knob may be turned meanwhile) */
/* (its state: menv_p, synth.c) */
static void menv_undo(void)
{
    if (menv_p.target && menv_p.moved) {
        volatile uint8_t *f = feg_field(menv_p.target - 1u);
        *f = (uint8_t)clamp((int32_t)*f - menv_p.moved, 0, 127);
    }
    menv_p.moved = 0;
}
static void menv_param(void)
{
    uint32_t len, a, ia, id, t = sys.menv_target;
    int32_t want, h;
    volatile uint8_t *f;
    if (t != menv_p.target) {                    /* (another parameter: the last one given back) */
        menv_undo();
        menv_p.target = (uint8_t)t;
        menv_p.stage = 2;
    }
    if (!t)
        return;
    if (synth_ons != menv_p.ons) {               /* (a note: from 0 again) */
        menv_p.ons = synth_ons;
        menv_p.stage = 0;
        menv_p.env = 0;
    }
    len = ENV_FRAMES[sys.menv_len & 127u];
    a = len * (sys.menv_skew & 127u) / 127u;
    ia = a > CTL ? ENV_MAX / a * CTL : ENV_MAX;   /* (a block's step: CTL samples) */
    id = len - a > CTL ? ENV_MAX / (len - a) * CTL : ENV_MAX;
    if (menv_p.stage == 0) {
        if (ENV_MAX - menv_p.env <= ia) { menv_p.env = ENV_MAX; menv_p.stage = 1; } else menv_p.env += ia;
    } else if (menv_p.stage == 1) {
        if (menv_p.env <= id) { menv_p.env = 0; menv_p.stage = 2; } else menv_p.env -= id;
    }
    h = (int32_t)(sys.menv_height & 127u);
    want = menv_p.stage == 2 ? 0 : (h * np_bend((int32_t)(menv_p.env >> 15), sys.menv_bend)) >> 15;
    if (sys.menv_height & 0x80u)
        want = -want;
    f = feg_field(t - 1u);
    {
        int32_t base = (int32_t)*f - menv_p.moved, now = clamp(base + want, 0, 127);
        *f = (uint8_t)now;
        menv_p.moved = (int16_t)(now - base);
    }
}

static void keys_block(void)
{
    if (keys_panic_req) {
        keys_panic_req = 0;
        keys_all_off();
        synth_kill();
    }
    if (rt_auto == 1u && (sys.arp_on || sys.k_on))   /* (one playing already: START only restarts it, arp.c) */
        rt_auto = 0;
    if (rt_auto == 1u) {                         /* MIDI START (CLOCK EXTERNAL), nothing on: the ARP and KARMA on,
                                                  * together (LOGIC OR if it was off) */
        if (!sys.k_logic)
            sys.k_logic = 1;
        sys.arp_on = 1;
        sys.k_on = 1;
        rt_auto = 3;                             /* (the notes after arp_follow has moved what is held over) */
    } else if (rt_auto == 2u) {                  /* MIDI STOP: both off */
        sys.arp_on = 0;
        sys.k_on = 0;
        rt_auto = 0;
    }
    arp_follow();
    if (rt_auto == 3u) {                         /* (no notes chosen yet: C major, latched, till a chord replaces it) */
        rt_auto = 0;
        if (!arp.n && !kar.n) {
            static const uint8_t CMAJ[3] = {60, 64, 67};
            uint32_t i;
            sys.arp_hold = 1;
            sys.k_latch = 1;
            for (i = 0; i < 3u; i++)
                note_start(CMAJ[i], KEY_VEL);
            for (i = 0; i < 3u; i++)
                note_end(CMAJ[i]);
        }
    }
    keyboard_block();
    midi_in_block();
    arp_block(CTL);
    karma_block(CTL);
    {   /* GATE FIT (synth.c): a quarter of the ARP's gate, of KARMA's step */
        uint32_t c = 0;
        if (sys.arp_on && arp.n && arp.len)
            c = arp.len * (8u + (sys.arp_gate & 127u) * 92u / 127u) / 400u;
        if (sys.k_on && kar.n && kar.steplen && (!c || kar.steplen / 4u < c))
            c = kar.steplen / 4u;
        env_cap = c;
    }
    menv_param();
    feg_block(CTL, voice_clock);
    mw_block();
    tape_block();                 /* (voice_clock: the note ons, the arpeggiator's too) */
}
