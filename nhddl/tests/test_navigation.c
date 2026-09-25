// Original LUNA code: Danny Nunez (dnunezx) 2026
#include "ui/navigation.h"
#include <assert.h>
#include <stdio.h>

static void testWrapping(void) {
  assert(lunaNavWrap(5, -1) == 4);
  assert(lunaNavWrap(5, 5) == 0);
  assert(lunaNavWrap(5, 12) == 2);
  assert(lunaNavWrap(0, 1) == -1);
  assert(lunaNavDirection(10, 9, 0) == 1);
  assert(lunaNavDirection(10, 0, 9) == -1);
  assert(lunaNavDirection(10, 3, 3) == 0);
}

static void testGridNavigation(void) {
  assert(lunaNavGridVertical(18, 1, -1) == 17);
  assert(lunaNavGridVertical(18, 17, 1) == 1);
  assert(lunaNavGridVertical(18, 3, 1) == 7);
  assert(lunaNavGridPage(34, 2, 1) == 18);
  assert(lunaNavGridPage(34, 18, 1) == 33);
  assert(lunaNavGridPage(34, 33, 1) == 1);
  assert(lunaNavPageBase(34, 32, 1) == 0);
  assert(lunaNavPageBase(34, 0, -1) == 32);
}

static void testBufferSelection(void) {
  const int pages[GRID_PAGE_BUFFERS] = {0, 16, -1};
  assert(lunaNavFindBuffer(pages, GRID_PAGE_BUFFERS, 16) == 1);
  assert(lunaNavFindBuffer(pages, GRID_PAGE_BUFFERS, 32) == -1);
  assert(lunaNavChooseBuffer(pages, GRID_PAGE_BUFFERS, 0, 1, -1) == 2);
  assert(lunaNavChooseBuffer(pages, GRID_PAGE_BUFFERS, 0, 1, 2) == -1);
}

static void testTiming(void) {
  LunaNavRepeatState repeat = {0};
  assert(lunaNavRepeatStep(&repeat, 1, 100, 260, 105) == 1);
  assert(lunaNavRepeatStep(&repeat, 1, 359, 260, 105) == 0);
  assert(lunaNavRepeatStep(&repeat, 1, 360, 260, 105) == 1);
  assert(lunaNavRepeatStep(&repeat, 1, 900, 260, 105) == 1);
  assert(lunaNavRepeatStep(&repeat, 1, 901, 260, 105) == 0);
  assert(lunaNavRepeatStep(&repeat, -1, 902, 260, 105) == 1);
  assert(lunaNavRepeatStep(&repeat, 0, 903, 260, 105) == 0);
  assert(lunaNavRepeatStep(&repeat, -1, 904, 260, 105) == 1);
  assert(lunaNavEase(0) == 0);
  assert(lunaNavEase(500) == 500);
  assert(lunaNavEase(1000) == 1000);
  assert(lunaNavAnimatedOffset(1000, 100, 420, 100) == 1000);
  assert(lunaNavAnimatedOffset(1000, 100, 420, 310) == 500);
  assert(lunaNavAnimatedOffset(1000, 100, 420, 520) == 0);
  assert(lunaNavGridCascadeProgress(89, 1, 0) == 0);
  assert(lunaNavGridCascadeProgress(415, 1, 0) == 500);
  assert(lunaNavGridCascadeProgress(1000, 3, 1) == 1000);
}

static void testRouting(void) {
  int selected;
  for (selected = 0; selected < 17; selected++) {
    uint32_t seed;
    for (seed = 0; seed < 64; seed++) {
      int target = lunaNavRandomTarget(17, selected, seed);
      assert(target >= 0 && target < 17);
      assert(target != selected);
    }
  }
  assert(lunaNavNextView(UI_VIEW_CLASSIC) == UI_VIEW_PSBBN);
  assert(lunaNavNextView(UI_VIEW_PSBBN) == UI_VIEW_GRID);
  assert(lunaNavNextView(UI_VIEW_GRID) == UI_VIEW_ORBIT);
  assert(lunaNavNextView(UI_VIEW_ORBIT) == UI_VIEW_ORBS);
  assert(lunaNavNextView(UI_VIEW_ORBS) == UI_VIEW_CLASSIC);
}

