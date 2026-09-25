// Original LUNA code: Danny Nunez (dnunezx) 2026
#ifndef LUNA_UI_VIEWS_H
#define LUNA_UI_VIEWS_H

#include "target.h"
#include "ui/navigation.h"
#include <gsKit.h>
#include <stdint.h>

void calculateCoverArtGeometry(void);
void setClassicArtOverlap(int overlap);
void drawTitleList(TargetList *titles, int selectedTitleIdx, int maxTitlesPerPage,
                   GSTEXTURE *selectedTitleCover, GSTEXTURE *previousCover,
                   GSTEXTURE *selectedTitleDisc, const uint8_t *favoriteFlags,
                   int favoritesOnly, int coverPending, int coverTransitionProgress,
                   uint32_t frameNowMs);
void drawPSBBNCollection(TargetList *titles, int selectedTitleIdx, GSTEXTURE **covers,
                         int flowOffset, int favoritesOnly, uint32_t frameNowMs,
                         const LunaCollectionMotion *motion);
void drawPSBBNGrid(TargetList *titles, int selectedTitleIdx, int activeWindowBase,
                   int activeWindowBuffer, int incomingWindowBase, int incomingWindowBuffer,
                   int selectedCoverBuffer, int cascadeDirection, int cascadeProgress,
                   uint32_t frameNowMs);
void drawOrbit(TargetList *titles, int selectedTitleIdx, GSTEXTURE **covers,
               int flowOffset, int randomActive, uint32_t frameNowMs);

#endif
