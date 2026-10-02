<p align="center">
  <img src="assets/luna-logo.svg" alt="LUNA logo" width="700">
</p>

LUNA (**Layered Unified Neutrino Architecture**) is a visual PS2 loader derived
from [NHDDL](https://github.com/pcm720/nhddl) and
[Neutrino](https://github.com/rickgaiser/neutrino). It preserves NHDDL's
established ISO discovery, title configuration, and Neutrino launch path while
adding a new library experience and a coordinated LUNA runtime.

The current configuration is designed for an FMCB-hosted frontend and Neutrino
runtime with games, artwork, and writable library state on an internal
ATA/exFAT hard drive. LUNA requires a hard drive for its intended library
performance: slower storage can make artwork retrieval and cache misses visibly
delay navigation.

This README describes the current source tree. A published release package may
have fewer features if the source has changed since that release.

## Requirements

- **Compatible PlayStation 2:** hardware testing has been done on a fat model PS2.
- **FMCB memory card:** with enough space for the LUNA application.
- **Internal ATA/exFAT hard drive:** an HDD with an MBR or GPT partition table and an exFAT partition. This is the optimized and officially supported game storage setup.
- **Network adapter or HDD bridge:** all hardware testing was done with a GameStar PS2 SATA HDD Adapter.
- **Artwork:** artwork is optional and is not required to launch an
  ISO. For the complete library presentation use
  [OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition)
  to prepare the drive and obtain the PSBBN artwork.

## Installation

These steps assume you already have a working FMCB memory card, a PS2
controller, and a USB flash drive that your PS2 can read. LUNA is copied to the
memory card; your games and artwork stay on the internal hard drive.
LUNA must be installed on a PS2 memory card for in-game return (IGR) to work.

### 1. Put LUNA on the USB drive

1. On your computer, download the memory-card slot 1 package whose filename
   ends in `-FMCB-mc0.zip` from the
   [LUNA releases page](https://github.com/dnunezx/LUNA/releases).
2. Open the ZIP file and choose **Extract** or **Extract all**. Open the
   extracted folders until you can see a folder named `APP_LUNA`.
3. Plug the USB flash drive into your computer. Open the USB drive and copy
   the entire `APP_LUNA` folder to the root of the USB drive. Do not copy
   only `luna.elf`; LUNA needs the files and folders inside `APP_LUNA` too.
4. Before removing the USB drive, check that the file is located here:

   ```text
   USB:/APP_LUNA/luna.elf
   ```

### 2. Copy LUNA to the memory card

5. Safely remove the USB drive from the computer and plug it into the PS2.
6. Turn on the PS2 and wait for the FMCB menu.
7. Open the **file manager** from the FMCB menu. On many
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

### Optional background music

Music is now optional. The release ZIP includes the soundtrack in a separate
`Ambient-Music-Add-On` folder. Installing only `APP_LUNA` leaves music out and
uses less memory-card space; LUNA works normally without it.

1. To add music, copy `ambient.wav` from `Ambient-Music-Add-On` to the USB
   drive, then use uLaunchELF or wLaunchELF to copy that file into the installed
   `mc0:/APP_LUNA` folder. For slot 2, use `mc1:/APP_LUNA`.
2. Check that the installed file is `mc0:/APP_LUNA/ambient.wav`. Copy only the
   file, not the entire add-on folder. It needs about 1.3 MB of extra card space.
3. Restart LUNA. Press **Triangle → Global**, set **Ambient sound** to **On**,
   and press **Start** to save.

To mute music, set **Ambient sound** to **Off** and save. To reclaim the extra
card space, exit LUNA and remove only `ambient.wav` from `APP_LUNA` with the
file manager. When upgrading from a version that included music, an existing
`ambient.wav` may remain installed; remove it if you want to use LUNA without music.

## Preparing the game drive

Prepare the ATA/exFAT drive on a computer before launching LUNA. The easiest
way is to use [OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition).

1. Connect or mount the ATA/exFAT game drive on your computer.
2. Open **OrbitPS2 Manager — LUNA Edition**, click **Mount Directory**, and
   select the root folder of your game drive.
3. The manager checks for these folders and can create any that are missing:

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

Internal ATA/exFAT HDD remains the supported performance target.

### Storage Devices menu

Press **Start → Storage Devices** to choose which sources appear in the
library: internal ATA/exFAT HDD, APA/HDLoader HDD, USB, MX4SIO, MMCE, iLink,
and UDPFS. Internal ATA storage remains the packaged default. Connect storage
before starting LUNA; live insertion and removal are not supported by this menu.

- **Cross** toggles a source. Choose **Apply changes & scan** to save and refresh.
- **Square** on an enabled source rescans that source while retaining the other
  sources' games. **Rescan all enabled devices** refreshes the complete library.
- **Circle** returns without applying pending edits.

Each source shows its game count, no device found, or a scan failure. The menu
also works when the library is empty. Preferences are written to `storage.cfg`
beside the LUNA executable, normally on the FMCB memory card, independently of
the game drives. Existing `luna.yaml` settings are used when no valid preferences
exist; explicit launch arguments ignore saved preferences.

MX4SIO and MMCE cannot be selected together. Changing the MX4SIO driver setup
or the UDPFS address prompts LUNA to restart its storage drivers and reopen
the library. A conflicting driver needed to access LUNA or Neutrino is protected.

**UDPFS console IP address** sets the PS2's address, not the server's address.
Use the controller keyboard and press Start to accept it; leave it empty to read
`SYS-CONF/IPCONFIG.DAT` from a memory card. The effective address is passed to
the matching LUNA Neutrino runtime when launching a network game. UDPFS requires
working Ethernet and a running UDPFS server. iLink requires a console with an
iLink port. Artwork performance on these optional sources still requires testing
on physical hardware; the internal HDD remains the supported performance target.

### Virtual memory cards on exFAT and APA/PFS drives

LUNA currently creates and assigns VMCs on exFAT and other supported local
file storage, including APA/PFS drives used for HDL games. APA drives appear in
the VMC drive picker when their PFS metadata partition is mounted. Cards use
that partition's `/VMC` folder, or `/OPL/VMC` when the metadata is stored on
`__common`.

APA/PFS VMCs use the matching bundled Neutrino runtime and require normal boot,
8 KiB PFS zones, and card files whose direct extents fit the shared 64-fragment
limit across the game and both card images. Quick boot, other PFS zone sizes,
and files requiring indirect inode segments are not supported. Neutrino
validates the mapping before allowing card reads and writes.

Open a game's **Options → Game → Virtual memory cards**. It shows **Disabled**
until a virtual card is assigned to either slot. Press **Cross** to open its
settings. Choose **Create new card** to make a formatted 8 MiB card named
`LUNA_001.bin` (then `LUNA_002.bin`, and so on) in that drive's `/VMC` folder.
Creation never replaces an existing image. You can also copy an existing raw
OPL `.bin` card image into that folder. Choose **VMC slot 1** or **VMC slot 2**,
select a card, and press **Start** on the Game tab to save the assignment.
Choose **Physical card** for an individual slot or **Disable virtual cards**
to return both slots to physical cards. Card creation is immediate; slot
assignments are saved with the Game tab. The main menu's **Virtual Memory
Cards** manager remains available for creating cards across drives.

Neutrino checks the card's superblock, page geometry, and file size before
launch. It accepts raw 8, 16, 32, and 64 MiB images; images with separate ECC
bytes or a mismatched length are rejected. Back up an existing OPL card before
its first LUNA save. A complete save, reboot, and reload on a physical PS2 is
still required to establish game compatibility.


## Per-game video output

Open **Options → Game → Video → Video out**. Up/down selects a mode, and
Cross enables it. Selecting an enabled mode again returns to **Default**,
which keeps the game's original output. Press **Start** to save;
**Triangle** returns to the Video submenu.

Neutrino offers 240p/288p, 480p/576p, and three 1080i scaling presets.
Field flipping in the Video submenu applies to the selected output mode.
These options change video output without automatically increasing a game's
internal rendering resolution. Neutrino is the sole game-launch backend;
saved settings for the removed OPL core are ignored.

## What LUNA adds

LUNA is more than a visual rename of NHDDL. It contains coordinated changes to
both the NHDDL-derived frontend and the Neutrino-derived game runtime.

| Area | LUNA addition |
| --- | --- |
| Library interface | A PS2-inspired glass interface with animated stars and crystals, LUNA branding, five selectable library views, and a dedicated Options screen. |
| List view | A list-and-cover layout with a rotating disc label, Favorites controls, and paired cover/disc artwork. |
| Collections view | A PSBBN-inspired cover flow with animated focus changes and a Collections/Favorites filter. |
| 3D view | A 6x4 display of projected game cases with a large selected-cover preview, focus zoom and turn, a clear-sleeve shimmer, scrolling titles, and a short entrance animation. |
| Orbit view | A depth-sorted ring of covers with perspective, fading, shared artwork caching, and a Square-button Random Scan that avoids reselecting the current title. |
| Scroll view (experimental) | A scrolling title and logo display alongside animated ambient orbs. |
| Favorites | Per-drive Favorites stored in `/LUNA/favorites.txt`, shared by List and Collections without modifying the game library. |
| Artwork | OPL-compatible covers plus optional disc labels and PSBBN-style square artwork, with view-specific caching and GS VRAM recovery. |
| Configured storage scan | The library scans ATA and HDL when available. USB, MX4SIO, MMCE, iLink, and UDPFS require explicit `mode:` entries. The shipped configuration uses internal ATA/exFAT and avoids waiting for a nonexistent second mass-storage device. |
| Safer persistent state | LUNA writes cache, last-title, global options, and per-title settings under `/LUNA`, reads legacy `/nhddl` state as a fallback, bounds stored paths, and replaces key files only after a complete temporary write. |
| FMCB deployment | The frontend and runtime can live together at `mc0:/APP_LUNA` while each hard drive retains its own artwork, cache, settings, and Favorites. |
| In-game return | Hold L1 + L2 + R1 + R2 + Start + Select for roughly one second to return from a game to the configured LUNA ELF. This has been tested on a physical FAT PS2. |
| Return safety | The Neutrino runtime coordinates optical and DEV9 shutdown before handing off to the configured return path. |
| Physical power button | LUNA adds a dedicated IOP-side safe-shutdown path. It coordinates DEV9 shutdown before issuing the standard power-off command, preserving the console's normal power-off behavior. |

## Library views and controls

Press **Circle** to cycle through enabled views. **List**, **Collections**,
**Orbit**, and **3D** are enabled by default. The **Views** tab in Options can
also enable **Scroll**, which is marked experimental.

### Views in motion

Three views are shown below using sample game artwork.

<table>
  <tr>
    <td align="center" width="50%"><strong>List</strong><br><img src="assets/previews/classic.gif" alt="List view with cover art and a rotating disc label" width="360"></td>
    <td align="center" width="50%"><strong>Collections</strong><br><img src="assets/previews/collection.gif" alt="Collections view moving through game artwork" width="360"></td>
  </tr>
  <tr>
    <td align="center"><strong>Orbit</strong><br><img src="assets/previews/orbit.gif" alt="Orbit view moving through a ring of game covers" width="360"></td>
  </tr>
</table>

The 3D view displays game cases in four rows and turns the selected case toward
the viewer. The experimental Scroll view shows animated ambient orbs beside a
scrolling title and logo display.

- **Cross:** launch the selected game.
- **Triangle:** open Options. Its tabs are **Game**, **Global**, **Views**,
  and **Orbs**.
- **List layout:** open the **Views** tab, press **Cross** or **Circle**
  on **List art layout** to choose Separate or Overlap, then press **Start**
  to save. Overlap places the spinning disc behind the cover, with its lower
  half hidden. The choice is saved for the library on that drive.
- **Background orb colors:** in the **Orbs** tab, choose **Orb color** and
  **Tail color** separately with Left or Right, then press **Start** to save.
  Original keeps the existing colors. These choices affect only the shared
  Ambient Orbs background; Orbit view, Scroll view, and the loading screen keep
  their own colors. The Orbs tab is available only when **Ambient Orbs** is
  selected as the Global background; other backgrounds leave it dimmed and
  skip it during tab navigation.
- **PlayStation 2 logo:** the startup logo is **On** by default. In the
  **Global** tab, switch **PlayStation 2 logo** On or Off and press **Start** to
  set the default for games on the current drive. A game that needs a different
  choice can use **Game → Video → Show PS2 logo**; save that game's choice with
  **Start**. Its setting takes precedence over the Global default.
- **Start:** open the main menu for File Explorer, Virtual Memory Cards,
  Exit LUNA, and Shutdown.
- **Hold R1:** open the quick menu in any library view. While holding R1,
  use Up/Down to choose and Cross to confirm; release R1 to close without
  choosing. The menu offers Show Favorites/Show All, Add/Remove from Favorites,
  Options, and Random in Orbit. It slides in from the right, with button icons
  beside Options and Random. R1 + Select switches the filter, R1 + Square adds/removes a
  favorite, R1 + Triangle opens Options, and R1 + R3 starts Random in Orbit.
  Navigation pauses while the menu is open.
- **Square in List:** a shortcut to add or remove the selected game from Favorites.
- **Select:** switch between the full library and Favorites in any library view.
- **Collections:** Left/Up and Right/Down move between covers; holding a direction
  repeats. Hold L2/R2 to fast
  scan; quick L2/R2 taps do nothing.
- **Square in Orbit:** start Random Scan. Any deliberate navigation
  input cancels it.
- **L2/R2 in List:** page backward/forward.
- **L2/R2 in 3D:** move backward or forward one page.

Hold **L1 + L2 + R1 + R2 + Start + Select** for roughly one second to return
from a game to LUNA. The default FMCB configuration targets
`mc0:/APP_LUNA/luna.elf`. IGR works only when LUNA is installed on a PS2 memory
card and `return_path` points to that installation. Return from Tony Hawk's
Pro Skater 4 was verified in PCSX2 from both the pregame screen and active
gameplay. In-game return has also been tested on a physical FAT PS2.

## Artwork layout

LUNA continues to use OPL-compatible title IDs and PNG artwork names:

```text
/ART/<TITLE_ID>_COV.png       140x200 cover used by List
/ART/<TITLE_ID>_ICO.png       optional 64x64 transparent disc label used by List
/ART/PSBBN/<TITLE_ID>.png     optional 256x256 square artwork for Collections and Orbit
/ART/ORBS/<TITLE_ID>_LGO.png  optional logo for Scroll
/ART/ORBS/<TITLE_ID>_BG.png   optional background for Scroll
```

Use
[OrbitPS2 Manager — LUNA Edition](https://github.com/dnunezx/OrbitPS2-Manager-LUNA-edition)
to obtain and prepare the square PSBBN artwork expected by Collections and Orbit.
Missing Scroll artwork shows a text or placeholder graphic instead.

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
List. Swapping hard drives also swaps their library, artwork, Favorites,
saved view, cache, and per-game configuration. Existing `/nhddl` cache and
option files can still be read for migration, but new writes go to `/LUNA`.

## Source layout

- `nhddl/`: frontend submodule from [nhddl-luna](https://github.com/dnunezx/nhddl-luna), configured for its `luna` branch.
- `neutrino/`: backend submodule from [neutrino-luna](https://github.com/dnunezx/neutrino-luna), configured for its `master` branch.
- `tools/`: local build, FMCB packaging, and package-verification helpers.
- `LICENSES/`: licenses retained from upstream projects and dependencies.

See [UPSTREAM.md](UPSTREAM.md) for exact source lineage and
[AUTHORS.md](AUTHORS.md) for attribution. The installation layout is described
above. This repository pins exact commits for both submodules; check them out
with `git submodule update --init --recursive`.

### Special thanks

Special thanks to **Ivan V ([pcm720](https://github.com/pcm720))**, the main
developer of NHDDL, and **Rick Gaiser ([Maximus32](https://github.com/ps2max32))**,
the creator of Neutrino. Their work forms the foundation of LUNA.

Thanks to **[CosmicScale](https://github.com/CosmicScale)** and his
**[PSBBN Definitive Project](https://github.com/CosmicScale/PSBBN-Definitive-Project)**,
which inspired LUNA's design and supplied assets used by LUNA.

Thanks to **[NathanNeurotic (Ripto)](https://github.com/NathanNeurotic)**
for helping with LUNA's ambient orbs, and to **[aap](https://github.com/aap/osdbits)**
for reverse engineering parts of the PlayStation 2's OSDSYS in `osdbits`.

Thanks to all of the LUNA beta testers.

## Repository contents

Apart from the interface preview GIFs, this source tree intentionally does
**not** contain game ISOs, console firmware, standalone cover artwork, virtual
hard-drive images, emulator binaries, development backups, or release packages.

LUNA modifications and original LUNA code are attributed to
**Danny Nunez (dnunezx) 2026**. Upstream code remains credited to its respective
authors and is distributed under the licenses preserved in `LICENSES/`.
