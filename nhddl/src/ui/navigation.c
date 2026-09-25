// Original LUNA code: Danny Nunez (dnunezx) 2026
#include "ui/navigation.h"
#include <stddef.h>
#include <string.h>

static float collectionAbs(float value) { return value < 0 ? -value : value; }

static int collectionRound(float value) {
  return (int)(value + (value < 0 ? -0.5f : 0.5f));
}

void lunaCollectionReset(LunaCollectionMotion *s, int focus, uint32_t now) {
  memset(s, 0, sizeof(*s));
  s->initialized = 1;
  s->focus = focus;
  s->lastMs = now;
}

static void collectionAnimate(LunaCollectionMotion *s, float target,
                               LunaCollectionMode mode, uint32_t duration) {
  float distance = target - s->position;
  float limit = collectionAbs(distance) * 3000.0f / duration;
  s->startPosition = s->position;
  s->target = target;
  s->startVelocity = s->velocity;
  // Monotone Hermite endpoints prevent a release from overshooting its title.
  if (s->startVelocity * distance < 0) s->startVelocity = 0;
  if (collectionAbs(s->startVelocity) > limit)
    s->startVelocity = distance < 0 ? -limit : limit;
  s->motionMs = 0;
  s->durationMs = duration;
  s->mode = mode;
}

void lunaCollectionBrake(LunaCollectionMotion *s) {
  int landing = collectionRound(s->position);
  uint32_t duration = COLLECTION_SETTLE_MS;
  // Use the next center in the current direction: never more than one cover
  // of drift, even when releasing near a boundary at maximum scan speed.
  if (s->velocity > 0) landing = s->position < 0 ? 0 : 1;
  if (s->velocity < 0) landing = s->position > 0 ? 0 : -1;
  float speed = collectionAbs(s->velocity);
  if (speed > 0) {
    uint32_t monotoneDuration = (uint32_t)(collectionAbs(landing - s->position) * 3000.0f / speed);
    if (duration > monotoneDuration) duration = monotoneDuration;
    if (duration == 0) duration = 1;
  }
  collectionAnimate(s, (float)landing, COLLECTION_SETTLE, duration);
}

void lunaCollectionUpdate(LunaCollectionMotion *s, int total, int direction,
                          int scanHeld, uint32_t now) {
  uint32_t elapsed = now - s->lastMs;
  s->lastMs = now;
  if (elapsed > 32) elapsed = 32; // Discard stall time, never fast-forward art.
  if (total <= 1) {
    lunaCollectionReset(s, total ? 0 : -1, now);
    return;
  }
  if (direction != s->direction || scanHeld != s->scanHeld) {
    int previousDirection = s->direction;
    int previousScanHeld = s->scanHeld;
    s->direction = direction;
    s->scanHeld = scanHeld;
    s->heldMs = 0;
    if (direction) {
      s->travelDirection = direction;
      if (scanHeld) {
        // L2/R2 taps are inert. A held scan starts only after the threshold,
        // without first jumping over any titles.
        if (s->mode == COLLECTION_SCAN && previousScanHeld)
          s->heldMs = COLLECTION_SCAN_HOLD_MS; // Keep scanning on reversal.
        else if (s->mode == COLLECTION_BROWSE)
          lunaCollectionBrake(s);
      } else if (s->mode == COLLECTION_BROWSE || s->mode == COLLECTION_SCAN) {
        // Preserve velocity through a live reversal; acceleration brakes it.
        s->mode = COLLECTION_BROWSE;
        s->heldMs = COLLECTION_HOLD_MS;
      } else {
        float destination = (s->mode == COLLECTION_STEP &&
                             (!previousDirection || previousDirection == direction))
                                ? s->target + direction : (float)direction;
        collectionAnimate(s, destination, COLLECTION_STEP, COLLECTION_STEP_MS);
      }
    } else if (s->mode == COLLECTION_BROWSE || s->mode == COLLECTION_SCAN) {
      lunaCollectionBrake(s);
    }
  }
  // Integrate in small steps so 50 Hz and 60 Hz follow the same trajectory.
  while (elapsed) {
    uint32_t step = elapsed > 4 ? 4 : elapsed;
    float dt = step / 1000.0f;
    elapsed -= step;
    if (direction && s->heldMs < 10000) s->heldMs += step;
    if (direction && scanHeld && s->heldMs >= COLLECTION_SCAN_HOLD_MS)
      s->mode = COLLECTION_SCAN;
    else if (direction && !scanHeld && s->heldMs >= COLLECTION_HOLD_MS)
      s->mode = COLLECTION_BROWSE;

    if (s->mode == COLLECTION_SCAN || s->mode == COLLECTION_BROWSE) {
      float speed = s->mode == COLLECTION_SCAN ? 10.0f : 6.0f;
      float smallLimit = total <= 4 ? 2.0f : total * 0.8f;
      if (speed > smallLimit) speed = smallLimit;
      float desired = direction * speed;
      float acceleration = (s->velocity * direction < 0 ? 60.0f : 16.0f) * dt;
      float previousVelocity = s->velocity;
      if (s->velocity < desired) {
        s->velocity += acceleration;
        if (s->velocity > desired) s->velocity = desired;
      } else {
        s->velocity -= acceleration;
        if (s->velocity < desired) s->velocity = desired;
      }
      s->position += (previousVelocity + s->velocity) * 0.5f * dt;
      s->target = s->startPosition = s->position;
    } else if (s->mode != COLLECTION_IDLE) {
      s->motionMs += step;
      if (s->motionMs >= s->durationMs) {
        s->position = s->target;
        s->velocity = 0;
        s->mode = COLLECTION_IDLE;
      } else {
        float t = (float)s->motionMs / s->durationMs;
        float t2 = t * t, t3 = t2 * t;
        float tangent = s->startVelocity * s->durationMs / 1000.0f;
        s->position = (2*t3 - 3*t2 + 1)*s->startPosition +
                      (t3 - 2*t2 + t)*tangent + (-2*t3 + 3*t2)*s->target;
        s->velocity = ((6*t2 - 6*t)*s->startPosition +
                       (3*t2 - 4*t + 1)*tangent + (-6*t2 + 6*t)*s->target) *
                      1000.0f / s->durationMs;
      }
    }
    int shift = collectionRound(s->position);
    if (shift) {
      s->focus = lunaNavWrap(total, s->focus + shift);
      s->position -= shift;
      s->target -= shift;
      s->startPosition -= shift;
    }
    float blend = (collectionAbs(s->velocity) - 3.0f) / 7.0f;
    if (blend < 0) blend = 0;
    if (blend > 1) blend = 1;
    s->speedBlend += (blend - s->speedBlend) * dt * 12.0f;
    if (s->mode == COLLECTION_SCAN) s->scanLabelMs = COLLECTION_SCAN_LABEL_MS;
    else if (s->mode == COLLECTION_IDLE)
      s->scanLabelMs = s->scanLabelMs > step ? s->scanLabelMs - step : 0;
  }
}

