#include "ui/ambient.h"
#include "common.h"
#include "dprintf.h"
#include <audsrv.h>
#include <delaythread.h>
#include <kernel.h>
#include <loadfile.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AMBIENT_BLOCK_BYTES 1024
#define AMBIENT_BLOCK_FRAMES 1017
#define AMBIENT_FEED_BYTES 1024
#define AMBIENT_CACHE_LIMIT (2 * 1024 * 1024)
#define AMBIENT_VOLUME 100

extern unsigned char audsrv_irx[] __attribute__((aligned(16)));
extern uint32_t size_audsrv_irx;
extern void *_gp;

static const int imaIndexChange[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static const int imaStep[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371,
    408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
    1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

typedef struct {
  int predictor;
  int index;
} ImaChannel;

static FILE *ambientFile;
static uint8_t *ambientData;
static long dataOffset;
static uint32_t dataBytes;
static uint32_t totalFrames;
static volatile int enabled;
static volatile int stopping;
static int threadId = -1;
static int controlSema = -1;
static int doneSema = -1;
static int audioInitialized;
static uint8_t threadStack[8192] __attribute__((aligned(16)));
static uint8_t encodedBlock[AMBIENT_BLOCK_BYTES] __attribute__((aligned(16)));
static int16_t decodedBlock[AMBIENT_BLOCK_FRAMES * 2] __attribute__((aligned(16)));

static uint16_t read16(const uint8_t *bytes) {
  return (uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8);
}

static uint32_t read32(const uint8_t *bytes) {
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static int inspectWave(FILE *file) {
  uint8_t header[20];
  int validFormat = 0;
  int hasData = 0;
  totalFrames = 0;
  dataBytes = 0;
  if (fread(header, 1, 12, file) != 12 || memcmp(header, "RIFF", 4) ||
      memcmp(header + 8, "WAVE", 4))
    return -1;

  while (fread(header, 1, 8, file) == 8) {
    uint32_t chunkSize = read32(header + 4);
    long next = ftell(file) + (long)chunkSize + (chunkSize & 1);
    if (!memcmp(header, "fmt ", 4) && chunkSize >= 20) {
      if (fread(header, 1, 20, file) != 20)
        return -1;
      validFormat = read16(header) == 0x11 && read16(header + 2) == 2 &&
                    read32(header + 4) == 22050 &&
                    read16(header + 12) == AMBIENT_BLOCK_BYTES &&
                    read16(header + 14) == 4 &&
                    read16(header + 18) == AMBIENT_BLOCK_FRAMES;
    } else if (!memcmp(header, "fact", 4) && chunkSize >= 4) {
      if (fread(header, 1, 4, file) != 4)
        return -1;
      totalFrames = read32(header);
    } else if (!memcmp(header, "data", 4)) {
      dataOffset = ftell(file);
      dataBytes = chunkSize;
      hasData = chunkSize >= AMBIENT_BLOCK_BYTES &&
                chunkSize % AMBIENT_BLOCK_BYTES == 0;
      break;
    }
    if (fseek(file, next, SEEK_SET))
      return -1;
  }
  return validFormat && hasData && totalFrames > 0 &&
                 totalFrames <=
                     (dataBytes / AMBIENT_BLOCK_BYTES) * AMBIENT_BLOCK_FRAMES
             ? 0
             : -1;
}

static int16_t decodeNibble(ImaChannel *channel, int nibble) {
  int step = imaStep[channel->index];
  int difference = step >> 3;
  if (nibble & 1) difference += step >> 2;
  if (nibble & 2) difference += step >> 1;
  if (nibble & 4) difference += step;
  channel->predictor += (nibble & 8) ? -difference : difference;
  if (channel->predictor > 32767) channel->predictor = 32767;
  if (channel->predictor < -32768) channel->predictor = -32768;
  channel->index += imaIndexChange[nibble];
  if (channel->index < 0) channel->index = 0;
  if (channel->index > 88) channel->index = 88;
  return (int16_t)channel->predictor;
}

static int decodeBlock(void) {
  ImaChannel channels[2];
  for (int channel = 0; channel < 2; channel++) {
    int offset = channel * 4;
    channels[channel].predictor = (int16_t)read16(encodedBlock + offset);
    channels[channel].index = encodedBlock[offset + 2];
    if (channels[channel].index > 88)
      return -1;
    decodedBlock[channel] = (int16_t)channels[channel].predictor;
  }
  for (int group = 0; group < 127; group++) {
    for (int channel = 0; channel < 2; channel++) {
      int offset = 8 + group * 8 + channel * 4;
      for (int byte = 0; byte < 4; byte++) {
        int sample = 1 + group * 8 + byte * 2;
        uint8_t packed = encodedBlock[offset + byte];
        decodedBlock[sample * 2 + channel] =
            decodeNibble(&channels[channel], packed & 0x0f);
        decodedBlock[(sample + 1) * 2 + channel] =
            decodeNibble(&channels[channel], packed >> 4);
      }
    }
  }
  return 0;
}

static void audioThread(void) {
  int streamStarted = 0;
  unsigned int blocksPlayed = 0;
  int stalledReported = 0;
  int resetQueue = 0;
  while (!stopping) {
    if (!enabled) {
      audsrv_stop_audio();
      resetQueue = 1;
      WaitSema(controlSema);
      continue;
    }

    if (ambientData == NULL && fseek(ambientFile, dataOffset, SEEK_SET))
      break;
    uint32_t framesLeft = totalFrames;
    uint32_t blockIndex = 0;
    if (resetQueue) {
      audsrv_fmt_t format = {.freq = 22050, .bits = 16, .channels = 2};
      if (audsrv_set_format(&format)) {
        DPRINTF("WARN: Ambient audio queue could not be reset\n");
        break;
      }
      resetQueue = 0;
    }
    if (audsrv_set_volume(AMBIENT_VOLUME)) {
      DPRINTF("WARN: Ambient audio output could not be configured\n");
      break;
    }
    while (!stopping && enabled && framesLeft > 0) {
      if (ambientData != NULL)
        memcpy(encodedBlock, ambientData + blockIndex * AMBIENT_BLOCK_BYTES,
               sizeof(encodedBlock));
      else if (fread(encodedBlock, 1, sizeof(encodedBlock), ambientFile) !=
               sizeof(encodedBlock)) {
        DPRINTF("WARN: Ambient audio stream could not be read\n");
        enabled = 0;
        break;
      }
      blockIndex++;
      if (decodeBlock()) {
        DPRINTF("WARN: Ambient audio stream could not be decoded\n");
        enabled = 0;
        break;
      }
      int frames = framesLeft < AMBIENT_BLOCK_FRAMES
                       ? (int)framesLeft : AMBIENT_BLOCK_FRAMES;
      int bytes = frames * 2 * sizeof(int16_t);
      int sent = 0;
      int waitCycles = 0;
      while (!stopping && enabled && sent < bytes) {
        int amount = bytes - sent;
        if (amount > AMBIENT_FEED_BYTES)
          amount = AMBIENT_FEED_BYTES;
        int available = audsrv_available();
        if (available < 0) {
          DPRINTF("WARN: Ambient audio queue is unavailable\n");
          enabled = 0;
          break;
        }
        if (available < amount) {
          if (++waitCycles == 200 && !stalledReported) {
            DPRINTF("WARN: Ambient audio queue stalled with %d bytes free\n",
                    available);
            stalledReported = 1;
          }
          DelayThread(5000);
          continue;
        }
        int count = audsrv_play_audio((char *)decodedBlock + sent, amount);
        if (count < 0) {
          DPRINTF("WARN: Ambient audio playback stopped\n");
          enabled = 0;
          break;
        }
        if (count == 0) {
          DelayThread(5000);
          continue;
        }
        sent += count;
        waitCycles = 0;
      }
      if (!streamStarted && sent == bytes) {
        DPRINTF("Ambient audio streaming\n");
        streamStarted = 1;
      }
      if (sent == bytes && ++blocksPlayed == 32)
        DPRINTF("Ambient audio stream sustained for 32 blocks\n");
      framesLeft -= frames;
    }
  }
  audsrv_stop_audio();
  SignalSema(doneSema);
  ExitThread();
}

void ambientStart(int shouldPlay) {
  char path[PATH_MAX];
  ee_sema_t semaphore;
  ee_thread_t thread;
  ee_thread_status_t callerStatus;
  int iopResult;
  if (threadId >= 0)
    return;
  if (ReferThreadStatus(GetThreadId(), &callerStatus) >= 0)
    DPRINTF("Ambient caller priority %d\n", callerStatus.current_priority);
#ifdef LUNA_EMULATOR_BUILD
  strcpy(path, "host:/ambient.wav");
#endif
#ifndef LUNA_EMULATOR_BUILD
  snprintf(path, sizeof(path), "mc%c:/APP_LUNA/ambient.wav",
           (!strncmp(LAUNCHER_OPTIONS.returnPath, "mc1:", 4)) ? '1' : '0');
#endif
  ambientFile = fopen(path, "rb");
  if (ambientFile == NULL) {
    DPRINTF("WARN: Ambient audio file is missing\n");
    return;
  }
  if (inspectWave(ambientFile)) {
    DPRINTF("WARN: Ambient audio file has an unsupported format\n");
    fclose(ambientFile);
    ambientFile = NULL;
    return;
  }
  if (dataBytes <= AMBIENT_CACHE_LIMIT) {
    ambientData = malloc(dataBytes);
    if (ambientData != NULL && !fseek(ambientFile, dataOffset, SEEK_SET) &&
        fread(ambientData, 1, dataBytes, ambientFile) == dataBytes) {
      fclose(ambientFile);
      ambientFile = NULL;
      DPRINTF("Ambient audio cached in EE memory (%u bytes)\n", dataBytes);
    } else {
      free(ambientData);
      ambientData = NULL;
      DPRINTF("WARN: Ambient audio cache unavailable; streaming from storage\n");
    }
  }
  if (SifLoadModule("rom0:LIBSD", 0, NULL) < 0 ||
      SifExecModuleBuffer(audsrv_irx, size_audsrv_irx, 0, NULL, &iopResult) < 0 ||
      audsrv_init()) {
    DPRINTF("WARN: Could not initialize ambient audio\n");
    goto fail;
  }
  audioInitialized = 1;
  audsrv_fmt_t format = {.freq = 22050, .bits = 16, .channels = 2};
  if (audsrv_set_format(&format))
    goto fail;
  semaphore.init_count = 0;
  semaphore.max_count = 1;
  semaphore.option = 0;
  controlSema = CreateSema(&semaphore);
  doneSema = CreateSema(&semaphore);
  if (controlSema < 0 || doneSema < 0)
    goto fail;
  memset(&thread, 0, sizeof(thread));
  thread.func = audioThread;
  thread.stack = threadStack;
  thread.stack_size = sizeof(threadStack);
  thread.gp_reg = &_gp;
  thread.initial_priority = 0x10;
  enabled = shouldPlay != 0;
  stopping = 0;
  threadId = CreateThread(&thread);
  if (threadId < 0)
    goto fail;
  if (StartThread(threadId, NULL) < 0) {
    DeleteThread(threadId);
    threadId = -1;
    goto fail;
  }
  DPRINTF("Ambient audio initialized from %s\n", path);
  return;

fail:
  DPRINTF("WARN: Could not start ambient audio\n");
  if (audioInitialized) {
    audsrv_quit();
    audioInitialized = 0;
  }
  if (controlSema >= 0) DeleteSema(controlSema);
  if (doneSema >= 0) DeleteSema(doneSema);
  controlSema = doneSema = -1;
  if (ambientFile != NULL) {
    fclose(ambientFile);
    ambientFile = NULL;
  }
  free(ambientData);
  ambientData = NULL;
}

void ambientSetEnabled(int shouldPlay) {
  int next = shouldPlay != 0;
  if (enabled == next)
    return;
  enabled = next;
  if (threadId >= 0) {
    SignalSema(controlSema);
  }
}

void ambientStop(void) {
  if (threadId >= 0) {
    stopping = 1;
    SignalSema(controlSema);
    WaitSema(doneSema);
    DeleteThread(threadId);
    threadId = -1;
  }
  if (audioInitialized) {
    audsrv_quit();
    audioInitialized = 0;
  }
  if (controlSema >= 0) DeleteSema(controlSema);
  if (doneSema >= 0) DeleteSema(doneSema);
  controlSema = doneSema = -1;
  if (ambientFile != NULL) {
    fclose(ambientFile);
    ambientFile = NULL;
  }
  free(ambientData);
  ambientData = NULL;
}
