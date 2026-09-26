# Neutrino source

LUNA uses [dnunezx/neutrino-luna](https://github.com/dnunezx/neutrino-luna)
as the `neutrino` Git submodule. Its first LUNA commit (`ade12df`) replays
LUNA's in-game return and DEV9 shutdown changes onto upstream Neutrino commit
`7be8de2`. The submodule pins the exact version used by this LUNA checkout.

After cloning LUNA, initialize the source with:

```powershell
git submodule update --init --recursive
```

Build the backend from the workspace root as described in `BUILDING.md`.
When updating the fork, commit and push changes in `neutrino` first, then
commit the new submodule pointer in LUNA.
