# HAWK CHUA

**A chaotic synthesizer firmware for the M-VAVE FM-1**, by **plieuwski** ([@plieuwski](https://www.instagram.com/plieuwski/)).
Its oscillator is **Chua's circuit**, the simplest electronic circuit that behaves chaotically: every voice is its own
circuit, and the screen draws the attractor it is tracing, live.

[![firmware](https://github.com/Plieuw/hawk-chua/actions/workflows/firmware.yml/badge.svg)](https://github.com/Plieuw/hawk-chua/actions/workflows/firmware.yml)
[![release](https://img.shields.io/github/v/release/Plieuw/hawk-chua)](https://github.com/Plieuw/hawk-chua/releases/latest)

![HAWK CHUA on the FM-1](docs/hawk-chua.png)

| | |
| --- | --- |
| **Install** (Chrome or Edge, USB cable) | **https://plieuw.github.io/hawk-chua/** |
| **Play it in the browser**, no FM-1 needed | https://plieuw.github.io/hawk-chua/webapp/try/ |
| **Download** the `.fwsc` | [latest release](https://github.com/Plieuw/hawk-chua/releases/latest) |
| **Manual** (Dutch) | [docs/HANDLEIDING.md](docs/HANDLEIDING.md) · [PDF](docs/HAWK-CHUA-handleiding.pdf) |
| **How the circuit works** (interactive) | [docs/chua.html](https://plieuw.github.io/hawk-chua/chua.html) |

## What is in it

- **The circuit as an oscillator**: CHAOS (α) walks the classic route to chaos — a pure limit cycle, period doubling,
  a single spiral, the double scroll, the saturated outer cycle; SHAPE (β) where those transitions lie. Pitch is the
  circuit's time scale, so a note stays in tune in the periodic regimes.
- **30 noise oscillators** (after Befaco's Noise Plethora), a piano, cluster saws, FM, and a TB-303 style saw.
- **A low pass gate** per voice (a vactrol's response), a pitch envelope (MOD ENV) and **FREE EG**: a knob movement
  recorded and played back, as an envelope or an LFO, onto any parameter.
- **FX**: 17 physical models (modal, strings, waveguides: voice, flute, brass, bow, reed, kalimba, piano, drum …).
- **SHIMMER**: reverbs (hall, temple, cathedral, mosque), instrument bodies, a Leslie, tempo delays and **MOUTH**, a
  talk box whose vowel can be moved by FREE EG.
- **ARP** (with a velocity and a gate sequence) and **KARMA**, a phrase generator, playing together through **LOGIC**
  (AND, OR, XOR …) on one grid, with a scale quantiser.
- **TAPE**: a looper in the tempo; **MULTI**: two sounds on two MIDI channels; USB-MIDI and USB audio, TRS MIDI.
- **UI STYLE** (GLO, fourth page): six looks for the screen: INITIAL, RAINBOW ROAD, GRADIENT, PSYCHEDELIC, MAGIC CATS and JEZUS, with motion, backdrop strength and how much they react to the sound.
- **512 presets** in four banks (two General MIDI banks, SYNTH, LAB), all at one loudness, plus your own.
- The settings are saved by themselves; works on both FM-1 versions (Puya P25Q80 and UC25HQ80 flash).

## Install

1. Open **https://plieuw.github.io/hawk-chua/** in Chrome or Edge.
2. Connect the FM-1 with a USB **data** cable and press **Install**.

This works from the official firmware, from Felucca and from other FM-1 firmwares; the same page installs later
versions. From the command line: `python3 tools/fm1_install.py hawk-chua.fwsc` (`pip3 install mido python-rtmidi`).
Back to the official firmware: the same tool with M-VAVE's V15 file. A failed install can always be repaired over
UBOOT with [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

## The code

The whole firmware is compiled as one file: [`firmware/src/felucca.c`](firmware/src/felucca.c) includes the others in
order. Where to look:

| File | What |
| --- | --- |
| [`synth.c`](firmware/src/synth.c) | the voices: Chua's circuit (Q20 fixed point RK2), the low pass gate, the output stage |
| [`noise.c`](firmware/src/noise.c) | the noise oscillators, the piano, the clusters |
| [`resonator.c`](firmware/src/resonator.c), [`waveguide.c`](firmware/src/waveguide.c), [`phys_dsp.c`](firmware/src/phys_dsp.c) | the FX models |
| [`shimmer.c`](firmware/src/shimmer.c) | the reverbs, bodies, delays and the MOUTH |
| [`keys.c`](firmware/src/keys.c), [`arp.c`](firmware/src/arp.c), [`karma.c`](firmware/src/karma.c) | keys and MIDI to notes; the ARP, KARMA and LOGIC (in the audio interrupt) |
| [`feg.c`](firmware/src/feg.c), [`tape.c`](firmware/src/tape.c) | FREE EG; the TAPE looper |
| [`ui.c`](firmware/src/ui.c), [`fx_art.c`](firmware/src/fx_art.c) | pages, knobs, LEDs, the presets, the settings; the FX drawings |
| [`core.h`](firmware/src/core.h) | the shared `sys` state (main loop ↔ audio) and constants |
| [`usb.c`](firmware/src/usb.c), [`midi_uart.c`](firmware/src/midi_uart.c), [`ota.c`](firmware/src/ota.c) | USB, TRS MIDI, the update path (keep it: it is how the next build gets on) |
| [`firmware/hal/`](firmware/hal) | the hardware: key matrix, LEDs, encoders, LCD, I2S, flash, the second core |
| [`tests/`](tests) | host tests, no FM-1 needed; [`web/emu/`](web/emu) the firmware as WebAssembly |
| [`tools/`](tools) | the build, table generators, the installer |

House rules (checked by the build): register access only in `hal/`; fixed point in the audio interrupt, and no 64-bit
divisions (the toolchain has none); the audio interrupt finishes within ~2.9 ms.

## Build and test

```
tools/get_toolchain.sh                 # the JieLi toolchain (once)
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
pip3 install Pillow fonttools mido python-rtmidi
./build.sh                             # -> build/hawk-chua.fwsc
sh tests/run_tests.sh                  # host tests
```

Linux x86-64 runs the toolchain directly, macOS in Docker: see [BUILDING.md](BUILDING.md).

**On GitHub** every push to `main` runs one workflow ([firmware.yml](.github/workflows/firmware.yml)): build → tests →
the browser version → the installer site on GitHub Pages; the `.fwsc` is under the run's *Artifacts*. A release:
*Actions → firmware → Run workflow* with a version `X.Y.Z` (one digit each), or push a tag `vX.Y.Z`. Debugging a
CRASH screen's addresses has its own workflow ([debug.yml](.github/workflows/debug.yml)).

**Your own fork**: fork, then *Settings → Pages → Source: GitHub Actions*; the next push publishes your installer at
`https://<you>.github.io/<repo>/`. Give a fork you release its own USB VID/PID (`firmware/src/usb.c`).

## License

GPL-3.0-only, like [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments, which this is
built on (hardware layer, USB, update path). If you share a build, share its source. See [LICENSE](LICENSE),
[LICENSING.md](LICENSING.md) and [CREDITS.md](CREDITS.md).
