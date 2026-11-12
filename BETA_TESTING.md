# HAWK CHUA — beta testing

This branch is a test build based on the current `main` branch. It is deliberately kept separate from stable releases.

## What this beta changes

- Adds a bit-identical fast path to voice mixing when ORBIT is off and the voice is centred (BASIS/pan result is zero).
- In that common case, the voice is rendered directly into its resonator bus. This avoids clearing and copying a temporary audio block.
- Existing DSP, presets, MIDI mappings, UI controls, storage format and update path are not intentionally changed.

## Validation

Every push to `beta/**` runs the same firmware build, host tests and browser build as the main branch. The beta workflow does not publish the installer site or create a release.

A successful host test/build is not a substitute for listening and CPU-load testing on a real FM-1.

## How to test

1. Open the repository's **Actions → firmware** workflow.
2. Select the run for this beta branch.
3. Download its `hawk-chua-...` artifact after the build and tests succeed.
4. Keep a copy of your presets/settings before testing.
5. Test the same patches at the same voice count on stable and beta. Listen for clicks, dropouts, changed levels, stereo changes, stuck notes and unexpected CPU/LIGHT behaviour.
6. Report the exact commit SHA, patch/settings, voice count and what happened.

Do not treat a beta artifact as a stable release. If anything behaves unexpectedly, return to the stable firmware using the normal installer/recovery process.

## Compatibility promise

The optimization is intended to be bit-identical. The existing host regression suite is the gate for merging it. No feature is intentionally removed, renamed or remapped.
