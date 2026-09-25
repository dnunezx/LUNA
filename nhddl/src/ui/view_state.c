#include "ui/view_state.h"
#include "devices/devices.h"
#include "dprintf.h"
#include "options.h"
#include <errno.h>
#include <ps2sdkapi.h>
#include <stdio.h>
#include <string.h>

static const char lastViewPath[] = "/lastView.txt";
static const char lastViewTempPath[] = "/lastView.txt.tmp";
static const char classicLayoutPath[] = "/classicLayout.txt";
static const char classicLayoutTempPath[] = "/classicLayout.txt.tmp";
static const char orbsViewPath[] = "/orbsView.txt";
static const char orbsViewTempPath[] = "/orbsView.txt.tmp";
static const char *const viewNames[] = {
    "classic", "collection", "grid", "orbit", "orbs"};

static struct DeviceMapEntry *viewDevice(Target *target) {
  if (target == NULL || target->device == NULL)
    return NULL;
  return target->device->metadev ? target->device->metadev : target->device;
}

static int readViewFile(const char *path, UILibraryView *view) {
  char name[24];
  FILE *file = fopen(path, "r");
  if (file == NULL)
    return -ENOENT;
  if (fgets(name, sizeof(name), file) == NULL) {
    fclose(file);
    return -EINVAL;
  }
  fclose(file);
  size_t nameLength = strcspn(name, "\r\n");
  if (name[nameLength] == '\0')
    return -EINVAL;
  name[nameLength] = '\0';
  for (int i = UI_VIEW_CLASSIC; i <= UI_VIEW_ORBS; i++) {
    if (!strcmp(name, viewNames[i])) {
      *view = (UILibraryView)i;
      return 0;
    }
  }
  return -EINVAL;
}

UILibraryView loadLastLibraryView(Target *target) {
  struct DeviceMapEntry *device = viewDevice(target);
  char path[PATH_MAX];
  UILibraryView view;

  if (device == NULL || device->mountpoint == NULL)
    return UI_VIEW_CLASSIC;
  // A completed temporary file is the latest state if power was lost between
  // removing the previous file and committing the replacement.
  if (!buildConfigFilePath(path, sizeof(path), device->mountpoint, lastViewTempPath) &&
      !readViewFile(path, &view)) {
    DPRINTF("Restored library view %s from %s\n", viewNames[view], path);
    return view;
  }
  if (!buildConfigFilePath(path, sizeof(path), device->mountpoint, lastViewPath) &&
      !readViewFile(path, &view)) {
    DPRINTF("Restored library view %s from %s\n", viewNames[view], path);
    return view;
  }
  return UI_VIEW_CLASSIC;
}

int saveLastLibraryView(Target *target, UILibraryView view) {
  struct DeviceMapEntry *device = viewDevice(target);
  char directory[PATH_MAX];
  char path[PATH_MAX];
  char tempPath[PATH_MAX];
  struct stat st;
  FILE *file;

  if (device == NULL || device->mountpoint == NULL ||
      view < UI_VIEW_CLASSIC || view > UI_VIEW_ORBS)
    return -EINVAL;
  if (buildConfigFilePath(directory, sizeof(directory), device->mountpoint, NULL) ||
      buildConfigFilePath(path, sizeof(path), device->mountpoint, lastViewPath) ||
      buildConfigFilePath(tempPath, sizeof(tempPath), device->mountpoint, lastViewTempPath))
    return -ENAMETOOLONG;
  if (stat(directory, &st) == -1 && mkdir(directory, 0777)) {
    DPRINTF("ERROR: Failed to create view state directory: %d\n", errno);
    return -EIO;
  }
  file = fopen(tempPath, "w");
  if (file == NULL)
    return -EIO;
  int writeResult = fprintf(file, "%s\n", viewNames[view]);
  int closeResult = fclose(file);
  if (writeResult < 0 || closeResult) {
    remove(tempPath);
    return -EIO;
  }
  remove(path);
  if (rename(tempPath, path)) {
    DPRINTF("ERROR: Failed to commit last view: %d\n", errno);
    return -EIO;
  }
  DPRINTF("Saved library view %s to %s\n", viewNames[view], path);
  return 0;
}

