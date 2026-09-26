// Original LUNA code: Danny Nunez (dnunezx) 2026
#include "devices/devices.h"
#include "ui/art_cache.h"
#include "ui/graphics.h"
#include "ui/navigation.h"

#include <malloc.h>
#include <gsToolkit.h>
#include <stdio.h>

#define PSBBN_THUMBNAIL_SIZE 64
#define GRID_THUMBNAIL_SIZE 64

typedef struct {
  uint8_t r, g, b, a;
} PSBBNPixel;

GSTEXTURE *coverTexture;
GSTEXTURE *classicPreviousCoverTexture;
GSTEXTURE *discTexture;
GSTEXTURE *psbbnCoverTextures[PSBBN_COVER_CACHE_COUNT];
uint8_t psbbnCoverLoaded[PSBBN_COVER_CACHE_COUNT];
static uint8_t psbbnCoverFullResolution[PSBBN_COVER_CACHE_COUNT];
static void *psbbnCoverSourcePixels[PSBBN_COVER_CACHE_COUNT];
static int psbbnCoverSourceWidth[PSBBN_COVER_CACHE_COUNT];
static int psbbnCoverSourceHeight[PSBBN_COVER_CACHE_COUNT];
GSTEXTURE *gridCoverTextures[GRID_PAGE_BUFFERS][GRID_PAGE_SIZE];
uint8_t gridCoverLoaded[GRID_PAGE_BUFFERS][GRID_PAGE_SIZE];
GSTEXTURE *gridSelectedTextures[GRID_SELECTED_BUFFERS];
uint8_t gridSelectedLoaded[GRID_SELECTED_BUFFERS];
GSTEXTURE *orbsLogoTextures[ORBS_LOGO_CACHE_COUNT];
uint8_t orbsLogoLoaded[ORBS_LOGO_CACHE_COUNT];
GSTEXTURE *orbsBackgroundTexture;
uint8_t orbsBackgroundLoaded;
static int orbsLogoTargets[ORBS_LOGO_CACHE_COUNT];
static int orbsBackgroundTarget = -1;

static const char artPath[] = "/ART";
static const char psbbnArtPath[] = "/ART/PSBBN";
static const char orbsArtPath[] = "/ART/ORBS";
static char artPathBuffer[255];

int artCacheInit(void) {
  coverTexture = calloc(sizeof(GSTEXTURE), 1);
  classicPreviousCoverTexture = calloc(sizeof(GSTEXTURE), 1);
  discTexture = calloc(sizeof(GSTEXTURE), 1);
  if (coverTexture == NULL || classicPreviousCoverTexture == NULL || discTexture == NULL) {
    artCacheShutdown();
    return -1;
  }
  coverTexture->Delayed = 1;
  classicPreviousCoverTexture->Delayed = 1;
  discTexture->Delayed = 1;

  for (int i = 0; i < PSBBN_COVER_CACHE_COUNT; i++) {
    psbbnCoverTextures[i] = calloc(sizeof(GSTEXTURE), 1);
    if (psbbnCoverTextures[i] == NULL) {
      artCacheShutdown();
      return -1;
    }
    psbbnCoverTextures[i]->Delayed = 1;
  }
  for (int buffer = 0; buffer < GRID_PAGE_BUFFERS; buffer++) {
    for (int i = 0; i < GRID_PAGE_SIZE; i++) {
      gridCoverTextures[buffer][i] = calloc(sizeof(GSTEXTURE), 1);
      if (gridCoverTextures[buffer][i] == NULL) {
        artCacheShutdown();
        return -1;
      }
      gridCoverTextures[buffer][i]->Delayed = 1;
    }
  }
  for (int buffer = 0; buffer < GRID_SELECTED_BUFFERS; buffer++) {
    gridSelectedTextures[buffer] = calloc(sizeof(GSTEXTURE), 1);
    if (gridSelectedTextures[buffer] == NULL) {
      artCacheShutdown();
      return -1;
    }
    gridSelectedTextures[buffer]->Delayed = 1;
  }
  orbsBackgroundTexture = calloc(sizeof(GSTEXTURE), 1);
  if (orbsBackgroundTexture == NULL) {
    artCacheShutdown();
    return -1;
  }
  orbsBackgroundTexture->Delayed = 1;
  orbsBackgroundTarget = -1;
  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    orbsLogoTextures[i] = calloc(sizeof(GSTEXTURE), 1);
    if (orbsLogoTextures[i] == NULL) {
      artCacheShutdown();
      return -1;
    }
    orbsLogoTextures[i]->Delayed = 1;
    orbsLogoTargets[i] = -1;
  }
  return 0;
}

