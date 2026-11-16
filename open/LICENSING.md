# Licensing: HAWK CHUA on FM1 Open

HAWK CHUA on FM1 Open (`open/`) is HAWK CHUA's sound and UI linked with M-VAVE's FM1 SDK, which is a binary library
without source. The GPL allows that only with a linking exception, so every part of this build carries one, or a licence
that allows it.

| What | Licence | Where |
| --- | --- | --- |
| HAWK CHUA's own sources: the sound, the UI, the shared state, and this build's layer on the SDK | GPL-3.0-only **with the GPL-3.0 linking exception** (M-VAVE's wording, `LICENSE.md`, applied by their author) | `firmware/src/{synth,keys,noise,feg,karma,arp,resonator,waveguide,shimmer,tape,fx_mod,ui,ui_style,fx_art}.c`, `sys.h`, the tables (`chua_tables.h`, `lpg_tables.h`, `knob_tables.h`, `preset_gain.h`, `fx_icons.h`), `open/app/hc_*`, `open/tools`, `open/tests` |
| FM1 Open's APP sources (the board: audio, panel, screen; the audio output) | GPL-3.0-only with M-VAVE's linking exception (`LICENSE.md`); three files changed for HAWK CHUA, marked | `open/app/board/`, `open/app/audio_output.*`, `open/app/*.h` |
| The FM1 SDK: the engine library, headers, boot and update files | M-VAVE's binary terms (`THIRD_PARTY.md`), unmodified | `open/sdk/` |
| The FX resonators (Rings, DaisySP) | MIT | `firmware/src/phys_dsp.c`, `phys_symp.c`, `LICENSES/MIT-*.txt` |
| Inter Tight, the UI text (rasterised at build time, `open/tools/gen_font.py`) | SIL OFL 1.1 | `assets/fonts`, `LICENSES/OFL-InterTight.txt` |
| The installer page (a separate program in the browser, not linked with the SDK) | GPL-3.0-only; from Felucca's installer (Leo Kuroshita, Hügelton Instruments) and Orkest's | `open/web/` |

**No Felucca code is in this firmware.** The Felucca build's platform layer (`firmware/src/{main,audio,usb,midi_uart,lcd,
gfx,libc,storage,storage_hw,ota,ota_hw,panel,felucca}.c`, `core.h`, `firmware/hal/`, the second core's support from X0X)
is GPL-3.0-only without an exception; this build uses none of it: the SDK is the platform, and the drawing, the text,
the store and the panel glue are new (`open/app`). The files the two builds share are HAWK CHUA's own (their
provenance: the repository marks Felucca's files with Leo Kuroshita's copyright line; the shared ones carry none).

Whoever distributes the firmware (`.fwsc`) or the data (`hawk-chua-data.bin`) gives the complete corresponding source
of the GPL parts with it (this repository) and the licence texts (`firmware/licences/` on the installer site).

"M-VAVE" and "FM-1" are trademarks of their respective owners. This firmware is independent; it is not affiliated with,
endorsed by or supported by them.
