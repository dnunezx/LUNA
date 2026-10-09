# Updating PSXCore with LUNA

PSXCore is an independently maintained submodule. LUNA owns its indexed
library, selection, artwork, frontend settings, argument preparation, and
frontend shutdown. PSXCore owns storage reopening, disc/request validation,
runtime adaptation, and game saves. PSXCore is reference-only: never modify its
source here. Consume clean upstream revisions; report required core changes to
the user for maintenance upstream. Do not work around this boundary with SDK
changes or injected runtime patches.

## Update and build

From the LUNA workspace in PowerShell:

```powershell
.\tools\update-psxcore.ps1 -Development
```

This fetches the current remote default-branch HEAD, refuses a dirty PSXCore
checkout or a non-forward update, checks out the exact commit, and runs
`tools/build-luna-psxcore.sh` in `ps2max/dev:v20260228`. Frontend edits remain
in place. Use `-Hardware` for a console build, and omit `-Development` for a
build without the local development launcher. `-BuildImage` selects another
compatible PS2SDK image.

Both the PSXCore library linked into LUNA and its standalone runtime ELF are
built from the same clean source checkout. The combined build never fetches;
updating is an explicit separate operation. Repeated builds of a recorded
commit therefore do not silently advance to newer source.

The emulator output directory is `nhddl/build-psxcore`; hardware output is
`nhddl/build-psxcore-hardware`. Each completed build publishes:

- `luna.elf` and `luna_unc.elf`.
- `psxcore-runtime-bootstrap.elf` and its SHA-256.
- `psxcore-build.txt`: exact core/frontend/SDK revisions, frontend dirty state,
  build options, runtime origin, container image identity, compiler version,
  UTC build time, and verification status. SDK Git revision is `unavailable`
  when the image omits its Git metadata; the container identity is recorded.
- `psxcore-build.sha256`: hashes of the frontend ELFs, linked core library,
  and staged runtime ELF. Run `sha256sum -c psxcore-build.sha256` in the output
  directory inside the container to check the set.

A failed combined build removes the previous completion manifest. A failed
update build leaves the fetched core checked out for inspection and reports
the previous revision. It does not commit, push, or discard source changes.
After verification, record the new submodule pointer with the LUNA changes.
Keep the runtime ELF and its manifest together when staging a build for use.

To explicitly rebuild an earlier commit:

```powershell
.\tools\update-psxcore.ps1 -Revision <full-commit> -Development
```

## Launch contract checked at a0c931f

LUNA's adapter in `nhddl/src/psxcore_launch.c` and `psxcore_transfer.c` stages
the standalone ELF and transports ordinary `argc`/`argv`. It does not duplicate
PSXCore request structures or inject runtime patches.

The current file backend accepts `--file massN:/path/Game.VCD` or
`--file-request massN:/path/REQUEST.BIN`. Direct-file selection also supports
`--pcsx2` and `--license`; those overrides cannot accompany a request that
already supplies them. `--init-cards` explicitly provisions missing cards;
normal launching does not automatically enable it. Synthetic probing is a
development operation, not gameplay verification.

The backend supports one internal ATA drive with FAT32/exFAT. It resets the
IOP and reopens storage itself. The selected VCD must remain accessible by
relative path and measured identity after that reset. Firmware defaults to
`/POPS/POPS.ELF` and `/POPS/IOPRP252.IMG` on the selected volume. Isolated cards
are routed to `/POPS/SAVES/<save_key>/card0` and `card1`.

LUNA discovers case-insensitive VCD filenames on ATA and reads bounded disc
metadata for artwork IDs without placing PS1 entries in the PS2 title cache.
R3 cycles PS2, PS1, and Mix, intersecting favorites. Selected PS1 games bypass
PS2 launch arguments, cheats, and VMC settings. Preparation checks firmware
and existing card ownership; missing cards require Square confirmation before
passing `--init-cards`. LUNA does not write card files. A missing save library
prompts immediately; checking an existing library displays disc-read progress
and permits cancellation. After confirmation, PSXCore validates the disc and
saves authoritatively.

Request mode uses PSXCore's existing version-3 `NpFileLaunchRequest`, currently
552 bytes, with a persisted save identity and measured image metadata. If
library launching needs that mode, construct and validate it through PSXCore's
public functions rather than reproducing its layout in LUNA. The older
no-argument HostFS/native-partition path remains a separate reference path.

Review `runtime/file_storage.c`, `include/psxcore_file.h`, and the public API
changes on each update. The standalone integration document still describes
mostly the older native-partition path; the current CLI implementation is the
source of truth for file launches.

## Accepting an update

Build success establishes compile/link compatibility. Run the local ELF and
argument-boundary checks against the newly produced runtime, then verify a
selected game, controls/audio, and saving followed by a cold reload using
copies of user save cards. Exercise mixed PS1/PS2 library behavior as those
controls are connected. Console acceptance requires a separate hardware run.
Record results locally, outside tracked source.

An external reference ELF may only be supplied with the development option.
Its manifest says `external-reference-unverified`; it cannot establish that
the current PSXCore source works. A normal paired build says
`matching-checkout`. Neither label implies gameplay or save acceptance.
