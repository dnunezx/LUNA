// Original LUNA code: Danny Nunez (dnunezx) 2026
#include "ui/view_internal.h"
#include <stdio.h>
#include <string.h>

void drawPSBBNCover(GSTEXTURE *cover, float x1, float y1, float size, int cacheIdx, int emphasis, int visibility) {
  float x2 = x1 + size;
  float y2 = y1 + size;
  // GS texture modulation treats 0x80 as neutral. End the emphasis curve at
  // neutral instead of 0xFF, which nearly doubles and clips the jacket RGB.
  int red = (0x58 + (emphasis * 0x28) / 1000) * visibility / 1000;
  int green = (0x64 + (emphasis * 0x1C) / 1000) * visibility / 1000;
  int blue = (0x70 + (emphasis * 0x10) / 1000) * visibility / 1000;
  int alpha = (0x40 + (emphasis * 0x40) / 1000) * visibility / 1000;
  int z = 4 + (emphasis * 2) / 1000;

  if (cover != NULL && psbbnCoverLoaded[cacheIdx]) {
    int previousAlphaTest = gsGlobal->Test->ATST;
    int previousAlphaReference = gsGlobal->Test->AREF;
    int previousAlphaFail = gsGlobal->Test->AFAIL;
    gsKit_TexManager_bind(gsGlobal, cover);
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
    gsGlobal->Test->ATST = 2;
    gsGlobal->Test->AREF = 0x80;
    gsGlobal->Test->AFAIL = 0;
    gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
    gsKit_set_test(gsGlobal, GS_ATEST_ON);
    // PSBBN database jackets are opaque RGB squares. Drawing the same texture
    // enlarged underneath does not create a feathered halo; it exposes a
    // second copy of the top and bottom edge. Keep one filtered jacket pass.
    gsKit_prim_sprite_texture(gsGlobal, cover, x1, y1, 0.0f, 0.0f, x2, y2, cover->Width - 1, cover->Height - 1, z,
                              GS_SETREG_RGBA(red, green, blue, alpha));
    gsGlobal->Test->ATST = previousAlphaTest;
    gsGlobal->Test->AREF = previousAlphaReference;
    gsGlobal->Test->AFAIL = previousAlphaFail;
    gsKit_set_test(gsGlobal, GS_ATEST_ON);
    gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  } else {
    int placeholderAlpha = (0x42 + (emphasis * 0x36) / 1000) * visibility / 1000;
    gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, z, GS_SETREG_RGBA(0x04, 0x14, 0x34, placeholderAlpha));
  }
}

static float psbbnCurveValue(int distance, int selectedSize, const uint8_t *percent, int pointCount) {
  int segment;
  int fraction;
  float valuePercent;

  if (distance <= 0)
    return selectedSize * percent[0] / 100.0f;
  segment = distance / 1000;
  if (segment >= pointCount - 1)
    return selectedSize * percent[pointCount - 1] / 100.0f;

  fraction = distance - (segment * 1000);
  // Give the cover approaching focus a gentle final settle while preserving
  // the tightly compressed spacing farther down the stream.
  if (segment == 0)
    fraction = lunaNavEase(fraction);
  valuePercent = percent[segment] + ((percent[segment + 1] - percent[segment]) * fraction) / 1000.0f;
  return selectedSize * valuePercent / 100.0f;
}

static float psbbnFutureCoverSize(int distance, int selectedSize) {
  // Preserve the two nearest approved neighbors, then taper the distant tail
  // so it recedes toward a vanishing point instead of ending as a hard stack.
  static const uint8_t sizePercent[] = {100, 44, 41, 38, 30, 22, 14};
  return psbbnCurveValue(distance, selectedSize, sizePercent, sizeof(sizePercent));
}

static float psbbnFutureCenterOffset(int distance, int selectedSize) {
  static const uint8_t offsetPercent[] = {0, 50, 72, 93, 112, 127, 138};
  return psbbnCurveValue(distance, selectedSize, offsetPercent, sizeof(offsetPercent));
}

static float psbbnOutgoingCenterOffset(int distance, int selectedSize) {
  int squaredProgress;

  if (distance > 1000)
    distance = 1000;
  // Stay near the focal point while the cover grows, then accelerate to the
  // right late in the glide so the enlarged sprite clears the viewport.
  squaredProgress = distance * distance / 1000;
  return (selectedSize * 160.0f * squaredProgress) / 100000.0f;
}

static float psbbnOutgoingCoverSize(int distance, int selectedSize) {
  float progress;

  if (distance > 1000)
    distance = 1000;
  // PSBBN sends the old focal jacket toward the viewer, but it clears the
  // viewport before becoming an extreme magnification. Capping this at 1.75x
  // keeps the 256x256 jacket inside a useful filtering range.
  progress = distance / 1000.0f;
  return selectedSize * (1.0f + 0.75f * progress * progress);
}

static int psbbnAbsolute(int value) {
  return (value < 0) ? -value : value;
}