static int loadCoverArtInto(struct DeviceMapEntry *device, char *titleID,
                            GSTEXTURE *texture) {
  if (device->metadev) { // Fallback to metadata device
    device = device->metadev;
  }
  // Reuse line buffer for building texture path
  snprintf(artPathBuffer, 255, "%s%s/%s_COV.png", device->mountpoint, artPath, titleID);
  gsKit_TexManager_invalidate(gsGlobal, texture);
  free(texture->Mem);
  texture->Mem = NULL;
  free(texture->Clut);
  texture->Clut = NULL;
  if (gsKit_texture_png(gsGlobal, texture, artPathBuffer)) {
    return -1;
  }
  gsKit_TexManager_bind(gsGlobal, texture);
  // Retain the decoded source in EE RAM. If the texture manager evicts the
  // cover while binding fonts or disc art, a later bind can safely re-upload it.
  return 0;
}

int loadCoverArt(struct DeviceMapEntry *device, char *titleID) {
  return loadCoverArtInto(device, titleID, coverTexture);
}

int loadNextClassicCoverArt(struct DeviceMapEntry *device, char *titleID) {
  GSTEXTURE *previous;
  int result = loadCoverArtInto(device, titleID, classicPreviousCoverTexture);
  previous = coverTexture;
  coverTexture = classicPreviousCoverTexture;
  classicPreviousCoverTexture = previous;
  return result;
}

void releaseClassicArtVRAM(void) {
  GSTEXTURE *textures[] = {coverTexture, classicPreviousCoverTexture, discTexture};
  for (int i = 0; i < 3; i++) {
    if (textures[i] != NULL && textures[i]->Vram != 0)
      gsKit_TexManager_free(gsGlobal, textures[i]);
    if (textures[i] != NULL)
      textures[i]->Vram = 0;
  }
}

// OPL Manager stores transparent disc-label artwork as ART/<TITLE_ID>_ICO.png.
int loadDiscArt(struct DeviceMapEntry *device, char *titleID) {
  if (device->metadev) {
    device = device->metadev;
  }
  snprintf(artPathBuffer, 255, "%s%s/%s_ICO.png", device->mountpoint, artPath, titleID);
  gsKit_TexManager_invalidate(gsGlobal, discTexture);
  free(discTexture->Mem);
  discTexture->Mem = NULL;
  free(discTexture->Clut);
  discTexture->Clut = NULL;
  if (loadPNGTextureRGBA(gsGlobal, discTexture, artPathBuffer)) {
    return -1;
  }
  discTexture->Filter = GS_FILTER_LINEAR;
  return 0;
}

static void releasePSBBNCoverCacheEntry(int cacheIdx) {
  GSTEXTURE *texture = psbbnCoverTextures[cacheIdx];
  void *sourcePixels = psbbnCoverSourcePixels[cacheIdx];

  if (texture->Vram != 0)
    gsKit_TexManager_free(gsGlobal, texture);
  texture->Vram = 0;
  if (texture->Mem != NULL && texture->Mem != sourcePixels)
    free(texture->Mem);
  texture->Mem = NULL;
  free(sourcePixels);
  psbbnCoverSourcePixels[cacheIdx] = NULL;
  psbbnCoverSourceWidth[cacheIdx] = 0;
  psbbnCoverSourceHeight[cacheIdx] = 0;
  psbbnCoverLoaded[cacheIdx] = 0;
  psbbnCoverFullResolution[cacheIdx] = 0;
}

