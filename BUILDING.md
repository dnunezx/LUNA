# Building LUNA locally

The validated local builds use the same PS2SDK container families as the upstream projects.

Clone this repository with its frontend, backend, and nested MMCE dependencies:

```powershell
git clone --recurse-submodules https://github.com/dnunezx/LUNA.git
```

In an existing checkout, run `git submodule update --init --recursive` before
building. LUNA `main` pins `nhddl/` to the `luna` branch of
`dnunezx/nhddl-luna` and `neutrino/` to the `master` branch of
`dnunezx/neutrino-luna`. Git records exact commits in LUNA `main`; advancing a
fork branch does not change a LUNA build until its submodule pointer is updated.

## Frontend

From the workspace root:

```powershell
docker run --rm -v "${PWD}:/workspace" -v "${PWD}\nhddl:/src" ps2max/dev:v20260228 sh -c 'set -eu
if [ ! -e "$PS2SDK/ps2dev.cmake" ]; then ln -s ../share/ps2dev.cmake "$PS2SDK/ps2dev.cmake"; fi
sh /workspace/tools/bootstrap-ps2-libtiff.sh
cmake -S /src -B /src/build-luna -DCMAKE_BUILD_TYPE=Release -DLUNA_EMULATOR_BUILD=OFF
cmake --build /src/build-luna --parallel 2'
```

Output: `nhddl/build-luna/luna.elf`

The glass UI is the sole frontend build path for the Release Candidate, CI
artifact, and emulator validation build. `LUNA_EMULATOR_BUILD` is kept off for
hardware and enables only PCSX2 direct-ELF path handling in the emulator
validation build.

## PSBBN neighboring-cover size tuning

The neighboring-cover scale curve is defined in
`nhddl/src/ui/gui.c`, inside `psbbnFutureCoverSize()`:

```c
static const uint8_t sizePercent[] = {100, 44, 41, 38, 30, 22, 14};
static const uint8_t offsetPercent[] = {0, 58, 82, 105, 127, 146, 160};
```

`100` is the focused cover. The remaining values are the six increasingly
distant covers in the left stream, expressed as percentages of focal size.
The matching offset curve stretches the distant tail toward the left-side
vanishing point. Starting after the nearest future cover, visibility fades
progressively from 100% to 15% at the farthest cached cover. Keep the first
three neighbor sizes stable when polishing only the distant tail; this
preserves the accepted focal handoff and monotonic growth into focus.

Rebuild the emulator test target from the workspace root with:

```powershell
docker run --rm -v "${PWD}/nhddl:/src" ps2max/dev:v20260228 sh /src/emulation/build-luna-emulator.sh
```

Output: `nhddl/build-emulator/luna.elf`

The emulator build also copies `nhddl/assets/ambient.wav` beside `luna.elf`.
PCSX2 reads it through `host:/ambient.wav`. The supplied MP3 is trimmed to a
60-second seamless loop and encoded as a 22.05 kHz stereo IMA ADPCM WAV. LUNA
loads the 1.33 MB compressed clip into EE memory, then decodes and queues small
pieces as it plays so cover-art reads cannot interrupt music playback.

The artwork and position counter retain the established PSBBN vertical anchor,
while Collection intentionally omits the title above the focal cover and docks
the `Collection`/`Favorites` selector independently near the footer. Select filters
the flow to a compact favorites-only title list while preserving adjacent-cache
recycling:

```c
selectedY = headerHeight + 34;
selectorCenterY = gsGlobal->Height - footerHeight - getFontLineHeight() - 16;
```

This separation intentionally frees the middle-left region for the fading
cover tail. Keep the selector above the footer and out of the stream's path.

## Grid view cache, geometry, and page cascade

Grid uses a separate page cache so its 4x4 layout cannot disturb Collection's
animation residency:

```c
#define GRID_COLUMNS 4
#define GRID_ROWS 4
#define GRID_PAGE_SIZE (GRID_COLUMNS * GRID_ROWS)
#define GRID_THUMBNAIL_SIZE 64
#define GRID_PAGE_BUFFERS 3
#define GRID_SELECTED_BUFFERS 2
#define GRID_FAST_TRACK_HOLD_MS 500
#define GRID_FAST_TRACK_STEP_MS 140
```

The three page buffers each hold sixteen 64x64 thumbnails: one visible page,
one protected previous page, and one direction-aware prefetched page. Two selected-cover
slots allow a replacement to load without discarding the visible cover first. Thumbnail loading uses the shared RGBA
decoder without binding the decoded 256x256 source to GS VRAM. Grid performs
its area downscale in EE memory and binds the 64x64 result once. Page preparation
is frame-budgeted to one source image per idle UI frame. A prepared page is
reused without decoding, and the page just left remains available for immediate
backtracking. If a requested page is not ready, preparation continues in the
background without locking input.

