// SPDX-License-Identifier: GPL-3.0-only
// The audio output: plays the blocks the emulator (emu-worker.js) renders ahead, reports how many it played.
class ChuaOut extends AudioWorkletProcessor {
  constructor() {
    super();
    this.q = [];          // Float32Array chunks, interleaved stereo
    this.off = 0;         // frames into q[0]
    this.played = 0;
    this.n = 0;
    this.port.onmessage = e => {
      if (e.data.port) { this.w = e.data.port; this.w.onmessage = m => this.q.push(m.data); }
    };
  }
  process(_in, outs) {
    const L = outs[0][0], R = outs[0][1] || outs[0][0];
    for (let i = 0; i < L.length; i++) {
      const c = this.q[0];
      if (!c) { L[i] = R[i] = 0; continue; }
      L[i] = c[2 * this.off]; R[i] = c[2 * this.off + 1];
      if (++this.off * 2 >= c.length) { this.q.shift(); this.off = 0; }
      this.played++;
    }
    if (this.w && ++this.n % 4 === 0) this.w.postMessage(this.played);
    return true;
  }
}
registerProcessor("chua-out", ChuaOut);
