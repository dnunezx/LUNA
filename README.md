

<p align="center">
  <img src="assets/luna-logo.svg" alt="LUNA logo" width="700">
</p>

LUNA (**Lightweight Unified Neutrino Access**) is a visual PS2 loader derived
from [NHDDL](https://github.com/pcm720/nhddl) and
[Neutrino](https://github.com/rickgaiser/neutrino). It preserves NHDDL's
established ISO discovery, title configuration, and Neutrino launch path while
adding a new library experience and a coordinated LUNA runtime.

The current configuration is designed for an FMCB-hosted frontend and Neutrino
runtime with games, artwork, and writable library state on an internal
ATA/exFAT hard drive.


## Requirements

- **Compatible PlayStation 2:** all hardware testing was done on a ps2 fat.
- **FMCB memory card:** with enough space for the LUNA application.
- **Internal ATA/exFAT drive:** an internal drive with an MBR- or GPT-formatted drive containing an exFAT partition. HDD and SDD are both supported.
- **Network adapter or HDD bridge:** all hardware testing was done with a GameStar PS2 SATA HDD Adapter.
- **Artwork:** artwork is optional and is not required to launch an
  ISO. For the complete library presentation use
  [OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition)
  to prepare the drive and obtain the PSBBN artwork.

## Installation

These steps assume you already have a working FMCB memory card, a PS2
controller, and a USB flash drive that your PS2 can read. LUNA is copied to the
memory card; your games and artwork stay on the internal hard drive.

### 1. Put LUNA on the USB drive

1. On your computer, download the `LUNA-v1.1.0-FMCB-mc0.zip` file.
2. Open the ZIP file and choose **Extract** or **Extract all**. Open the
   extracted folders until you can see a folder named `APP_LUNA`.
3. Plug the USB flash drive into your computer. Open the USB drive and copy
   the entire `APP_LUNA` folder to the USB drive's main screen. Do not copy
   only `luna.elf`; LUNA needs the files and folders inside `APP_LUNA` too.
4. Before removing the USB drive, check that the file is located here:

   ```text
   USB:/APP_LUNA/luna.elf
   ```

### 2. Copy LUNA to the memory card

5. Safely remove the USB drive from the computer and plug it into the PS2.
6. Turn on the PS2 and wait for the FMCB menu.
7. Open the **ELF installer** or **file manager** from the FMCB menu. On many
   FMCB cards this program is called **uLaunchELF** or **wLaunchELF**. This is
   the program that lets you copy files between the USB drive and memory card.
8. In the file manager, open `mass:/`. This is usually the USB drive. Find
   `APP_LUNA`, highlight the folder, and choose **Copy**.
9. Go back to the device list and open `mc0:/`. This is the memory card in
   slot 1. Choose **Paste** and wait for the copy to finish.
10. Check that the memory card now contains these files. The folder name must
    remain `APP_LUNA`:

   ```text
   mc0:/APP_LUNA/luna.elf
   mc0:/APP_LUNA/luna.yaml
   mc0:/APP_LUNA/neutrino.elf
   mc0:/APP_LUNA/config/
   mc0:/APP_LUNA/modules/
   ```

### 3. Add LUNA to the FMCB menu

11. Return to the FMCB menu and open **FMCB Configurator**.
12. Choose an empty menu item or application slot. Set the name to `LUNA`.
13. Set the program path to:

    ```text
    mc0:/APP_LUNA/luna.elf
    ```

14. Save the FMCB settings. Return to the main FMCB menu and restart the PS2
    if the new menu item does not appear immediately.
15. Select **LUNA** from the FMCB menu. LUNA should start and scan the games
    on the internal hard drive.

The packaged configuration is for the memory card in slot 1 (`mc0:`). For a
card installed in slot 2, use `mc1:/APP_LUNA/luna.elf` in the FMCB menu and
change `return_path` in `luna.yaml` from `mc0:` to `mc1:`.

## Preparing the game drive

Prepare the ATA/exFAT drive on a computer before launching LUNA. The easiest
way is to use [OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition).

1. Connect or mount the ATA/exFAT game drive on your computer.
2. Open **OrbitPS2 Manager — LUNA Edition** and click **Mount Directory** and select your drive
3. The manager checks for and can create these folders:

   ```text
   CD/
   DVD/
   VCD/
   POPS/
   APPS/
   ART/
   CFG/
   VMC/
   ```

   The manager does not create these folders when it starts. You
   must click **OK** when the prompt appears. If you cancel it, mount the
   drive again and accept the folder-creation prompt. Imports can also create
   the specific folders they need automatically.
4. Use the manager's **Import** feature instead of manually placing game
   files:

   - Choose **PS2 DVD** for a PS2 DVD `.iso` or `.zso`; it places the game in
     `DVD/`.
   - Choose **PS2 CD** for a PS2 CD image; it converts and places the game in
     `CD/`.
   - Let the manager read each disc image to find its title ID. Human-readable
     game names are fine, and the manager can use the title ID for artwork.

5. Use the manager's artwork tools after importing. OPL-style covers and disc
   labels go in `ART/`. LUNA's square artwork goes in `ART/PSBBN/`. The
   manager saves the square artwork using the required
   `<TITLE_ID>.png` filename.
6. Check that the drive now looks similar to this:

   ```text
   /CD/
   /DVD/Example Game.iso
   /ART/SLUS_200.02_COV.png
   /ART/SLUS_200.02_ICO.png
   /ART/PSBBN/SLUS_200.02.png
   /CFG/
   /VMC/
   ```

7. Safely eject the drive from the computer and return it to the PS2. LUNA
   will scan the standard OPL folders when it starts. Do not move the game
   files or artwork to the FMCB memory card.

If an existing drive already works with OPL, preserve its layout and mount its
root directory in the manager. LUNA uses the same title-ID conventions for
OPL-compatible cover art.

### Game storage modes
LUNA is configured to use only the internal ATA drive because initializing every
available storage device adds time to startup, even when those devices aren’t
being used. Keeping the scan focused on one device helps the library load faster
and makes startup more predictable; other device modes can be enabled when
needed.
The supplied package is configured for an internal ATA/exFAT drive. LUNA's
library scans ATA and HDL devices when available. USB, MX4SIO, MMCE, iLink,
and UDPFS are scanned only when their `mode:` entry is explicitly enabled in
`luna.yaml`:

- **MX4SIO** SD storage (`mode: mx4sio`). This mode must be enabled explicitly
  and makes MMCE devices unavailable while active.
- **USB mass storage** (`mode: usb`).
- **MMCE** devices, including SD2PSX and MemCard PRO2 (`mode: mmce`).
- **iLink / IEEE 1394** storage (`mode: ilink`).
- **UDPFS / UDPBD** network storage (`mode: udpfs`); requires the PS2 network
  address and a compatible server.
- **HD Loader (HDL)** APA-partitioned HDD (`mode: hdl`), subject to the limits
  described above.
- **ATA** MBR/GPT exFAT storage (`mode: ata`), set by default


## What LUNA adds

LUNA is more than a visual rename of NHDDL. It contains coordinated changes to
both the NHDDL-derived frontend and the Neutrino-derived game runtime.

| Area | LUNA addition |
| --- | --- |
| Library interface | A PS2-inspired glass interface with animated stars and crystals, LUNA branding, and five switchable library views. |
| Classic view | A refined list-and-cover layout with a rotating disc label, Favorites controls, and paired cover/disc artwork. |
| Collection view | A PSBBN-inspired cover flow with animated focus changes and a Collection/Favorites filter. |
| Grid view | A 4x4 artwork grid with paged caching, row-cascade transitions, large selected-cover preview, and fast-track shoulder navigation. |
| Orbit view | A depth-sorted ring of covers with perspective, fading, shared artwork caching, and a Square-button Random Scan that avoids reselecting the current title. |
| Orbs view | Seven spinning lights with fading trails, centered on a dark screen without artwork. |
| Favorites | Per-drive Favorites stored in `/LUNA/favorites.txt`, shared by Classic and Collection without modifying the game library. |
| Artwork | OPL-compatible covers plus optional disc labels and PSBBN-style square artwork, with view-specific caching and GS VRAM recovery. |
| Configured storage scan | The library scans ATA and HDL when available. USB, MX4SIO, MMCE, iLink, and UDPFS require explicit `mode:` entries. The shipped configuration uses internal ATA/exFAT and avoids waiting for a nonexistent second mass-storage device. |
| Safer persistent state | LUNA writes cache, last-title, global options, and per-title settings under `/LUNA`, reads legacy `/nhddl` state as a fallback, bounds stored paths, and replaces key files only after a complete temporary write. |
| FMCB deployment | The frontend and runtime can live together at `mc0:/APP_LUNA` while each hard drive retains its own artwork, cache, settings, and Favorites. |
| In-game return | **Work in progress.** ~~The planned LUNA Neutrino runtime will recognize a held controller combination and return directly to a configured memory-card ELF or through the HDD/browser boot chain.~~ |
| Return safety | The planned return path will request a coordinated optical/DEV9 shutdown and fail closed if safe shutdown cannot be confirmed. |
| Physical power button | LUNA adds a dedicated IOP-side safe-shutdown path. It coordinates DEV9 shutdown before issuing the standard power-off command, preserving the console's normal power-off behavior. |

## Library views and controls

Press **Circle** to cycle through **Classic**, **Collection**, **Grid**, **Orbit**, and **Orbs**.

### Views in motion

The four artwork views are shown below. The previews use sample game artwork.

<table>
  <tr>
    <td align="center" width="50%"><strong>Classic</strong><br><img src="assets/previews/classic.gif" alt="Classic list view with cover art and a rotating disc label" width="360"></td>
    <td align="center" width="50%"><strong>Collection</strong><br><img src="assets/previews/collection.gif" alt="Collection view moving through game artwork" width="360"></td>
  </tr>
  <tr>
    <td align="center"><strong>Grid</strong><br><img src="assets/previews/grid.gif" alt="Grid view showing cover thumbnails and a selected game preview" width="360"></td>
    <td align="center"><strong>Orbit</strong><br><img src="assets/previews/orbit.gif" alt="Orbit view moving through a ring of game covers" width="360"></td>
  </tr>
</table>

The **Orbs** view centers the spinning lights and their trails. The selected
game name and controls remain below the animation; this view needs no artwork.

- **Cross:** launch the selected game.
- **Triangle:** open the options menu, with **Per-game settings** first and
  **Global settings** second.
- **Classic layout:** choose **Global settings**, press **Cross** or **Circle**
  on **Classic art layout** to choose Separate or Overlap, then press **Start**
  to save. Overlap places the spinning disc behind the cover, with its lower
  half hidden. The choice is saved for the library on that drive.
- **Start:** exit the library.
- **Square in Classic:** add or remove the selected game from Favorites.
- **Select in Classic or Collection:** switch between the full library and
  Favorites.
- **Collection:** Left/Up and Right/Down move between covers; holding a direction
  repeats. L1/R1 jump backward or forward by a list page. Hold L2/R2 to fast
  scan; quick L2/R2 taps do nothing.
- **Square in Orbit:** start Random Scan. Any deliberate navigation
  input cancels it.
- **L1/L2 or R1/R2 in Grid:** tap for one page or hold for fast-track paging.
  Artwork loading resumes only at the final page when the buttons are released.

The in-game return feature is currently a work in progress. ~~Its planned control
combination is **L1 + L2 + R1 + R2 + Start + Select** held for roughly one
second; the default FMCB configuration is intended to return to
`mc0:/APP_LUNA/luna.elf`.~~

## Artwork layout

LUNA continues to use OPL-compatible title IDs and PNG artwork names:

```text
/ART/<TITLE_ID>_COV.png       140x200 cover used by Classic
/ART/<TITLE_ID>_ICO.png       optional 64x64 transparent disc label used by Classic
/ART/PSBBN/<TITLE_ID>.png     optional 256x256 square artwork for Collection, Grid, and Orbit
```

Use
[OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition)
to obtain and prepare the square PSBBN artwork expected by Collection, Grid,
and Orbit.

## Storage and configuration

The supplied FMCB configuration uses:

```yaml
mode: ata
return_path: mc0:/APP_LUNA/luna.elf
```

Writable state stays with the game drive:

```text
/ART/
/LUNA/cache.bin
/LUNA/lastTitle.bin
/LUNA/lastView.txt
/LUNA/favorites.txt
/LUNA/global.yaml
/LUNA/<game name>.yaml
```

LUNA saves the selected library view as `lastView.txt` whenever Circle switches
views and restores it on the next start. A missing or invalid file starts in
Classic. Swapping hard drives also swaps their library, artwork, Favorites,
saved view, cache, and per-game configuration. Existing `/nhddl` cache and
option files can still be read for migration, but new writes go to `/LUNA`.

## Source layout

- `nhddl/`: frontend submodule from [nhddl-luna](https://github.com/dnunezx/nhddl-luna), pinned to its `luna` branch.
- `neutrino/`: backend submodule from [neutrino-luna](https://github.com/dnunezx/neutrino-luna), pinned to its `master` branch.
- `tools/`: local build, FMCB packaging, and package-verification helpers.
- `LICENSES/`: licenses retained from upstream projects and dependencies.

See [UPSTREAM.md](UPSTREAM.md) for exact source lineage and
[AUTHORS.md](AUTHORS.md) for attribution. The installation layout is described
above, and both forks are checked out with `git submodule update --init --recursive`.

## Repository contents

Apart from the interface preview GIFs, this source tree intentionally does
**not** contain game ISOs, console firmware, standalone cover artwork, virtual
hard-drive images, emulator binaries, development backups, or release packages.

LUNA modifications and original LUNA code are attributed to
**Danny Nunez (dnunezx) 2026**. Upstream code remains credited to its respective
authors and is distributed under the licenses preserved in `LICENSES/`.
