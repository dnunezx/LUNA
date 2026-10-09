# Upstream lineage

LUNA contains modified source from the following projects:

| Component | Upstream | Base revision |
| --- | --- | --- |
| Frontend | `https://github.com/pcm720/nhddl` | `89141d470ea2e0a2f81ad485874869f057bca082` |
| Backend | `https://github.com/rickgaiser/neutrino` | `4c95d56713e32536aa4dc2a45e440cd6efa2d7bb` |
| MMCE module | `https://github.com/ps2-mmce/mmceman` | `0e78d1b35c71d1ee43b880c433586d830249b6f0` |
| OPL game-core source | `https://github.com/ps2homebrew/Open-PS2-Loader` | `3e3f34e9f94b058f7fd5b13727cb86e94fd3b35d` |
| Experimental PSXCore | `https://github.com/dnunezx/PSXCore` (private) | `a0c931f8d9672a53fc0a0ff5118ed139cc0c44b2` |

The active LUNA frontend is maintained in `dnunezx/nhddl-luna` on its `luna`
branch. The active backend is maintained in `dnunezx/neutrino-luna` on its
`master` branch. OPL source is maintained in `dnunezx/Open-PS2-Loader` on its
`opl-luna` branch. LUNA `main` links all three as submodules and pins their
exact commits; the base revisions above document the original upstream lineage.
The experimental `psxcore` submodule is pinned separately and requires
authenticated repository access. Its license and third-party notices remain
inside that submodule. User-supplied POPS firmware is not distributed with LUNA.
PSXCore is reference-only and must never be edited in this workspace. LUNA passes its existing CLI arguments to the
standalone ELF. Runtime/storage changes are maintained in PSXCore independently
of frontend integration.
LUNA builds a small set of OPL's EE and IOP game-core payloads separately.
The frontend selects OPL's ATA, general block-device, or file-handle payloads
for ATA/APA HDD, USB, MX4SIO, iLink, MMCE, and UDPFS games. MMCE and UDPFS
share Neutrino's transport drivers through their file-handle interface.
Neutrino remains the default runtime. Neutrino's controller hooks, game
patches, and disc-emulation modules also include OPL-derived code.
That code, the VMC formatter, and artwork lineage retain their source notices
and AFL-3.0 license in `LICENSES/NHDDL-AFL-3.0.txt`.
Both forks rename the private in-memory IOPRP device to avoid colliding
with PCSX2 HostFS.

LUNA's original code and modifications are attributed to:

**Danny Nunez (dnunezx) 2026**

Existing source-level copyright, attribution, and license notices are retained.
Files copied verbatim from upstream remain attributed only to their upstream
authors. Files modified for LUNA carry an explicit LUNA modification notice.
