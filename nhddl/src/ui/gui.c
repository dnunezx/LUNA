// LUNA modifications: Danny Nunez (dnunezx) 2026
#include "common.h"
#include "dprintf.h"
#include "favorites.h"
#include "neutrino.h"
#include "options.h"
#include "ui/args.h"
#include "ui/ambient.h"
#include "ui/art_cache.h"
#include "ui/graphics.h"
#include "ui/handoff.h"
#include "ui/navigation.h"
#include "ui/pad.h"
#include "ui/ui.h"
#include "ui/view_internal.h"
#include "ui/view_state.h"
#include <dmaKit.h>
#include <errno.h>
#include <gsKit.h>
#include <gsToolkit.h>
#include <kernel.h>
#include <libpad.h>
#include <malloc.h>
#include <ps2sdkapi.h>
#include <stdint.h>
#include <stdio.h>
#include <timer.h>

#define DIV_ROUND(n, d) (n + (d - 1)) / d

#define PSBBN_TIMER_TICKS_PER_MS 576ULL
#define GRID_LEFT_SHOULDERS (PAD_L1 | PAD_L2)
#define GRID_RIGHT_SHOULDERS (PAD_R1 | PAD_R2)
#define OPTIONS_FADE_DURATION_MS 360

// Seven phase-offset points share a slowly breathing, tilting ellipse.
#define ORB_COUNT 7
#define ORB_ORBIT_PERIOD_MS 2200
#define ORB_SPREAD_PERIOD_MS 4300
#define ORB_TILT_PERIOD_MS 6100
#define ORB_DESYNC_PERIOD_MS 5200
#define ORB_CONTRACT_PERIOD_MS 24000
#define ORB_PULSE_PERIOD_MS 1800
#define ORB_SPREAD_PHASE 2600
#define ORB_DESYNC_PHASE 3800
#define ORB_TRAIL_STEP_MS 40
#define ORB_TRAIL_SEGMENTS 6

typedef struct {
  GSTEXTURE libraryFrame;
} OptionsBackdrop;

typedef enum {
  OPTIONS_MENU,
  OPTIONS_PER_GAME,
  OPTIONS_GLOBAL
} OptionsPage;

void closeUI();
int uiLoop(TargetList *titles);
int uiTitleOptionsLoop(Target *title, int *classicArtOverlap, int *orbsEnabled,
                       int *orbsBackground, int *ambientEnabled);
int uiArgumentListLoop(Target *target, ArgumentList *titleArguments, const OptionsBackdrop *backdrop);
void uiLaunchTitle(Target *target, ArgumentList *arguments, GSTEXTURE *cover);
void drawGameID(const char *game_id);
int createSplashThread();
void uiSplashThread();
void closeUISplashThread();

GSGLOBAL *gsGlobal;
char lineBuffer[255];
static int orbsBackground = 0;

const int keepoutArea = 20;
const int headerHeight = 40;
const int footerHeight = 60;

uint32_t uiNowMs(void) {
  return (uint32_t)((GetTimerSystemTime() >> 8) / PSBBN_TIMER_TICKS_PER_MS);
}

static const int discSin[32] = {0,   25,  49,  71,  90,  106, 117, 125, 127, 125, 117, 106, 90,  71,  49,  25,
                                0,  -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25};

int discWave(uint32_t phase) {
  int index = (phase >> 11) & 31;
  int next = (index + 1) & 31;
  int fraction = (phase >> 3) & 0xFF;
  return discSin[index] + ((discSin[next] - discSin[index]) * fraction) / 256;
}

int psbbnFieldStableY(int y) {
  return (gsGlobal->Interlace == GS_INTERLACED) ? (y & ~1) : y;
}

int psbbnFieldStableHeight(void) {
  return (gsGlobal->Interlace == GS_INTERLACED) ? 2 : 1;
}
#ifdef LUNA_GLASS_UI
static uint32_t glassStartMs = 0;

typedef struct {
  float x;
  float y;
  int depth;
} GlassPoint;

static const int glassSin[32] = {0,   25,  49,  71,  90,  106, 117, 125, 127, 125, 117, 106, 90,  71,  49,  25,
                                 0,  -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25};

#define GLASS_STAR_TILE_SIZE 16
#define GLASS_STAR_ATLAS_WIDTH 64
#define GLASS_STAR_ATLAS_HEIGHT 32

typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} GlassStarPixel;

static GSTEXTURE glassStarAtlas;
static GlassStarPixel glassStarAtlasPixels[GLASS_STAR_ATLAS_WIDTH * GLASS_STAR_ATLAS_HEIGHT] __attribute__((aligned(128)));

static uint64_t glassColor(int red, int green, int blue, int alpha, int brightness);
void drawOrbitalDisc(int centerX, int centerY, int radius, int z, uint64_t centerColor, uint64_t edgeColor);

static uint32_t glassElapsedMs(uint32_t frameNowMs) {
  return frameNowMs - glassStartMs;
}

static uint32_t glassPhase(uint32_t elapsedMs, uint32_t periodMs, uint32_t offsetMs) {
  return (uint32_t)((((uint64_t)(elapsedMs + offsetMs)) << 16) / periodMs);
}

static int glassWave(uint32_t phase) {
  int index = (phase >> 11) & 31;
  int next = (index + 1) & 31;
  int fraction = (phase >> 3) & 0xFF;
  return glassSin[index] + ((glassSin[next] - glassSin[index]) * fraction) / 256;
}

static int glassStarEdgeAlpha(int x, int width, int fadeWidth) {
  int distance;

  if (x < 0)
    distance = x + fadeWidth;
  else if (x >= width)
    distance = width + fadeWidth - x;
  else {
    distance = x;
    if ((width - 1 - x) < distance)
      distance = width - 1 - x;
  }

  if (distance <= 0)
    return 0;
  if (distance >= fadeWidth)
    return 255;
  return (distance * 255) / fadeWidth;
}

static int clampColor(int value) {
  if (value < 0)
    return 0;
  if (value > 255)
    return 255;
  return value;
}

static int glassIntegerSqrt(int value) {
  int root = 0;
  while ((root + 1) * (root + 1) <= value)
    root++;
  return root;
}

static void compositeGlassStarDisc(GlassStarPixel *pixel, int distance, int radius, int red, int green, int blue,
                                   int centerAlpha) {
  if (distance >= radius)
    return;

  const int sourceAlpha = centerAlpha * (radius - distance) / radius;
  const int oldAlpha = pixel->alpha;
  const int combinedAlpha = sourceAlpha + (oldAlpha * (0x80 - sourceAlpha) + 0x40) / 0x80;
  if (combinedAlpha <= 0)
    return;

  const int denominator = combinedAlpha * 0x80;
  pixel->red = (red * sourceAlpha * 0x80 + pixel->red * oldAlpha * (0x80 - sourceAlpha) + denominator / 2) /
               denominator;
  pixel->green =
      (green * sourceAlpha * 0x80 + pixel->green * oldAlpha * (0x80 - sourceAlpha) + denominator / 2) / denominator;
  pixel->blue =
      (blue * sourceAlpha * 0x80 + pixel->blue * oldAlpha * (0x80 - sourceAlpha) + denominator / 2) / denominator;
  pixel->alpha = combinedAlpha;
}

static void initGlassStarAtlas(void) {
  static const int starRed[3] = {0x82, 0xA4, 0xC6};
  static const int starGreen[3] = {0xAA, 0xC4, 0xDE};
  static const int starBlue[3] = {0xD0, 0xE2, 0xF4};
  static const int discBrightness[4] = {-28, -12, 12, 32};
  static const int discAlpha[4] = {0x80 / 7, 0x80 / 3, 0x80, 0x80 / 2};
  // Normalized radii preserve the two existing star profiles: 4/2/1/1 and 5/3/2/1.
  static const int discRadius[2][4] = {{128, 64, 32, 32}, {128, 77, 51, 26}};

  for (int i = 0; i < GLASS_STAR_ATLAS_WIDTH * GLASS_STAR_ATLAS_HEIGHT; i++) {
    glassStarAtlasPixels[i].red = 0;
    glassStarAtlasPixels[i].green = 0;
    glassStarAtlasPixels[i].blue = 0;
    glassStarAtlasPixels[i].alpha = 0;
  }

  for (int sizeIndex = 0; sizeIndex < 2; sizeIndex++) {
    for (int layer = 0; layer < 3; layer++) {
      const int tileX = layer * GLASS_STAR_TILE_SIZE;
      const int tileY = sizeIndex * GLASS_STAR_TILE_SIZE;
      for (int y = 0; y < GLASS_STAR_TILE_SIZE; y++) {
        for (int x = 0; x < GLASS_STAR_TILE_SIZE; x++) {
          GlassStarPixel *pixel = &glassStarAtlasPixels[(tileY + y) * GLASS_STAR_ATLAS_WIDTH + tileX + x];
          const int dx = (x * 2 + 1 - GLASS_STAR_TILE_SIZE) * 8;
          const int dy = (y * 2 + 1 - GLASS_STAR_TILE_SIZE) * 8;
          const int distance = glassIntegerSqrt(dx * dx + dy * dy);

          // Give transparent edge texels the outer glow color to avoid dark linear-filter fringes.
          pixel->red = clampColor(starRed[layer] + discBrightness[0]);
          pixel->green = clampColor(starGreen[layer] + discBrightness[0]);
          pixel->blue = clampColor(starBlue[layer] + discBrightness[0]);
          for (int disc = 0; disc < 4; disc++)
            compositeGlassStarDisc(pixel, distance, discRadius[sizeIndex][disc],
                                   clampColor(starRed[layer] + discBrightness[disc]),
                                   clampColor(starGreen[layer] + discBrightness[disc]),
                                   clampColor(starBlue[layer] + discBrightness[disc]), discAlpha[disc]);
        }
      }
    }
  }

  glassStarAtlas.Width = GLASS_STAR_ATLAS_WIDTH;
  glassStarAtlas.Height = GLASS_STAR_ATLAS_HEIGHT;
  glassStarAtlas.PSM = GS_PSM_CT32;
  glassStarAtlas.ClutPSM = 0;
  glassStarAtlas.TBW = 0;
  glassStarAtlas.Mem = (u32 *)glassStarAtlasPixels;
  glassStarAtlas.Clut = NULL;
  glassStarAtlas.Vram = 0;
  glassStarAtlas.VramClut = 0;
  glassStarAtlas.Filter = GS_FILTER_LINEAR;
  glassStarAtlas.ClutStorageMode = 0;
  glassStarAtlas.Delayed = GS_SETTING_ON;
  gsKit_TexManager_bind(gsGlobal, &glassStarAtlas);
}

static void drawGlassStarDisc(int x, int y, int size, int red, int green, int blue, int alpha, int brightness) {
  drawOrbitalDisc(x, y, size, 0, glassColor(red, green, blue, alpha, brightness),
                  glassColor(red, green, blue, 0, brightness));
}

static void drawGlassStarSprite(int x, int y, int size, int red, int green, int blue, int alpha) {
  if (alpha <= 0)
    return;
  drawGlassStarDisc(x, y, size + 3, red, green, blue, alpha / 7, -28);
  drawGlassStarDisc(x, y, size + 1, red, green, blue, alpha / 3, -12);
  drawGlassStarDisc(x, y, size, red, green, blue, alpha, 12);
  drawGlassStarDisc(x, y, 1, red, green, blue, alpha / 2, 32);
}

static void drawGlassBackgroundStarSprite(int x, int y, int size, int layer, int alpha) {
  if (alpha <= 0)
    return;
  if (alpha > 0x80)
    alpha = 0x80;

  const int radius = size + 3;
  const int tileX = layer * GLASS_STAR_TILE_SIZE;
  const int tileY = (size - 1) * GLASS_STAR_TILE_SIZE;
  gsKit_prim_sprite_texture(gsGlobal, &glassStarAtlas, x - radius, y - radius, tileX, tileY, x + radius, y + radius,
                            tileX + GLASS_STAR_TILE_SIZE - 1, tileY + GLASS_STAR_TILE_SIZE - 1, 0,
                            GS_SETREG_RGBA(0x80, 0x80, 0x80, alpha));
}

static uint64_t glassColor(int red, int green, int blue, int alpha, int brightness) {
  return GS_SETREG_RGBA(clampColor(red + brightness), clampColor(green + brightness), clampColor(blue + brightness), alpha);
}

void drawGlassDiamond(int centerX, int centerY, int radius, int z, uint64_t color) {
  gsKit_prim_line(gsGlobal, centerX, centerY - radius, centerX + radius, centerY, z, color);
  gsKit_prim_line(gsGlobal, centerX + radius, centerY, centerX, centerY + radius, z, color);
  gsKit_prim_line(gsGlobal, centerX, centerY + radius, centerX - radius, centerY, z, color);
  gsKit_prim_line(gsGlobal, centerX - radius, centerY, centerX, centerY - radius, z, color);
}

void drawGlassPanel(int x1, int y1, int x2, int y2, int z) {
  const uint64_t glass = GS_SETREG_RGBA(0x05, 0x0D, 0x22, 0x54);
  const uint64_t glassInner = GS_SETREG_RGBA(0x18, 0x46, 0x70, 0x14);
  const uint64_t shine = GS_SETREG_RGBA(0x88, 0xD8, 0xFF, 0x4A);
  const uint64_t edge = GS_SETREG_RGBA(0x24, 0x68, 0x98, 0x38);

  gsKit_prim_sprite(gsGlobal, x1, y1, x2, y2, z, glass);
  gsKit_prim_sprite(gsGlobal, x1 + 2, y1 + 2, x2 - 2, y1 + 5, z + 1, glassInner);
  gsKit_prim_sprite(gsGlobal, x1, y1, x2, y1 + 1, z + 2, shine);
  gsKit_prim_sprite(gsGlobal, x1, y1, x1 + 1, y2, z + 2, shine);
  gsKit_prim_sprite(gsGlobal, x1, y2 - 1, x2, y2, z + 1, edge);
  gsKit_prim_sprite(gsGlobal, x2 - 1, y1, x2, y2, z + 1, edge);
}

static void projectCrystalPoint(GlassPoint *point, int centerX, int centerY,
                                int size, uint32_t yawPhase, uint32_t pitchPhase, uint32_t rollPhase,
                                int sourceX, int sourceY, int sourceZ) {
  const int yawSine = glassWave(yawPhase);
  const int yawCosine = glassWave(yawPhase + (8 << 11));
  const int pitchSine = glassWave(pitchPhase);
  const int pitchCosine = glassWave(pitchPhase + (8 << 11));
  const int rollSine = glassWave(rollPhase);
  const int rollCosine = glassWave(rollPhase + (8 << 11));
  const int yawX = (sourceX * yawCosine - sourceZ * yawSine) / 127;
  const int yawZ = (sourceX * yawSine + sourceZ * yawCosine) / 127;
  const int pitchY = (sourceY * pitchCosine - yawZ * pitchSine) / 127;
  const int pitchZ = (sourceY * pitchSine + yawZ * pitchCosine) / 127;
  const int rollX = (yawX * rollCosine - pitchY * rollSine) / 127;
  const int rollY = (yawX * rollSine + pitchY * rollCosine) / 127;
  const int perspective = 640 - pitchZ;

  point->x = (float)centerX + ((float)rollX * (float)size * 640.0f) / (127.0f * (float)perspective);
  point->y = (float)centerY - ((float)rollY * (float)size * 640.0f) / (127.0f * (float)perspective);
  point->depth = pitchZ;
}