static void drawCollectionFooter(void) {
  int baseY = gsGlobal->Height - footerHeight + 8;
  int circleX = 34;
  int crossX = gsGlobal->Width / 2 - 42;
  int triangleX = gsGlobal->Width - 132;
  drawIconWindow(circleX, baseY, 0, gsGlobal->Height, 6, FontMainColor, ALIGN_CENTER, ICON_CIRCLE);
  drawTextWindow(circleX + getIconWidth(ICON_CIRCLE) + 6, baseY, crossX - 8, gsGlobal->Height, 6,
                 FontMainColor, ALIGN_VCENTER, "Grid");
  drawIconWindow(crossX, baseY, 0, gsGlobal->Height, 6, FontMainColor, ALIGN_CENTER, ICON_CROSS);
  drawTextWindow(crossX + getIconWidth(ICON_CROSS) + 6, baseY, triangleX - 8, gsGlobal->Height, 6,
                 FontMainColor, ALIGN_VCENTER, "Launch");
  drawIconWindow(triangleX, baseY, 0, gsGlobal->Height, 6, FontMainColor, ALIGN_CENTER, ICON_TRIANGLE);
  drawTextWindow(triangleX + getIconWidth(ICON_TRIANGLE) + 6, baseY, gsGlobal->Width - keepoutArea,
                 gsGlobal->Height, 6, FontMainColor, ALIGN_VCENTER, "Options");
}

void formatPSBBNTitle(const char *source, char *destination, int maxWidth) {
  size_t length;

  snprintf(destination, 255, "%s", source);
  if (getLineWidth(destination) <= maxWidth)
    return;

  length = strnlen(destination, 251);
  while (length > 0) {
    destination[length] = '\0';
    if (getLineWidth(destination) + getLineWidth("...") <= maxWidth)
      break;
    length--;
  }
  strncat(destination, "...", 254 - strlen(destination));
}

