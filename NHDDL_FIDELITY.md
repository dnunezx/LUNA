# NHDDL fidelity contract

LUNA is an NHDDL-derived frontend, not a replacement implementation of
NHDDL's core behavior. The default and mandatory rule is to preserve the
behavior of the pinned frontend source:

This contract applies to the current LUNA Release Candidate and remains a required gate for later builds.

```text
pcm720/nhddl 89141d470ea2e0a2f81ad485874869f057bca082
```

Critical inherited paths must remain functionally equivalent to that source
unless the user explicitly authorizes a specific divergence after the
upstream behavior, risk, and validation plan have been documented.

## Critical inherited paths

Treat these as compatibility contracts rather than convenient edit points:

- storage-device discovery, initialization, and mount selection;
- ISO scanning, title-ID extraction, and title-cache behavior;
- launcher configuration, per-title options, and argument construction;
- the NHDDL-to-Neutrino launch handoff;
- metadata-device fallback and cover-art path construction;
- cover decoding, texture-manager invalidation/binding, and memory lifetime.

Branding, layout, animation, panel geometry, colors, and draw order may change
around those contracts. A visual problem must first be diagnosed in the visual
layer. Do not change an inherited loader, parser, scanner, cache, or launch path
to mask a rendering or composition bug.

## Cover-art invariant

LUNA must retain NHDDL's cover-art flow in `src/ui/gui.c`:

1. Use the metadata device when NHDDL supplies one.
2. Resolve `<mountpoint>/ART/<TITLE_ID>_COV.png`.
3. Invalidate the current managed texture.
4. Load the file with `gsKit_texture_png()`.
5. Bind it through the gsKit texture manager.
6. Retain the decoded cover pixels while Classic View is active so gsKit can
   re-upload the cover if the paired disc texture displaces it from GS VRAM.
7. Release those pixels when the cover is replaced or the UI closes.
8. Draw the managed texture without changing its decoded image content.

Do not silently substitute a custom cover decoder, rewrite artwork at runtime,
add a fallback cover filename, or preprocess files to compensate for an
unproven loader problem. Source artwork used for fidelity tests must retain its
recorded hash.

## Classic paired-art extension

The approved Classic View disc-label feature is a documented LUNA extension,
not a replacement for NHDDL's cover path. It resolves the matching
`<mountpoint>/ART/<TITLE_ID>_ICO.png`, normalizes the indexed/transparent PNG to
RGBA in EE memory, and lets gsKit manage the resulting texture. The original
`_COV.png` lookup, metadata fallback, native cover decoder, and decoded cover
content remain unchanged.

Adding the second texture exposed a GS VRAM eviction case: NHDDL historically
discarded the cover's decoded pixels after its first upload, so gsKit could not
safely restore an evicted cover. The symptom was a noisy/corrupted cover while
the disc appeared below it. LUNA now retains the cover pixels for the lifetime
of the selected Classic entry, rebinds the managed cover before drawing, and
releases the boot-only logo texture after the splash. The disc renderer uses an
alpha test so transparent PNG corners do not become a rotating square.

Rollback is localized: remove the `_ICO.png` load/draw path and restore the
upstream immediate cover-buffer release. On 2026-08-25 both frontend targets
compiled, and the user visually confirmed the corrected cover and rotating disc
in PCSX2. This is emulator evidence only, not physical-console proof.

Grid is likewise a UI-only LUNA extension. It reads the existing
`/ART/PSBBN/<TITLE_ID>.png` assets through the metadata-device fallback and
keeps two view-local thumbnail pages and two selected-cover slots. Its thumbnail
path may defer the GS upload only long enough to downscale decoded pixels in EE
memory; the final texture is still managed by the established texture manager.
The one-artwork-per-frame loader and animation state must not alter ISO scanning,
title ordering, options, or the Neutrino launch handoff. Leaving Grid must
release both page buffers and both selected-cover slots before another view's
artwork is populated.

The 2026-08-24 flat-cyan cover regression is the reference example. File
lookup and NHDDL decoding were working; LUNA's glass frame was drawn at a
higher Z layer than the cover texture and hid it. The correct fix changed only
the glass draw order. The NHDDL cover-loading path and indexed OPL artwork were
left unchanged.

## Required change gate

Before accepting a change near a critical path:

1. Compare the affected function with pinned NHDDL source.
2. State whether the change is UI-only, a documented LUNA extension, or an
   upstream-path divergence.
3. Prefer a fix outside the inherited path whenever the defect originates
   outside it.
4. Rebuild with the validated PS2 toolchain.
5. Exercise the real virtual-HDD path in PCSX2, not a synthetic preview list.
6. Distinguish file/hash evidence, emulator evidence, and physical-hardware
   evidence in the result.

If a critical divergence is genuinely required, stop and obtain explicit user
approval. Record the reason, exact upstream difference, rollback, and focused
test evidence here or in a linked design note before implementation.

## LUNA persisted-state migration

On 2026-09-14 the user explicitly approved moving frontend state from `/nhddl`
to `/LUNA`. LUNA writes `lastTitle.bin`, `cache.bin`, `global.yaml`, and
title-specific argument files only under `/LUNA`, while reads fall back to the
legacy `/nhddl` paths. Last-title and cache writes are staged through a complete
temporary file before replacement; interrupted temporary files remain readable.
The last-title payload and cached path lengths are bounded by `PATH_MAX` before
copying or allocation. Rollback consists of restoring `BASE_CONFIG_PATH` to
`/nhddl`, removing the legacy fallback probes, and reverting the staged-write
paths. Required validation is a PS2 toolchain build followed by virtual-HDD tests
covering new state, legacy-only state, oversized/corrupt records, and interrupted
temporary writes; physical-console persistence remains a separate gate.

## HDD-only library scan

On 2026-09-18 the user explicitly required LUNA to scan games only from a hard
drive. The library loop therefore accepts only `MODE_ATA` and `MODE_HDL`
devices, even if another inherited NHDDL backend was initialized. For the
shipped ATA-only configuration, BDM discovery stops after finding the single
internal ATA drive instead of waiting for a nonexistent `mass1:` device.

The initial BDM readiness delay and retry behavior remain unchanged. The risk
is limited to configurations that intentionally used LUNA to aggregate games
from removable or network devices, which are no longer supported. Rollback is
localized to removing `LUNA_LIBRARY_MODES`, its scan-loop guard, and the
ATA-only early exit in `initBDMDevices()`. Validation requires both PS2 builds,
an ATA virtual-HDD library test in PCSX2, confirmation that an attached USB
game library is excluded, and a physical-console startup timing check.

## USB library scan

On 2026-09-23 the user requested USB game support, superseding the HDD-only
restriction for USB devices. Pinned NHDDL scans every initialized device; LUNA
retains its scan guard but adds `MODE_USB` to the accepted modes. Other removable
and network modes remain excluded. The packaged configuration remains ATA-only;
USB users must select `mode: usb` in the active `luna.yaml`.

The change risks slower startup on USB and depends on the drive mounting before
the bounded BDM probe ends. Rollback is removing `MODE_USB` from
`LUNA_LIBRARY_MODES`. Both frontend targets built with the PS2 toolchain. PCSX2
detected a 100 MiB MBR/FAT32 USB test image as `usb0` (`mass0:`), and LUNA listed
its homebrew `LUNA Diagnostics.iso`. Game launch and physical-console behavior
remain unverified.