static void drawGlassCube(int centerX, int centerY, int size, uint32_t yawPhase, int red, int green, int blue,
                          int stableOutline) {
  // Clean-room crystal renderer based only on observation of the stock System
  // Configuration animation: a tumbling translucent shell around a dark core.
  static const int source[8][3] = {{-127, -127, -127}, {127, -127, -127}, {127, 127, -127}, {-127, 127, -127},
                                    {-127, -127, 127},  {127, -127, 127},  {127, 127, 127},  {-127, 127, 127}};
  static const int faceVertices[6][4] = {{3, 2, 0, 1}, {7, 6, 4, 5}, {3, 7, 0, 4},
                                         {2, 6, 1, 5}, {3, 2, 7, 6}, {0, 1, 4, 5}};
  static const int faceShade[6] = {-18, 8, -26, -4, 28, -34};
  static const int edgeVertices[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                          {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  const uint32_t pitchPhase = ((yawPhase * 5) / 7) + (5 << 11);
  const uint32_t rollPhase = ((yawPhase * 3) / 11) + (2 << 11);
  GlassPoint shell[8];
  GlassPoint core[8];
  int faceOrder[6] = {0, 1, 2, 3, 4, 5};
  int faceDepth[6];

  for (int i = 0; i < 8; i++) {
    projectCrystalPoint(&shell[i], centerX, centerY, size, yawPhase, pitchPhase, rollPhase,
                        source[i][0], source[i][1], source[i][2]);
    projectCrystalPoint(&core[i], centerX, centerY, (size * 68) / 100,
                        yawPhase, pitchPhase, rollPhase, source[i][0], source[i][1], source[i][2]);
  }

  for (int face = 0; face < 6; face++) {
    faceDepth[face] = 0;
    for (int vertex = 0; vertex < 4; vertex++)
      faceDepth[face] += shell[faceVertices[face][vertex]].depth;
  }

  // Draw rear faces first so the transparent front faces retain their depth.
  for (int i = 0; i < 5; i++) {
    for (int j = i + 1; j < 6; j++) {
      if (faceDepth[faceOrder[i]] > faceDepth[faceOrder[j]]) {
        int swap = faceOrder[i];
        faceOrder[i] = faceOrder[j];
        faceOrder[j] = swap;
      }
    }
  }

  // The low-alpha shell remains visible behind the central smoked volume.
  for (int order = 0; order < 6; order++) {
    int face = faceOrder[order];
    int a = faceVertices[face][0];
    int b = faceVertices[face][1];
    int c = faceVertices[face][2];
    int d = faceVertices[face][3];
    int shade = faceShade[face] + faceDepth[face] / 18;
    uint64_t light = glassColor(red, green, blue, 0x25, shade + 34);
    uint64_t mid = glassColor(red, green, blue, 0x1D, shade + 8);
    uint64_t dark = glassColor(red, green, blue, 0x16, shade - 24);
    gsKit_prim_quad_gouraud(gsGlobal, shell[a].x, shell[a].y, shell[b].x, shell[b].y, shell[c].x, shell[c].y, shell[d].x,
                            shell[d].y, 0, light, mid, dark, mid);
  }

  // A dark inner cube creates the stock crystal's dense central volume.
  for (int order = 0; order < 6; order++) {
    int face = faceOrder[order];
    int a = faceVertices[face][0];
    int b = faceVertices[face][1];
    int c = faceVertices[face][2];
    int d = faceVertices[face][3];
    int shade = faceShade[face] + faceDepth[face] / 22;
    uint64_t light = glassColor(red / 2, green / 2, blue / 2, 0x42, shade + 8);
    uint64_t mid = glassColor(red / 3, green / 3, blue / 3, 0x48, shade - 12);
    uint64_t dark = glassColor(red / 4, green / 4, blue / 4, 0x50, shade - 28);
    gsKit_prim_quad_gouraud(gsGlobal, core[a].x, core[a].y, core[b].x, core[b].y, core[c].x, core[c].y, core[d].x, core[d].y, 0,
                            light, mid, dark, mid);
  }

  // Keep the large loading cube's outline subdued so interlaced output does
  // not turn its moving one-pixel edges into a white shimmer.
  for (int i = 0; i < 12; i++) {
    int a = edgeVertices[i][0];
    int b = edgeVertices[i][1];
    int edgeDepth = (shell[a].depth + shell[b].depth) / 2;
    int edgeAlpha = stableOutline ? 0x30 + (edgeDepth + 180) / 14 : 0x38 + (edgeDepth + 180) / 8;
    int edgeBrightness = stableOutline ? 18 + (edgeDepth + 180) / 12 : 46 + (edgeDepth + 180) / 5;
    int maxAlpha = stableOutline ? 0x50 : 0x78;
    if (edgeAlpha > maxAlpha)
      edgeAlpha = maxAlpha;
    gsKit_prim_line(gsGlobal, shell[a].x, shell[a].y, shell[b].x, shell[b].y, 0,
                    glassColor(red, green, blue, edgeAlpha, edgeBrightness));
  }

  if (!stableOutline) {
    int nearest = 0;
    for (int i = 1; i < 8; i++) {
      if (shell[i].depth > shell[nearest].depth)
        nearest = i;
    }
    drawGlassStarSprite((int)(shell[nearest].x + 0.5f), (int)(shell[nearest].y + 0.5f), 1, clampColor(red + 90),
                        clampColor(green + 90), clampColor(blue + 90), 0x58);
  }
}

void drawOrbitalDisc(int centerX, int centerY, int radius, int z, uint64_t centerColor, uint64_t edgeColor) {
  for (int i = 0; i < 32; i++) {
    int next = (i + 1) & 31;
    int x1 = centerX + (glassSin[(i + 8) & 31] * radius) / 127;
    int y1 = centerY + (glassSin[i] * radius) / 127;
    int x2 = centerX + (glassSin[(next + 8) & 31] * radius) / 127;
    int y2 = centerY + (glassSin[next] * radius) / 127;
    gsKit_prim_triangle_gouraud(gsGlobal, centerX, centerY, x1, y1, x2, y2, z, centerColor, edgeColor, edgeColor);
  }
}

// Cubic interpolation keeps the orb path and its velocity smooth between the
// 32 entries in the shared sine table. gsKit accepts fractional coordinates.
static float orbWave(uint32_t phase) {
  const int index = (phase >> 11) & 31;
  const float p0 = glassSin[(index + 31) & 31];
  const float p1 = glassSin[index];
  const float p2 = glassSin[(index + 1) & 31];
  const float p3 = glassSin[(index + 2) & 31];
  const float t = (phase & 2047) / 2048.0f;
  return 0.5f * ((2.0f * p1) +
                 t * ((p2 - p0) +
                      t * ((2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) +
                           t * (-p0 + 3.0f * p1 - 3.0f * p2 + p3))));
}

static void drawOrbGlowDisc(float centerX, float centerY, float radius,
                            int z, uint64_t centerColor, uint64_t edgeColor) {
  const float scale = radius / 127.0f;
  for (int i = 0; i < 16; i++) {
    const int point = i * 2;
    const int next = (point + 2) & 31;
    const float x1 = centerX + glassSin[(point + 8) & 31] * scale;
    const float y1 = centerY + glassSin[point] * scale;
    const float x2 = centerX + glassSin[(next + 8) & 31] * scale;
    const float y2 = centerY + glassSin[next] * scale;
    gsKit_prim_triangle_gouraud(gsGlobal, centerX, centerY, x1, y1, x2, y2,
                                z, centerColor, edgeColor, edgeColor);
  }
}

typedef struct {
  uint32_t orbit;
  float breath;
  float desync;
  float tilt;
  float scale;
} OrbMotion;

typedef struct {
  uint32_t base;
  float spreadWeight;
  float desyncWeight;
} OrbPath;

static OrbMotion orbMotion(uint32_t elapsedMs) {
  OrbMotion motion;
  motion.orbit = glassPhase(elapsedMs, ORB_ORBIT_PERIOD_MS, 0);
  motion.breath = orbWave(glassPhase(elapsedMs, ORB_SPREAD_PERIOD_MS, 0));
  motion.desync = orbWave(glassPhase(elapsedMs, ORB_DESYNC_PERIOD_MS, 0));
  float tiltWave = orbWave(glassPhase(elapsedMs, ORB_TILT_PERIOD_MS, 0));
  if (tiltWave < 0)
    tiltWave = -tiltWave;
  motion.tilt = 26.0f + (tiltWave * 74.0f) / 127.0f;
  // Fast grouping rides on a slower swell, as in the reference animation.
  const float slowScale = 80.0f + orbWave(glassPhase(elapsedMs,
                                  ORB_CONTRACT_PERIOD_MS, 0) + (8 << 11)) * 20.0f / 127.0f;
  const float pulseScale = 92.0f + orbWave(glassPhase(elapsedMs,
                                   ORB_PULSE_PERIOD_MS, 0) + (8 << 11)) * 8.0f / 127.0f;
  motion.scale = slowScale * pulseScale / 100.0f;
  return motion;
}

static void orbPosition(const OrbPath *path, const OrbMotion *motion,
                        int centerX, int centerY, int radiusX, int radiusY,
                        float *x, float *y, float *depth) {
  const int spread = (int)(path->spreadWeight * motion->breath *
                           ORB_SPREAD_PHASE / (127.0f * 127.0f));
  const uint32_t phaseX = motion->orbit + path->base + spread;
  const int offsetY = (int)(path->desyncWeight * motion->desync *
                            ORB_DESYNC_PHASE / (127.0f * 127.0f));
  const uint32_t phaseY = phaseX + offsetY;
  if (depth != NULL)
    *depth = (orbWave(phaseX) + 127.0f) / 2.0f;
  *x = centerX + orbWave(phaseX + (8 << 11)) * radiusX * motion->scale / (127.0f * 100.0f);
  *y = centerY + orbWave(phaseY) * radiusY * motion->tilt * motion->scale / (127.0f * 100.0f * 100.0f);
}

static void drawOrbTrailSegment(float oldX, float oldY, float newX, float newY,
                                float oldWidth, float newWidth,
                                int z, uint64_t oldColor, uint64_t newColor) {
  const float dx = newX - oldX;
  const float dy = newY - oldY;
  const float absDx = dx < 0 ? -dx : dx;
  const float absDy = dy < 0 ? -dy : dy;
  const float longer = absDx > absDy ? absDx : absDy;
  const float shorter = absDx > absDy ? absDy : absDx;
  const float length = longer + shorter * 0.375f;
  if (length < 0.01f)
    return;
  const float oldOffsetX = -dy * oldWidth / (2.0f * length);
  const float oldOffsetY = dx * oldWidth / (2.0f * length);
  const float newOffsetX = -dy * newWidth / (2.0f * length);
  const float newOffsetY = dx * newWidth / (2.0f * length);
  gsKit_prim_quad_gouraud(gsGlobal,
                          oldX + oldOffsetX, oldY + oldOffsetY,
                          newX + newOffsetX, newY + newOffsetY,
                          oldX - oldOffsetX, oldY - oldOffsetY,
                          newX - newOffsetX, newY - newOffsetY, z,
                          oldColor, newColor, oldColor, newColor);
}

static void drawSevenOrbs(int centerX, int centerY, int radiusX, int radiusY,
                          uint32_t elapsedMs, int glowScale, int trailZ) {
  OrbMotion motion[ORB_TRAIL_SEGMENTS + 1];
  for (int sample = 0; sample <= ORB_TRAIL_SEGMENTS; sample++) {
    const uint32_t age = sample * ORB_TRAIL_STEP_MS;
    motion[sample] = orbMotion(elapsedMs > age ? elapsedMs - age : 0);
  }

  // (Cs - 0) * As + Cd: overlapping halos merge into a brighter light.
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 2, 0, 1, 0), 0);
  for (int i = 0; i < ORB_COUNT; i++) {
    OrbPath path;
    path.base = (uint32_t)(((uint64_t)i << 16) / ORB_COUNT);
    path.spreadWeight = orbWave(path.base + (2 << 11));
    path.desyncWeight = orbWave(path.base);
    float x, y, depth;
    orbPosition(&path, &motion[0], centerX, centerY, radiusX, radiusY,
                &x, &y, &depth);
    float newX = x;
    float newY = y;
    for (int segment = 1; segment <= ORB_TRAIL_SEGMENTS; segment++) {
      float oldX, oldY;
      orbPosition(&path, &motion[segment], centerX, centerY, radiusX, radiusY,
                  &oldX, &oldY, NULL);
      const int oldOuterAlpha = 4 + (ORB_TRAIL_SEGMENTS - segment) * 28 / ORB_TRAIL_SEGMENTS;
      const int newOuterAlpha = 4 + (ORB_TRAIL_SEGMENTS - segment + 1) * 28 / ORB_TRAIL_SEGMENTS;
      const int oldInnerAlpha = 5 + (ORB_TRAIL_SEGMENTS - segment) * 45 / ORB_TRAIL_SEGMENTS;
      const int newInnerAlpha = 5 + (ORB_TRAIL_SEGMENTS - segment + 1) * 45 / ORB_TRAIL_SEGMENTS;
      const int oldOuterWidth = 3 + (ORB_TRAIL_SEGMENTS - segment) * 6 / ORB_TRAIL_SEGMENTS;
      const int newOuterWidth = 3 + (ORB_TRAIL_SEGMENTS - segment + 1) * 6 / ORB_TRAIL_SEGMENTS;
      const int oldInnerWidth = 2 + (ORB_TRAIL_SEGMENTS - segment) * 3 / ORB_TRAIL_SEGMENTS;
      const int newInnerWidth = 2 + (ORB_TRAIL_SEGMENTS - segment + 1) * 3 / ORB_TRAIL_SEGMENTS;
      drawOrbTrailSegment(oldX, oldY, newX, newY, oldOuterWidth, newOuterWidth, trailZ,
                          GS_SETREG_RGBA(0x40, 0x78, 0xC8, oldOuterAlpha),
                          GS_SETREG_RGBA(0x40, 0x78, 0xC8, newOuterAlpha));
      drawOrbTrailSegment(oldX, oldY, newX, newY, oldInnerWidth, newInnerWidth, trailZ,
                          GS_SETREG_RGBA(0xA0, 0xD8, 0xFF, oldInnerAlpha),
                          GS_SETREG_RGBA(0xA0, 0xD8, 0xFF, newInnerAlpha));
      newX = oldX;
      newY = oldY;
    }
        const float haloRadius = (18.0f + depth / 14.0f) * glowScale / 100.0f;
        const float coreRadius = (5.6f + depth / 52.0f) * glowScale / 100.0f;
    const int haloAlpha = 0x13 + (int)(depth / 13.0f);
    const int coreAlpha = 0x50 + (int)(depth / 3.0f);

    drawOrbGlowDisc(x, y, haloRadius, trailZ + 1,
                    GS_SETREG_RGBA(0x70, 0xA8, 0xE8, haloAlpha),
                    GS_SETREG_RGBA(0x70, 0xA8, 0xE8, 0));
    drawOrbGlowDisc(x, y, coreRadius, trailZ + 1,
                    GS_SETREG_RGBA(0xE0, 0xF0, 0xFF, coreAlpha),
                    GS_SETREG_RGBA(0xE0, 0xF0, 0xFF, 0));
  }
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
}

static void drawOrbitalStars(int width, int height, uint32_t elapsedMs) {
  static const int speedPixelsPerSecond[3] = {4, 8, 13};
  const int edgeFadeWidth = 18;

  gsKit_TexManager_bind(gsGlobal, &glassStarAtlas);
  for (int i = 0; i < 58; i++) {
    int layer = i % 3;
    int baseX = (i * 97 + i * i * 13 + 31) % width;
    int baseY = (i * 53 + i * i * 7 + 19) % height;
    int scroll = (int)(((uint64_t)elapsedMs * speedPixelsPerSecond[layer]) / 1000ULL) % width;
    int y = baseY + glassWave(glassPhase(elapsedMs, 7000 + layer * 1300, i * 113)) * (layer + 1) / 160;
    int x = (baseX + scroll) % width;
    int twinkle = glassWave(glassPhase(elapsedMs, 2600 + layer * 900, i * 173)) / 14;
    int size = ((layer == 2) && ((i % 5) == 0)) ? 2 : 1;
    int alpha = 0x20 + layer * 0x12;
    int edgeAlpha = (alpha * glassStarEdgeAlpha(x, width, edgeFadeWidth)) / 255;
    drawGlassBackgroundStarSprite(x, y, size, layer, edgeAlpha + twinkle);

    // Cross-fade a duplicate through the opposite edge during wraparound.
    if (x < edgeFadeWidth)
      drawGlassBackgroundStarSprite(x + width, y, size, layer,
                                    (alpha * glassStarEdgeAlpha(x + width, width, edgeFadeWidth)) / 255 + twinkle);
    else if (x >= width - edgeFadeWidth)
      drawGlassBackgroundStarSprite(x - width, y, size, layer,
                                    (alpha * glassStarEdgeAlpha(x - width, width, edgeFadeWidth)) / 255 + twinkle);
  }
}

void drawColorStarBackground(uint32_t frameNowMs) {
  const int width = gsGlobal->Width;
  const int height = gsGlobal->Height;
  const uint32_t elapsedMs = glassElapsedMs(frameNowMs);
  const uint64_t black = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80);

  gsKit_prim_quad_gouraud(gsGlobal, 0, 0, width, 0, 0, height, width, height, 0, black, black, black, black);

  drawOrbitalStars(width, height, elapsedMs);
}