int loadClassicArtOverlap(Target *target) {
  struct DeviceMapEntry *device = viewDevice(target);
  const char *paths[] = {classicLayoutTempPath, classicLayoutPath};
  char path[PATH_MAX];
  char name[24];

  if (device == NULL || device->mountpoint == NULL)
    return 0;
  for (int i = 0; i < 2; i++) {
    if (buildConfigFilePath(path, sizeof(path), device->mountpoint, paths[i]))
      continue;
    FILE *file = fopen(path, "r");
    if (file == NULL)
      continue;
    int readable = fgets(name, sizeof(name), file) != NULL;
    fclose(file);
    if (!readable)
      continue;
    name[strcspn(name, "\r\n")] = '\0';
    if (!strcmp(name, "overlap"))
      return 1;
    if (!strcmp(name, "separate"))
      return 0;
  }
  return 0;
}

int saveClassicArtOverlap(Target *target, int overlap) {
  struct DeviceMapEntry *device = viewDevice(target);
  char directory[PATH_MAX];
  char path[PATH_MAX];
  char tempPath[PATH_MAX];
  struct stat st;
  FILE *file;

  if (device == NULL || device->mountpoint == NULL)
    return -EINVAL;
  if (buildConfigFilePath(directory, sizeof(directory), device->mountpoint, NULL) ||
      buildConfigFilePath(path, sizeof(path), device->mountpoint, classicLayoutPath) ||
      buildConfigFilePath(tempPath, sizeof(tempPath), device->mountpoint, classicLayoutTempPath))
    return -ENAMETOOLONG;
  if (stat(directory, &st) == -1 && mkdir(directory, 0777)) {
    DPRINTF("ERROR: Failed to create Classic layout directory: %d\n", errno);
    return -EIO;
  }
  file = fopen(tempPath, "w");
  if (file == NULL) {
    DPRINTF("ERROR: Failed to open Classic layout temp file: %d\n", errno);
    return -EIO;
  }
  int writeResult = fprintf(file, "%s\n", overlap ? "overlap" : "separate");
  int closeResult = fclose(file);
  if (writeResult < 0 || closeResult) {
    DPRINTF("ERROR: Failed to write Classic layout temp file: %d\n", errno);
    remove(tempPath);
    return -EIO;
  }
  remove(path);
  if (rename(tempPath, path)) {
    DPRINTF("ERROR: Failed to commit Classic layout: %d\n", errno);
    return -EIO;
  }
  DPRINTF("Saved Classic artwork layout %s to %s\n",
          overlap ? "overlap" : "separate", path);
  return 0;
}

int loadOrbsViewEnabled(Target *target) {
  struct DeviceMapEntry *device = viewDevice(target);
  const char *paths[] = {orbsViewTempPath, orbsViewPath};
  char path[PATH_MAX];
  char value[24];

  if (device == NULL || device->mountpoint == NULL)
    return 0;
  for (int i = 0; i < 2; i++) {
    if (buildConfigFilePath(path, sizeof(path), device->mountpoint, paths[i]))
      continue;
    FILE *file = fopen(path, "r");
    if (file == NULL)
      continue;
    int readable = fgets(value, sizeof(value), file) != NULL;
    fclose(file);
    if (!readable)
      continue;
    value[strcspn(value, "\r\n")] = '\0';
    if (!strcmp(value, "enabled"))
      return 1;
    if (!strcmp(value, "disabled"))
      return 0;
  }
  return 0;
}

int saveOrbsViewEnabled(Target *target, int enabled) {
  struct DeviceMapEntry *device = viewDevice(target);
  char directory[PATH_MAX];
  char path[PATH_MAX];
  char tempPath[PATH_MAX];
  struct stat st;
  FILE *file;

  if (device == NULL || device->mountpoint == NULL)
    return -EINVAL;
  if (buildConfigFilePath(directory, sizeof(directory), device->mountpoint, NULL) ||
      buildConfigFilePath(path, sizeof(path), device->mountpoint, orbsViewPath) ||
      buildConfigFilePath(tempPath, sizeof(tempPath), device->mountpoint, orbsViewTempPath))
    return -ENAMETOOLONG;
  if (stat(directory, &st) == -1 && mkdir(directory, 0777))
    return -EIO;
  file = fopen(tempPath, "w");
  if (file == NULL)
    return -EIO;
  int writeResult = fprintf(file, "%s\n", enabled ? "enabled" : "disabled");
  int closeResult = fclose(file);
  if (writeResult < 0 || closeResult) {
    remove(tempPath);
    return -EIO;
  }
  remove(path);
  if (rename(tempPath, path))
    return -EIO;
  return 0;
}