Grid retains six discrete sixteen-game pages. When the selection crosses a page
boundary or a shoulder tap requests another page, the complete incoming page and its
large selected cover are prepared first. The visible title, counter, highlight,
and large preview remain on the current page during that work. A 340 ms cascade
then commits the selection, sweeps the old covers out, and brings the new covers
in from the opposite side. Rows begin from top to bottom with a short stagger;
each incoming row follows its outgoing row after a second short delay. Travel is
limited to roughly one thumbnail width and paired with a fade so the cascade
does not cover the large selected-art panel. Input is gated only while the
cascade is visibly moving.
Circle cycles Classic, Collection, Grid, and Orbit. Artwork is released when
the next view uses a different cache family.

L1 and L2 share the backward path; R1 and R2 share the forward path. Grid does
not act on the initial press. Releasing before `GRID_FAST_TRACK_HOLD_MS`
performs one normal page request. Reaching the threshold enters fast-track and
moves a new lightweight page shell every `GRID_FAST_TRACK_STEP_MS`. Artwork
work is disabled from the initial shoulder press through the entire held
interval, preventing the half-second decision window from decoding an
intermediate page. Cached page buffers may still be drawn, but no PNG,
thumbnail, or selected-preview load is started. On release, only the final
destination is returned to the normal loader. As its art fills in, the final
page remains in place; completion promotes that page directly and does not
replay the normal Page Cascade. The large selected-art area uses the explicit
`FAST TRACK` state while this lightweight navigation is active.

The left matrix is flat and axis-aligned. `gridFlatQuad()` maps normalized cell
coordinates into one rectangular region, and `gsKit_prim_quad_texture()` draws
each cover without row convergence, skew, or a receding edge. Keep the large
right-side selected cover axis-aligned as well.

After Grid changes, verify all four D-pad directions, page entry in both
directions, first/last-page wrapping, top-to-bottom row timing, held input after
each cascade, tap and hold behavior for L1/L2/R1/R2, release after several fast
steps, direction reversal during fast-track, partial final pages, missing-art cells,
all four view transitions, and return to Classic without artwork corruption.
File presence and a successful build do not replace the rendered PCSX2 check.

## Orbit geometry

Orbit reuses the ten-entry PSBBN cache and the same 420 ms moving-focus state
as Collection. The selected cover is the front point of a
tilted ring and must remain centered and front-facing. The other nine covers
are textured quadrilaterals: their horizontal projection narrows, their side
edges slope, and their size, brightness, and opacity fall with depth. The rear
arc is drawn before the front arc. Keep the title and counter below the focal
cover, suppress wrapped duplicates in short libraries, and do not allocate a
separate Orbit texture pool.

After Orbit changes, verify exact focal centering, forward and reverse rotation,
held movement, mid-glide reversal, wraparound, short libraries, missing art,
title/counter handoff, and artwork recovery when switching views.
The textured quad slope and rear-to-front overlap require a rendered PCSX2
check; a successful build alone is not sufficient.

Square starts Orbit's one-press Random Scan. It mixes the current title with
the UI timer and chooses a different title. `ORBIT_RANDOM_STEP_MS` advances
toward that title along the shorter direction, one adjacent cache position
every 85 ms. Each step recycles nine cache entries and loads one new PNG.
`RANDOM SCAN` identifies the active traversal. Any deliberate non-Square input
cancels it, and holding Square does not retrigger it. Verify short and long
scans, cancellation, wraparound, and the selected title and counter at arrival.

## Background-star atlas

The 58 moving stars in `nhddl/src/ui/gui.c` use one 64x32 CT32 atlas containing
the three color layers and two size profiles. The atlas is generated once at UI
initialization and retained by the gsKit texture manager. Each star is one
filtered native sprite; edge cross-fades temporarily add a second sprite only
while wrapping. Do not restore the former four 32-triangle disc fans for the
background: that path submitted 7,424 base triangles per frame. The disc helper
remains available for larger orbital effects and the crystal highlight.

After changing the atlas, verify both star sizes, all three parallax colors,
twinkle range, left/right edge cross-fades, texture-manager reinitialization,
and output on interlaced hardware. Preserve neutral `0x80` texture modulation
and the PS2 alpha range when tuning the baked pixels.

## Classic cover and disc presentation

Classic View's artwork geometry and motion are defined together in
`nhddl/src/ui/gui.c`:

```c
#define COVER_ART_MAX_WIDTH 140
#define COVER_ART_LOWER_RESERVE 96
#define COVER_ART_VERTICAL_OFFSET -4
#define DISC_ART_SIZE 112
#define DISC_ROTATION_PERIOD_MS 30000
```

`COVER_ART_LOWER_RESERVE` preserves the approved cover position independently
of the disc size. `COVER_ART_VERTICAL_OFFSET` provides the small upward polish
adjustment. The disc is centered dynamically between the cover bottom and the
lower panel border, keeping equal breathing room around its outer glow.

This 140x200 cover and 112-pixel disc composition received user visual
acceptance in PCSX2 on 2026-08-25. Treat later Classic geometry changes as a
new visual pass rather than an unfinished correction.