static void drawGlassBackground(uint32_t frameNowMs) {
  const int width = gsGlobal->Width;
  const int height = gsGlobal->Height;
  const uint32_t elapsedMs = glassElapsedMs(frameNowMs);
  const int orbitX = width * 65 / 100;
  const int orbitY = height * 52 / 100;

  drawColorStarBackground(frameNowMs);

  // Two floating glass cubes retain the PS2 BIOS geometry without crowding the library.
  // Share one orbital phase and keep the crystals half a revolution apart.
  // The nested ellipses retain at least 96 pixels of center separation, so
  // their paths cannot collide even at their closest vertical alignment.
  uint32_t orbitPhase = glassPhase(elapsedMs, 36000, 0);
  uint32_t oppositePhase = orbitPhase + (16 << 11);
  int nearX = orbitX + (glassWave(orbitPhase + (8 << 11)) * 146) / 127;
  int nearY = orbitY + (glassWave(orbitPhase) * 56) / 127;
  int farX = orbitX + (glassWave(oppositePhase + (8 << 11)) * 96) / 127;
  int farY = orbitY + (glassWave(oppositePhase) * 40) / 127;
  drawGlassCube(nearX, nearY, 9, glassPhase(elapsedMs, 18000, 3000), 0x38, 0x98, 0xD8, 1);
  drawGlassCube(farX, farY, 7, glassPhase(elapsedMs, 26000, 12000), 0x78, 0x68, 0xC8, 1);
}

#endif

void drawSharedLibraryBackground(uint32_t frameNowMs) {
#ifdef LUNA_GLASS_UI
  if (orbsBackground) {
    const int width = gsGlobal->Width;
    const int height = gsGlobal->Height;
    const uint64_t black = GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80);
    gsKit_prim_quad_gouraud(gsGlobal, 0, 0, width, 0, 0, height,
                            width, height, 0, black, black, black, black);
    drawSevenOrbs(width * 65 / 100, height * 52 / 100,
                  width * 20 / 100, height * 30 / 100,
                  glassElapsedMs(frameNowMs), 100, 0);
  } else {
    drawGlassBackground(frameNowMs);
  }
#else
  (void)frameNowMs;
#endif
}

static int orbsVisualCacheIndex(int flowOffset) {
  int closest = ORBS_LOGO_CACHE_FOCUS;
  int closestDistance = 0x7FFFFFFF;
  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    int position = (i - ORBS_LOGO_CACHE_FOCUS) * 1000 + flowOffset;
    int distance = position < 0 ? -position : position;
    if (distance < closestDistance) {
      closest = i;
      closestDistance = distance;
    }
  }
  return closest;
}

static void drawOrbsLogo(GSTEXTURE *texture, float x, float y,
                         float width, float height, int brightness) {
  int previousAlphaTest = gsGlobal->Test->ATST;
  int previousAlphaReference = gsGlobal->Test->AREF;
  int previousAlphaFail = gsGlobal->Test->AFAIL;
  gsKit_TexManager_bind(gsGlobal, texture);
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsGlobal->Test->ATST = 2;
  gsGlobal->Test->AREF = 0x80;
  gsGlobal->Test->AFAIL = 0;
  gsKit_set_primalpha(gsGlobal, GS_BLEND_BACK2FRONT, 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
  gsKit_prim_sprite_texture(gsGlobal, texture, x, y, 0.0f, 0.0f,
                            x + width, y + height,
                            texture->Width - 1, texture->Height - 1, 6,
                            GS_SETREG_RGBA(brightness, brightness, brightness, 0x80));
  gsGlobal->Test->ATST = previousAlphaTest;
  gsGlobal->Test->AREF = previousAlphaReference;
  gsGlobal->Test->AFAIL = previousAlphaFail;
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
}

static void drawOrbsView(TargetList *titles, int selectedTitleIdx,
                         int flowOffset, int visualFocus, uint32_t now) {
  const int width = gsGlobal->Width;
  const int height = gsGlobal->Height;
  const int listTop = headerHeight + 12;
  const int listBottom = height - footerHeight - 12;
  const int centerY = (listTop + listBottom) / 2;
  const int rowPitch = (listBottom - listTop) / 4;
  const int logoCenterX = width - 153;
  const int visualTitleIdx = lunaNavWrap(titles->total,
      selectedTitleIdx + visualFocus - ORBS_LOGO_CACHE_FOCUS);
  char title[255];

  if (orbsBackgroundLoaded) {
    GSTEXTURE *background = orbsBackgroundTexture;
    gsKit_TexManager_bind(gsGlobal, background);
    gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
    gsKit_prim_sprite_texture(gsGlobal, background, 0.0f, 0.0f,
                              0.0f, 0.0f, (float)width, (float)height,
                              background->Width - 1, background->Height - 1, 0,
                              GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80));
    gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  }

  // Keep the orbit visible over the art and make a quiet area for the logos.
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_prim_sprite(gsGlobal, 0, 0, width, height, 1,
                    GS_SETREG_RGBA(0x00, 0x02, 0x0C, 0x20));
  gsKit_prim_sprite(gsGlobal, 0, 0, width / 2, height, 2,
                    GS_SETREG_RGBA(0x02, 0x06, 0x12, 0x26));
  gsKit_prim_quad_gouraud(gsGlobal, width - 350, 0, width, 0,
                          width - 350, height, width, height, 2,
                          GS_SETREG_RGBA(0x02, 0x06, 0x12, 0x08),
                          GS_SETREG_RGBA(0x02, 0x06, 0x12, 0x72),
                          GS_SETREG_RGBA(0x02, 0x06, 0x12, 0x08),
                          GS_SETREG_RGBA(0x02, 0x06, 0x12, 0x72));
  gsKit_prim_sprite(gsGlobal, 0, 0, width, headerHeight + 3, 3,
                    GS_SETREG_RGBA(0x02, 0x05, 0x10, 0x4A));
  gsKit_prim_sprite(gsGlobal, 0, height - footerHeight, width, height, 3,
                    GS_SETREG_RGBA(0x02, 0x05, 0x10, 0x62));

  drawTextWindow(keepoutArea + 10, headerHeight - getFontLineHeight(),
                 width - keepoutArea, 0, 7, FontMainColor, ALIGN_LEFT, "ORBS");
  snprintf(lineBuffer, sizeof(lineBuffer), "%d/%d", visualTitleIdx + 1,
           titles->total);
  drawTextWindow(width - 116, headerHeight - getFontLineHeight(),
                 width - keepoutArea - 8, 0, 7, FontMainColor,
                 ALIGN_RIGHT, lineBuffer);

  gsKit_prim_sprite(gsGlobal, width - 283, centerY - 37,
                    width - 35, centerY + 37, 4,
                    GS_SETREG_RGBA(0x18, 0x34, 0x58, 0x56));
  drawOrbitalDisc(width - 20, centerY, 10, 6,
                  GS_SETREG_RGBA(0xD0, 0xEB, 0xFF, 0x80),
                  GS_SETREG_RGBA(0x30, 0x8C, 0xD8, 0));
#ifdef LUNA_GLASS_UI
  drawSevenOrbs(width * 27 / 100, centerY, width * 20 / 100,
                (listBottom - listTop) * 36 / 100,
                glassElapsedMs(now), 100, 5);
#endif

  for (int i = 0; i < ORBS_LOGO_CACHE_COUNT; i++) {
    int position = (i - ORBS_LOGO_CACHE_FOCUS) * 1000 + flowOffset;
    int distance = position < 0 ? -position : position;
    int targetIdx = lunaNavWrap(titles->total,
        selectedTitleIdx + i - ORBS_LOGO_CACHE_FOCUS);
    int duplicate = 0;
    int proximity;
    float logoWidth;
    float logoHeight;
    float rowY;
    if (distance > 1350)
      continue;
    for (int j = 0; j < ORBS_LOGO_CACHE_COUNT; j++) {
      int otherPosition = (j - ORBS_LOGO_CACHE_FOCUS) * 1000 + flowOffset;
      int otherDistance = otherPosition < 0 ? -otherPosition : otherPosition;
      int otherTarget = lunaNavWrap(titles->total,
          selectedTitleIdx + j - ORBS_LOGO_CACHE_FOCUS);
      if (j != i && otherTarget == targetIdx &&
          (otherDistance < distance || (otherDistance == distance && j < i))) {
        duplicate = 1;
        break;
      }
    }
    if (duplicate)
      continue;
    proximity = distance < 1000 ? 1000 - distance : 0;
    logoWidth = 188.0f + 44.0f * lunaNavEase(proximity) / 1000.0f;
    logoHeight = 62.0f + 14.0f * lunaNavEase(proximity) / 1000.0f;
    rowY = centerY + position * rowPitch / 1000.0f;
    if (orbsLogoLoaded[i])
      drawOrbsLogo(orbsLogoTextures[i], logoCenterX - logoWidth / 2.0f,
                   rowY - logoHeight / 2.0f, logoWidth, logoHeight,
                   i == visualFocus ? 0x80 : 0x58);
    else {
      formatPSBBNTitle(getTargetByIdx(titles, targetIdx)->name, title, 190);
      drawTextWindow(width - 270, (int)rowY - getFontLineHeight() / 2,
                     width - 35, (int)rowY + getFontLineHeight() / 2, 6,
                     i == visualFocus ? FontMainColor : HeaderTextColor,
                     ALIGN_CENTER, title);
    }
  }

  const int footerY = height - footerHeight + 8;
  const int circleX = 26;
  const int crossX = width * 39 / 100;
  const int triangleX = width * 69 / 100;
  drawIconWindow(circleX, footerY, 0, height, 8, FontMainColor,
                 ALIGN_CENTER, ICON_CIRCLE);
  drawTextWindow(circleX + getIconWidth(ICON_CIRCLE) + 6, footerY,
                 crossX - 8, height, 8, FontMainColor, ALIGN_VCENTER, "Views");
  drawIconWindow(crossX, footerY, 0, height, 8, FontMainColor,
                 ALIGN_CENTER, ICON_CROSS);
  drawTextWindow(crossX + getIconWidth(ICON_CROSS) + 6, footerY,
                 triangleX - 8, height, 8, FontMainColor, ALIGN_VCENTER, "Launch");
  drawIconWindow(triangleX, footerY, 0, height, 8, FontMainColor,
                 ALIGN_CENTER, ICON_TRIANGLE);
  drawTextWindow(triangleX + getIconWidth(ICON_TRIANGLE) + 6, footerY,
                 width - keepoutArea, height, 8, FontMainColor,
                 ALIGN_VCENTER, "Options");
}

void initVMode(GSGLOBAL *gsGlobal) {
  switch (LAUNCHER_OPTIONS.vmode) {
  case GS_MODE_NTSC:
    DPRINTF("Forcing NTSC mode\n");
    gsGlobal->Mode = GS_MODE_NTSC;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    break;
  case GS_MODE_PAL:
    DPRINTF("Forcing PAL mode\n");
    gsGlobal->Mode = GS_MODE_PAL;
    gsGlobal->Interlace = GS_INTERLACED;
    gsGlobal->Field = GS_FIELD;
    gsGlobal->Width = 640;
    gsGlobal->Height = 512;
    break;
  case GS_MODE_DTV_480P:
    DPRINTF("Forcing 480p mode\n");
    gsGlobal->Mode = GS_MODE_DTV_480P;
    gsGlobal->Interlace = GS_NONINTERLACED;
    gsGlobal->Field = GS_FRAME;
    gsGlobal->Width = 640;
    gsGlobal->Height = 448;
    break;
  default:
  }
}

int uiInit() {
  if (gsGlobal != NULL) {
    DPRINTF("Reinitializing UI\n");
    closeUI();
  }
  StartTimerSystemTime();
#ifdef LUNA_GLASS_UI
  glassStartMs = uiNowMs();
#endif
  gsGlobal = gsKit_init_global();
  initVMode(gsGlobal);
  gsGlobal->PSM = GS_PSM_CT24; // Set color depth to avoid PAL VRAM issues
  gsGlobal->PSMZ = GS_PSMZ_16S;
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsGlobal->DoubleBuffering = GS_SETTING_ON;
  // Setup TEST register to ignore fully transparent pixels
  gsGlobal->Test->ATST = 7;    // Set alpha test method to NOTEQUAL (pixels with A not equal to AREF pass)
  gsGlobal->Test->AREF = 0x00; // Set reference value to 0x00 (transparent)
  gsGlobal->Test->AFAIL = 0;   // Don't update buffers when test fails

  dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC, D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

  // Initialize the DMAC
  int res;
  if ((res = dmaKit_chan_init(DMA_CHANNEL_GIF))) {
    DPRINTF("ERROR: Failed to initlize DMAC: %d\n", res);
    return res;
  }

  // Init screen
  gsKit_vram_clear(gsGlobal);
  gsKit_init_screen(gsGlobal);
  gsKit_display_buffer(gsGlobal); // Switch display buffer to avoid garbage appearing on screen
  gsKit_TexManager_init(gsGlobal);
#ifdef LUNA_GLASS_UI
  initGlassStarAtlas();
#endif
  // Set alpha and mode, clear active buffer
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);
  gsKit_clear(gsGlobal, BGColor);

  // Initialize resources
  if (initGraphics()) {
    DPRINTF("ERROR: Failed to initialize font\n");
    return -1;
  };

  // Init cover geometry and artwork caches.
  calculateCoverArtGeometry();
  if (artCacheInit())
    return -1;

  return 0;
}

// Invalidates currently loaded texture and loads a new one
// Frees textures and deinits gsKit
void closeUI() {
  gsKit_vram_clear(gsGlobal);
  closeFont();
  artCacheShutdown();
  gsKit_deinit_global(gsGlobal);
}

