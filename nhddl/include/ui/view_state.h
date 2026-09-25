#ifndef LUNA_UI_VIEW_STATE_H
#define LUNA_UI_VIEW_STATE_H

#include "target.h"
#include "ui/navigation.h"

// The selected view is stored with the selected title's metadata on its game drive.
// Missing or invalid state falls back to Classic.
UILibraryView loadLastLibraryView(Target *target);
int saveLastLibraryView(Target *target, UILibraryView view);

// Classic artwork layout is a library-wide preference on the metadata drive.
// Missing or invalid state keeps the original separate cover and disc layout.
int loadClassicArtOverlap(Target *target);
int saveClassicArtOverlap(Target *target, int overlap);

// Orbs is an experimental library view and is disabled by default.
int loadOrbsViewEnabled(Target *target);
int saveOrbsViewEnabled(Target *target, int enabled);

#endif
