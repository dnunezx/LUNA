// Original LUNA code: Danny Nunez (dnunezx) 2026
#ifndef LUNA_UI_VIEW_INTERNAL_H
#define LUNA_UI_VIEW_INTERNAL_H

#include "ui/graphics.h"
#include "ui/navigation.h"
#include "ui/art_cache.h"
#include "ui/views.h"
#include <stdint.h>

extern char lineBuffer[255];
extern const int keepoutArea;
extern const int headerHeight;
extern const int footerHeight;
uint32_t uiNowMs(void);
int discWave(uint32_t phase);
int psbbnFieldStableY(int y);
int psbbnFieldStableHeight(void);
void drawGlassDiamond(int centerX, int centerY, int radius, int z, uint64_t color);
void drawGlassPanel(int x1, int y1, int x2, int y2, int z);
void drawOrbitalDisc(int centerX, int centerY, int radius, int z,
                     uint64_t centerColor, uint64_t edgeColor);
void drawColorStarBackground(uint32_t frameNowMs);
void drawSharedLibraryBackground(uint32_t frameNowMs);
#ifdef LUNA_GLASS_UI
void drawPSBBNFocusGlow(int left, int top, int rowRight, int textRight);
#endif
void drawPSBBNCover(GSTEXTURE *cover, float x1, float y1, float size, int cacheIdx,
                    int emphasis, int visibility);
void formatPSBBNTitle(const char *source, char *destination, int maxWidth);

#endif
