# Rules for working on this firmware (learnt the hard way)

FM1 Open betas 1 and 2 (November 2026) left two FM-1s unable to start: they restarted before USB came up, so no
installer, the Workbench or a UBOOT rescue script could reach them; only the FM-1 Transporter (an RP2040 on the USB
lines) gets them back. Before anything goes onto an FM-1:

- **Never publish a build that changes the platform layer without it having run on hardware first.** Host tests
  passing says nothing about start-up, RAM or flash behaviour on the device. A beta goes to the user as a file to
  try on one FM-1 that has a Transporter at hand, never straight onto an install page.
- **On M-VAVE's FM1 SDK, keep the runtime heap near what M-VAVE's own app leaves (~250 KB), not Build.ps1's
  48 KB minimum.** The SDK's Bluetooth and USB stacks allocate from it at start-up; the betas left ~52 KB.
- **Follow the SDK's rules literally:** everything the audio interrupt runs, in internal SRAM (FM1_AUDIO_CODE); no
  memory regions M-VAVE's app does not use (the D-cache RAM ways); no code executing from the user flash until
  that has been proven on a device.
- **A safety net has to cover the platform's own start-up, not only our code.** A boot counter in our code did not
  help: the crash was before USB, in the SDK's part.
- **Say plainly what is untested and what a failure costs** (here: a Transporter, a few euros and a week) before
  the user installs, and recommend trying on one unit only.
- Recovery tools: `open/rescue/` (UBOOT script, the SDK's update SysEx F0 22 24 35 7D F7 helper page), M-VAVE's FM1
  Open built unchanged (release `fm1-open-rescue`), github.com/kurogedelic/FM-1-transporter.
