#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""PIANO (firmware/src/waveguide.c): its stiffness all-passes per note, fitted to a piano's inharmonicity B.

Each string loop runs at the half rate (22050 Hz); M first-order all-passes of one coefficient a stretch its
partials sharp. For each MIDI note: the M (0..8) and a whose partials (the first 16 below ~9 kHz) come closest to
k f0 sqrt(1 + B k^2) / sqrt(1 + B), and their delay at the fundamental (taken off the loop: the note in tune).
Writes tab.h (WG_PN_M, WG_PN_A, WG_PN_T), pasted into waveguide.c.  python3 tools/piano_dispersion.py"""
import numpy as np
fs = 22050.0
def B_of(n):
    pts = [(21, 3.0e-4), (30, 1.6e-4), (40, 1.3e-4), (50, 2.2e-4), (60, 4.0e-4), (72, 8.5e-4), (84, 2.5e-3), (96, 6.0e-3), (108, 1.5e-2)]
    return float(np.exp(np.interp(n, [p[0] for p in pts], [np.log(p[1]) for p in pts])))
W = np.linspace(1e-4, np.pi*0.999, 40000)
def phi1(w, a):  # phase lag of one first-order allpass (a + z^-1)/(1 + a z^-1)
    return w - 2*np.arctan2(a*np.sin(w), 1 + a*np.cos(w))
A = np.linspace(-0.95, -0.01, 189)
PH = {a: phi1(W, a) for a in A}
rows = []
for n in range(128):
    nn = min(max(n, 21), 108)
    f0 = 440*2**((n-69)/12); w0 = 2*np.pi*f0/fs
    B = B_of(nn)
    K = int(min(16, 0.42*fs/f0))
    best = (1e18, 0, 0.0)
    if K >= 2 and fs/f0 > 4:
        k = np.arange(1, K+1)
        tgt = k*f0*np.sqrt(1+B*k*k)/np.sqrt(1+B)
        for M in range(0, 9):
            for a in ([0.0] if M == 0 else A):
                ph = PH[a] if M else W*0
                tau0 = (np.interp(w0, W, ph)/w0) if M else 0.0
                L = fs/f0 - M*tau0
                if L < 3: continue
                tot = W*L + M*ph
                wk = np.interp(2*np.pi*k, tot, W)
                fk = wk*fs/(2*np.pi)
                err = np.sum((1200*np.log2(fk[1:]/tgt[1:]))**2 / k[1:]) + 0.05*M
                if err < best[0]: best = (err, M, a)
    err, M, a = best
    tau = (M*np.interp(w0, W, PH[a])/w0) if M else 0.0
    rows.append((n, M, round(float(a), 3), round(float(tau), 2), round(float(err), 2)))
for r in rows[21:109:4]: print(r)
with open('tab.h', 'w') as f:
    f.write("/* PIANO's stiffness per note (tools/piano_dispersion.py): its first-order all-passes, their coefficient (Q15)\n * and their delay at the fundamental (half-rate samples, Q8), fitted to a piano's inharmonicity B */\n")
    f.write("static const uint8_t WG_PN_M[128] = {" + ", ".join(str(r[1]) for r in rows) + "};\n")
    f.write("static const int16_t WG_PN_A[128] = {" + ", ".join(str(int(round(r[2]*32768))) for r in rows) + "};\n")
    f.write("static const uint16_t WG_PN_T[128] = {" + ", ".join(str(min(65535, int(round(r[3]*256)))) for r in rows) + "};\n")