The active metadata device supplies `/ART/<TITLE_ID>_COV.png` and the matching
`/ART/<TITLE_ID>_ICO.png`. Do not generate a disc from the cover. A missing
`_ICO.png` must render only the faint empty-disc outline.

After changing this path, rebuild both targets and test a real mock entry from
the DEV9 fixture. Confirm that the cover remains intact while navigating, the
disc has no square background, clockwise rotation continues, and switching
between Classic and Collection does not corrupt either artwork path. File
presence and successful compilation are not substitutes for the rendered test.

## NHDDL fidelity gate

Before accepting changes near storage, scanning, metadata, cover loading,
configuration, or launch behavior, compare the affected source with the pinned
NHDDL commit:

```powershell
git -C nhddl diff 89141d470ea2e0a2f81ad485874869f057bca082 -- src/main.c src/options.c src/target.c src/forwarder.c src/neutrino.c src/devices src/ui/gui.c
```

Classify every relevant difference as UI-only, an intentional LUNA
extension, or an upstream-path divergence. Do not accept an unclassified
critical-path change. A divergence requires explicit user approval and a
documented rollback before implementation.

For cover-art work, confirm that `loadCoverArt()` still uses NHDDL's metadata
fallback, `_COV.png` path, and `gsKit_texture_png()`. The documented LUNA
paired-art exception retains decoded cover pixels while Classic View is active
so an evicted texture can be re-uploaded. Verify source-art hashes and rendered
output independently; a successful file lookup is not rendered proof.

## Backend

```powershell
docker run --rm -v "${PWD}/neutrino:/src" -w /src ps2max/dev:v20260228 sh -c "make clean all copy"
```

Outputs used by the package are under `neutrino/ee/loader/`. The `copy` target
is required after a clean build because it stages the rebuilt IOP modules and
EE core in `neutrino/ee/loader/modules/`.

Before packaging, check the rebuilt EE core against the protected stack:

```powershell
python nhddl/tools/check_ee_core_budget.py neutrino/ee/loader/modules/ee_core.elf
```

The gate requires at least `0x4000` bytes of static headroom before the stack at
`0x00094000`. This is intentionally stricter than merely accepting a successful
link, so new resident IGR or UI state cannot silently consume the final margin.

## Packaging

The existing hardware-test package remains untouched. The most recent hardware output, `nhddl/build-luna/luna.elf`, is copied byte-for-byte to `dist/LUNA-Release-Candidate.elf`. Its checksum is recorded in `dist/SHA256SUMS.txt`.

The package includes `APP_LUNA/ambient.wav` on the memory card. Game-drive
artwork and writable library state remain on the game drive.

The FMCB package reuses the promoted ELF and matching Neutrino build. It does
not move artwork or writable library state onto the memory card. Build and
verify the fixed-slot `mc0:` package from the workspace root with:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\package-fmcb.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-fmcb-package.ps1
```

Outputs:

```text
dist/LUNA-FMCB-mc0/
dist/LUNA-FMCB-mc0.zip
```

For the one-minute soundtrack and anti-stutter candidate, build the hardware
frontend with `-DLUNA_RELEASE_VERSION=v1.1.1-rc.2`, copy it to
`dist/LUNA-v1.1.1-rc.2.elf`, then run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\package-fmcb.ps1 -Version v1.1.1-rc.2
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\tools\verify-fmcb-package.ps1 -PackagePath .\dist\LUNA-v1.1.1-rc.2-FMCB-mc0 -LauncherPath .\dist\LUNA-v1.1.1-rc.2.elf
```

The versioned ZIP is `dist/LUNA-v1.1.1-rc.2-FMCB-mc0.zip`. ZIP entry paths use
forward slashes so Linux extractors retain the `APP_LUNA/config` and
`APP_LUNA/modules` directories.

The package copies `nhddl/examples/luna.yaml` as `APP_LUNA/luna.yaml`. That
configuration selects only `mode: ata` and sets
`return_path: mc0:/APP_LUNA/luna.elf`. `APP_LUNA` must be copied to `mc0:/APP_LUNA`.
The verifier rejects any packaged `ART`, Favorites, cache, last-title, or global
options file because those belong to each ATA hard drive. The former HDD-chain
configuration remains available as `nhddl/examples/luna-hdd.yaml`.

The 2026-09-19 promotion retains the 2026-09-18 Favorites, UI, startup, return,
and safe power-button work and adds the shared background-star atlas. The
promoted frontend ELF is 412,756 bytes with SHA-256
`4a0a980009db4bd9a9c58b8fa5bb2b18469c97c94944dd25762d753189a44bfc`.
The verified FMCB archive containing the matching rebuilt runtime is 678,277
bytes with SHA-256
`5c2d2b0377cfee5c70de3a20cb6947b42db01102a1a66411bb78f6a23d32f7ab`.
The user confirmed that this complete package passed physical-hardware testing
on 2026-09-19. The first-person/inverted Orbit concept is not implemented in
this promotion.

A Git tag or hosted release is a separate publication step and is not implied by this local Release Candidate label.