// Main UI loop. Displays the target list.
int uiLoop(TargetList *titles) {
  // Reinitialize UI if video mode doesn't match
  if ((LAUNCHER_OPTIONS.vmode != VMODE_NONE) && (gsGlobal->Mode != LAUNCHER_OPTIONS.vmode)) {
    uiInit();
  }

  int res = 0;
  uint8_t *favoriteFlags = NULL;
  TargetList *favoriteTitles = NULL;
  if ((gsGlobal == NULL) && (res = uiInit())) {
    DPRINTF("ERROR: Failed to init UI: %d\n", res);
    goto exit;
  }

  // The splash logo is never drawn in either library view. Releasing its large
  // texture leaves stable VRAM for Classic's cover and disc pair.
  releaseBootLogo();
  // Init gamepad inputs
  initPad();

  int isCoverUninitialized = 1;
  int isDiscUninitialized = 1;
  int selectedTitleIdx = 0;
  int maxTitlesPerPage = (gsGlobal->Height - (headerHeight + footerHeight)) / getFontLineHeight();
  int psbbnCoverBaseIdx = -1;
  int psbbnAnimationTargetIdx = -1;
  int psbbnAnimationStartOffset = 0;
  uint32_t psbbnAnimationStart = 0;
  uint32_t psbbnAnimationDuration = PSBBN_ANIMATION_DURATION_MS;
  LunaCollectionScan collectionScan = {0};
  int collectionVisualTitleIdx = -1;
  int collectionVisualCoverIdx = PSBBN_COVER_CACHE_FOCUS;
  int collectionActionCoverIdx = PSBBN_COVER_CACHE_FOCUS;
  int gridActivePageBuffer = 0;
  int gridIncomingPageBuffer = -1;
  int gridPreviousPageBuffer = -1;
  int gridActivePageBase = -1;
  int gridIncomingPageBase = -1;
  int gridPageBases[GRID_PAGE_BUFFERS] = {-1, -1, -1};
  int gridPageComplete[GRID_PAGE_BUFFERS] = {0, 0, 0};
  int gridPageNextSlot[GRID_PAGE_BUFFERS] = {0, 0, 0};
  int gridSelectedActiveBuffer = 0;
  int gridSelectedIncomingBuffer = 1;
  int gridSelectedActiveIdx = -1;
  int gridSelectedRequestedIdx = -1;
  int gridSelectedAttemptedIdx = -1;
  int gridPendingSelectedIdx = -1;
  int gridPendingSelectedCoverIdx = -1;
  int gridPageDirection = 0;
  int gridPrefetchDirection = 1;
  int gridCascadeActive = 0;
  int gridCascadeDirection = 0;
  uint32_t gridCascadeStart = 0;
  int gridShoulderDirection = 0;
  uint32_t gridShoulderHoldStart = 0;
  int gridFastTrackActive = 0;
  int gridFastTrackSettling = 0;
  int gridFastTrackDirection = 0;
  int gridFastTrackSelectedIdx = -1;
  int gridFastTrackPreviousPageBase = -1;
  int gridFastTrackPageBase = -1;
  uint32_t gridFastTrackSlideStart = 0;
  uint32_t gridFastTrackNextStep = 0;
  int orbitRandomActive = 0;
  int orbitRandomTargetIdx = -1;
  int orbitRandomDirection = 1;
  uint32_t orbitRandomNextStep = 0;
  int orbitRandomButtonHeld = 0;
  int orbsVisualTitleIdx = -1;
  int favoriteButtonHeld = 0;
  int favoritesTabButtonHeld = 0;
  int favoritesOnly = 0;
  int collectionFavoritesOnly = 0;
  int classicArtRequestedIdx = -1;
  int classicNavHeld = 0;
  int classicDisplayedCoverAvailable = 0;
  int classicDisplayedDiscAvailable = 0;
  int classicPreviousCoverAvailable = 0;
  int classicArtOverlap = 0;
  int orbsEnabled = 0;
  int ambientEnabled = 1;
  uint32_t classicArtDueMs = 0;
  uint32_t classicCoverFadeStartMs = 0;
  LunaNavRepeatState classicRepeat = {0};
  UILibraryView view = UI_VIEW_CLASSIC;
  Target *curTarget = titles->first;

  // Get last launched title and find it in the target list
  char *lastTitle = calloc(sizeof(char), PATH_MAX + 1);
  if (!getLastLaunchedTitle(lastTitle, PATH_MAX + 1)) {
    int mountpointLen;
    while (curTarget != NULL) {
      // Compare paths without the mountpoint
      mountpointLen = getRelativePathIdx(curTarget->fullPath);
      if (mountpointLen == -1)
        mountpointLen = 0;

      if (!strcmp(lastTitle, &curTarget->fullPath[mountpointLen])) {
        selectedTitleIdx = curTarget->idx;
        break;
      }
      curTarget = curTarget->next;
    }
    // Reinitialize target if last launched title couldn't be loaded
    if (curTarget == NULL) {
      curTarget = titles->first;
    }
  }
  free(lastTitle);
  view = loadLastLibraryView(curTarget);
  orbsEnabled = loadOrbsViewEnabled(curTarget);
  orbsBackground = loadOrbsBackground(curTarget);
  ambientEnabled = loadAmbientSoundEnabled(curTarget);
  ambientSetEnabled(ambientEnabled);
  if (view == UI_VIEW_ORBS && !orbsEnabled)
    view = UI_VIEW_CLASSIC;
  classicArtOverlap = loadClassicArtOverlap(curTarget);
  setClassicArtOverlap(classicArtOverlap);

  favoriteFlags = calloc((size_t)titles->total, sizeof(*favoriteFlags));
  if (favoriteFlags == NULL) {
    res = -ENOMEM;
    goto exit;
  }
  loadFavoriteFlags(titles, favoriteFlags, (size_t)titles->total);
  favoriteTitles = buildFavoriteTargetList(titles, favoriteFlags, (size_t)titles->total);
  if (favoriteTitles == NULL) {
    res = -ENOMEM;
    goto exit;
  }

  // Classic textures are unnecessary when restoring another view.
  if (view == UI_VIEW_CLASSIC) {
    isCoverUninitialized = loadCoverArt(curTarget->device, curTarget->id);
    isDiscUninitialized = loadDiscArt(curTarget->device, curTarget->id);
    classicDisplayedCoverAvailable = !isCoverUninitialized;
    classicDisplayedDiscAvailable = !isDiscUninitialized;
  }

  // Main UI loop
  int frameCount = 0;
  int prevInput = 0;
  int input = 0;
  int optionsTriangleHeld = 0;
  while (1) {
    gsKit_clear(gsGlobal, BGColor);
    gsKit_TexManager_nextFrame(gsGlobal);

    // A random destination may be far from the current title. Move toward it
    // one adjacent cache position at a time so refreshPSBBNCovers() recycles
    // nine entries and loads only one new PNG per step instead of ten at once.
    if (view == UI_VIEW_ORBIT && orbitRandomActive && uiNowMs() >= orbitRandomNextStep) {
      selectedTitleIdx = lunaNavWrap(titles->total, selectedTitleIdx + orbitRandomDirection);
      if (selectedTitleIdx == orbitRandomTargetIdx) {
        orbitRandomActive = 0;
        orbitRandomTargetIdx = -1;
      } else {
        orbitRandomNextStep = uiNowMs() + ORBIT_RANDOM_STEP_MS;
      }
    }

    // Reload target if index has changed
    if (curTarget->idx != selectedTitleIdx) {
      curTarget = getTargetByIdx(titles, selectedTitleIdx);
      if (view == UI_VIEW_CLASSIC) {
        // Keep input polling light while moving through the list. The old art
        // remains visible, but is not eligible for a launch handoff.
        isCoverUninitialized = 1;
        isDiscUninitialized = 1;
        classicArtRequestedIdx = selectedTitleIdx;
        classicArtDueMs = uiNowMs() + CLASSIC_ART_SETTLE_MS;
      }
    }

    if (view == UI_VIEW_CLASSIC && classicArtRequestedIdx == selectedTitleIdx &&
        !classicNavHeld && (int32_t)(uiNowMs() - classicArtDueMs) >= 0) {
      classicPreviousCoverAvailable = classicDisplayedCoverAvailable;
      isCoverUninitialized = loadNextClassicCoverArt(curTarget->device, curTarget->id);
      isDiscUninitialized = loadDiscArt(curTarget->device, curTarget->id);
      classicDisplayedCoverAvailable = !isCoverUninitialized;
      classicDisplayedDiscAvailable = !isDiscUninitialized;
      classicCoverFadeStartMs = uiNowMs();
      classicArtRequestedIdx = -1;
    }

    if (view == UI_VIEW_PSBBN || view == UI_VIEW_ORBIT || view == UI_VIEW_ORBS) {
      TargetList *flowTitles = (view == UI_VIEW_PSBBN && collectionFavoritesOnly) ? favoriteTitles : titles;
      int flowSelectedTitleIdx = (view == UI_VIEW_PSBBN && collectionFavoritesOnly)
                                     ? lunaNavMarkedRank(favoriteFlags, titles->total, selectedTitleIdx)
                                     : selectedTitleIdx;
      uint32_t now = uiNowMs();
      int flowOffset;

      if (view == UI_VIEW_PSBBN && flowTitles->total <= 0) {
        if (psbbnCoverBaseIdx >= 0)
          releasePSBBNCovers();
        psbbnCoverBaseIdx = -1;
        psbbnAnimationTargetIdx = -1;
        psbbnAnimationStartOffset = 0;
        drawPSBBNCollection(flowTitles, 0, psbbnCoverTextures, 0, collectionFavoritesOnly, now);
        goto library_view_drawn;
      }

      // Finish synchronous artwork loading before sampling the glide clock.
      if (view != UI_VIEW_ORBS) {
        if (psbbnCoverBaseIdx != flowSelectedTitleIdx)
          refreshPSBBNCovers(flowTitles, flowSelectedTitleIdx, psbbnCoverBaseIdx);
        psbbnCoverBaseIdx = flowSelectedTitleIdx;
      } else {
        refreshOrbsLogos(flowTitles, flowSelectedTitleIdx);
      }
      now = uiNowMs();

      if (psbbnAnimationTargetIdx < 0) {
        psbbnAnimationTargetIdx = flowSelectedTitleIdx;
        psbbnAnimationStartOffset = 0;
        psbbnAnimationStart = now;
        psbbnAnimationDuration = PSBBN_ANIMATION_DURATION_MS;
      } else if (psbbnAnimationTargetIdx != flowSelectedTitleIdx) {
        int currentOffset = lunaNavAnimatedOffset(psbbnAnimationStartOffset, psbbnAnimationStart, psbbnAnimationDuration, now);
        int direction = lunaNavDirection(flowTitles->total, psbbnAnimationTargetIdx, flowSelectedTitleIdx);
        psbbnAnimationStartOffset = currentOffset + direction * 1000;
        if (view == UI_VIEW_PSBBN && collectionScan.active) {
          psbbnAnimationStartOffset = collectionScan.heldDirection * 1000;
          psbbnAnimationDuration = COLLECTION_SCAN_STEP_MS;
        } else if (currentOffset * direction < 0) {
          int reversalDistance = (psbbnAnimationStartOffset < 0) ? -psbbnAnimationStartOffset : psbbnAnimationStartOffset;
          if (reversalDistance > 1000)
            reversalDistance = 1000;
          // Opposite input clears the remaining travel promptly instead of
          // letting two depth directions linger for a full new glide.
          psbbnAnimationDuration = 180 + (reversalDistance * 240) / 1000;
        } else {
          psbbnAnimationDuration = PSBBN_ANIMATION_DURATION_MS;
        }
        psbbnAnimationTargetIdx = flowSelectedTitleIdx;
        psbbnAnimationStart = now;
      }

      flowOffset = lunaNavAnimatedOffset(psbbnAnimationStartOffset, psbbnAnimationStart, psbbnAnimationDuration, now);
      if (view != UI_VIEW_ORBS)
        updatePSBBNCoverResidency(flowOffset);
      now = uiNowMs();
      flowOffset = lunaNavAnimatedOffset(psbbnAnimationStartOffset, psbbnAnimationStart, psbbnAnimationDuration, now);
      if (view == UI_VIEW_PSBBN) {
        collectionVisualCoverIdx = PSBBN_COVER_CACHE_FOCUS;
        if (flowOffset >= 500)
          collectionVisualCoverIdx--;
        else if (flowOffset < -500)
          collectionVisualCoverIdx++;
        int visualRank = lunaNavWrap(flowTitles->total, flowSelectedTitleIdx +
                                    collectionVisualCoverIdx - PSBBN_COVER_CACHE_FOCUS);
        collectionVisualTitleIdx = collectionFavoritesOnly
                                      ? lunaNavMarkedByRank(favoriteFlags, titles->total, visualRank)
                                      : visualRank;
        drawPSBBNCollection(flowTitles, flowSelectedTitleIdx, psbbnCoverTextures,
                            flowOffset, collectionFavoritesOnly, now);
      } else if (view == UI_VIEW_ORBIT)
        drawOrbit(titles, selectedTitleIdx, psbbnCoverTextures, flowOffset,
                  orbitRandomActive, now);
      else {
        int visualFocus = orbsVisualCacheIndex(flowOffset);
        orbsVisualTitleIdx = lunaNavWrap(titles->total,
            selectedTitleIdx + visualFocus - ORBS_LOGO_CACHE_FOCUS);
        refreshOrbsBackground(getTargetByIdx(titles, orbsVisualTitleIdx));
        drawOrbsView(titles, selectedTitleIdx, flowOffset, visualFocus, now);
      }
    } else if (view == UI_VIEW_GRID) {
      int didLoadArtwork = 0;
      int gridCascadeProgress = 0;
      int pendingPageBuffer = -1;
      int pendingPageBase = -1;
      int loadPageBuffer = -1;
      uint32_t now = uiNowMs();

      if (gridActivePageBase < 0) {
        gridActivePageBase = (selectedTitleIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
        prepareGridPageBuffer(gridActivePageBuffer, gridActivePageBase, gridPageBases,
                              gridPageComplete, gridPageNextSlot);
      }

      // A held shoulder deliberately performs no artwork work, including the
      // half-second decision window. Fast-track moves lightweight page shells,
      // then the normal loader prepares only the page where the user stops.
      if (!gridFastTrackActive && gridShoulderDirection == 0) {
        if (!gridPageComplete[gridActivePageBuffer])
          loadPageBuffer = gridActivePageBuffer;

      if (!gridCascadeActive && gridPendingSelectedIdx >= 0) {
        pendingPageBase = (gridPendingSelectedIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
        pendingPageBuffer = lunaNavFindBuffer(gridPageBases, GRID_PAGE_BUFFERS, pendingPageBase);
        if (pendingPageBuffer < 0) {
          pendingPageBuffer = lunaNavChooseBuffer(gridPageBases, GRID_PAGE_BUFFERS, gridActivePageBuffer,
                                                  gridPreviousPageBuffer, gridIncomingPageBuffer);
          if (pendingPageBuffer >= 0)
            prepareGridPageBuffer(pendingPageBuffer, pendingPageBase, gridPageBases,
                                  gridPageComplete, gridPageNextSlot);
        }
        if (pendingPageBuffer >= 0 && !gridPageComplete[pendingPageBuffer])
          loadPageBuffer = pendingPageBuffer;
      }

      if (loadPageBuffer >= 0) {
        gridPageComplete[loadPageBuffer] =
            loadGridPageStep(titles, gridPageBases[loadPageBuffer], loadPageBuffer,
                             &gridPageNextSlot[loadPageBuffer], &didLoadArtwork);
      }

      if (!gridCascadeActive && pendingPageBuffer >= 0 && gridPageComplete[pendingPageBuffer]) {
        if (!didLoadArtwork && gridPendingSelectedCoverIdx != gridPendingSelectedIdx) {
          Target *pendingTarget = getTargetByIdx(titles, gridPendingSelectedIdx);
          refreshGridSelectedCover(pendingTarget, gridSelectedIncomingBuffer);
          gridPendingSelectedCoverIdx = gridPendingSelectedIdx;
          didLoadArtwork = 1;
        }

        if (gridPendingSelectedCoverIdx == gridPendingSelectedIdx) {
          int previousSelectedBuffer = gridSelectedActiveBuffer;
          gridSelectedActiveBuffer = gridSelectedIncomingBuffer;
          gridSelectedIncomingBuffer = previousSelectedBuffer;
          gridSelectedActiveIdx = gridPendingSelectedIdx;
          gridSelectedRequestedIdx = gridPendingSelectedIdx;
          gridSelectedAttemptedIdx = gridPendingSelectedIdx;
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;

          selectedTitleIdx = gridPendingSelectedIdx;
          curTarget = getTargetByIdx(titles, selectedTitleIdx);
          if (gridFastTrackSettling) {
            // Fast-track already animated the lightweight destination shell.
            // Promote its completed artwork in place instead of replaying the
            // normal page cascade after the last PNG finishes loading.
            int previousActiveBuffer = gridActivePageBuffer;
            gridActivePageBuffer = pendingPageBuffer;
            gridActivePageBase = pendingPageBase;
            gridPreviousPageBuffer = previousActiveBuffer;
            gridIncomingPageBuffer = -1;
            gridIncomingPageBase = -1;
            gridPrefetchDirection = (gridFastTrackDirection != 0) ? gridFastTrackDirection : gridPageDirection;
            gridPendingSelectedIdx = -1;
            gridPendingSelectedCoverIdx = -1;
            gridPageDirection = 0;
            gridCascadeActive = 0;
            gridCascadeDirection = 0;
            gridCascadeProgress = 0;
            gridFastTrackSettling = 0;
          } else {
            gridIncomingPageBuffer = pendingPageBuffer;
            gridIncomingPageBase = pendingPageBase;
            gridCascadeDirection = gridPageDirection;
            if (gridCascadeDirection == 0)
              gridCascadeDirection = (gridIncomingPageBase > gridActivePageBase) ? 1 : -1;
            gridCascadeStart = uiNowMs();
            gridCascadeActive = 1;
          }
        }
      }

      now = uiNowMs();
      if (gridCascadeActive) {
        uint32_t elapsed = now - gridCascadeStart;
        gridCascadeProgress = (elapsed >= GRID_CASCADE_DURATION_MS)
                                  ? 1000
                                  : (int)((elapsed * 1000ULL) / GRID_CASCADE_DURATION_MS);
        if (gridCascadeProgress >= 1000) {
          int previousActiveBuffer = gridActivePageBuffer;
          gridActivePageBuffer = gridIncomingPageBuffer;
          gridActivePageBase = gridIncomingPageBase;
          gridPreviousPageBuffer = previousActiveBuffer;
          gridIncomingPageBuffer = -1;
          gridIncomingPageBase = -1;
          gridPrefetchDirection = gridCascadeDirection;
          gridPendingSelectedIdx = -1;
          gridPendingSelectedCoverIdx = -1;
          gridPageDirection = 0;
          gridCascadeActive = 0;
          gridCascadeDirection = 0;
          gridCascadeProgress = 0;
        }
      }

      if (gridSelectedRequestedIdx != selectedTitleIdx) {
        gridSelectedRequestedIdx = selectedTitleIdx;
        gridSelectedAttemptedIdx = -1;
      }

      if (!didLoadArtwork && gridSelectedActiveIdx != gridSelectedRequestedIdx &&
          gridSelectedAttemptedIdx != gridSelectedRequestedIdx) {
        Target *selectedTarget = getTargetByIdx(titles, gridSelectedRequestedIdx);
        gridSelectedAttemptedIdx = gridSelectedRequestedIdx;
        if (refreshGridSelectedCover(selectedTarget, gridSelectedIncomingBuffer)) {
          int previousActiveBuffer = gridSelectedActiveBuffer;
          gridSelectedActiveBuffer = gridSelectedIncomingBuffer;
          gridSelectedActiveIdx = gridSelectedRequestedIdx;
          gridSelectedIncomingBuffer = previousActiveBuffer;
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
        } else {
          releaseGridTexture(gridSelectedTextures[gridSelectedActiveBuffer]);
          gridSelectedLoaded[gridSelectedActiveBuffer] = 0;
          gridSelectedActiveIdx = gridSelectedRequestedIdx;
        }
      }

        if (!didLoadArtwork && !gridCascadeActive && gridPendingSelectedIdx < 0 &&
            gridPageComplete[gridActivePageBuffer] && gridSelectedActiveIdx == gridSelectedRequestedIdx) {
          for (int prefetchPass = 0; prefetchPass < 2; prefetchPass++) {
            int direction = (prefetchPass == 0) ? gridPrefetchDirection : -gridPrefetchDirection;
            int prefetchPageBase = lunaNavPageBase(titles->total, gridActivePageBase, direction);
            int prefetchBuffer = lunaNavFindBuffer(gridPageBases, GRID_PAGE_BUFFERS, prefetchPageBase);

            if (prefetchBuffer < 0) {
              prefetchBuffer = lunaNavChooseBuffer(gridPageBases, GRID_PAGE_BUFFERS, gridActivePageBuffer,
                                                   gridPreviousPageBuffer, gridIncomingPageBuffer);
              if (prefetchBuffer >= 0)
                prepareGridPageBuffer(prefetchBuffer, prefetchPageBase, gridPageBases,
                                      gridPageComplete, gridPageNextSlot);
            }
            if (prefetchBuffer >= 0 && !gridPageComplete[prefetchBuffer]) {
              gridPageComplete[prefetchBuffer] =
                  loadGridPageStep(titles, gridPageBases[prefetchBuffer], prefetchBuffer,
                                   &gridPageNextSlot[prefetchBuffer], &didLoadArtwork);
              break;
            }
          }
        }
      }

      // All artwork work for this frame is complete. From here onward every
      // moving element uses this one timestamp.
      now = uiNowMs();
      if (gridCascadeActive) {
        uint32_t elapsed = now - gridCascadeStart;
        gridCascadeProgress = (elapsed >= GRID_CASCADE_DURATION_MS)
                                  ? 1000
                                  : (int)((elapsed * 1000ULL) / GRID_CASCADE_DURATION_MS);
      }

      if (gridFastTrackActive) {
        uint32_t elapsed = now - gridFastTrackSlideStart;
        int fastTrackProgress = (elapsed >= GRID_FAST_TRACK_STEP_MS)
                                    ? 1000
                                    : (int)((elapsed * 1000ULL) / GRID_FAST_TRACK_STEP_MS);
        int previousBuffer = lunaNavFindBuffer(gridPageBases, GRID_PAGE_BUFFERS, gridFastTrackPreviousPageBase);
        int destinationBuffer = lunaNavFindBuffer(gridPageBases, GRID_PAGE_BUFFERS, gridFastTrackPageBase);

        if (previousBuffer < 0 || !gridPageComplete[previousBuffer])
          previousBuffer = -1;
        if (destinationBuffer < 0 || !gridPageComplete[destinationBuffer])
          destinationBuffer = -1;
        drawPSBBNGrid(titles, gridFastTrackSelectedIdx, gridFastTrackPreviousPageBase, previousBuffer,
                      gridFastTrackPageBase, destinationBuffer, -1,
                      gridFastTrackDirection, fastTrackProgress, now);
      } else if (gridFastTrackSettling && !gridCascadeActive) {
        int destinationBuffer = lunaNavFindBuffer(gridPageBases, GRID_PAGE_BUFFERS, gridFastTrackPageBase);
        if (destinationBuffer < 0)
          destinationBuffer = -1;
        drawPSBBNGrid(titles, gridFastTrackSelectedIdx, gridFastTrackPageBase, destinationBuffer,
                      -1, -1, -1, gridFastTrackDirection, 0, now);
      } else {
        drawPSBBNGrid(titles, selectedTitleIdx, gridActivePageBase, gridActivePageBuffer,
                      gridIncomingPageBase, gridIncomingPageBuffer, gridSelectedActiveBuffer,
                      gridCascadeDirection, gridCascadeProgress, now);
      }
    } else {
      int favoritesEmpty = favoritesOnly && lunaNavMarkedCount(favoriteFlags, titles->total) == 0;
      const uint32_t frameNowMs = uiNowMs();
      const int coverPending = classicArtRequestedIdx == selectedTitleIdx;
      uint32_t fadeElapsed = frameNowMs - classicCoverFadeStartMs;
      int coverFadeProgress = (classicPreviousCoverAvailable && !coverPending &&
                               fadeElapsed < CLASSIC_COVER_FADE_DURATION_MS)
                                  ? (int)(fadeElapsed * 1000U / CLASSIC_COVER_FADE_DURATION_MS)
                                  : 1000;
      drawTitleList(titles, selectedTitleIdx, maxTitlesPerPage,
                    (classicDisplayedCoverAvailable && !favoritesEmpty) ? coverTexture : NULL,
                    (classicPreviousCoverAvailable && !favoritesEmpty && !coverPending &&
                     coverFadeProgress < 1000) ? classicPreviousCoverTexture : NULL,
                    (classicDisplayedDiscAvailable && !favoritesEmpty) ? discTexture : NULL,
                    favoriteFlags, favoritesOnly, coverPending && !favoritesEmpty,
                    coverFadeProgress, frameNowMs);
    }

  library_view_drawn:
    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);
    usleep(1000);

    // Keep rendering after options close, while ignoring the Triangle press
    // that closed them until the button is released.
    input = pollInput();
    if (optionsTriangleHeld) {
      if (input & PAD_TRIANGLE)
        input &= ~PAD_TRIANGLE;
      else
        optionsTriangleHeld = 0;
    }

    if ((input & PAD_SQUARE) == 0) {
      orbitRandomButtonHeld = 0;
      favoriteButtonHeld = 0;
    }
    if ((input & PAD_SELECT) == 0)
      favoritesTabButtonHeld = 0;

    if (view == UI_VIEW_GRID) {
      uint32_t now = uiNowMs();
      int leftHeld = (input & GRID_LEFT_SHOULDERS) != 0;
      int rightHeld = (input & GRID_RIGHT_SHOULDERS) != 0;
      int shoulderDirection = (rightHeld && !leftHeld) ? 1 : ((leftHeld && !rightHeld) ? -1 : 0);

      if (shoulderDirection == 0) {
        if (gridFastTrackActive) {
          int destinationPageBase = (gridFastTrackSelectedIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;

          gridFastTrackActive = 0;
          gridPageDirection = gridFastTrackDirection;
          if (destinationPageBase == gridActivePageBase) {
            gridPendingSelectedIdx = -1;
            gridPendingSelectedCoverIdx = -1;
            selectedTitleIdx = gridFastTrackSelectedIdx;
            curTarget = getTargetByIdx(titles, selectedTitleIdx);
            gridFastTrackSettling = 0;
          } else {
            gridPendingSelectedIdx = gridFastTrackSelectedIdx;
            gridPendingSelectedCoverIdx = -1;
            gridFastTrackSettling = 1;
          }
          input = 0;
        } else if (gridShoulderDirection != 0) {
          // A shoulder released before the hold threshold is a normal
          // one-page tap. Deferring this decision prevents any art decode
          // from starting while the user may still enter fast-track.
          int navigationIdx = (gridPendingSelectedIdx >= 0) ? gridPendingSelectedIdx : selectedTitleIdx;
          int candidate = lunaNavGridPage(titles->total, navigationIdx, gridShoulderDirection);

          if ((candidate / GRID_PAGE_SIZE) * GRID_PAGE_SIZE == gridActivePageBase) {
            gridPendingSelectedIdx = -1;
            selectedTitleIdx = candidate;
          } else {
            gridPendingSelectedIdx = candidate;
          }
          if (gridPendingSelectedCoverIdx >= 0 && gridPendingSelectedCoverIdx != gridPendingSelectedIdx) {
            releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
            gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
            gridPendingSelectedCoverIdx = -1;
          }
          gridPageDirection = gridShoulderDirection;
          input = 0;
        }
        gridShoulderDirection = 0;
        gridShoulderHoldStart = 0;
      } else if (shoulderDirection != gridShoulderDirection) {
        gridShoulderDirection = shoulderDirection;
        gridShoulderHoldStart = now;

        // Once fast-track is active, reversing direction remains immediate;
        // the user has already satisfied the hold threshold.
        if (gridFastTrackActive) {
          gridFastTrackDirection = shoulderDirection;
          gridFastTrackPreviousPageBase = gridFastTrackPageBase;
          gridFastTrackSelectedIdx = lunaNavGridPage(titles->total, gridFastTrackSelectedIdx, shoulderDirection);
          gridFastTrackPageBase = (gridFastTrackSelectedIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
          gridFastTrackSlideStart = now;
          gridFastTrackNextStep = now + GRID_FAST_TRACK_STEP_MS;
        }
      } else if (!gridFastTrackActive && now - gridShoulderHoldStart >= GRID_FAST_TRACK_HOLD_MS) {
        int navigationIdx = (gridPendingSelectedIdx >= 0) ? gridPendingSelectedIdx : selectedTitleIdx;

        // If the normal first page change already reached its cascade, accept
        // that prepared page immediately before entering lightweight tracking.
        if (gridCascadeActive && gridIncomingPageBuffer >= 0) {
          int previousActiveBuffer = gridActivePageBuffer;
          gridActivePageBuffer = gridIncomingPageBuffer;
          gridActivePageBase = gridIncomingPageBase;
          gridPreviousPageBuffer = previousActiveBuffer;
          gridIncomingPageBuffer = -1;
          gridIncomingPageBase = -1;
          gridPrefetchDirection = gridCascadeDirection;
          gridCascadeActive = 0;
          gridCascadeDirection = 0;
          gridCascadeStart = 0;
          navigationIdx = selectedTitleIdx;
        } else if (gridPendingSelectedCoverIdx >= 0) {
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
        }

        gridPendingSelectedIdx = -1;
        gridPendingSelectedCoverIdx = -1;
        gridFastTrackActive = 1;
        gridFastTrackSettling = 0;
        gridFastTrackDirection = shoulderDirection;
        gridFastTrackPreviousPageBase = (navigationIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
        gridFastTrackSelectedIdx = lunaNavGridPage(titles->total, navigationIdx, shoulderDirection);
        gridFastTrackPageBase = (gridFastTrackSelectedIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
        gridFastTrackSlideStart = now;
        gridFastTrackNextStep = now + GRID_FAST_TRACK_STEP_MS;
      } else if (gridFastTrackActive && now >= gridFastTrackNextStep) {
        gridFastTrackPreviousPageBase = gridFastTrackPageBase;
        gridFastTrackSelectedIdx = lunaNavGridPage(titles->total, gridFastTrackSelectedIdx, shoulderDirection);
        gridFastTrackPageBase = (gridFastTrackSelectedIdx / GRID_PAGE_SIZE) * GRID_PAGE_SIZE;
        gridFastTrackSlideStart = now;
        gridFastTrackNextStep = now + GRID_FAST_TRACK_STEP_MS;
      }
    }

    if (view == UI_VIEW_PSBBN || view == UI_VIEW_ORBIT || view == UI_VIEW_ORBS) {
      // Held navigation starts a new step roughly every 180 ms while each
      // glide lasts 420 ms. The accumulated fractional offset keeps the whole
      // stream continuous while several cover transitions overlap.
      frameCount = (frameCount + 1) % ((gsGlobal->Mode == GS_MODE_PAL) ? PSBBN_REPEAT_FRAMES_PAL : PSBBN_REPEAT_FRAMES_NTSC);
    } else if (gsGlobal->Mode == GS_MODE_PAL) {
      frameCount = (frameCount + 1) % 8; // Preserve Classic's established repeat cadence.
    } else {
      frameCount = (frameCount + 1) % 10;
    }

    if (view == UI_VIEW_PSBBN) {
      const int rawInput = input;
      const int actionButtons = PAD_CROSS | PAD_TRIANGLE | PAD_CIRCLE | PAD_SELECT | PAD_START;
      int scanDirection = 0;
      if (!(rawInput & actionButtons)) {
        int left = (rawInput & PAD_L2) != 0;
        int right = (rawInput & PAD_R2) != 0;
        scanDirection = right == left ? 0 : (right ? 1 : -1);
      }
      int scanStep = lunaCollectionScanUpdate(&collectionScan, scanDirection, uiNowMs());
      input = rawInput & ~(PAD_L2 | PAD_R2);
      if (scanDirection)
        input &= ~(PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN | PAD_L1 | PAD_R1);
      if (scanStep) {
        if (collectionFavoritesOnly) {
          int next = lunaNavMarkedStep(favoriteFlags, titles->total, selectedTitleIdx, scanStep);
          if (next >= 0)
            selectedTitleIdx = next;
        } else if (titles->total > 1) {
          selectedTitleIdx = lunaNavWrap(titles->total, selectedTitleIdx + scanStep);
        }
      }
    }

    if (view == UI_VIEW_CLASSIC) {
      const int rawInput = input;
      const int navButtons = PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN;
      int direction = 0;
      int navInput = 0;
      if (rawInput & (PAD_LEFT | PAD_UP)) {
        direction = -1;
        navInput = (rawInput & PAD_UP) ? PAD_UP : PAD_LEFT;
      } else if (rawInput & (PAD_RIGHT | PAD_DOWN)) {
        direction = 1;
        navInput = (rawInput & PAD_DOWN) ? PAD_DOWN : PAD_RIGHT;
      }
      classicNavHeld = direction != 0;
      input = rawInput & ~prevInput & ~navButtons;
      if (lunaNavRepeatStep(&classicRepeat, direction, uiNowMs(),
                            CLASSIC_REPEAT_DELAY_MS, CLASSIC_REPEAT_INTERVAL_MS))
        input |= navInput;
      prevInput = rawInput;
      if (!input)
        continue;
    } else {
      if (frameCount && (input == prevInput))
        continue;
      frameCount = 0;
      prevInput = input;
    }

    // Grid shoulders are handled by the tap/hold state machine above.
    if (view == UI_VIEW_GRID && (input & (GRID_LEFT_SHOULDERS | GRID_RIGHT_SHOULDERS)))
      continue;

    // Page preparation is background work. Only the visible cascade gates
    // interaction so page loading never feels like a frozen UI.
    if (view == UI_VIEW_GRID && gridCascadeActive)
      continue;
    if (view == UI_VIEW_GRID && gridFastTrackActive)
      continue;

    // Actions use the logo at the fixed Orbs marker even when the next
    // queued selection has not finished gliding into place.
    if (view == UI_VIEW_ORBS && orbsVisualTitleIdx >= 0 &&
        (input & (PAD_CROSS | PAD_TRIANGLE | PAD_CIRCLE | PAD_START))) {
      selectedTitleIdx = orbsVisualTitleIdx;
      curTarget = getTargetByIdx(titles, selectedTitleIdx);
      psbbnAnimationTargetIdx = -1;
      psbbnAnimationStartOffset = 0;
    }

    // Any deliberate input interrupts an in-progress random scan and resumes
    // normal manual control immediately. Square below starts a fresh scan.
    if (view == UI_VIEW_ORBIT && orbitRandomActive && (input & ~PAD_SQUARE) != 0)
      orbitRandomActive = 0;

    collectionActionCoverIdx = PSBBN_COVER_CACHE_FOCUS;
    if (view == UI_VIEW_PSBBN &&
        psbbnAnimationDuration == COLLECTION_SCAN_STEP_MS &&
        (input & (PAD_CROSS | PAD_TRIANGLE | PAD_CIRCLE | PAD_SELECT | PAD_START)) &&
        collectionVisualTitleIdx >= 0) {
      selectedTitleIdx = collectionVisualTitleIdx;
      curTarget = getTargetByIdx(titles, selectedTitleIdx);
      collectionActionCoverIdx = collectionVisualCoverIdx;
      psbbnAnimationTargetIdx = -1;
      psbbnAnimationStartOffset = 0;
      psbbnAnimationDuration = PSBBN_ANIMATION_DURATION_MS;
    }

    if ((view == UI_VIEW_CLASSIC || view == UI_VIEW_PSBBN) &&
        (input & PAD_SELECT) && !favoritesTabButtonHeld) {
      favoritesTabButtonHeld = 1;
      if (view == UI_VIEW_CLASSIC)
        favoritesOnly = !favoritesOnly;
      else
        collectionFavoritesOnly = !collectionFavoritesOnly;
      if ((favoritesOnly || collectionFavoritesOnly) && !favoriteFlags[selectedTitleIdx]) {
        int firstFavorite = lunaNavMarkedByRank(favoriteFlags, titles->total, 0);
        if (firstFavorite >= 0)
          selectedTitleIdx = firstFavorite;
      }
      if (view == UI_VIEW_PSBBN) {
        releasePSBBNCovers();
        psbbnCoverBaseIdx = -1;
        psbbnAnimationTargetIdx = -1;
        psbbnAnimationStartOffset = 0;
      }
    } else if ((input & PAD_CROSS) &&
               (!(favoritesOnly || collectionFavoritesOnly) ||
                lunaNavMarkedCount(favoriteFlags, titles->total) > 0)) {
      // Copy target, free title list and launch
      Target *target = copyTarget(curTarget);
      freeTargetList(favoriteTitles);
      favoriteTitles = NULL;
      free(favoriteFlags);
      favoriteFlags = NULL;
      freeTargetList(titles);
      GSTEXTURE *handoffCover = NULL;
      if (view == UI_VIEW_CLASSIC && !isCoverUninitialized) {
        handoffCover = coverTexture;
      } else if (view == UI_VIEW_GRID && gridSelectedActiveBuffer >= 0 &&
                 gridSelectedLoaded[gridSelectedActiveBuffer]) {
        handoffCover = gridSelectedTextures[gridSelectedActiveBuffer];
      } else if ((view == UI_VIEW_PSBBN || view == UI_VIEW_ORBIT) &&
                 psbbnCoverLoaded[collectionActionCoverIdx]) {
        handoffCover = psbbnCoverTextures[collectionActionCoverIdx];
      }
      uiLaunchTitle(target, NULL, handoffCover);
      // Something went wrong, main loop must exit immediately
      return -1;
    } else if (input & PAD_CIRCLE) {
      UILibraryView previousView = view;
      view = lunaNavNextView(view, orbsEnabled);
      collectionScan = (LunaCollectionScan){0};
      favoritesOnly = 0;
      collectionFavoritesOnly = 0;

      if (previousView == UI_VIEW_CLASSIC) {
        releaseClassicArtVRAM();
        isCoverUninitialized = 1;
        isDiscUninitialized = 1;
        classicDisplayedCoverAvailable = 0;
        classicDisplayedDiscAvailable = 0;
        classicPreviousCoverAvailable = 0;
      } else if (previousView == UI_VIEW_PSBBN || previousView == UI_VIEW_ORBIT) {
        releasePSBBNCovers();
      } else if (previousView == UI_VIEW_ORBS) {
        releaseOrbsArt();
      } else if (previousView == UI_VIEW_GRID) {
        releaseGridCovers();
      }

      psbbnCoverBaseIdx = -1;
      psbbnAnimationTargetIdx = -1;
      psbbnAnimationStartOffset = 0;
      psbbnAnimationDuration = PSBBN_ANIMATION_DURATION_MS;
      gridActivePageBuffer = 0;
      gridIncomingPageBuffer = -1;
      gridPreviousPageBuffer = -1;
      gridActivePageBase = -1;
      gridIncomingPageBase = -1;
      for (int buffer = 0; buffer < GRID_PAGE_BUFFERS; buffer++) {
        gridPageBases[buffer] = -1;
        gridPageComplete[buffer] = 0;
        gridPageNextSlot[buffer] = 0;
      }
      gridSelectedActiveBuffer = 0;
      gridSelectedIncomingBuffer = 1;
      gridSelectedActiveIdx = -1;
      gridSelectedRequestedIdx = -1;
      gridSelectedAttemptedIdx = -1;
      gridPendingSelectedIdx = -1;
      gridPendingSelectedCoverIdx = -1;
      gridPageDirection = 0;
      gridPrefetchDirection = 1;
      gridCascadeActive = 0;
      gridCascadeDirection = 0;
      gridCascadeStart = 0;
      gridShoulderDirection = 0;
      gridShoulderHoldStart = 0;
      gridFastTrackActive = 0;
      gridFastTrackSettling = 0;
      gridFastTrackDirection = 0;
      gridFastTrackSelectedIdx = -1;
      gridFastTrackPreviousPageBase = -1;
      gridFastTrackPageBase = -1;
      gridFastTrackSlideStart = 0;
      gridFastTrackNextStep = 0;
      orbitRandomActive = 0;
      orbitRandomTargetIdx = -1;
      orbitRandomButtonHeld = 0;
      if (view == UI_VIEW_CLASSIC) {
        isCoverUninitialized = loadCoverArt(curTarget->device, curTarget->id);
        isDiscUninitialized = loadDiscArt(curTarget->device, curTarget->id);
        classicDisplayedCoverAvailable = !isCoverUninitialized;
        classicDisplayedDiscAvailable = !isDiscUninitialized;
        classicPreviousCoverAvailable = 0;
        classicArtRequestedIdx = -1;
        classicNavHeld = 0;
        classicRepeat.direction = 0;
      }
      if (saveLastLibraryView(curTarget, view))
        DPRINTF("WARN: Could not save selected library view\n");
    } else if (view == UI_VIEW_CLASSIC && (input & PAD_SQUARE) && !favoriteButtonHeld &&
               (!favoritesOnly || lunaNavMarkedCount(favoriteFlags, titles->total) > 0)) {
      int wasFavorite = favoriteFlags[selectedTitleIdx] != 0;
      int previousRank = lunaNavMarkedRank(favoriteFlags, titles->total, selectedTitleIdx);
      favoriteButtonHeld = 1;
      favoriteFlags[selectedTitleIdx] = !wasFavorite;
      if (saveFavoriteFlags(titles, favoriteFlags, (size_t)titles->total, curTarget)) {
        favoriteFlags[selectedTitleIdx] = wasFavorite;
      } else {
        TargetList *updatedFavorites = buildFavoriteTargetList(titles, favoriteFlags,
                                                                (size_t)titles->total);
        if (updatedFavorites != NULL) {
          freeTargetList(favoriteTitles);
          favoriteTitles = updatedFavorites;
        }
      }
      if (favoritesOnly && wasFavorite && favoriteFlags[selectedTitleIdx] == 0) {
        int remaining = lunaNavMarkedCount(favoriteFlags, titles->total);
        if (remaining > 0) {
          selectedTitleIdx = lunaNavMarkedByRank(favoriteFlags, titles->total,
                                                (previousRank < remaining) ? previousRank : 0);
          curTarget = getTargetByIdx(titles, selectedTitleIdx);
          isCoverUninitialized = 1;
          isDiscUninitialized = 1;
          classicArtRequestedIdx = selectedTitleIdx;
          classicArtDueMs = uiNowMs() + CLASSIC_ART_SETTLE_MS;
        }
      }
    } else if (view == UI_VIEW_ORBIT && (input & PAD_SQUARE) && !orbitRandomButtonHeld) {
      // Pick a different destination every time, then let the adjacent-step
      // scanner reach it without forcing a ten-PNG cache rebuild in one frame.
      if (titles->total > 1) {
        uint32_t randomSeed = uiNowMs() ^ ((uint32_t)(selectedTitleIdx + 1) * 2654435761U);
        orbitRandomTargetIdx = lunaNavRandomTarget(titles->total, selectedTitleIdx, randomSeed);
        orbitRandomDirection = lunaNavDirection(titles->total, selectedTitleIdx, orbitRandomTargetIdx);
        orbitRandomActive = 1;
        orbitRandomNextStep = uiNowMs();
      }
      orbitRandomButtonHeld = 1;
    } else if (view == UI_VIEW_GRID && (input & (PAD_LEFT | PAD_RIGHT | PAD_UP | PAD_DOWN))) {
      int navigationIdx = (gridPendingSelectedIdx >= 0) ? gridPendingSelectedIdx : selectedTitleIdx;
      int candidate = navigationIdx;
      int direction;

      if (input & PAD_LEFT)
        candidate = ((navigationIdx - 1) + titles->total) % titles->total;
      else if (input & PAD_RIGHT)
        candidate = (navigationIdx + 1) % titles->total;
      else if (input & PAD_UP)
        candidate = lunaNavGridVertical(titles->total, navigationIdx, -1);
      else if (input & PAD_DOWN)
        candidate = lunaNavGridVertical(titles->total, navigationIdx, 1);

      direction = lunaNavDirection(titles->total, navigationIdx, candidate);
      if ((candidate / GRID_PAGE_SIZE) * GRID_PAGE_SIZE == gridActivePageBase) {
        if (gridPendingSelectedCoverIdx >= 0) {
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
        }
        gridPendingSelectedIdx = -1;
        gridPendingSelectedCoverIdx = -1;
        selectedTitleIdx = candidate;
      } else {
        if (gridPendingSelectedCoverIdx >= 0 && gridPendingSelectedCoverIdx != candidate) {
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
          gridPendingSelectedCoverIdx = -1;
        }
        gridPendingSelectedIdx = candidate;
      }
      gridPageDirection = direction;
    } else if (input & (PAD_LEFT | PAD_UP)) {
      // Point to the previous title
      if (favoritesOnly || collectionFavoritesOnly) {
        int favoriteIdx = lunaNavMarkedStep(favoriteFlags, titles->total, selectedTitleIdx, -1);
        if (favoriteIdx >= 0)
          selectedTitleIdx = favoriteIdx;
      } else {
        selectedTitleIdx = ((selectedTitleIdx - 1) + titles->total) % titles->total;
      }
    } else if (input & (PAD_RIGHT | PAD_DOWN)) {
      // Advance to the next title
      if (favoritesOnly || collectionFavoritesOnly) {
        int favoriteIdx = lunaNavMarkedStep(favoriteFlags, titles->total, selectedTitleIdx, 1);
        if (favoriteIdx >= 0)
          selectedTitleIdx = favoriteIdx;
      } else {
        selectedTitleIdx = (selectedTitleIdx + 1) % titles->total;
      }
    } else if (input & GRID_RIGHT_SHOULDERS) {
      // Switch to the next page.
      if (view == UI_VIEW_GRID) {
        int navigationIdx = (gridPendingSelectedIdx >= 0) ? gridPendingSelectedIdx : selectedTitleIdx;
        int candidate = lunaNavGridPage(titles->total, navigationIdx, 1);
        if ((candidate / GRID_PAGE_SIZE) * GRID_PAGE_SIZE == gridActivePageBase) {
          gridPendingSelectedIdx = -1;
          selectedTitleIdx = candidate;
        } else {
          gridPendingSelectedIdx = candidate;
        }
        if (gridPendingSelectedCoverIdx >= 0 && gridPendingSelectedCoverIdx != gridPendingSelectedIdx) {
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
          gridPendingSelectedCoverIdx = -1;
        }
        gridPageDirection = 1;
      } else if (favoritesOnly || collectionFavoritesOnly) {
        int favoriteIdx = lunaNavMarkedPage(favoriteFlags, titles->total, selectedTitleIdx,
                                            maxTitlesPerPage, 1);
        if (favoriteIdx >= 0)
          selectedTitleIdx = favoriteIdx;
      } else if (selectedTitleIdx == titles->total - 1) {
        selectedTitleIdx = 0; // Wrap around if the last title is selected
      } else {
        selectedTitleIdx += maxTitlesPerPage;
        if (selectedTitleIdx >= titles->total)
          selectedTitleIdx = titles->total - 1;
      }
    } else if (input & GRID_LEFT_SHOULDERS) {
      // Switch to the previous page.
      if (view == UI_VIEW_GRID) {
        int navigationIdx = (gridPendingSelectedIdx >= 0) ? gridPendingSelectedIdx : selectedTitleIdx;
        int candidate = lunaNavGridPage(titles->total, navigationIdx, -1);
        if ((candidate / GRID_PAGE_SIZE) * GRID_PAGE_SIZE == gridActivePageBase) {
          gridPendingSelectedIdx = -1;
          selectedTitleIdx = candidate;
        } else {
          gridPendingSelectedIdx = candidate;
        }
        if (gridPendingSelectedCoverIdx >= 0 && gridPendingSelectedCoverIdx != gridPendingSelectedIdx) {
          releaseGridTexture(gridSelectedTextures[gridSelectedIncomingBuffer]);
          gridSelectedLoaded[gridSelectedIncomingBuffer] = 0;
          gridPendingSelectedCoverIdx = -1;
        }
        gridPageDirection = -1;
      } else if (favoritesOnly || collectionFavoritesOnly) {
        int favoriteIdx = lunaNavMarkedPage(favoriteFlags, titles->total, selectedTitleIdx,
                                            maxTitlesPerPage, -1);
        if (favoriteIdx >= 0)
          selectedTitleIdx = favoriteIdx;
      } else if (selectedTitleIdx == 0) {
        selectedTitleIdx = titles->total - 1; // Wrap around if the first title is selected
      } else {
        selectedTitleIdx -= maxTitlesPerPage;
        if (selectedTitleIdx < 0)
          selectedTitleIdx = 0;
      }
    } else if ((input & PAD_TRIANGLE) &&
               (!(favoritesOnly || collectionFavoritesOnly) ||
                lunaNavMarkedCount(favoriteFlags, titles->total) > 0)) {
      prevInput = 0; // Reset previous input
      // Enter title options screen
      if ((res = uiTitleOptionsLoop(curTarget, &classicArtOverlap, &orbsEnabled,
                                    &orbsBackground, &ambientEnabled)) < 0) {
        // Something went wrong, main loop must exit immediately
        ambientStop();
        freeTargetList(favoriteTitles);
        free(favoriteFlags);
        return -1;
      }
      if (view == UI_VIEW_ORBS && !orbsEnabled) {
        releaseOrbsArt();
        view = UI_VIEW_CLASSIC;
        isCoverUninitialized = loadCoverArt(curTarget->device, curTarget->id);
        isDiscUninitialized = loadDiscArt(curTarget->device, curTarget->id);
        classicDisplayedCoverAvailable = !isCoverUninitialized;
        classicDisplayedDiscAvailable = !isDiscUninitialized;
        classicPreviousCoverAvailable = 0;
        classicArtRequestedIdx = -1;
        classicNavHeld = 0;
        classicRepeat.direction = 0;
        if (saveLastLibraryView(curTarget, view))
          DPRINTF("WARN: Could not save selected library view\n");
      }
      optionsTriangleHeld = (pollInput() & PAD_TRIANGLE) != 0;
      input = 0;
    } else if (input & PAD_START) {
      // Quit
      break;
    }
  }

exit:
  ambientStop();
  if (favoriteTitles != NULL)
    freeTargetList(favoriteTitles);
  free(favoriteFlags);
  closePad();
  closeUI();
  return res;
}
// Draws title list




// The last library frame stays in the other screen buffer while the options
// screen draws repeatedly into the current buffer. No artwork texture or extra
// full-size framebuffer allocation is needed for the backdrop.
static void drawOptionsBackdrop(const OptionsBackdrop *backdrop) {
  const int width = gsGlobal->Width;
  const int height = gsGlobal->Height;
  const GSTEXTURE *frame = &backdrop->libraryFrame;
  GSTEXTURE sharpFrame = *frame;
  sharpFrame.Filter = GS_FILTER_NEAREST;
  static const int sampleX[] = {-9, 9, 0, 0, 0};
  static const int sampleY[] = {0, 0, -9, 9, 0};
  // Each successive fixed-alpha blend gives all five samples equal weight.
  static const int sampleAlpha[] = {0x80, 0x40, 0x2B, 0x20, 0x1A};

  gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
  gsKit_set_test(gsGlobal, GS_ATEST_OFF);
  gsKit_prim_sprite_texture(gsGlobal, &sharpFrame, 0, 0, 0, 0, width, height,
                            width, height, 0,
                            GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80));

  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  for (int i = 0; i < 5; i++) {
    gsKit_set_primalpha(gsGlobal,
                        GS_SETREG_ALPHA(0, 1, 2, 1, sampleAlpha[i]), 0);
    gsKit_prim_sprite_texture(gsGlobal, frame, 0, 0, sampleX[i], sampleY[i],
                              width, height, width + sampleX[i],
                              height + sampleY[i], 0,
                              GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80));
  }
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
}

static void drawOptionsSheet(const OptionsBackdrop *backdrop) {
  drawOptionsBackdrop(backdrop);
  gsKit_prim_sprite(gsGlobal, 0, 0, gsGlobal->Width, gsGlobal->Height, 0,
                    GS_SETREG_RGBA(0x02, 0x08, 0x16, 0x70));
}

// Blend the untouched library frame over the finished menu. Fading that copy
// away reveals the options and gradually brings in the dark, blurred backdrop.
static void drawOptionsFade(const OptionsBackdrop *backdrop, int progress) {
  if (progress >= 1000)
    return;
  const GSTEXTURE *frame = &backdrop->libraryFrame;
  GSTEXTURE sharpFrame = *frame;
  sharpFrame.Filter = GS_FILTER_NEAREST;
  int alpha = ((1000 - progress) * 0x80 + 500) / 1000;
  gsKit_set_test(gsGlobal, GS_ATEST_OFF);
  gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 2, 1, alpha), 0);
  gsKit_prim_sprite_texture(gsGlobal, &sharpFrame, 0, 0, 0, 0,
                            gsGlobal->Width, gsGlobal->Height,
                            gsGlobal->Width, gsGlobal->Height, 0,
                            GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80));
  gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
  gsKit_set_test(gsGlobal, GS_ATEST_ON);
}

