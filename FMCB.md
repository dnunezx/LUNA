# LUNA on an FMCB memory card

This layout runs the complete LUNA and Neutrino runtime from an FMCB memory
card in slot 1 (`mc0:`). The supplied configuration reads games, artwork,
caches, options, Favorites, and the saved library view from the internal
ATA/exFAT hard drive.

## Install

1. Back up the FMCB memory card and the target hard drive.
2. Copy the packaged `APP_LUNA` directory to the root of the memory card,
   keeping its name `APP_LUNA`. The complete directory uses about 2.26 MB.
3. Confirm these paths exist exactly as shown; memory-card paths can be
   case-sensitive:

   ```text
   mc0:/APP_LUNA/luna.elf
   mc0:/APP_LUNA/luna.yaml
   mc0:/APP_LUNA/neutrino.elf
   mc0:/APP_LUNA/ambient.wav
   mc0:/APP_LUNA/config/
   mc0:/APP_LUNA/modules/
   ```

4. In the Free McBoot Configurator, add a menu item named `LUNA` whose path is
   `mc0:/APP_LUNA/luna.elf`, then save the FMCB configuration.
5. Keep the game drive's existing ISO and artwork layout. LUNA scans only the
   ATA backend because the packaged `luna.yaml` contains `mode: ata`.

The 1.33 MB, one-minute soundtrack is included in `APP_LUNA`. LUNA plays it by
default, loops it across its screens, and offers **Ambient sound: Off** under
Options → Global settings. If the file is absent, LUNA continues silently.

For USB games, change `mode: ata` to `mode: usb` in
`APP_LUNA/luna.yaml` before copying the folder to the memory card. This selects
the USB game library; the default package remains configured for ATA.

The packaged return target is `mc0:/APP_LUNA/luna.elf`. A card intentionally used
in slot 2 must change both the FMCB menu entry and `return_path` in `luna.yaml`
from `mc0:` to `mc1:`.

During gameplay, one press of the console's physical power button retains the
normal power-off behavior. Neutrino's priority-1 IOP listener acknowledges the
native CD/DVD power event, finishes active optical work, shuts down DEV9 when
present, and issues the standard `sceCdPowerOff` command. This is independent
of IGR, controller hooks, and the running game; the button is not reinterpreted
as an in-game return.

## What remains on the selected game drive

LUNA continues to use the selected game's storage device as its metadata
device. These paths therefore remain tied to each hard drive:

```text
/ART/
/LUNA/cache.bin
/LUNA/lastTitle.bin
/LUNA/favorites.txt
/LUNA/global.yaml
/LUNA/ambientSound.txt
/LUNA/<game name>.yaml
```

Swapping game drives swaps their library, art, scan cache, options, last-title
record, and Favorites set. The memory card contains no Favorites database.

## Hardware validation status

The complete 2026-09-19 `LUNA-FMCB-mc0.zip` package passed user-reported
physical-hardware testing. The `v1.1.1-rc.2` package includes the one-minute
memory-card soundtrack and audio scheduling changes. It has passed package
verification, but has not yet
been tested on a physical console. Use the following checklist for regressions
and different hardware. Verify the archive against `dist/SHA256SUMS.txt`.

On 2026-09-23, the first-frame IGR change returned from a user-supplied Tony
Hawk's Pro Skater 4 ISO to LUNA in isolated PCSX2. Neutrino loaded the emulator
target `host:/luna.elf`, and LUNA rebuilt the library afterward. This confirms
the emulator control flow; the `mc0:/APP_LUNA/luna.elf` return still needs a
physical-console test.

## Hardware regression checklist

1. Boot LUNA from the FMCB menu and confirm the splash reports the ATA backend.
2. Confirm the library, covers, and existing per-drive Favorites appear.
3. Add or remove one Favorite, restart LUNA, and confirm it persisted on the
   same drive. Swap drives and confirm the other drive has its own set.
4. Launch a small known-good game and play long enough to exercise sustained
   HDD reads.
5. Press L1 + L2 + R1 + R2 + Start + Select together. The return hook now acts
   on the first detected frame instead of giving a game's own soft reset a
   45-frame head start. A successful direct return reloads
   `mc0:/APP_LUNA/luna.elf` without using the HDD boot chain.
6. If the screen turns solid red, power off normally. The fail-closed return
   path intentionally refused to reset or reload after an unsafe shutdown.
7. Relaunch the game and verify it still reads correctly. Power down and run a
   read-only filesystem check on the HDD before broad testing.
8. Relaunch the game, perform sustained HDD reads, and press the physical power
   button once. Confirm that the console powers off normally and does not return
   to LUNA. Run another read-only HDD filesystem check afterward.

Repeat physical-console validation when any part of the console, network
adapter, SATA/IDE bridge, memory card, hard drive, frontend, or runtime changes.