// PSBBN jacket art fades into the scene instead of sitting inside a hard
// rectangular card. Apply the feather once to the decoded EE-RAM pixels so it
// is inherited by both the focused texture and the runtime thumbnails.
static void featherPSBBNCoverEdges(GSTEXTURE *texture) {
  PSBBNPixel *pixels = (PSBBNPixel *)texture->Mem;
  int shortestSide = (texture->Width < texture->Height) ? texture->Width : texture->Height;
  int featherWidth = shortestSide * 7 / 100;

  if (pixels == NULL || featherWidth < 2)
    return;

  for (int y = 0; y < texture->Height; y++) {
    int edgeY = y;
    if (texture->Height - 1 - y < edgeY)
      edgeY = texture->Height - 1 - y;

    for (int x = 0; x < texture->Width; x++) {
      int edgeDistance = x;
      int inverseAlpha;
      int progress;
      int feather;

      if (texture->Width - 1 - x < edgeDistance)
        edgeDistance = texture->Width - 1 - x;
      if (edgeY < edgeDistance)
        edgeDistance = edgeY;
      if (edgeDistance >= featherWidth)
        continue;

      // Smoothstep avoids a visible straight threshold at the inside edge.
      progress = edgeDistance * 1000 / featherWidth;
      feather = (int)(((int64_t)progress * progress * (3000 - 2 * progress)) / 1000000LL);
      inverseAlpha = pixels[y * texture->Width + x].a;
      pixels[y * texture->Width + x].a = 128 - (((128 - inverseAlpha) * feather) / 1000);
    }
  }
}

static void setPSBBNCoverResidentSize(int cacheIdx, int selected) {
  GSTEXTURE *texture = psbbnCoverTextures[cacheIdx];
  void *previousUpload = texture->Mem;
  void *uploadPixels;

  if (!psbbnCoverLoaded[cacheIdx] || psbbnCoverSourcePixels[cacheIdx] == NULL)
    return;
  if (texture->Vram != 0 && psbbnCoverFullResolution[cacheIdx] == (selected != 0))
    return;

  if (texture->Vram != 0)
    gsKit_TexManager_free(gsGlobal, texture);
  texture->Vram = 0;
  if (previousUpload != NULL && previousUpload != psbbnCoverSourcePixels[cacheIdx])
    free(previousUpload);
  texture->VramClut = 0;
  texture->Clut = NULL;
  texture->PSM = GS_PSM_CT32;
  texture->Filter = GS_FILTER_LINEAR;

  if (selected) {
    texture->Width = psbbnCoverSourceWidth[cacheIdx];
    texture->Height = psbbnCoverSourceHeight[cacheIdx];
    uploadPixels = psbbnCoverSourcePixels[cacheIdx];
  } else {
    PSBBNPixel *thumbnail = memalign(128, PSBBN_THUMBNAIL_SIZE * PSBBN_THUMBNAIL_SIZE * sizeof(PSBBNPixel));
    PSBBNPixel *source = (PSBBNPixel *)psbbnCoverSourcePixels[cacheIdx];
    for (int y = 0; y < PSBBN_THUMBNAIL_SIZE; y++) {
      int sourceY1 = y * psbbnCoverSourceHeight[cacheIdx] / PSBBN_THUMBNAIL_SIZE;
      int sourceY2 = (y + 1) * psbbnCoverSourceHeight[cacheIdx] / PSBBN_THUMBNAIL_SIZE;
      if (sourceY2 <= sourceY1)
        sourceY2 = sourceY1 + 1;
      for (int x = 0; x < PSBBN_THUMBNAIL_SIZE; x++) {
        int sourceX1 = x * psbbnCoverSourceWidth[cacheIdx] / PSBBN_THUMBNAIL_SIZE;
        int sourceX2 = (x + 1) * psbbnCoverSourceWidth[cacheIdx] / PSBBN_THUMBNAIL_SIZE;
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;
        int samples = 0;

        if (sourceX2 <= sourceX1)
          sourceX2 = sourceX1 + 1;
        for (int sourceY = sourceY1; sourceY < sourceY2; sourceY++) {
          for (int sourceX = sourceX1; sourceX < sourceX2; sourceX++) {
            PSBBNPixel pixel = source[sourceY * psbbnCoverSourceWidth[cacheIdx] + sourceX];
            red += pixel.r;
            green += pixel.g;
            blue += pixel.b;
            alpha += pixel.a;
            samples++;
          }
        }
        thumbnail[y * PSBBN_THUMBNAIL_SIZE + x].r = red / samples;
        thumbnail[y * PSBBN_THUMBNAIL_SIZE + x].g = green / samples;
        thumbnail[y * PSBBN_THUMBNAIL_SIZE + x].b = blue / samples;
        thumbnail[y * PSBBN_THUMBNAIL_SIZE + x].a = alpha / samples;
      }
    }
    texture->Width = PSBBN_THUMBNAIL_SIZE;
    texture->Height = PSBBN_THUMBNAIL_SIZE;
    uploadPixels = thumbnail;
  }

  texture->Mem = uploadPixels;
  gsKit_TexManager_bind(gsGlobal, texture);
  psbbnCoverFullResolution[cacheIdx] = (selected != 0);
}

