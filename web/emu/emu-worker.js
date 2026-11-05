// SPDX-License-Identifier: GPL-3.0-only
// The emulator's thread: emu.wasm (the firmware) runs here, away from the page's own work. It renders the audio
// ahead of the worklet (emu-worklet.js), runs a UI frame every 16 ms and sends the screen and the LEDs to the page.
const FS = 44100, CTL = 32, STEP = 8;        // render 8 blocks (256 frames) at a time
let e = null, mem = null, port = null, played = 0, made = 0, lead = 3072, t0 = 0, freeMade = 0, lastUi = 0;
let ledPrev = "", queue = [], nextOk = 0, hidden = false;
const HOLD_MS = 24;   // (after Felucca's browser build: a click's down and up can come within one block, the FM-1's
                      // contacts never close that short and the UI reads them once a frame: each change of a button or
                      // a key stays 24 ms of the device's time before the next one goes in)
function input() {
  const now = e.emu_now_ms();
  while (queue.length && now - nextOk >= 0) {
    const d = queue.shift();
    if (d.t === "btn") e.emu_button(d.b, d.down); else e.emu_key(d.k, d.down);
    nextOk = now + HOLD_MS;
  }
}
// MIDI CC 20 .. 26 turn SELECT, ALGORITHM, PRESETS, KNOB 1 .. 4 (relative: 65 = +1, 63 = -1; or absolute, by how far
// the value moved), CC 27 the MASTER volume; the firmware's own CC map uses none of them (as Felucca's browser build)
const ccLast = [];
function knobCC(cc, v) {
  if (cc === 27) { e.emu_master(((v * 1023 / 127 | 0) ** 2) >> 8); return true; }
  const r = cc - 20;
  if (r < 0 || r > 6) return false;
  const was = ccLast[r]; ccLast[r] = v;
  if (v === 63 || v === 65) e.emu_turn(r, v - 64);
  else if (was !== undefined) e.emu_turn(r, v - was);
  return true;
}

function store(obj, p, n) {
  postMessage({ t: "store", obj, data: new Uint8Array(e.memory.buffer, p, n).slice() });
}

function render(blocks, keep) {
  if (queue.length) input();
  const n = e.emu_render(blocks);
  if (!keep) return n * CTL;
  const a = new Int32Array(e.memory.buffer, e.emu_audio(), 2 * CTL * n), f = new Float32Array(a.length);
  for (let i = 0; i < a.length; i++) { const v = a[i] / 32768; f[i] = v > 1 ? 1 : v < -1 ? -1 : v; }
  port.postMessage(f, [f.buffer]);
  return n * CTL;
}

function tick() {
  const now = performance.now();
  if (port) {
    if (made < played) made = played;          // (an underrun: start from where the output is)
    while (made < played + lead) made += render(STEP, true);
  } else {                                     // no audio yet: the time runs on the clock, the sound is dropped
    const due = Math.floor((now - t0) * FS / 1000);
    let k = 0;
    while (freeMade < due && k++ < 64) freeMade += render(STEP, false);
    if (freeMade < due - FS) freeMade = due;
  }
  if (now - lastUi >= (hidden ? 250 : 16)) {
    lastUi = now;
    if (e.emu_frame()) {
      const fb = new Uint16Array(e.memory.buffer, e.emu_fb(), 240 * 240).slice();
      postMessage({ t: "fb", fb }, [fb.buffer]);
    }
    const leds = Array.from(new Uint8Array(e.memory.buffer, e.emu_leds(), 11)).join(",");
    if (leds !== ledPrev) { ledPrev = leds; postMessage({ t: "leds", leds: leds.split(",").map(Number) }); }
  }
}

onmessage = async m => {
  const d = m.data;
  if (d.t === "init") {
    const { instance } = await WebAssembly.instantiateStreaming(fetch(d.wasm), { env: { js_store: store } })
      .catch(async () => WebAssembly.instantiate(await (await fetch(d.wasm)).arrayBuffer(), { env: { js_store: store } }));
    e = instance.exports;
    for (const [obj, data] of Object.entries(d.stores || {})) {
      new Uint8Array(e.memory.buffer, e.emu_store_buf(), data.length).set(data);
      e.emu_store(Number(obj), data.length);
    }
    e.emu_init();
    e.emu_master(d.master ?? 2048);
    const ids = []; for (let b = 0; b < 14; b++) ids.push(e.emu_led_id(b));
    postMessage({ t: "ready", ids });
    t0 = performance.now();
    setInterval(tick, 4);
    tick();
  } else if (!e) {
  } else if (d.t === "audio") {
    port = d.port;
    played = made = 0;
    port.onmessage = p => { played = p.data; };
  } else if (d.t === "lead") lead = d.frames;
  else if (d.t === "btn" || d.t === "key") queue.push(d);
  else if (d.t === "hidden") hidden = d.on;
  else if (d.t === "turn") e.emu_turn(d.r, d.s);
  else if (d.t === "midi") { if ((d.st & 0xF0) !== 0xB0 || !knobCC(d.d1, d.d2)) e.emu_midi(d.st, d.d1, d.d2); }
  else if (d.t === "master") e.emu_master(d.q12);
};
