// SPDX-License-Identifier: GPL-3.0-only
// HAWK CHUA on FM1 Open: put HAWK CHUA's data (the sound's and the UI's code, the presets) into the FM1's user flash
// over Web MIDI (SysEx). The protocol is the firmware's (open/app/hc_data.c, after Orkest's bank): INFO, BEGIN, DATA
// (512 bytes a message, each answered with the next offset), END (the FM1 checks it all, then commits). A NAK carries
// the offset the FM1 expects: the upload resumes from there. Nothing is used until END succeeds.

const HC_HDR = [0xF0, 0x7D, 0x48, 0x43];
const HC_CHUNK = 512;
const HC_PORTS = /fm.?1|hawk|usb.?midi/i;
export const HC_STATES = ["OK", "NOT INSTALLED", "DAMAGED", "DAMAGED (CRC)", "FOR ANOTHER BUILD", "ADDRESS", "LOADING"];

function hput7(v, n) {
  const out = [];
  for (let i = 0; i < n; i++, v = Math.floor(v / 128)) out.push(v & 0x7F);
  return out;
}
function hget7(a, off, n) {
  let v = 0;
  for (let i = n - 1; i >= 0; i--) v = v * 128 + a[off + i];
  return v;
}
function hpack(bytes) {                      // 8 bytes per 7: the first holds the others' top bits
  const out = [];
  for (let g = 0; g < bytes.length; g += 7) {
    let top = 0;
    const grp = bytes.subarray(g, g + 7);
    grp.forEach((b, i) => { top |= (b >> 7) << i; });
    out.push(top);
    for (let i = 0; i < 7; i++) out.push(i < grp.length ? grp[i] & 0x7F : 0);
  }
  return out;
}
const hcFail = (code, message, detail = "") => Object.assign(new Error(message), { code, detail });

export class DataLink {
  constructor(input, output) {
    this.input = input; this.output = output; this.waiter = null;
    this.onmsg = (e) => {
      const d = e.data;
      if (d.length >= 6 && d[0] === 0xF0 && d[1] === 0x7D && d[2] === 0x48 && d[3] === 0x43 && this.waiter) {
        const w = this.waiter; this.waiter = null; w(d.subarray(4, d.length - 1));
      }
    };
    input.addEventListener("midimessage", this.onmsg);
  }
  close() { this.input.removeEventListener("midimessage", this.onmsg); }
  request(body, ms = 3000) {
    return new Promise((resolve) => {
      const t = setTimeout(() => { this.waiter = null; resolve(null); }, ms);
      this.waiter = (r) => { clearTimeout(t); resolve(r); };
      try { this.output.send([...HC_HDR, ...body, 0xF7]); } catch (e) { clearTimeout(t); this.waiter = null; resolve(null); }
    });
  }
  async info() {
    const r = await this.request([0x10], 800);
    if (!r || r[0] !== 0x11) return null;
    return { state: r[1], total: hget7(r, 2, 5), room: hget7(r, 7, 5), version: String.fromCharCode(...r.subarray(12)) };
  }
}

// an FM1 running HAWK CHUA on FM1 Open: the first port pair that answers INFO
export async function findHawk(midi) {
  for (const input of midi.inputs.values()) {
    if (input.state !== "connected" || !HC_PORTS.test(input.name || "")) continue;
    const output = [...midi.outputs.values()].find((o) => o.name === input.name && o.state !== "disconnected");
    if (!output) continue;
    const link = new DataLink(input, output);
    const info = await link.info();
    if (info) return { link, info };
    link.close();
  }
  return null;
}

// write the data: progress(bytesDone, total). Throws with .code: notfound, toobig, lost, rejected, check (.detail)
export async function uploadData(midi, data, progress = () => {}) {
  const dev = await findHawk(midi);
  if (!dev) throw hcFail("notfound", "HAWK CHUA (FM1 Open) not found on any MIDI port");
  const { link, info } = dev;
  try {
    if (data.length > info.room) throw hcFail("toobig", `the data is ${data.length} B, the FM1 holds ${info.room} B`);
    let r = await link.request([0x20, ...hput7(data.length, 5)], 5000);
    if (!r || r[0] !== 0x7E) throw hcFail(r ? "rejected" : "lost", "the FM1 did not start the upload");
    let off = 0, tries = 0;
    while (off < data.length) {
      const chunk = data.subarray(off, Math.min(data.length, off + HC_CHUNK));
      let sum = 0;
      chunk.forEach((b) => { sum ^= b; });
      r = await link.request([0x21, ...hput7(off, 5), ...hput7(chunk.length, 2), ...hpack(chunk), sum & 0x7F], 3000);
      if (r && r[0] === 0x7E && r[1] === 0x21) {
        off = hget7(r, 2, 5); tries = 0; progress(off, data.length);
        continue;
      }
      if (++tries > 4) throw hcFail("lost", `no answer at ${off} B`);
      if (r && r[0] === 0x7D) off = hget7(r, 3, 5);         // the FM1 says where it is: resume there
      else await new Promise((s) => setTimeout(s, 300));
    }
    r = await link.request([0x22], 15000);                  // END: the FM1 checks it all (CRC, build, address)
    if (!r || r[0] !== 0x7E) {
      if (r && r[0] === 0x7D && r[2] >= 16) {
        const st = r[2] - 16, at = hget7(r, 3, 5);
        throw hcFail("check", "the FM1 did not accept the data", `${HC_STATES[st] || st}${st === 5 ? " 0x" + at.toString(16) : ""}`);
      }
      throw hcFail(r ? "check" : "lost", "the FM1 did not accept the data");
    }
    progress(data.length, data.length);
    return await link.info();
  } finally {
    link.close();
  }
}