int lunaCollectionOffset(const LunaCollectionMotion *s) {
  return (int)(-s->position * 1000.0f);
}

void lunaCollectionCacheLayout(int total, int focus, int offset, int *targets) {
  for (int i = 0; i < PSBBN_COVER_CACHE_COUNT; i++) {
    targets[i] = lunaNavWrap(total, focus + i - PSBBN_COVER_CACHE_FOCUS);
    for (int j = 0; j < i; j++) {
      if (targets[i] < 0 || targets[j] != targets[i]) continue;
      int a = (i - PSBBN_COVER_CACHE_FOCUS)*1000 + offset;
      int b = (j - PSBBN_COVER_CACHE_FOCUS)*1000 + offset;
      if (collectionAbs((float)a) < collectionAbs((float)b) ||
          (collectionAbs((float)a) == collectionAbs((float)b) && i == PSBBN_COVER_CACHE_FOCUS)) targets[j] = -1;
      else targets[i] = -1;
    }
  }
}

int lunaNavWrap(int total, int index) {
  if (total <= 0)
    return -1;
  index %= total;
  return (index < 0) ? index + total : index;
}

int lunaNavRepeatStep(LunaNavRepeatState *state, int direction, uint32_t now,
                      uint32_t initialDelayMs, uint32_t intervalMs) {
  if (direction == 0) {
    state->direction = 0;
    return 0;
  }
  if (direction != state->direction) {
    state->direction = direction;
    state->nextStepMs = now + initialDelayMs;
    return 1;
  }
  if ((int32_t)(now - state->nextStepMs) < 0)
    return 0;
  // Do not queue missed repeats after a slow artwork decode or frame.
  state->nextStepMs = now + intervalMs;
  return 1;
}

int lunaNavGridVertical(int total, int index, int direction) {
  int candidate;
  int column;

  if (total <= 0 || index < 0 || index >= total || direction == 0)
    return index;
  candidate = index + direction * GRID_COLUMNS;
  column = index % GRID_COLUMNS;
  if (candidate >= 0 && candidate < total)
    return candidate;
  if (direction < 0) {
    int last = total - 1;
    candidate = last - ((last - column) % GRID_COLUMNS);
    return (candidate >= 0) ? candidate : index;
  }
  return (column < total) ? column : total - 1;
}

int lunaNavGridPage(int total, int index, int direction) {
  int pageCount;
  int page;
  int cell;
  int targetPage;
  int candidate;

  if (total <= 0 || index < 0 || index >= total)
    return index;
  pageCount = (total + GRID_PAGE_SIZE - 1) / GRID_PAGE_SIZE;
  page = index / GRID_PAGE_SIZE;
  cell = index % GRID_PAGE_SIZE;
  targetPage = lunaNavWrap(pageCount, page + direction);
  candidate = targetPage * GRID_PAGE_SIZE + cell;
  return (candidate < total) ? candidate : total - 1;
}

int lunaNavPageBase(int total, int pageBase, int direction) {
  int pageCount;
  int page;

  if (total <= 0)
    return -1;
  pageCount = (total + GRID_PAGE_SIZE - 1) / GRID_PAGE_SIZE;
  page = pageBase / GRID_PAGE_SIZE;
  return lunaNavWrap(pageCount, page + direction) * GRID_PAGE_SIZE;
}

