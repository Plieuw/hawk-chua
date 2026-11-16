# HAWK CHUA on FM1 Open

HAWK CHUA on **[FM1 Open](https://github.com/m-vave/fm1_open)**, M-VAVE's own open firmware for the FM-1. The sound
and the UI are HAWK CHUA's (the same sources as the Felucca build, `../firmware/src`); the platform is M-VAVE's SDK:
audio out, **Bluetooth LE MIDI**, USB MIDI, the TRS jack, USB and Bluetooth audio, the panel, the screen, the flash and
the official update path. Nothing of the Felucca build's hardware layer comes along (`LICENSING.md`).

**Install:** `https://plieuw.github.io/hawk-chua/open/` (Chrome or Edge): step 1 the firmware, step 2 HAWK CHUA's data.

## Two steps, and why

FM1 Open's app area holds ~454 KB, and the SDK takes ~370 KB of it. So the firmware holds the platform layer, the
store and a plain screen, and **HAWK CHUA's data** (step 2, ~200 KB in the FM-1's user flash) holds the sound's and the
UI's code, the presets and their tables, linked to run from there. The firmware checks the data (its CRC, that it was
built with this firmware, the address it runs at) before it uses it; without it the FM-1 is silent and asks for step 2.
After every new firmware, step 2 again. Saved presets and settings live at the end of the user flash and stay.

## What is different from the Felucca build

| | |
| --- | --- |
| Bluetooth LE MIDI | in and out (peripheral: a phone or computer connects to the FM-1) |
| The second core | not used: the SDK's RTOS runs on both cores. One core for the sound: the CPU meter and LIGHT (the synth gets lighter at ~95 %) matter more |
| Audio blocks | the SDK's 128 frames (2.9 ms); keys and MIDI still every 32 frames |
| The tape | 1.1 s a track (was 7.2 s): the SDK's RAM. It lives in the D-cache's RAM ways |
| The screen | a new renderer (`app/hc_gfx.c`) and fonts (`tools/gen_font.py`), Inter Tight as before |
| The remote page, the USB serial port, the update loader | not in it: the SDK has its own update path |
| Update mode | OCT- + OCT+ held 5 s: the SDK's recovery |

## How it fits together

| Path | What |
| --- | --- |
| `app/hc_open.c` | one compilation unit: the layer below, then `synth.c`, `keys.c`, `ui.c` from `../firmware/src` |
| `app/hc_audio.c` | the render the SDK's audio interrupt calls (`app/board/audio.c`) |
| `app/hc_midi.c` | MIDI in from every port to keys.c's ring; MIDI out to USB, Bluetooth LE and TRS |
| `app/hc_panel.h` | the buttons, encoders and LEDs on the SDK's panel scan (`app/board/panel.c`) |
| `app/hc_gfx.c`, `hc_fonts.h` | the canvas, lines and text on the SDK's screen calls |
| `app/hc_store.c` | the settings and presets in the user flash (A/B copies, a record written last) |
| `app/hc_data.c` | HAWK CHUA's data: the check, the SysEx upload (step 2) |
| `app/hc_sdk.c` | the control task (the Felucca build's main loop) and the SDK's hooks |
| `tools/build_open.py` | the build on Linux (after Orkest V14's port of M-VAVE's `Build.ps1`): the firmware and the data |
| `tools/ovl_split.py` | which code and tables go into the data |
| `tools/make_site_open.py`, `web/` | the installer page |
| `tests/` | host tests against a stand-in SDK: step 2, the sound, every page's screen, the store (also under ASan / UBSan) |

```
sh open/tests/run_tests.sh                                              # host tests, screens in open/build/host
python3 open/tools/build_open.py open ~/.jieli/toolchain --out open/build/hawk-chua-open.fwsc   # + hawk-chua-data.bin
```

The update identity is `FM-1_8990007` (FM1 Open's `FM-1_8xxxxxx` range).