// Loads one PSBBN square artwork asset from the metadata device. The decoded
// source stays in EE RAM while the focused and immediate transition covers are
// full-size in GS VRAM; other neighbors use compact runtime thumbnails. Source
// PNG files are not altered, and promotion reuses their full decoded pixels.
static int loadPSBBNCoverArt(struct DeviceMapEntry *device, char *titleID, int cacheIdx, int selected) {
  GSTEXTURE *texture = psbbnCoverTextures[cacheIdx];

  if (device->metadev) { // Fallback to metadata device
    device = device->metadev;
  }
  releasePSBBNCoverCacheEntry(cacheIdx);
  snprintf(artPathBuffer, 255, "%s%s/%s.png", device->mountpoint, psbbnArtPath, titleID);
  if (loadPNGTextureRGBA(gsGlobal, texture, artPathBuffer)) {
    return -1;
  }

  featherPSBBNCoverEdges(texture);
  psbbnCoverSourcePixels[cacheIdx] = texture->Mem;
  psbbnCoverSourceWidth[cacheIdx] = texture->Width;
  psbbnCoverSourceHeight[cacheIdx] = texture->Height;
  psbbnCoverLoaded[cacheIdx] = 1;
  setPSBBNCoverResidentSize(cacheIdx, selected);
  return 0;
}

void releasePSBBNCovers(void) {
  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++)
    releasePSBBNCoverCacheEntry(cacheIdx);
}

void releaseGridTexture(GSTEXTURE *texture) {
  if (texture->Vram != 0)
    gsKit_TexManager_free(gsGlobal, texture);
  texture->Vram = 0;
  free(texture->Mem);
  free(texture->Clut);
  texture->Mem = NULL;
  texture->Clut = NULL;
}

void releaseOrbsArt(void) {
  if (orbsBackgroundTexture != NULL)
    releaseGridTexture(orbsBackgroundTexture);
  orbsBackgroundLoaded = 0;
  orbsBackgroundTarget = -1;
  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    if (orbsLogoTextures[i] != NULL)
      releaseGridTexture(orbsLogoTextures[i]);
    orbsLogoLoaded[i] = 0;
    orbsLogoTargets[i] = -1;
  }
}

static void loadOrbsLogo(TargetList *titles, int targetIdx, int cacheIdx) {
  Target *target = getTargetByIdx(titles, targetIdx);
  struct DeviceMapEntry *device = target->device;
  GSTEXTURE *texture = orbsLogoTextures[cacheIdx];

  if (device->metadev)
    device = device->metadev;
  releaseGridTexture(texture);
  snprintf(artPathBuffer, sizeof(artPathBuffer), "%s%s/%s_LGO.png",
           device->mountpoint, orbsArtPath, target->id);
  orbsLogoLoaded[cacheIdx] =
      loadPNGTextureRGBA(gsGlobal, texture, artPathBuffer) == 0;
  if (orbsLogoLoaded[cacheIdx])
    texture->Filter = GS_FILTER_LINEAR;
  orbsLogoTargets[cacheIdx] = targetIdx;
}