static void presentOptionsFrame(void) {
  gsKit_queue_exec(gsGlobal);
  gsKit_finish();
  gsKit_vsync_wait();
  // Keep ActiveBuffer fixed: the other buffer holds the untouched library view.
  gsKit_display_buffer(gsGlobal);
  usleep(1000);
}

void drawTitleOptionsFooter(int baseX) {
  drawIconWindow(baseX, gsGlobal->Height - footerHeight, 0, gsGlobal->Height, 0, FontMainColor, ALIGN_CENTER, ICON_CIRCLE);
  drawIconWindow(baseX + getIconWidth(ICON_CIRCLE), gsGlobal->Height - footerHeight, 0, gsGlobal->Height, 0, FontMainColor, ALIGN_CENTER, ICON_CROSS);
  drawTextWindow(baseX + 5 + getIconWidth(ICON_CIRCLE) + getIconWidth(ICON_CROSS), gsGlobal->Height - 1 - footerHeight, 0, gsGlobal->Height, 0,
                 HeaderTextColor, ALIGN_VCENTER, "Toggle");

  drawIconWindow((gsGlobal->Width * 3 / 8) - getIconWidth(ICON_SQUARE), gsGlobal->Height - footerHeight, gsGlobal->Width, gsGlobal->Height, 0,
                 FontMainColor, ALIGN_VCENTER, ICON_SQUARE);
  drawTextWindow((gsGlobal->Width * 3 / 8) + 5, gsGlobal->Height - footerHeight, gsGlobal->Width, gsGlobal->Height, 0, HeaderTextColor, ALIGN_VCENTER,
                 "Test");

  drawIconWindow((gsGlobal->Width * 5 / 8), gsGlobal->Height - footerHeight, gsGlobal->Width - getLineWidth("Save") - 5, gsGlobal->Height, 0,
                 FontMainColor, ALIGN_VCENTER, ICON_START);
  drawTextWindow((gsGlobal->Width * 5 / 8) + 5 + getIconWidth(ICON_START), gsGlobal->Height - 1 - footerHeight, gsGlobal->Width, gsGlobal->Height, 0,
                 HeaderTextColor, ALIGN_VCENTER, "Save");

  drawIconWindow(gsGlobal->Width - baseX - 5 - getIconWidth(ICON_TRIANGLE) - getLineWidth("Back"), gsGlobal->Height - footerHeight,
                 gsGlobal->Width - baseX, gsGlobal->Height, 0, FontMainColor, ALIGN_VCENTER | ALIGN_LEFT, ICON_TRIANGLE);
  drawTextWindow(0, gsGlobal->Height - 1 - footerHeight, gsGlobal->Width - baseX, gsGlobal->Height, 0, HeaderTextColor, ALIGN_VCENTER | ALIGN_RIGHT,
                 "Back");

  drawTextWindow(0, gsGlobal->Height - 1 - footerHeight - getFontLineHeight() / 2, gsGlobal->Width, gsGlobal->Height, 0, HeaderTextColor,
                 ALIGN_TOP | ALIGN_HCENTER, "Switch views");
  drawIconWindow(0, gsGlobal->Height - footerHeight - getFontLineHeight() / 2, (gsGlobal->Width - getLineWidth("Switch views")) / 2 - 5,
                 gsGlobal->Height, 0, FontMainColor, ALIGN_TOP | ALIGN_RIGHT, ICON_L1);
  drawIconWindow((gsGlobal->Width + getLineWidth("Switch views")) / 2 + 5, gsGlobal->Height - footerHeight - getFontLineHeight() / 2, gsGlobal->Width,
                 gsGlobal->Height, 0, FontMainColor, ALIGN_TOP | ALIGN_LEFT, ICON_R1);
}