int lunaNavFindBuffer(const int *pageBases, int bufferCount, int pageBase) {
  int buffer;
  for (buffer = 0; buffer < bufferCount; buffer++) {
    if (pageBases[buffer] == pageBase)
      return buffer;
  }
  return -1;
}

int lunaNavChooseBuffer(const int *pageBases, int bufferCount, int activeBuffer,
                        int previousBuffer, int incomingBuffer) {
  int buffer;
  for (buffer = 0; buffer < bufferCount; buffer++) {
    if (buffer != activeBuffer && buffer != previousBuffer &&
        buffer != incomingBuffer && pageBases[buffer] < 0)
      return buffer;
  }
  for (buffer = 0; buffer < bufferCount; buffer++) {
    if (buffer != activeBuffer && buffer != previousBuffer && buffer != incomingBuffer)
      return buffer;
  }
  return -1;
}

int lunaNavDirection(int total, int fromIdx, int toIdx) {
  int forwardDistance;
  int backwardDistance;

  if (total <= 0 || fromIdx < 0 || fromIdx == toIdx)
    return 0;
  forwardDistance = lunaNavWrap(total, toIdx - fromIdx);
  backwardDistance = lunaNavWrap(total, fromIdx - toIdx);
  return (forwardDistance <= backwardDistance) ? 1 : -1;
}

int lunaNavEase(int progress) {
  int inverse = 1000 - progress;
  return 1000 - (int)(((int64_t)inverse * inverse * (3000 - (2 * inverse))) /
                      1000000LL);
}

int lunaNavAnimatedOffset(int startOffset, uint32_t startTime, uint32_t duration,
                          uint32_t now) {
  uint32_t elapsed = now - startTime;
  int progress = (duration == 0 || elapsed >= duration)
                     ? 1000
                     : (int)((elapsed * 1000ULL) / duration);
  return (startOffset * (1000 - lunaNavEase(progress))) / 1000;
}

int lunaNavGridCascadeProgress(int progress, int row, int incoming) {
  int start = row * GRID_CASCADE_ROW_STAGGER +
              (incoming ? GRID_CASCADE_FOLLOW_DELAY : 0);
  int rowProgress;
  if (progress <= start)
    return 0;
  rowProgress = ((progress - start) * 1000) / GRID_CASCADE_ROW_DURATION;
  return (rowProgress > 1000) ? 1000 : rowProgress;
}

int lunaNavRandomTarget(int total, int selectedIndex, uint32_t randomSeed) {
  int jump;
  if (total <= 1)
    return selectedIndex;
  jump = 1 + (int)(randomSeed % (uint32_t)(total - 1));
  return lunaNavWrap(total, selectedIndex + jump);
}

int lunaNavMarkedCount(const uint8_t *marked, int total) {
  int count = 0;
  int index;
  if (marked == NULL || total <= 0)
    return 0;
  for (index = 0; index < total; index++)
    count += marked[index] != 0;
  return count;
}

int lunaNavMarkedRank(const uint8_t *marked, int total, int index) {
  int rank = 0;
  int current;
  if (marked == NULL || index < 0 || index >= total || !marked[index])
    return -1;
  for (current = 0; current < index; current++)
    rank += marked[current] != 0;
  return rank;
}

int lunaNavMarkedByRank(const uint8_t *marked, int total, int rank) {
  int count = lunaNavMarkedCount(marked, total);
  int current;
  if (count <= 0)
    return -1;
  rank = lunaNavWrap(count, rank);
  for (current = 0; current < total; current++) {
    if (marked[current] && rank-- == 0)
      return current;
  }
  return -1;
}

int lunaNavMarkedStep(const uint8_t *marked, int total, int index, int direction) {
  int rank = lunaNavMarkedRank(marked, total, index);
  if (rank < 0)
    return lunaNavMarkedByRank(marked, total, (direction < 0) ? -1 : 0);
  return lunaNavMarkedByRank(marked, total, rank + ((direction < 0) ? -1 : 1));
}

int lunaNavMarkedPage(const uint8_t *marked, int total, int index, int pageSize,
                      int direction) {
  int count = lunaNavMarkedCount(marked, total);
  int rank = lunaNavMarkedRank(marked, total, index);
  if (count <= 0 || pageSize <= 0)
    return -1;
  if (rank < 0)
    return lunaNavMarkedByRank(marked, total, (direction < 0) ? count - 1 : 0);
  if (direction > 0)
    rank = (rank == count - 1) ? 0 : ((rank + pageSize < count) ? rank + pageSize : count - 1);
  else
    rank = (rank == 0) ? count - 1 : ((rank - pageSize > 0) ? rank - pageSize : 0);
  return lunaNavMarkedByRank(marked, total, rank);
}

UILibraryView lunaNavNextView(UILibraryView view) {
  return (UILibraryView)((view + 1) % (UI_VIEW_ORBS + 1));
}