void refreshOrbsLogos(TargetList *titles, int selectedTitleIdx) {
  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    int wanted = lunaNavWrap(titles->total,
                             selectedTitleIdx + i - ORBS_LOGO_CACHE_FOCUS);
    int match = -1;
    if (orbsLogoTargets[i] == wanted)
      continue;
    for (int j = i + 1; j < ORBS_LOGO_CACHE_COUNT; j++) {
      if (orbsLogoTargets[j] == wanted) {
        match = j;
        break;
      }
    }
    if (match >= 0) {
      GSTEXTURE *texture = orbsLogoTextures[i];
      uint8_t loaded = orbsLogoLoaded[i];
      int target = orbsLogoTargets[i];
      orbsLogoTextures[i] = orbsLogoTextures[match];
      orbsLogoLoaded[i] = orbsLogoLoaded[match];
      orbsLogoTargets[i] = orbsLogoTargets[match];
      orbsLogoTextures[match] = texture;
      orbsLogoLoaded[match] = loaded;
      orbsLogoTargets[match] = target;
    } else {
      loadOrbsLogo(titles, wanted, i);
    }
  }
}

void refreshOrbsBackground(Target *target) {
  struct DeviceMapEntry *device;
  if (orbsBackgroundTarget == target->idx)
    return;
  device = target->device;
  if (device->metadev)
    device = device->metadev;
  releaseGridTexture(orbsBackgroundTexture);
  snprintf(artPathBuffer, sizeof(artPathBuffer), "%s%s/%s_BG.png",
           device->mountpoint, orbsArtPath, target->id);
  orbsBackgroundLoaded =
      loadPNGTextureRGBA(gsGlobal, orbsBackgroundTexture, artPathBuffer) == 0;
  if (orbsBackgroundLoaded)
    orbsBackgroundTexture->Filter = GS_FILTER_LINEAR;
  orbsBackgroundTarget = target->idx;
}

static int loadGridCoverArt(struct DeviceMapEntry *device, char *titleID, GSTEXTURE *texture, int thumbnail) {
  if (device->metadev)
    device = device->metadev;

  releaseGridTexture(texture);
  snprintf(artPathBuffer, 255, "%s%s/%s.png", device->mountpoint, psbbnArtPath, titleID);
  if (thumbnail ? decodePNGTextureRGBA(gsGlobal, texture, artPathBuffer) : loadPNGTextureRGBA(gsGlobal, texture, artPathBuffer))
    return -1;
  texture->Filter = GS_FILTER_LINEAR;

  if (thumbnail) {
    PSBBNPixel *source = (PSBBNPixel *)texture->Mem;
    int sourceWidth = texture->Width;
    int sourceHeight = texture->Height;
    PSBBNPixel *pixels = memalign(128, GRID_THUMBNAIL_SIZE * GRID_THUMBNAIL_SIZE * sizeof(PSBBNPixel));

    if (pixels == NULL) {
      releaseGridTexture(texture);
      return -1;
    }
    for (int y = 0; y < GRID_THUMBNAIL_SIZE; y++) {
      int sourceY1 = y * sourceHeight / GRID_THUMBNAIL_SIZE;
      int sourceY2 = (y + 1) * sourceHeight / GRID_THUMBNAIL_SIZE;
      if (sourceY2 <= sourceY1)
        sourceY2 = sourceY1 + 1;
      for (int x = 0; x < GRID_THUMBNAIL_SIZE; x++) {
        int sourceX1 = x * sourceWidth / GRID_THUMBNAIL_SIZE;
        int sourceX2 = (x + 1) * sourceWidth / GRID_THUMBNAIL_SIZE;
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;
        int samples = 0;

        if (sourceX2 <= sourceX1)
          sourceX2 = sourceX1 + 1;
        for (int sourceY = sourceY1; sourceY < sourceY2; sourceY++) {
          for (int sourceX = sourceX1; sourceX < sourceX2; sourceX++) {
            PSBBNPixel pixel = source[sourceY * sourceWidth + sourceX];
            red += pixel.r;
            green += pixel.g;
            blue += pixel.b;
            alpha += pixel.a;
            samples++;
          }
        }
        pixels[y * GRID_THUMBNAIL_SIZE + x].r = red / samples;
        pixels[y * GRID_THUMBNAIL_SIZE + x].g = green / samples;
        pixels[y * GRID_THUMBNAIL_SIZE + x].b = blue / samples;
        pixels[y * GRID_THUMBNAIL_SIZE + x].a = alpha / samples;
      }
    }

    free(source);
    texture->Mem = (u32 *)pixels;
    texture->Width = GRID_THUMBNAIL_SIZE;
    texture->Height = GRID_THUMBNAIL_SIZE;
    texture->VramClut = 0;
    texture->Clut = NULL;
    texture->PSM = GS_PSM_CT32;
    texture->Filter = GS_FILTER_LINEAR;
    gsKit_TexManager_bind(gsGlobal, texture);
  }
  return 0;
}

