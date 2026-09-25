// Original LUNA code: Danny Nunez (dnunezx) 2026
#ifndef LUNA_UI_ART_CACHE_H
#define LUNA_UI_ART_CACHE_H

#include "target.h"
#include "ui/navigation.h"
#include <gsKit.h>
#include <stdint.h>

extern GSTEXTURE *coverTexture;
extern GSTEXTURE *classicPreviousCoverTexture;
extern GSTEXTURE *discTexture;
extern GSTEXTURE *psbbnCoverTextures[PSBBN_COVER_CACHE_COUNT];
extern uint8_t psbbnCoverLoaded[PSBBN_COVER_CACHE_COUNT];
extern GSTEXTURE *gridCoverTextures[GRID_PAGE_BUFFERS][GRID_PAGE_SIZE];
extern uint8_t gridCoverLoaded[GRID_PAGE_BUFFERS][GRID_PAGE_SIZE];
extern GSTEXTURE *gridSelectedTextures[GRID_SELECTED_BUFFERS];
extern uint8_t gridSelectedLoaded[GRID_SELECTED_BUFFERS];
extern GSTEXTURE *orbsLogoTextures[ORBS_LOGO_CACHE_COUNT];
extern uint8_t orbsLogoLoaded[ORBS_LOGO_CACHE_COUNT];
extern GSTEXTURE *orbsBackgroundTexture;
extern uint8_t orbsBackgroundLoaded;

int artCacheInit(void);
void artCacheShutdown(void);
int loadCoverArt(struct DeviceMapEntry *device, char *titleID);
int loadNextClassicCoverArt(struct DeviceMapEntry *device, char *titleID);
int loadDiscArt(struct DeviceMapEntry *device, char *titleID);
void releaseClassicArtVRAM(void);
void releasePSBBNCovers(void);
void releasePSBBNCoverVRAM(void);
void prepareCollectionCovers(TargetList *titles, int focus);
void releaseGridCovers(void);
void releaseGridTexture(GSTEXTURE *texture);
int loadGridPageStep(TargetList *titles, int pageBase, int buffer, int *nextSlot,
                     int *didLoadArtwork);
int refreshGridSelectedCover(Target *target, int buffer);
void prepareGridPageBuffer(int buffer, int pageBase, int *pageBases,
                           int *pageComplete, int *pageNextSlot);
void refreshPSBBNCovers(TargetList *titles, int selectedTitleIdx, int previousTitleIdx);
void updatePSBBNCoverResidency(int flowOffset);
void refreshOrbsLogos(TargetList *titles, int selectedTitleIdx);
void refreshOrbsBackground(Target *target);
void releaseOrbsArt(void);
// Collection remaps by identity and prepares at most one missing cover per call.
int refreshCollectionCovers(TargetList *titles, int focus, int offset,
                             int direction, int allowLoad, int fast);

#endif