static void testMarkedNavigation(void) {
  const uint8_t marked[] = {0, 1, 0, 1, 1, 0, 0, 1};
  assert(lunaNavMarkedCount(marked, 8) == 4);
  assert(lunaNavMarkedRank(marked, 8, 1) == 0);
  assert(lunaNavMarkedRank(marked, 8, 4) == 2);
  assert(lunaNavMarkedRank(marked, 8, 2) == -1);
  assert(lunaNavMarkedByRank(marked, 8, 3) == 7);
  assert(lunaNavMarkedByRank(marked, 8, 4) == 1);
  assert(lunaNavMarkedStep(marked, 8, 1, -1) == 7);
  assert(lunaNavMarkedStep(marked, 8, 7, 1) == 1);
  assert(lunaNavMarkedPage(marked, 8, 1, 2, 1) == 4);
  assert(lunaNavMarkedPage(marked, 8, 4, 2, 1) == 7);
  assert(lunaNavMarkedPage(marked, 8, 7, 2, 1) == 1);
  assert(lunaNavMarkedPage(marked, 8, 1, 2, -1) == 7);
}

static float absolute(float value) { return value < 0 ? -value : value; }

static void collectionTicks(LunaCollectionMotion *s, int total, int direction,
                             int scanHeld, int duration, int frameMs) {
  while (duration > 0) {
    int step = duration < frameMs ? duration : frameMs;
    lunaCollectionUpdate(s, total, direction, scanHeld, s->lastMs + step);
    duration -= step;
  }
}

static void testCollectionTaps(void) {
  LunaCollectionMotion s;
  lunaCollectionReset(&s, 4, 0);
  lunaCollectionUpdate(&s, 30, 1, 0, 0);
  collectionTicks(&s, 30, 1, 0, 20, 10);
  collectionTicks(&s, 30, 0, 0, 400, 10);
  assert(s.focus == 5 && s.position == 0 && s.mode == COLLECTION_IDLE);
  // Three quick taps are three steps, even before the first glide completes.
  for (int i = 0; i < 3; i++) {
    collectionTicks(&s, 30, 1, 0, 20, 10);
    collectionTicks(&s, 30, 0, 0, 40, 10);
  }
  collectionTicks(&s, 30, 0, 0, 400, 10);
  assert(s.focus == 8 && s.position == 0);
  lunaCollectionReset(&s, 0, 0);
  collectionTicks(&s, 30, -1, 0, 20, 10);
  collectionTicks(&s, 30, 0, 0, 400, 10);
  assert(s.focus == 29 && s.position == 0);
}

static void testCollectionHoldAndRelease(void) {
  LunaCollectionMotion s;
  lunaCollectionReset(&s, 5, 0);
  collectionTicks(&s, 100, 1, 0, 1500, 16);
  assert(s.mode == COLLECTION_BROWSE && absolute(s.velocity - 6) < 0.01f);
  for (int i = 0; i < 50; i++) {
    float previous = s.velocity;
    collectionTicks(&s, 100, 1, 0, 16, 16);
    assert(absolute(previous - s.velocity) < 0.01f);
    assert(absolute(s.position) <= 0.501f);
    assert(absolute(s.target) <= 0.501f);
  }
  float before = s.focus + s.position;
  collectionTicks(&s, 100, 0, 0, 240, 16);
  assert(s.mode == COLLECTION_IDLE && s.position == 0);
  assert(s.focus >= before && s.focus - before <= 1.01f);
  // A live reversal brakes continuously rather than resetting the velocity.
  collectionTicks(&s, 100, 1, 0, 1500, 16);
  float previous = s.velocity;
  collectionTicks(&s, 100, -1, 0, 16, 16);
  assert(s.velocity > 0 && previous - s.velocity < 0.97f);
  collectionTicks(&s, 100, -1, 0, 1000, 16);
  assert(s.velocity < -5.9f);
  lunaCollectionBrake(&s);
  collectionTicks(&s, 100, 0, 0, 240, 16);
  assert(s.mode == COLLECTION_IDLE);
}

static void testCollectionScan(void) {
  LunaCollectionMotion s;
  lunaCollectionReset(&s, 4, 0);
  collectionTicks(&s, 100, 1, 1, 20, 10);
  assert(s.focus == 4 && s.position == 0 && s.mode == COLLECTION_IDLE);
  collectionTicks(&s, 100, 0, 0, 400, 10);
  assert(s.focus == 4 && s.position == 0); // A tap does nothing.
  collectionTicks(&s, 100, 1, 1, 440, 10);
  assert(s.focus == 4 && s.position == 0 && s.mode == COLLECTION_IDLE);
  collectionTicks(&s, 100, 1, 1, 20, 10);
  assert(s.mode == COLLECTION_SCAN && s.focus == 4);
  collectionTicks(&s, 100, 1, 1, 2000, 16);
  assert(s.mode == COLLECTION_SCAN && absolute(s.velocity - 10) < 0.01f);
  assert(s.scanLabelMs == COLLECTION_SCAN_LABEL_MS);
  float before = s.focus + s.position;
  collectionTicks(&s, 100, 0, 0, 180, 10);
  assert(s.mode == COLLECTION_IDLE && s.focus - before <= 1.01f && s.focus >= before);
  assert(s.scanLabelMs > 0);
  collectionTicks(&s, 100, 0, 0, 500, 10);
  assert(s.scanLabelMs == 0);

  lunaCollectionReset(&s, 50, 0);
  collectionTicks(&s, 100, -1, 1, 2000, 16);
  assert(s.mode == COLLECTION_SCAN && absolute(s.velocity + 10) < 0.01f);
  before = s.focus + s.position;
  collectionTicks(&s, 100, 1, 1, 16, 16);
  assert(s.mode == COLLECTION_SCAN && s.velocity < 0 && s.velocity > -10);
  assert(before - (s.focus + s.position) < 0.2f); // Reversal does not jump.
}