void releaseGridCovers(void) {
  for (int buffer = 0; buffer < GRID_PAGE_BUFFERS; buffer++) {
    for (int slot = 0; slot < GRID_PAGE_SIZE; slot++) {
      releaseGridTexture(gridCoverTextures[buffer][slot]);
      gridCoverLoaded[buffer][slot] = 0;
    }
  }
  for (int buffer = 0; buffer < GRID_SELECTED_BUFFERS; buffer++) {
    releaseGridTexture(gridSelectedTextures[buffer]);
    gridSelectedLoaded[buffer] = 0;
  }
}

static void resetGridPageBuffer(int buffer) {
  for (int slot = 0; slot < GRID_PAGE_SIZE; slot++) {
    releaseGridTexture(gridCoverTextures[buffer][slot]);
    gridCoverLoaded[buffer][slot] = 0;
  }
}

// Loads at most one PNG per UI frame. Page changes therefore keep input and
// animation cadence responsive instead of decoding all sixteen covers in one
// blocking burst.
int loadGridPageStep(TargetList *titles, int pageBase, int buffer, int *nextSlot, int *didLoadArtwork) {
  *didLoadArtwork = 0;
  while (*nextSlot < GRID_PAGE_SIZE) {
    int slot = (*nextSlot)++;
    int targetIdx = pageBase + slot;

    if (targetIdx < titles->total) {
      Target *target = getTargetByIdx(titles, targetIdx);
      gridCoverLoaded[buffer][slot] =
          (loadGridCoverArt(target->device, target->id, gridCoverTextures[buffer][slot], 1) == 0);
      *didLoadArtwork = 1;
      break;
    }
  }
  return *nextSlot >= GRID_PAGE_SIZE;
}

int refreshGridSelectedCover(Target *target, int buffer) {
  gridSelectedLoaded[buffer] = (loadGridCoverArt(target->device, target->id, gridSelectedTextures[buffer], 0) == 0);
  return gridSelectedLoaded[buffer];
}

void prepareGridPageBuffer(int buffer, int pageBase, int *pageBases, int *pageComplete, int *pageNextSlot) {
  resetGridPageBuffer(buffer);
  pageBases[buffer] = pageBase;
  pageComplete[buffer] = 0;
  pageNextSlot[buffer] = 0;
}

static void loadPSBBNCoverCacheEntry(TargetList *titles, int selectedTitleIdx, int cacheIdx) {
  int targetIdx = lunaNavWrap(titles->total, selectedTitleIdx + cacheIdx - PSBBN_COVER_CACHE_FOCUS);
  Target *target = getTargetByIdx(titles, targetIdx);
  psbbnCoverLoaded[cacheIdx] = (loadPSBBNCoverArt(target->device, target->id, cacheIdx,
                                                  cacheIdx == PSBBN_COVER_CACHE_FOCUS ||
                                                  cacheIdx == PSBBN_COVER_CACHE_FOCUS - 1) == 0);
}