static void drawOptionsCategoryFooter(int baseX, OptionsPage page) {
  drawIconWindow(baseX, gsGlobal->Height - footerHeight, 0, gsGlobal->Height,
                 0, FontMainColor, ALIGN_CENTER, ICON_CROSS);
  drawTextWindow(baseX + getIconWidth(ICON_CROSS) + 5,
                 gsGlobal->Height - 1 - footerHeight, 0, gsGlobal->Height,
                 0, HeaderTextColor, ALIGN_VCENTER,
                 page == OPTIONS_MENU ? "Select" : "Toggle");
  if (page == OPTIONS_GLOBAL) {
    drawIconWindow((gsGlobal->Width * 5 / 8), gsGlobal->Height - footerHeight,
                   gsGlobal->Width - getLineWidth("Save") - 5, gsGlobal->Height,
                   0, FontMainColor, ALIGN_VCENTER, ICON_START);
    drawTextWindow((gsGlobal->Width * 5 / 8) + 5 + getIconWidth(ICON_START),
                   gsGlobal->Height - 1 - footerHeight, gsGlobal->Width,
                   gsGlobal->Height, 0, HeaderTextColor, ALIGN_VCENTER, "Save");
  }
  drawIconWindow(gsGlobal->Width - baseX - 5 - getIconWidth(ICON_TRIANGLE) -
                     getLineWidth("Back"),
                 gsGlobal->Height - footerHeight, gsGlobal->Width - baseX,
                 gsGlobal->Height, 0, FontMainColor, ALIGN_VCENTER | ALIGN_LEFT,
                 ICON_TRIANGLE);
  drawTextWindow(0, gsGlobal->Height - 1 - footerHeight,
                 gsGlobal->Width - baseX, gsGlobal->Height, 0,
                 HeaderTextColor, ALIGN_VCENTER | ALIGN_RIGHT, "Back");
}