void drawPSBBNCollection(TargetList *titles, int selectedTitleIdx, GSTEXTURE **covers, int flowOffset,
                         int favoritesOnly, uint32_t frameNowMs) {
  int top = headerHeight + 12;
  int bottom = gsGlobal->Height - footerHeight - 18;
  int selectedSize = (bottom - top) * 76 / 100;
  int maxSelectedSize = gsGlobal->Width * 38 / 100;
  int selectedX;
  int selectedY;
  int centerY;
  int panelRight;
  int focalCenterX;
  int position[PSBBN_COVER_CACHE_COUNT];
  int distance[PSBBN_COVER_CACHE_COUNT];
  int targetIndex[PSBBN_COVER_CACHE_COUNT];
  uint8_t drawable[PSBBN_COVER_CACHE_COUNT];
  uint8_t drawn[PSBBN_COVER_CACHE_COUNT] = {0};
  int visualFocus = -1;
  int visualFocusDistance = 0x7FFFFFFF;
  int panelLineY;
  int panelLineHeight = psbbnFieldStableHeight();

  if (selectedSize > maxSelectedSize)
    selectedSize = maxSelectedSize;
  selectedSize = selectedSize * 115 / 100;
  selectedX = gsGlobal->Width - keepoutArea - selectedSize - 40;
  selectedY = headerHeight + 34;
  centerY = selectedY + selectedSize / 2;
  focalCenterX = selectedX + selectedSize / 2;
  panelRight = selectedX - 26;

  drawSharedLibraryBackground(frameNowMs);

  // The original shell keeps the Collection selector on the left and lets
  // the cover stack overlap its right edge.
  panelLineY = psbbnFieldStableY(headerHeight + 42);
  gsKit_prim_sprite(gsGlobal, 24, panelLineY, panelRight, panelLineY + panelLineHeight, 1, GS_SETREG_RGBA(0x2A, 0x74, 0xB8, 0x18));
  gsKit_prim_line(gsGlobal, 24, panelLineY, 24, psbbnFieldStableY(bottom), 1, GS_SETREG_RGBA(0x2A, 0x8C, 0xE8, 0x14));
  // Dock the selector low on the left so the cover tail can recede through the
  // space it previously occupied without colliding with the words.
  int selectorCenterY = psbbnFieldStableY(gsGlobal->Height - footerHeight - getFontLineHeight() - 16);
  int collectionTextY = selectorCenterY - getFontLineHeight() - 8;
  int favoritesTextY = selectorCenterY + 10;
  const char *collectionLabel = "Collection";
  const char *favoritesLabel = "Favorites";
  int activeTextY = favoritesOnly ? favoritesTextY : collectionTextY;
  int activeTextRight = 70 + getLineWidth(favoritesOnly ? favoritesLabel : collectionLabel);
  drawPSBBNFocusGlow(70, activeTextY, panelRight - 8, activeTextRight);
  drawTextWindow(70, collectionTextY, panelRight - 8, 0, 6,
                 favoritesOnly ? HeaderTextColor : FontMainColor, ALIGN_LEFT, "Collection");
  drawTextWindow(70, favoritesTextY, panelRight - 8, 0, 6,
                 favoritesOnly ? FontMainColor : HeaderTextColor, ALIGN_LEFT, "Favorites");

  if (titles->total <= 0) {
    drawTextWindow(40, headerHeight + 96, panelRight, selectorCenterY - getFontLineHeight() * 2,
                   5, HeaderTextColor, ALIGN_CENTER,
                   "NO FAVORITES YET\nAdd favorites in Classic List");
    drawCollectionFooter();
    return;
  }

  // Every cached title occupies one point on the same continuous horizontal
  // path. A fractional flow offset moves the whole sequence; size and glow are
  // derived from distance to the focal point instead of fixed card positions.
  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++) {
    int relativeIndex = cacheIdx - PSBBN_COVER_CACHE_FOCUS;
    position[cacheIdx] = relativeIndex * 1000 + flowOffset;
    distance[cacheIdx] = psbbnAbsolute(position[cacheIdx]);
    targetIndex[cacheIdx] = lunaNavWrap(titles->total, selectedTitleIdx + relativeIndex);
    drawable[cacheIdx] = 1;
    if (distance[cacheIdx] < visualFocusDistance) {
      visualFocus = cacheIdx;
      visualFocusDistance = distance[cacheIdx];
    }
  }

  // Keep the position counter tied to the cover nearest the animated focal
  // point. Collection intentionally omits the title above the focal cover.
  if (visualFocus >= 0) {
    char focusCounter[32];
    int counterRight = gsGlobal->Width - keepoutArea;

    snprintf(focusCounter, sizeof(focusCounter), "%d/%d", targetIndex[visualFocus] + 1, titles->total);
    drawTextWindow(selectedX + selectedSize - 58, favoritesTextY, counterRight, 0, 5,
                   GS_SETREG_RGBA(0xC8, 0xD4, 0xE8, 0x80), ALIGN_RIGHT, focusCounter);
  }

  // Small libraries wrap within the ten-entry cache. Keep only the closest
  // occurrence of each title so a one-game collection never draws duplicates.
  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++) {
    for (int priorIdx = 0; priorIdx < cacheIdx; priorIdx++) {
      if (!drawable[priorIdx] || targetIndex[priorIdx] != targetIndex[cacheIdx])
        continue;
      if (distance[priorIdx] <= distance[cacheIdx])
        drawable[cacheIdx] = 0;
      else
        drawable[priorIdx] = 0;
    }
  }

  // Future covers form the background stream and draw far-to-near. Covers on
  // the outgoing side represent foreground depth: draw them after the entire
  // stream, with the largest/closest zoom drawn last. This prevents the next
  // cover from clipping through the old cover halfway through the handoff.
  for (int pass = 0; pass < PSBBN_COVER_CACHE_COUNT; pass++) {
    int futureToDraw = -1;
    int outgoingToDraw = -1;
    int cacheToDraw;

    for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++) {
      if (!drawable[cacheIdx] || drawn[cacheIdx])
        continue;
      if (position[cacheIdx] < 0) {
        if (outgoingToDraw < 0 || distance[cacheIdx] < distance[outgoingToDraw])
          outgoingToDraw = cacheIdx;
      } else if (futureToDraw < 0 || distance[cacheIdx] > distance[futureToDraw]) {
        futureToDraw = cacheIdx;
      }
    }

    cacheToDraw = (futureToDraw >= 0) ? futureToDraw : outgoingToDraw;
    if (cacheToDraw < 0)
      break;

    drawn[cacheToDraw] = 1;
    float size;
    float itemCenterX;
    float itemCenterY;

    if (position[cacheToDraw] < 0) {
      size = psbbnOutgoingCoverSize(distance[cacheToDraw], selectedSize);
      itemCenterX = focalCenterX + psbbnOutgoingCenterOffset(distance[cacheToDraw], selectedSize);
      itemCenterY = centerY;
    } else {
      size = psbbnFutureCoverSize(distance[cacheToDraw], selectedSize);
      itemCenterX = focalCenterX - psbbnFutureCenterOffset(distance[cacheToDraw], selectedSize);
      itemCenterY = centerY + (distance[cacheToDraw] * 3.0f) / 2000.0f;
    }
    float x1 = itemCenterX - size / 2.0f;
    float y1 = itemCenterY - size / 2.0f;
    int focusProgress = 1000 - ((distance[cacheToDraw] < 1000) ? distance[cacheToDraw] : 1000);
    int emphasis = lunaNavEase(focusProgress);
    int visibility = 1000;

    if (position[cacheToDraw] >= 0 && distance[cacheToDraw] > 1000) {
      int fadeDistance = distance[cacheToDraw] - 1000;
      if (fadeDistance > 5000)
        fadeDistance = 5000;
      visibility = 1000 - (fadeDistance * 850) / 5000;
    }

    if (x1 + size > 24.0f && x1 < (float)(gsGlobal->Width - keepoutArea))
      drawPSBBNCover(covers[cacheToDraw], x1, y1, size, cacheToDraw, emphasis, visibility);
  }
  drawCollectionFooter();
}