void refreshPSBBNCovers(TargetList *titles, int selectedTitleIdx, int previousTitleIdx) {
  int direction = lunaNavDirection(titles->total, previousTitleIdx, selectedTitleIdx);

  if (previousTitleIdx >= 0 && direction > 0 && lunaNavWrap(titles->total, previousTitleIdx + 1) == selectedTitleIdx) {
    GSTEXTURE *recycledTexture = psbbnCoverTextures[0];
    void *recycledSource = psbbnCoverSourcePixels[0];
    int recycledWidth = psbbnCoverSourceWidth[0];
    int recycledHeight = psbbnCoverSourceHeight[0];
    uint8_t recycledFullResolution = psbbnCoverFullResolution[0];
    for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT - 1; cacheIdx++) {
      psbbnCoverTextures[cacheIdx] = psbbnCoverTextures[cacheIdx + 1];
      psbbnCoverLoaded[cacheIdx] = psbbnCoverLoaded[cacheIdx + 1];
      psbbnCoverSourcePixels[cacheIdx] = psbbnCoverSourcePixels[cacheIdx + 1];
      psbbnCoverSourceWidth[cacheIdx] = psbbnCoverSourceWidth[cacheIdx + 1];
      psbbnCoverSourceHeight[cacheIdx] = psbbnCoverSourceHeight[cacheIdx + 1];
      psbbnCoverFullResolution[cacheIdx] = psbbnCoverFullResolution[cacheIdx + 1];
    }
    psbbnCoverTextures[PSBBN_COVER_CACHE_COUNT - 1] = recycledTexture;
    psbbnCoverSourcePixels[PSBBN_COVER_CACHE_COUNT - 1] = recycledSource;
    psbbnCoverSourceWidth[PSBBN_COVER_CACHE_COUNT - 1] = recycledWidth;
    psbbnCoverSourceHeight[PSBBN_COVER_CACHE_COUNT - 1] = recycledHeight;
    psbbnCoverFullResolution[PSBBN_COVER_CACHE_COUNT - 1] = recycledFullResolution;
    releasePSBBNCoverCacheEntry(PSBBN_COVER_CACHE_COUNT - 1);
    loadPSBBNCoverCacheEntry(titles, selectedTitleIdx, PSBBN_COVER_CACHE_COUNT - 1);
    return;
  }

  if (previousTitleIdx >= 0 && direction < 0 && lunaNavWrap(titles->total, previousTitleIdx - 1) == selectedTitleIdx) {
    GSTEXTURE *recycledTexture = psbbnCoverTextures[PSBBN_COVER_CACHE_COUNT - 1];
    void *recycledSource = psbbnCoverSourcePixels[PSBBN_COVER_CACHE_COUNT - 1];
    int recycledWidth = psbbnCoverSourceWidth[PSBBN_COVER_CACHE_COUNT - 1];
    int recycledHeight = psbbnCoverSourceHeight[PSBBN_COVER_CACHE_COUNT - 1];
    uint8_t recycledFullResolution = psbbnCoverFullResolution[PSBBN_COVER_CACHE_COUNT - 1];
    for (int cacheIdx = PSBBN_COVER_CACHE_COUNT - 1; cacheIdx > 0; cacheIdx--) {
      psbbnCoverTextures[cacheIdx] = psbbnCoverTextures[cacheIdx - 1];
      psbbnCoverLoaded[cacheIdx] = psbbnCoverLoaded[cacheIdx - 1];
      psbbnCoverSourcePixels[cacheIdx] = psbbnCoverSourcePixels[cacheIdx - 1];
      psbbnCoverSourceWidth[cacheIdx] = psbbnCoverSourceWidth[cacheIdx - 1];
      psbbnCoverSourceHeight[cacheIdx] = psbbnCoverSourceHeight[cacheIdx - 1];
      psbbnCoverFullResolution[cacheIdx] = psbbnCoverFullResolution[cacheIdx - 1];
    }
    psbbnCoverTextures[0] = recycledTexture;
    psbbnCoverSourcePixels[0] = recycledSource;
    psbbnCoverSourceWidth[0] = recycledWidth;
    psbbnCoverSourceHeight[0] = recycledHeight;
    psbbnCoverFullResolution[0] = recycledFullResolution;
    releasePSBBNCoverCacheEntry(0);
    loadPSBBNCoverCacheEntry(titles, selectedTitleIdx, 0);
    return;
  }

  releasePSBBNCovers();
  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++)
    loadPSBBNCoverCacheEntry(titles, selectedTitleIdx, cacheIdx);
}