static void drawTitleOptionsFrame(const OptionsBackdrop *backdrop, Target *target,
                                  OptionsPage page, int selectedCategory,
                                  int selectedGlobal, int pendingOverlap,
                                  int pendingOrbs, int pendingBackground,
                                  int pendingAmbient,
                                  int activeArgumentIdx,
                                  int saveError, int progress) {
  int baseX = keepoutArea + 10;
  const int lineHeight = getFontLineHeight();
  int i;
  // The destination buffer still has the library's old depth values. Draw
  // this composed screen in command order, then restore normal library depth.
  gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
  drawOptionsSheet(backdrop);

  if (page == OPTIONS_MENU)
    snprintf(lineBuffer, sizeof(lineBuffer), "Options");
  else if (page == OPTIONS_GLOBAL)
    snprintf(lineBuffer, sizeof(lineBuffer), "Global settings");
  else
    snprintf(lineBuffer, sizeof(lineBuffer), "%s\n%s", target->name, target->id);
  drawTextWindow(baseX, headerHeight - lineHeight,
                 gsGlobal->Width - baseX, 0, 0, HeaderTextColor,
                 ALIGN_HCENTER, lineBuffer);

  const int menuTop = headerHeight + 1.5 * lineHeight;
  const int menuBottom = gsGlobal->Height - footerHeight - lineHeight;
  if (page == OPTIONS_MENU) {
    drawText(baseX, menuTop, 0, gsGlobal->Width - baseX, 0,
             selectedCategory == 0 ? ColorSelected : FontMainColor,
             "Per-game settings");
    drawText(baseX, menuTop + lineHeight + lineHeight / 2, 0,
             gsGlobal->Width - baseX, 0,
             selectedCategory == 1 ? ColorSelected : FontMainColor,
             "Global settings");
    drawOptionsCategoryFooter(baseX, page);
  } else if (page == OPTIONS_GLOBAL) {
    snprintf(lineBuffer, sizeof(lineBuffer), "Classic art layout: %s",
             pendingOverlap ? "Overlap" : "Separate");
    drawText(baseX, menuTop, 0, gsGlobal->Width - baseX, 0,
             selectedGlobal == 0 ? ColorSelected : FontMainColor, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "Orbs view (Experimental): %s",
             pendingOrbs ? "On" : "Off");
    drawText(baseX, menuTop + lineHeight + lineHeight / 2, 0,
             gsGlobal->Width - baseX, 0,
             selectedGlobal == 1 ? ColorSelected : FontMainColor, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "Background (Experimental): %s",
             pendingBackground ? "Orbs" : "Stars & cubes");
    drawText(baseX, menuTop + 2 * (lineHeight + lineHeight / 2), 0,
             gsGlobal->Width - baseX, 0,
             selectedGlobal == 2 ? ColorSelected : FontMainColor, lineBuffer);
    snprintf(lineBuffer, sizeof(lineBuffer), "Ambient sound: %s",
             pendingAmbient ? "On" : "Off");
    drawText(baseX, menuTop + 3 * (lineHeight + lineHeight / 2), 0,
             gsGlobal->Width - baseX, 0,
             selectedGlobal == 3 ? ColorSelected : FontMainColor, lineBuffer);
    drawOptionsCategoryFooter(baseX, page);
  } else {
    int focusY = menuTop;
    int scrollOffset = 0;
    for (i = 0; i < activeArgumentIdx; i++)
      focusY += uiArguments[i].rowCount * lineHeight + lineHeight / 2;
    focusY += (uiArguments[activeArgumentIdx].focusRowOffset +
               uiArguments[activeArgumentIdx].activeElementIdx) * lineHeight;
    if (focusY + lineHeight > menuBottom)
      scrollOffset = focusY + lineHeight - menuBottom;

    int startY = menuTop - scrollOffset;
    for (i = 0; i < uiArgumentsTotal; i++) {
      startY = lineHeight / 2 +
               uiArguments[i].draw(&uiArguments[i], (i == activeArgumentIdx) ? 1 : 0,
                                    baseX, startY, 0, gsGlobal->Width - baseX,
                                    menuTop, menuBottom);
    }
    drawTitleOptionsFooter(baseX);
  }
  if (saveError)
    drawTextWindow(baseX, gsGlobal->Height - footerHeight - getFontLineHeight(),
                   gsGlobal->Width - baseX, gsGlobal->Height - footerHeight, 0,
                   ErrorTextColor, ALIGN_HCENTER,
                   page == OPTIONS_GLOBAL ? "Could not save global settings" : "Could not save game settings");
  drawOptionsFade(backdrop, progress);
  gsKit_set_test(gsGlobal, GS_ZTEST_ON);
  presentOptionsFrame();
}

// Handles the options menu and its per-game and global settings pages.
// Returns -1 if error occurs
int uiTitleOptionsLoop(Target *target, int *classicArtOverlap, int *orbsEnabled,
                       int *orbsBackgroundSetting, int *ambientEnabled) {
  int res = 0;
  int saveError = 0;
  int pendingOverlap = *classicArtOverlap;
  int pendingOrbs = *orbsEnabled;
  int pendingBackground = *orbsBackgroundSetting;
  int pendingAmbient = *ambientEnabled;
  int titleArgumentsChanged = 0;
  OptionsPage page = OPTIONS_MENU;
  int selectedCategory = 0;
  int selectedGlobal = 0;

  // Load arguments from config files
  ArgumentList *titleArguments = loadLaunchArgumentLists(target);
  int input = 0;
  int activeArgumentIdx = 0;

  // Parse arguments
  for (int i = 0; i < (uiArgumentsTotal); i++)
    uiArguments[i].parse(&uiArguments[i], titleArguments);

  OptionsBackdrop backdrop = {0};
  backdrop.libraryFrame.Width = gsGlobal->Width;
  backdrop.libraryFrame.Height = gsGlobal->Height;
  backdrop.libraryFrame.PSM = gsGlobal->PSM;
  backdrop.libraryFrame.TBW = gsGlobal->Width / 64;
  backdrop.libraryFrame.Vram = gsGlobal->ScreenBuffer[(gsGlobal->ActiveBuffer ^ 1) & 1];
  backdrop.libraryFrame.Filter = GS_FILTER_LINEAR;

  uint32_t fadeStart = uiNowMs();
  int progress;
  int closeRequested = 0;
  int triangleReleased = 0;
  do {
    uint32_t elapsed = uiNowMs() - fadeStart;
    progress = elapsed >= OPTIONS_FADE_DURATION_MS
                   ? 1000 : (int)(elapsed * 1000U / OPTIONS_FADE_DURATION_MS);
    drawTitleOptionsFrame(&backdrop, target, page, selectedCategory,
                          selectedGlobal, pendingOverlap, pendingOrbs,
                          pendingBackground, pendingAmbient,
                          activeArgumentIdx, saveError, progress);
    int heldInput = pollInput();
    if (!(heldInput & PAD_TRIANGLE))
      triangleReleased = 1;
    else if (triangleReleased)
      closeRequested = 1;
  } while (progress < 1000);

  if (closeRequested)
    goto exit;

  int i;
  while (1) {
    drawTitleOptionsFrame(&backdrop, target, page, selectedCategory,
                          selectedGlobal, pendingOverlap, pendingOrbs,
                          pendingBackground, pendingAmbient,
                          activeArgumentIdx, saveError, 1000);

    // Process user inputs
    input = waitForInput(-1);
    if (page == OPTIONS_MENU) {
      if (input & (PAD_UP | PAD_DOWN))
        selectedCategory = 1 - selectedCategory;
      else if (input & (PAD_CROSS | PAD_CIRCLE)) {
        page = selectedCategory == 0 ? OPTIONS_PER_GAME : OPTIONS_GLOBAL;
        saveError = 0;
        activeArgumentIdx = 0;
        if (page == OPTIONS_PER_GAME)
          for (i = 0; i < uiArgumentsTotal; i++)
            uiArguments[i].parse(&uiArguments[i], titleArguments);
      } else if (input & PAD_TRIANGLE)
        goto exit;
      continue;
    }
    if (page == OPTIONS_GLOBAL) {
      if (input & PAD_UP) {
        selectedGlobal = (selectedGlobal + 3) % 4;
      } else if (input & PAD_DOWN) {
        selectedGlobal = (selectedGlobal + 1) % 4;
      } else if (input & (PAD_CROSS | PAD_CIRCLE)) {
        if (selectedGlobal == 0)
          pendingOverlap = !pendingOverlap;
        else if (selectedGlobal == 1)
          pendingOrbs = !pendingOrbs;
        else if (selectedGlobal == 2)
          pendingBackground = !pendingBackground;
        else
          pendingAmbient = !pendingAmbient;
      } else if (input & PAD_START) {
        saveError = 0;
        if (pendingOverlap != *classicArtOverlap) {
          saveError = saveClassicArtOverlap(target, pendingOverlap);
          if (!saveError) {
            *classicArtOverlap = pendingOverlap;
            setClassicArtOverlap(pendingOverlap);
          }
        }
        if (!saveError && pendingOrbs != *orbsEnabled) {
          saveError = saveOrbsViewEnabled(target, pendingOrbs);
          if (!saveError)
            *orbsEnabled = pendingOrbs;
        }
        if (!saveError && pendingBackground != *orbsBackgroundSetting) {
          saveError = saveOrbsBackground(target, pendingBackground);
          if (!saveError)
            *orbsBackgroundSetting = pendingBackground;
        }
        if (!saveError && pendingAmbient != *ambientEnabled) {
          saveError = saveAmbientSoundEnabled(target, pendingAmbient);
          if (!saveError) {
            *ambientEnabled = pendingAmbient;
            ambientSetEnabled(pendingAmbient);
          }
        }
        if (!saveError)
          page = OPTIONS_MENU;
      } else if (input & PAD_TRIANGLE) {
        pendingOverlap = *classicArtOverlap;
        pendingOrbs = *orbsEnabled;
        pendingBackground = *orbsBackgroundSetting;
        pendingAmbient = *ambientEnabled;
        saveError = 0;
        page = OPTIONS_MENU;
      }
      continue;
    }
    if (input & (PAD_L1 | PAD_R1)) {
      // Show full argument list
      res = uiArgumentListLoop(target, titleArguments, &backdrop);
      if (res < 0)
        goto exit;
      if (res == 2) {
        page = OPTIONS_MENU;
        titleArgumentsChanged = 0;
      } else {
        titleArgumentsChanged = 1;
        for (i = 0; i < uiArgumentsTotal; i++)
          uiArguments[i].parse(&uiArguments[i], titleArguments);
      }
      res = 0;
    } else if (input & PAD_SQUARE) {
      // Launch title without saving arguments
      uiLaunchTitle(target, titleArguments, NULL);
      res = -1; // If this was somehow reached, something went terribly wrong
      goto exit;
    } else if (input & PAD_START) {
      saveError = updateTitleLaunchArguments(target, titleArguments);
      if (!saveError) {
        page = OPTIONS_MENU;
        titleArgumentsChanged = 0;
      }
    } else if (input & PAD_TRIANGLE) {
      // Back discards changes made on this settings page.
      if (titleArgumentsChanged) {
        freeArgumentList(titleArguments);
        titleArguments = loadLaunchArgumentLists(target);
        for (i = 0; i < uiArgumentsTotal; i++)
          uiArguments[i].parse(&uiArguments[i], titleArguments);
        titleArgumentsChanged = 0;
      }
      saveError = 0;
      page = OPTIONS_MENU;
    } else {
      switch (uiArguments[activeArgumentIdx].handleInput(&uiArguments[activeArgumentIdx], input)) {
      case ACTION_CHANGED:
        titleArgumentsChanged = 1;
        uiArguments[activeArgumentIdx].marshal(&uiArguments[activeArgumentIdx], titleArguments);
        break;
      case ACTION_NEXT_ARGUMENT:
        if (activeArgumentIdx < uiArgumentsTotal - 1)
          activeArgumentIdx++;
        break;
      case ACTION_PREV_ARGUMENT:
        if (activeArgumentIdx > 0)
          activeArgumentIdx--;
        break;
      default:
      }
    }
  }
exit:
  if (res >= 0) {
    fadeStart = uiNowMs();
    do {
      uint32_t elapsed = uiNowMs() - fadeStart;
      progress = elapsed >= OPTIONS_FADE_DURATION_MS
                     ? 0 : 1000 - (int)(elapsed * 1000U / OPTIONS_FADE_DURATION_MS);
      drawTitleOptionsFrame(&backdrop, target, page, selectedCategory,
                            selectedGlobal, pendingOverlap, pendingOrbs,
                            pendingBackground, pendingAmbient,
                            activeArgumentIdx, 0, progress);
    } while (progress > 0);
  }
  freeArgumentList(titleArguments);
  return res;
}

