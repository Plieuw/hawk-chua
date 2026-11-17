# HAWK CHUA on VIBE FM

The FM-1 firmware as a VIBE FM firmware: one `hawkchua.wasm` that plays in the browser (the Playground at
[play.thefirmwarehub.com](https://play.thefirmwarehub.com): drop the `.wasm` onto the device), in VIBE FM VST (drop the
`.zip` onto its window) and on the VIBE FM. The firmware's own sources (`firmware/src`) run unchanged;
`hawkchua/main.c` stands in for the FM-1's hardware. Its own screen, shown in the middle of the VIBE's display.

| VIBE FM | HAWK CHUA |
|---|---|
| keys, pads | play (velocity), C3 at OCT 0 |
| encoders 1–4 | KNOB 1–4 |
| A, B, DATA (and ◀ ▶) | SELECT, ALGORITHM, PRESETS |
| SOUND · ENV · LFO · ARP · FX · SEQ | HOME · ENV · LFO (FREE EG) · ARP · FX · SEQ |
| F1–F6 | EDIT · GLO · SCL · SAVE · REC · PLAY |
| OCT −/+, REC, PLAY, STOP | as on the FM-1 |
| RETURN | the HOME page |
| HOME | what every control does |
| ENTER held | FOCUS: every control with DATA + ENTER |

Build: `VIBE_SDK=path/to/vibe-fm-sdk sh vibe/build.sh` (Zig 0.13.0: `zig`, or `pip install ziglang==0.13.0` and
`ZIG="python3 -m ziglang"`) → `build/vibe/hawkchua.wasm` and `.zip`. The CI builds both with every push (artifact
`hawk-chua-vibe-…`) and puts them on every release. Checked with the SDK's `vibe-check` (PASSED).
