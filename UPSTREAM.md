# Upstream lineage

LUNA contains modified source from the following projects:

| Component | Upstream | Base revision |
| --- | --- | --- |
| Frontend | `https://github.com/pcm720/nhddl` | `89141d470ea2e0a2f81ad485874869f057bca082` |
| Backend | `https://github.com/rickgaiser/neutrino` | `4c95d56713e32536aa4dc2a45e440cd6efa2d7bb` |
| MMCE module | `https://github.com/ps2-mmce/mmceman` | `0e78d1b35c71d1ee43b880c433586d830249b6f0` |
| OPL game-core source | `https://github.com/ps2homebrew/Open-PS2-Loader` | `3e3f34e9f94b058f7fd5b13727cb86e94fd3b35d` |

The active LUNA frontend is maintained in `dnunezx/nhddl-luna` on its `luna`
branch. The active backend is maintained in `dnunezx/neutrino-luna` on its
`master` branch. OPL source is maintained in `dnunezx/Open-PS2-Loader` on its
`opl-luna` branch. LUNA `main` links all three as submodules and pins their
exact commits; the base revisions above document the original upstream lineage.
LUNA builds a small set of OPL's EE and IOP game-core payloads separately.
The frontend loads them when an ATA game selects OPL in its per-game settings.
The fork renames the private in-memory IOPRP device to avoid colliding with
PCSX2 HostFS. Neutrino remains the default runtime.

LUNA's original code and modifications are attributed to:

**Danny Nunez (dnunezx) 2026**

Existing source-level copyright, attribution, and license notices are retained.
Files copied verbatim from upstream remain attributed only to their upstream
authors. Files modified for LUNA carry an explicit LUNA modification notice.