// Handles all arguments in arugment list
// Returns -1 after a failed launch, 0 to return to per-game settings,
// or 2 after saving the game settings.
int uiArgumentListLoop(Target *target, ArgumentList *titleArguments,
                       const OptionsBackdrop *backdrop) {
  int selectedArgIdx = 0;
  int input = 0;
  int saveError = 0;

  Argument *curArgument = titleArguments->first;
  while (1) {
    gsKit_set_test(gsGlobal, GS_ZTEST_OFF);
    drawOptionsSheet(backdrop);
    int baseX = keepoutArea + 10;

    // Draw header
    snprintf(lineBuffer, 255, "%s\n%s", target->name, target->id);
    drawTextWindow(baseX, headerHeight - getFontLineHeight(), gsGlobal->Width - baseX, 0, 0, HeaderTextColor, ALIGN_HCENTER, lineBuffer);
    drawTextWindow(baseX, headerHeight + 1.5 * getFontLineHeight(), gsGlobal->Width - baseX, 0, 0, FontMainColor, ALIGN_HCENTER, "Launch arguments");

    // Draw footer
    drawTitleOptionsFooter(baseX);
    if (saveError)
      drawTextWindow(baseX, gsGlobal->Height - footerHeight - getFontLineHeight(), gsGlobal->Width - baseX,
                     gsGlobal->Height - footerHeight, 0, ErrorTextColor, ALIGN_HCENTER,
                     "Could not save game settings");

    int startY = headerHeight + 2.5 * getFontLineHeight();
    int idx = 0;

    // Set number of elements per page according to line height and available screen height
    int maxArguments = (gsGlobal->Height - startY - footerHeight - getFontLineHeight() / 2) / getFontLineHeight();
    int curPage = selectedArgIdx / maxArguments;

    snprintf(lineBuffer, 255, "Page %d/%d", curPage + 1, (!titleArguments->total) ? 1 : DIV_ROUND(titleArguments->total, maxArguments));
    startY = drawTextWindow(baseX, startY - getFontLineHeight(), gsGlobal->Width - baseX, 0, 0, HeaderTextColor, ALIGN_RIGHT, lineBuffer);

    Argument *argument = titleArguments->first;
    while (argument != NULL) {
      // Do not display arguments before the current page
      if (idx < maxArguments * curPage) {
        idx++;
        goto next;
      }
      // Do not display arguments beyond the current page
      if (idx >= maxArguments * (curPage + 1)) {
        break;
      }

      // Draw argument
      if (!argument->isDisabled)
        drawIconWindow(baseX, startY, 20, startY + getFontLineHeight(), 0, FontMainColor, ALIGN_CENTER, ICON_ENABLED);

      snprintf(lineBuffer, 255, "%s%s%s %s", ((argument->isGlobal) ? "[G] " : ""), argument->arg, (!strlen(argument->value)) ? "" : ":",
               argument->value);
      startY = drawText(baseX + getIconWidth(ICON_ENABLED), startY, 0, 0, 0, ((selectedArgIdx == idx) ? ColorSelected : FontMainColor), lineBuffer);

      idx++;
    next:
      argument = argument->next;
    }

    gsKit_set_test(gsGlobal, GS_ZTEST_ON);
    presentOptionsFrame();

    // Process user inputs
    input = waitForInput(-1);
    if (input & (PAD_L1 | PAD_R1)) {
      return 0;
    } else if (input & PAD_SQUARE) {
      // Launch title without saving arguments
      uiLaunchTitle(target, titleArguments, NULL);
      return -1; // If this was somehow reached, something went terribly wrong
    } else if (input & PAD_START) {
      saveError = updateTitleLaunchArguments(target, titleArguments);
      if (!saveError)
        return 2;
    } else if (input & PAD_TRIANGLE) {
      return 0;
    }

    // Ignore inputs when the argument is not initialized
    if (!curArgument)
      continue;

    if (input & (PAD_CROSS | PAD_CIRCLE)) {
      // Toggle argument
      curArgument->isDisabled = !curArgument->isDisabled;
      // If the argument was disabled, reset global flag
      if (curArgument->isDisabled)
        curArgument->isGlobal = 0;
    } else if (input & PAD_UP) {
      // Point to the previous argument
      selectedArgIdx = (selectedArgIdx - 1 + titleArguments->total) % titleArguments->total;
      curArgument = (curArgument->prev) ? curArgument->prev : titleArguments->last;
    } else if (input & PAD_DOWN) {
      // Advance to the next argument
      selectedArgIdx = (selectedArgIdx + 1) % titleArguments->total;
      curArgument = (curArgument->next) ? curArgument->next : titleArguments->first;
    }
  }
}

// Displays Game ID and launches the title
void uiLaunchTitle(Target *target, ArgumentList *arguments, GSTEXTURE *cover) {
  UILaunchHandoff handoff = {.target = target, .cover = cover};

  // Present immediately, then continue with real launch work. There is no
  // minimum display time or transition delay.
  uiPresentLaunchHandoff(target, cover, LAUNCH_STAGE_PREPARING);
  closePad();

  if (arguments == NULL)
    arguments = loadLaunchArgumentLists(target);

  // Keep the final framebuffer resident while Neutrino loads. The process
  // replacement reclaims these UI resources without exposing a black frame.
  launchTitleWithProgress(target, arguments, uiLaunchHandoffProgress, &handoff);

  // launchTitleWithProgress normally never returns. Retain cleanup for an
  // unsupported target mode or another pre-exec failure.
  closeUI();
}

//
// GameID code based on https://github.com/CosmicScale/Retro-GEM-PS2-Disc-Launcher
//

static uint8_t calculateCRC(const uint8_t *data, int len) {
  uint8_t crc = 0x00;
  for (int i = 0; i < len; i++) {
    crc += data[i];
  }
  return 0x100 - crc;
}

void drawGameID(const char *gameID) {
  uint8_t data[64] = {0};
  int gidlen = strnlen(gameID, 11); // Ensure the length does not exceed 11 characters

  int dpos = 0;
  data[dpos++] = 0xA5; // detect word
  data[dpos++] = 0x00; // address offset
  dpos++;
  data[dpos++] = gidlen;

  memcpy(&data[dpos], gameID, gidlen);
  dpos += gidlen;

  data[dpos++] = 0x00;
  data[dpos++] = 0xD5; // end word
  data[dpos++] = 0x00; // padding

  int data_len = dpos;
  data[2] = calculateCRC(&data[3], data_len - 3);

  int xstart = (gsGlobal->Width / 2) - (data_len * 8);
  int ystart = gsGlobal->Height - (((gsGlobal->Height / 8) * 2) + 20);
  int height = 2;

  for (int i = 0; i < data_len; i++) {
    for (int j = 7; j >= 0; j--) {
      int x = xstart + (i * 16 + ((7 - j) * 2));
      int x1 = x + 1;
      gsKit_prim_sprite(gsGlobal, x, ystart, x1, ystart + height, 0, GS_SETREG_RGBA(0xFF, 0x00, 0xFF, 0x80));

      uint32_t color = (data[i] >> j) & 1 ? GS_SETREG_RGBA(0x00, 0xFF, 0xFF, 0x80) : GS_SETREG_RGBA(0xFF, 0xFF, 0x00, 0x80);
      gsKit_prim_sprite(gsGlobal, x1, ystart, x1 + 1, ystart + height, 0, color);
    }
  }
}

//
// Splash screen functions
//

struct {
  int32_t doneSema;          // Used to signal UI splash thread to exit
  int32_t newStringSema;     // Used to signal UI splash thread that a new string is ready
  int32_t drawnSema;         // Used to signal that UI splash thread has finished drawing or closed
  UILogLevelType level;      // Log level
  char neutrinoVersion[100]; // Neutrino version string
  char buf[255];             // String buffer. String must be null-terminated
} logBuffer = {};
#define THREAD_STACK_SIZE 0x1000
static uint8_t threadStack[THREAD_STACK_SIZE] __attribute__((aligned(16)));

// Initializes and starts UI splash thread
int startSplashScreen() {
  DPRINTF("Starting UI splash thread\n");
  // Initialize splash semaphores
  ee_sema_t semaphore;
  semaphore.init_count = 0;
  semaphore.max_count = 1;
  semaphore.option = 0;
  logBuffer.drawnSema = CreateSema(&semaphore);
  logBuffer.newStringSema = CreateSema(&semaphore);
  logBuffer.doneSema = CreateSema(&semaphore);

  // Initialize thread
  ee_thread_t thread;
  thread.func = uiSplashThread;
  thread.stack = threadStack;
  thread.stack_size = THREAD_STACK_SIZE;
  thread.gp_reg = &_gp;
  thread.initial_priority = 0x2;
  thread.attr = thread.option = 0;

  // Start thread
  int32_t threadID;
  if ((threadID = CreateThread(&thread)) >= 0) {
    if (StartThread(threadID, NULL) < 0) {
      DeleteThread(threadID);
      threadID = -1;
    }
  }

  return threadID;
}

// Draws loading splash screen in a separate thread
void uiSplashThread() {
  gsKit_mode_switch(gsGlobal, GS_ONESHOT);
  int logStartY = gsGlobal->Height - footerHeight - getFontLineHeight() * 3;

  // Redraw continuously so the loading crystal keeps moving between boot
  // messages. The main thread still owns every initialization and scan step.
  while (PollSema(logBuffer.doneSema) != logBuffer.doneSema) {
    if (PollSema(logBuffer.newStringSema) == logBuffer.newStringSema) {
      SignalSema(logBuffer.drawnSema);
    }

    gsKit_TexManager_nextFrame(gsGlobal);
#ifdef LUNA_GLASS_UI
    const uint32_t elapsedMs = glassElapsedMs(uiNowMs());
    // Keep the transparent logo over a visibly blue-purple field.
    const uint64_t topLeft = GS_SETREG_RGBA(0x04, 0x0B, 0x26, 0x80);
    const uint64_t topRight = GS_SETREG_RGBA(0x20, 0x10, 0x48, 0x80);
    const uint64_t bottomLeft = GS_SETREG_RGBA(0x08, 0x18, 0x40, 0x80);
    const uint64_t bottomRight = GS_SETREG_RGBA(0x34, 0x18, 0x60, 0x80);
    gsKit_prim_quad_gouraud(gsGlobal, 0, 0, gsGlobal->Width, 0, 0, gsGlobal->Height, gsGlobal->Width, gsGlobal->Height, 0,
                            topLeft, topRight, bottomLeft, bottomRight);
    drawGlassCube(gsGlobal->Width / 2, gsGlobal->Height * 57 / 100, 30,
                  glassPhase(elapsedMs, 9000, 0), 0x38, 0xA8, 0xE0, 1);
#else
    gsKit_clear(gsGlobal, BGColor);
#endif

    drawBootLogo(gsGlobal->Width / 2, keepoutArea + 8, gsGlobal->Width * 41 / 100, 2);

    // Successful boot stays intentionally minimal. Only surface a fatal error
    // so a failed initialization cannot be mistaken for endless loading.
    if ((logBuffer.level == LEVEL_ERROR) && (logBuffer.buf[0] != '\0'))
      drawTextWindow(0, logStartY, gsGlobal->Width, gsGlobal->Height - footerHeight, 2, ErrorTextColor, ALIGN_CENTER, logBuffer.buf);

    gsKit_queue_exec(gsGlobal);
    gsKit_finish();
    gsKit_sync_flip(gsGlobal);
    usleep(16000);
  }
  gsKit_queue_reset(gsGlobal->Per_Queue);
  DeleteSema(logBuffer.doneSema);
  DeleteSema(logBuffer.newStringSema);
  SignalSema(logBuffer.drawnSema);
  ExitDeleteThread();
}

// Stops UI splash thread
void stopUISplashThread() {
  SignalSema(logBuffer.doneSema);
  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);
  DeleteSema(logBuffer.drawnSema);
}

// Logs to splash screen and debug console in a thread-safe way
void uiSplashLogString(UILogLevelType level, const char *str, ...) {
  va_list args;
  va_start(args, str);

  logBuffer.level = level;
  vsnprintf(logBuffer.buf, 255, str, args);
  va_end(args);
  DPRINTF(logBuffer.buf);

  if (!gsGlobal)
    return;

  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);

  switch (level) {
  case LEVEL_INFO_NODELAY:
    return;
  case LEVEL_INFO:
    sleep(1);
    return;
  case LEVEL_WARN:
  case LEVEL_ERROR:
    sleep(2);
    return;
  }
}

// Sets Neutrino version on the splash screen
void uiSplashSetNeutrinoVersion(const char *str) {
  if (!gsGlobal)
    return;

  if (str[0] == '\0')
    return;

  strcpy(logBuffer.neutrinoVersion, "Neutrino");
  strncat(logBuffer.neutrinoVersion, str, 100 - 10);

  SignalSema(logBuffer.newStringSema);
  WaitSema(logBuffer.drawnSema);
}
