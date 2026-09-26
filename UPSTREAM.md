# Upstream lineage

LUNA contains modified source from the following projects:

| Component | Upstream | Base revision |
| --- | --- | --- |
| Frontend | `https://github.com/pcm720/nhddl` | `89141d470ea2e0a2f81ad485874869f057bca082` |
| Backend | `https://github.com/rickgaiser/neutrino` | `4c95d56713e32536aa4dc2a45e440cd6efa2d7bb` |
| MMCE module | `https://github.com/ps2-mmce/mmceman` | `0e78d1b35c71d1ee43b880c433586d830249b6f0` |

The active LUNA frontend is maintained in `dnunezx/nhddl-luna` on its `luna`
branch. The active backend is maintained in `dnunezx/neutrino-luna` on its
`master` branch. LUNA `main` links both as submodules and pins their exact
commits; the base revisions above document the original upstream lineage.

LUNA's original code and modifications are attributed to:

**Danny Nunez (dnunezx) 2026**

Existing source-level copyright, attribution, and license notices are retained.
Files copied verbatim from upstream remain attributed only to their upstream
authors. Files modified for LUNA carry an explicit LUNA modification notice.
