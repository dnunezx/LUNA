// Original LUNA code: Danny Nunez (dnunezx) 2026
#ifndef LUNA_UI_NAVIGATION_H
#define LUNA_UI_NAVIGATION_H

#include <stdint.h>

#define PSBBN_COVER_CACHE_COUNT 10
#define PSBBN_COVER_CACHE_FOCUS 3
#define PSBBN_ANIMATION_DURATION_MS 420
#define PSBBN_REPEAT_FRAMES_NTSC 11
#define PSBBN_REPEAT_FRAMES_PAL 9
#define GRID_COLUMNS 4
#define GRID_ROWS 4
#define GRID_PAGE_SIZE (GRID_COLUMNS * GRID_ROWS)
#define GRID_PAGE_BUFFERS 3
#define GRID_SELECTED_BUFFERS 2
#define GRID_CASCADE_DURATION_MS 340
#define GRID_CASCADE_ROW_STAGGER 90
#define GRID_CASCADE_FOLLOW_DELAY 80
#define GRID_CASCADE_ROW_DURATION 650
#define GRID_FAST_TRACK_HOLD_MS 500
#define GRID_FAST_TRACK_STEP_MS 140
#define ORBIT_RANDOM_STEP_MS 85
#define CLASSIC_REPEAT_DELAY_MS 260
#define CLASSIC_REPEAT_INTERVAL_MS 105
#define CLASSIC_ART_SETTLE_MS 90
#define CLASSIC_COVER_FADE_DURATION_MS 180

#define COLLECTION_STEP_MS 300
#define COLLECTION_HOLD_MS 280
#define COLLECTION_SCAN_HOLD_MS 450
#define COLLECTION_SETTLE_MS 160
#define COLLECTION_SCAN_LABEL_MS 400

typedef enum {
  COLLECTION_IDLE, COLLECTION_STEP, COLLECTION_BROWSE,
  COLLECTION_SCAN, COLLECTION_SETTLE
} LunaCollectionMode;

typedef struct {
  int initialized, focus, direction, scanHeld, travelDirection;
  float position, velocity, target, startPosition, startVelocity, speedBlend;
  uint32_t lastMs, heldMs, motionMs, durationMs, scanLabelMs;
  LunaCollectionMode mode;
} LunaCollectionMotion;

void lunaCollectionReset(LunaCollectionMotion *state, int focus, uint32_t now);
void lunaCollectionUpdate(LunaCollectionMotion *state, int total, int direction,
                          int scanHeld, uint32_t now);
void lunaCollectionBrake(LunaCollectionMotion *state);
int lunaCollectionOffset(const LunaCollectionMotion *state);
void lunaCollectionCacheLayout(int total, int focus, int offset, int *targets);

typedef struct {
  int direction;
  uint32_t nextStepMs;
} LunaNavRepeatState;

typedef enum {
  UI_VIEW_CLASSIC = 0,
  UI_VIEW_PSBBN = 1,
  UI_VIEW_GRID = 2,
  UI_VIEW_ORBIT = 3,
  UI_VIEW_ORBS = 4,
} UILibraryView;

int lunaNavWrap(int total, int index);
int lunaNavRepeatStep(LunaNavRepeatState *state, int direction, uint32_t now,
                      uint32_t initialDelayMs, uint32_t intervalMs);
int lunaNavGridVertical(int total, int index, int direction);
int lunaNavGridPage(int total, int index, int direction);
int lunaNavPageBase(int total, int pageBase, int direction);
int lunaNavFindBuffer(const int *pageBases, int bufferCount, int pageBase);
int lunaNavChooseBuffer(const int *pageBases, int bufferCount, int activeBuffer,
                        int previousBuffer, int incomingBuffer);
int lunaNavDirection(int total, int fromIdx, int toIdx);
int lunaNavEase(int progress);
int lunaNavAnimatedOffset(int startOffset, uint32_t startTime, uint32_t duration,
                          uint32_t now);
int lunaNavGridCascadeProgress(int progress, int row, int incoming);
int lunaNavRandomTarget(int total, int selectedIndex, uint32_t randomSeed);
int lunaNavMarkedCount(const uint8_t *marked, int total);
int lunaNavMarkedRank(const uint8_t *marked, int total, int index);
int lunaNavMarkedByRank(const uint8_t *marked, int total, int rank);
int lunaNavMarkedStep(const uint8_t *marked, int total, int index, int direction);
int lunaNavMarkedPage(const uint8_t *marked, int total, int index, int pageSize,
                      int direction);
UILibraryView lunaNavNextView(UILibraryView view);

#endif