static void testCollectionTimingAndSmallLists(void) {
  LunaCollectionMotion a, b;
  lunaCollectionReset(&a, 5, 0);
  lunaCollectionReset(&b, 5, 0);
  collectionTicks(&a, 100, 1, 0, 3000, 16);
  collectionTicks(&b, 100, 1, 0, 3000, 20);
  assert(absolute((a.focus + a.position) - (b.focus + b.position)) < 0.03f);
  float before = a.focus + a.position;
  lunaCollectionUpdate(&a, 100, 1, 0, a.lastMs + 5000);
  assert(a.focus + a.position - before < 0.21f);
  lunaCollectionReset(&a, 0, UINT32_MAX - 20);
  collectionTicks(&a, 100, 1, 0, 20, 10);
  collectionTicks(&a, 100, 0, 0, 400, 10);
  assert(a.focus == 1 && a.mode == COLLECTION_IDLE);
  for (int total = 0; total <= 4; total++) {
    lunaCollectionReset(&a, 0, 0);
    collectionTicks(&a, total, 1, 1, 4000, 16);
    assert(a.focus == -1 || (a.focus >= 0 && a.focus < total));
    assert(absolute(a.velocity) <= 2.01f);
    if (total <= 1) assert(a.mode == COLLECTION_IDLE);
  }
  // Long held navigation keeps local coordinates bounded across many wraps.
  lunaCollectionReset(&a, 0, 0);
  collectionTicks(&a, 7, -1, 1, 600000, 20);
  assert(a.focus >= 0 && a.focus < 7);
  assert(absolute(a.position) <= 0.501f && absolute(a.target) <= 0.501f);
}

static void testCollectionCacheLayout(void) {
  int targets[PSBBN_COVER_CACHE_COUNT];
  for (int total = 0; total <= 14; total++) {
    for (int focus = 0; focus < (total ? total : 1); focus++) {
      for (int offset = -500; offset <= 500; offset += 100) {
        lunaCollectionCacheLayout(total, focus, offset, targets);
        int count = 0;
        for (int i = 0; i < PSBBN_COVER_CACHE_COUNT; i++) {
          if (targets[i] < 0) continue;
          count++;
          assert(targets[i] < total);
          for (int j = 0; j < i; j++) assert(targets[i] != targets[j]);
        }
        assert(count == (total < PSBBN_COVER_CACHE_COUNT ? total : PSBBN_COVER_CACHE_COUNT));
        if (total) assert(targets[PSBBN_COVER_CACHE_FOCUS] == focus);
      }
    }
  }
}

static void testCollectionBrakeBoundaries(void) {
  for (int direction = -1; direction <= 1; direction += 2) {
    for (int offset = -499; offset <= 499; offset++) {
      LunaCollectionMotion s;
      lunaCollectionReset(&s, 30, 0);
      s.position = offset / 1000.0f;
      s.velocity = direction * 10.0f;
      s.mode = COLLECTION_SCAN;
      float before = s.focus + s.position;
      lunaCollectionBrake(&s);
      collectionTicks(&s, 100, 0, 0, 200, 10);
      assert(s.mode == COLLECTION_IDLE && s.position == 0);
      assert((s.focus - before) * direction >= 0);
      assert(absolute(s.focus - before) <= 1.001f);
    }
  }
}

int main(void) {
  testWrapping();
  testGridNavigation();
  testBufferSelection();
  testTiming();
  testRouting();
  testMarkedNavigation();
  testCollectionTaps();
  testCollectionHoldAndRelease();
  testCollectionScan();
  testCollectionTimingAndSmallLists();
  testCollectionCacheLayout();
  testCollectionBrakeBoundaries();
  puts("navigation tests passed");
  return 0;
}
