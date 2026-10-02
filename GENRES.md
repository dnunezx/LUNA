# Genre metadata contract

Luna Loader supplies primary genres; LUNA reads and groups them. The new
library view and its two-stick navigation are intentionally deferred.

## HDD layout

For each game, use its canonical uppercase title ID (for example SLUS_203.12):

```
CFG/SLUS_203.12.cfg
ART/SLUS_203.12_COV.png
ART/PSBBN/SLUS_203.12.png
```

The CFG contains `Genre=RPG` alongside any existing OPL settings. LUNA reads
CFG from the same metadata device used for artwork (including APA/HDL's
separate metadata partition). Genre data is independent of LUNA's launcher
YAML options. Neither game files nor artwork files are renamed or moved.

## Labels and missing data

Both implementations use bounded ASCII labels of at most 47 characters.
Whitespace and hyphens are normalized; common aliases such as `Role playing
games` / `RPG` and `Driving` / `Racing` share a group. Compound values use the
first genre separated by `/`, `,`, `;`, or `|`. Unrecognized valid labels
remain usable custom groups. Invalid, missing, or empty values display as
`Uncategorized`, placed after named groups. One game appears in one group.

## Loader integration

`genres.service.ts` reads, assigns, bulk assigns, and fills missing genres.
Its IPC/preload APIs are `genreLabels`, `readGameGenres`, `assignGameGenres`,
and `fillGameGenres`. Angular's CfgService exposes these operations for the
future controls. LibraryService loads local genres into Game.genre when the
HDD library is scanned. Scanning never needs the network.

PS2 import and artwork download paths automatically fetch missing genres from
the English [PS2 OPL CFG Database](https://github.com/Tom-Bruise/PS2-OPL-CFG-Database)
at `CFG_en/<GAMEID>.cfg`. Only Genre is imported; catalog compatibility/VMC
values are ignored. Existing genre fields, including an explicit
`Genre=Uncategorized`, always win. Unknown catalog entries remain retryable
on a later run. Network errors are reported independently of artwork/import
success. Automatic PS1 lookup is not included; local PS1 CFG read/assignment
APIs work. No catalog files are bundled.

Genre updates re-read CFG under a per-file write lock, preserve all other
keys, and atomically replace the file through a temporary file. Manual and
bulk APIs return per-game results, including errors and whether data changed.

## LUNA integration

`storageRefresh` calls `lunaLoadLibraryGenres` once the canonical title list is
ready, including retained sources. Target.genre holds the normalized label;
TargetList.genres owns an index with alphabetically sorted groups and titles.
The index borrows Target pointers and never changes Target.idx, linked-list
order, launch paths, or artwork keys. It is invalidated when a target is freed
and released with the library. Allocation/metadata failure never blocks boot.

The future view can use `lunaGenreGroupCount` and `lunaGenreGroupTarget`, with
optional favorite flags indexed by canonical Target.idx. Each group also
provides selectedTitleIdx for remembering selection within that session.

## Verification

Run `sh tests/run-host-tests.sh` in nhddl for parsing, metadata-device fallback,
group ordering, canonical identity, favorites, and refresh tests. In Loader,
build then run `node tests/genres.test.cjs`; add `--live` for public-catalog
lookup checks. Offline tests cover CFG preservation, manual overrides,
concurrent assignment, missing catalog entries, and network errors.
