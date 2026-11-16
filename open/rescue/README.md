# FM1 Open rescue

For an FM-1 that no longer starts after **HAWK CHUA FM1 Open beta 1** (withdrawn): the installers and M-VAVE's
Workbench no longer find it. This puts a working firmware back through the chip's own update mode (UBOOT), with no
extra hardware, from macOS or Linux. Run in a terminal:

```
bash -c "$(curl -fsSL https://raw.githubusercontent.com/Plieuw/hawk-chua/main/open/rescue/fm1_open_rescue.sh)"
```

Then, when it says it is waiting: plug the FM-1 in and switch it on (if nothing happens: switch it off, hold REC and
switch it on). It reads and saves the whole flash first (`~/fm1-rescue/fm1-backup-*.bin`) and asks before it writes.

What it writes: Orkest V14 (FM1 Open, known to run on the FM-1; the pre-release `fm1-open-rescue`) when the FM-1's
flash head is FM1 Open's, or M-VAVE's V15 / V16 when it is theirs. The head itself (the bootloader) is never written.
From Orkest V14, https://plieuw.github.io/orkest/ ("Terug naar de officiële V15") or M-VAVE's Workbench installs
anything else.

`fm1_open_rescue.py` and `.sh`: Charles Vestal's `fm1_rescue` from fm1-x0x / hugelton/felucca-bsp (MPL-2.0), changed only
to also accept an FM1 Open package (identity FM-1_8xxxxxx).