// Held input can leave the rendered focal point more than one title behind the
// logical selection. Keep the two textures nearest the moving visual focus at
// full resolution; pinning resolution to the fixed cache focus would enlarge a
// 64x64 thumbnail during fast traversal. Demote first so promotion never causes
// a temporary third full-size allocation in the GS's constrained VRAM.
void updatePSBBNCoverResidency(int flowOffset) {
  int closest = -1;
  int nextClosest = -1;
  int closestDistance = 0x7FFFFFFF;
  int nextClosestDistance = 0x7FFFFFFF;

  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++) {
    int position;
    int distance;

    if (!psbbnCoverLoaded[cacheIdx])
      continue;
    position = (cacheIdx - PSBBN_COVER_CACHE_FOCUS) * 1000 + flowOffset;
    distance = (position < 0) ? -position : position;
    if (distance < closestDistance) {
      nextClosest = closest;
      nextClosestDistance = closestDistance;
      closest = cacheIdx;
      closestDistance = distance;
    } else if (distance < nextClosestDistance) {
      nextClosest = cacheIdx;
      nextClosestDistance = distance;
    }
  }

  for (int cacheIdx = 0; cacheIdx < PSBBN_COVER_CACHE_COUNT; cacheIdx++) {
    if (cacheIdx != closest && cacheIdx != nextClosest)
      setPSBBNCoverResidentSize(cacheIdx, 0);
  }
  if (closest >= 0)
    setPSBBNCoverResidentSize(closest, 1);
  if (nextClosest >= 0)
    setPSBBNCoverResidentSize(nextClosest, 1);
}

void artCacheShutdown(void) {
  if (orbsBackgroundTexture != NULL) {
    free(orbsBackgroundTexture->Mem);
    free(orbsBackgroundTexture->Clut);
    free(orbsBackgroundTexture);
    orbsBackgroundTexture = NULL;
  }
  orbsBackgroundLoaded = 0;
  orbsBackgroundTarget = -1;
  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    if (orbsLogoTextures[i] != NULL) {
      free(orbsLogoTextures[i]->Mem);
      free(orbsLogoTextures[i]->Clut);
      free(orbsLogoTextures[i]);
      orbsLogoTextures[i] = NULL;
    }
    orbsLogoLoaded[i] = 0;
    orbsLogoTargets[i] = -1;
  }
  if (coverTexture != NULL) {
    free(coverTexture->Mem);
    free(coverTexture->Clut);
    free(coverTexture);
    coverTexture = NULL;
  }
  if (classicPreviousCoverTexture != NULL) {
    free(classicPreviousCoverTexture->Mem);
    free(classicPreviousCoverTexture->Clut);
    free(classicPreviousCoverTexture);
    classicPreviousCoverTexture = NULL;
  }
  if (discTexture != NULL) {
    free(discTexture->Mem);
    free(discTexture->Clut);
    free(discTexture);
    discTexture = NULL;
  }
  for (int i = 0; i < PSBBN_COVER_CACHE_COUNT; i++) {
    if (psbbnCoverTextures[i] != NULL) {
      if (psbbnCoverTextures[i]->Mem != NULL &&
          psbbnCoverTextures[i]->Mem != psbbnCoverSourcePixels[i])
        free(psbbnCoverTextures[i]->Mem);
      free(psbbnCoverTextures[i]);
      psbbnCoverTextures[i] = NULL;
    }
    free(psbbnCoverSourcePixels[i]);
    psbbnCoverSourcePixels[i] = NULL;
    psbbnCoverSourceWidth[i] = 0;
    psbbnCoverSourceHeight[i] = 0;
    psbbnCoverLoaded[i] = 0;
    psbbnCoverFullResolution[i] = 0;
  }
  for (int buffer = 0; buffer < GRID_PAGE_BUFFERS; buffer++) {
    for (int i = 0; i < GRID_PAGE_SIZE; i++) {
      if (gridCoverTextures[buffer][i] != NULL) {
        free(gridCoverTextures[buffer][i]->Mem);
        free(gridCoverTextures[buffer][i]->Clut);
        free(gridCoverTextures[buffer][i]);
        gridCoverTextures[buffer][i] = NULL;
      }
      gridCoverLoaded[buffer][i] = 0;
    }
  }
  for (int buffer = 0; buffer < GRID_SELECTED_BUFFERS; buffer++) {
    if (gridSelectedTextures[buffer] != NULL) {
      free(gridSelectedTextures[buffer]->Mem);
      free(gridSelectedTextures[buffer]->Clut);
      free(gridSelectedTextures[buffer]);
      gridSelectedTextures[buffer] = NULL;
    }
    gridSelectedLoaded[buffer] = 0;
  }
}
