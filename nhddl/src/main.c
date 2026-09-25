// LUNA modifications: Danny Nunez (dnunezx) 2026
#include "common.h"
#include "devices/devices.h"
#include "devices/init.h"
#include "devices/title_id.h"
#include "dprintf.h"
#include "forwarder.h"
#include "neutrino.h"
#include "options.h"
#include "target.h"
#include "ui/ui.h"
#include "ui/ambient.h"
#include "ui/view_state.h"
#include <ctype.h>
#include <debug.h>
#include <fcntl.h>
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Launcher options
LauncherOptions LAUNCHER_OPTIONS = {.returnPath = "hdd"};
// Options file name relative to CWD
static const char optionsFile[] = "luna.yaml";
static const char legacyOptionsFile[] = "nhddl.yaml";
static const char rootFallbackPath[] = "/luna/luna.yaml";
static const char legacyRootFallbackPath[] = "/nhddl/nhddl.yaml";

// Supported options
#define OPTION_VMODE "video"
#define OPTION_MODE "mode"
#define OPTION_UDPFS_IP "udpfs_ip"
#define OPTION_IMAGE "dvd"
#define OPTION_NO_INIT "noinit"
#define OPTION_RETURN_PATH "return_path"

// ATA and HDL retain their existing scan behavior. Other library sources
// require an explicit mode entry in the options file.
#define LUNA_LIBRARY_DEFAULT_MODES (MODE_ATA | MODE_HDL)
#define LUNA_LIBRARY_OPT_IN_MODES (MODE_USB | MODE_MX4SIO | MODE_MMCE | MODE_ILINK | MODE_UDPFS)
static ModeType configuredLibraryModes = MODE_NONE;

#ifndef GIT_VERSION
#define GIT_VERSION "v-0.0.0-unknown"
#endif

// Does a quick init for options given in argv
int argInit();
// Initializes modules, NHDDL configuraton, Neutrino path and device map
int init(char *elfPath);
char *resolveRootDevice(char *argv0);
// Loads NHDDL options from optionsFile
int loadOptions(char *cwdPath);
// Attempts to parse argv into LAUNCHER_OPTIONS
void parseArgv(int argc, char *argv[]);
// Parses argv[0] for mode postfix
ModeType parseFilename(const char *path);
// Tries to load IPCONFIG.DAT from memory card
void parseIPConfig();

int main(int argc, char *argv[]) {
  DPRINTF("*************\nLUNA %s\nLightweight Unified Neutrino Access\nBased on NHDDL by pcm720\n*************\n", GIT_VERSION);

  for (int i = 0; i < argc; i++)
    DPRINTF("argv[%d] = %s\n", i, argv[i]);

  // Parse arguments
  if ((argc > 0 && argv[0][0] == '-') || (argc > 1 && argv[1][0] == '-'))
    parseArgv(argc, argv);

  int res;
  if (LAUNCHER_OPTIONS.image && ((LAUNCHER_OPTIONS.mode != MODE_NONE) && !(LAUNCHER_OPTIONS.mode & MODE_HDL))) {
    res = forwardBoot();
    init_scr();
    logString("\n\nERROR: Failed to forward to Neutrino: %d\n", res);
    goto fail;
  }

  DPRINTF("Initializing UI\n");
  if ((res = uiInit())) {
    init_scr();
    logString("\n\nERROR: Failed to init UI: %d\n", res);
    goto fail;
  }

  // Start splash screen thread
  if ((res = startSplashScreen()) < 0) {
    init_scr();
    logString("\n\nERROR: Failed to start splash screen thread: %d\n", res);
    goto fail;
  }

  TargetList *titles;
  if ((argc > 0 && argv[0][0] == '-') || (argc > 1 && argv[1][0] == '-'))
    // If argv contains arguments, use them for init
    res = argInit();
  else {
    LAUNCHER_OPTIONS.mode = parseFilename(argv[0]);
    res = init(argv[0]);
  }

  if (res)
    goto fail;

  uiSplashLogString(LEVEL_INFO_NODELAY, "Building target list...\n");

  titles = calloc(1, sizeof(TargetList));
  if (titles == NULL)
    goto fail;

  // Scan every initialized device for entries
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (deviceModeMap[i].mode == MODE_NONE || deviceModeMap[i].mountpoint == NULL)
      break;

    // Ignore devices without a scan function
    if (deviceModeMap[i].scan == NULL)
      continue;

    // Only scan optional storage modes selected in the options file. The
    // runtime mode can also include drivers loaded to access the boot path.
    if (!(deviceModeMap[i].mode & (LUNA_LIBRARY_DEFAULT_MODES | configuredLibraryModes))) {
      DPRINTF("Skipping unconfigured library device %s\n", deviceModeMap[i].mountpoint);
      continue;
    }

    res = deviceModeMap[i].scan(titles, &deviceModeMap[i]);
    if (res != 0) {
      DPRINTF("WARN: failed to scan %s: %d\n", deviceModeMap[i].mountpoint, res);
    }
  }

  if (titles->total == 0) {
    freeTargetList(titles);
    uiSplashLogString(LEVEL_WARN, "No targets found\n");
    goto fail;
  }

  // Let the audio worker preempt long cover decodes in the UI thread.
  if (ChangeThreadPriority(GetThreadId(), 0x20) < 0)
    DPRINTF("WARN: Could not lower UI thread priority for ambient audio\n");

  // Start the soundtrack as soon as the library drive is ready, while the
  // splash is still visible. Use the restored title for its audio preference.
  Target *audioTarget = titles->first;
  char lastAudioTitle[PATH_MAX + 1];
  if (!getLastLaunchedTitle(lastAudioTitle, sizeof(lastAudioTitle))) {
    for (Target *candidate = titles->first; candidate != NULL;
         candidate = candidate->next) {
      int relative = getRelativePathIdx(candidate->fullPath);
      if (relative < 0)
        relative = 0;
      if (!strcmp(lastAudioTitle, candidate->fullPath + relative)) {
        audioTarget = candidate;
        break;
      }
    }
  }
  ambientStart(loadAmbientSoundEnabled(audioTarget));

  stopUISplashThread();
  if ((res = uiLoop(titles))) {
    init_scr();
    logString("\n\nERROR: UI loop failed: %d\n", res);
    goto fail;
  }
  DPRINTF("UI loop done, exiting\n");
  freeTargetList(titles);
  return 0;

fail:
  ambientStop();
  sleep(10);
  return 1;
}

// Initializes device map while logging errors
int initDevices() {
  // Device backends already perform bounded readiness probes. Do not add a
  // second, unconditional one-second splash delay before those probes begin.
  uiSplashLogString(LEVEL_INFO_NODELAY, "Waiting for storage devices...\n");
  int res = initDeviceMap();
  if ((res < 0)) {
    uiSplashLogString(LEVEL_ERROR, "Failed to initialize devices\n");
    return -EIO;
  }
  if (!res) {
    uiSplashLogString(LEVEL_ERROR, "No devices found\n");
    return -ENODEV;
  }
  return 0;
}

// Reads Neutrino version and displays Neutrino path and version on the splash screen
void showNeutrinoSplash() {
  // Get Neturino version
  char *neutrinoVersion = getNeutrinoVersion();
  uiSplashSetNeutrinoVersion(neutrinoVersion);
  // Successful boot messages are hidden by LUNA's minimal splash, so waiting
  // a full second here only makes the library appear later.
  uiSplashLogString(LEVEL_INFO_NODELAY, "Found Neutrino at\n%s\n", NEUTRINO_ELF_PATH);
  free(neutrinoVersion);
}

// Does a quick init for options given in argv
int argInit() {
  int res;
  char cwdPath[PATH_MAX + 1];

  if ((res = initModules(LAUNCHER_OPTIONS.mode)) != 0)
    return res;

  // Initialize device map
  if (initDevices() < 0)
    return -EIO;

  // Search for neutrino.elf
  getcwd(cwdPath, PATH_MAX + 1);
  if (findNeutrinoELF(cwdPath)) {
    uiSplashLogString(LEVEL_ERROR, "Couldn't find neutrino.elf\n");
    return -ENOENT;
  }

  showNeutrinoSplash();
  return 0;
}

// Initializes modules, NHDDL configuraton, Neutrino path and device map
int init(char *elfPath) {
  uiSplashLogString(LEVEL_INFO_NODELAY, "Initializing...\n");
  int initialModules = 0;
  if (elfPath) {
    // Guess root device
    elfPath = resolveRootDevice(elfPath);
    char *path = strrchr(elfPath, '/');
    if (path)
      *(++path) = '\0'; // Terminate the path at directory

    initialModules = LAUNCHER_OPTIONS.mode;

    // Try to load options
    if (loadOptions(elfPath)) {
      DPRINTF("Failed to load options file, will use defaults\n");
      // Default to loading all devices
      LAUNCHER_OPTIONS.mode = MODE_ALL;
    }
  }

  int res = 0;
  if (initialModules != LAUNCHER_OPTIONS.mode) {
    // Load modules
    if ((res = initModules(LAUNCHER_OPTIONS.mode)) != 0) {
      free(elfPath);
      return res;
    }
  }

  // Initialize device map
  if (initDevices() < 0) {
    free(elfPath);
    return -EIO;
  }

  // Search for neutrino.elf
  res = findNeutrinoELF(elfPath);
  free(elfPath);
  if (res < 0) {
    uiSplashLogString(LEVEL_ERROR, "Couldn't find neutrino.elf\n");
    return -ENOENT;
  }

  showNeutrinoSplash();
  return 0;
}

// Parses mode string into enum
ModeType parseMode(const char *modeStr) {
  if (!strncmp(modeStr, "ata", 3))
    return MODE_ATA;
  if (!strncmp(modeStr, "mx4sio", 6))
    return MODE_MX4SIO;
  if (!strncmp(modeStr, "udpfs", 5))
    return MODE_UDPFS;
  if (!strncmp(modeStr, "usb", 3))
    return MODE_USB;
  if (!strncmp(modeStr, "ilink", 5))
    return MODE_ILINK;
  if (!strncmp(modeStr, "mmce", 4))
    return MODE_MMCE;
  if (!strncmp(modeStr, "hdl", 3))
    return MODE_HDL;
  return MODE_NONE;
}

// Parses argv[0] for mode postfix
ModeType parseFilename(const char *path) {
  char *modeStr = strrchr(path, '-');
  if (!modeStr)
    return MODE_NONE;

  modeStr++;

  if (!strncmp(modeStr, "ata", 3))
    return MODE_ATA;
  if (!strncmp(modeStr, "m4s", 3))
    return MODE_MX4SIO;
  if (!strncmp(modeStr, "udpfs", 5))
    return MODE_UDPFS;
  if (!strncmp(modeStr, "usb", 3))
    return MODE_USB;
  if (!strncmp(modeStr, "ilink", 5))
    return MODE_ILINK;
  if (!strncmp(modeStr, "mmce", 4))
    return MODE_MMCE;
  if (!strncmp(modeStr, "hdl", 3))
    return MODE_HDL;

  return MODE_NONE;
}

// Parses video mode string into enum
VModeType parseVMode(const char *modeStr) {
  if (!strncmp(modeStr, "ntsc", 4))
    return VMODE_NTSC;
  if (!strncmp(modeStr, "pal", 3))
    return VMODE_PAL;
  if (!strncmp(modeStr, "480p", 4))
    return VMODE_480P;
  return VMODE_NONE;
}

// Attempts to parse argv into LAUNCHER_OPTIONS
void parseArgv(int argc, char *argv[]) {
  LAUNCHER_OPTIONS.mode = MODE_NONE;
  char *arg;
  for (int i = 0; i < argc; i++) {
    arg = argv[i];
    if ((arg == NULL) || (arg[0] != '-'))
      continue;

    // Find argument name
    char *val = strchr(arg, '=');
    if (val) {
      // Terminate argument and advance pointers to point to value and argument
      *val = '\0';
      val++;
    }
    arg++;

    if (val && !strcmp(OPTION_VMODE, arg)) {
      DPRINTF("Using VMode %s\n", val);
      LAUNCHER_OPTIONS.vmode = parseVMode(val);
    } else if (val && !strcmp(OPTION_MODE, arg)) {
      DPRINTF("Using mode %s\n", val);
      LAUNCHER_OPTIONS.mode |= parseMode(val);
    } else if (val && !strcmp(OPTION_UDPFS_IP, arg)) {
      DPRINTF("Using UDPFS IP %s\n", val);
      strlcpy(LAUNCHER_OPTIONS.udpfsIp, val, sizeof(LAUNCHER_OPTIONS.udpfsIp));
    } else if (val && !strcmp(OPTION_IMAGE, arg)) {
      DPRINTF("Using image %s\n", val);
      LAUNCHER_OPTIONS.image = strdup(val);
    } else if (val && !strcmp(OPTION_RETURN_PATH, arg)) {
      DPRINTF("Using in-game return path %s\n", val);
      strlcpy(LAUNCHER_OPTIONS.returnPath, val, sizeof(LAUNCHER_OPTIONS.returnPath));
    } else if (!strcmp(OPTION_NO_INIT, arg)) {
      DPRINTF("Skipping IOP init\n");
      LAUNCHER_OPTIONS.noInit = 1;
    }
  }

  if (LAUNCHER_OPTIONS.mode == MODE_NONE)
    LAUNCHER_OPTIONS.mode = MODE_ALL;
}

// Loads NHDDL options from optionsFile
int loadOptions(char *cwdPath) {
  char lineBuffer[PATH_MAX + sizeof(legacyRootFallbackPath) + 1];
  if (cwdPath[0] != '\0') {
    // If path is valid, try it
    strcpy(lineBuffer, cwdPath);
    strcat(lineBuffer, optionsFile);
    if (tryFile(lineBuffer)) {
      strcpy(lineBuffer, cwdPath);
      strcat(lineBuffer, legacyOptionsFile);
    }
    if (tryFile(lineBuffer)) {
      DPRINTF("Trying device fallback path\n");
      char *mountpoint = strchr(lineBuffer, '/');
      if (mountpoint) {
        *mountpoint = '\0';
        strcat(lineBuffer, rootFallbackPath);
        if (tryFile(lineBuffer)) {
          *strchr(lineBuffer, '/') = '\0';
          strcat(lineBuffer, legacyRootFallbackPath);
          if (tryFile(lineBuffer))
            return -ENOENT;
        }
      } else
        return -ENOENT;
    }
  } else
    return -ENOENT;

  // Load NHDDL options file into ArgumentList
  ArgumentList *options = calloc(1, sizeof(ArgumentList));
  if (loadArgumentList(options, NULL, lineBuffer)) {
    // Else, fail
    DPRINTF("Can't load options file, will use defaults\n");
    freeArgumentList(options);
    return -ENOENT;
  }

  // Parse the list into Options
  Argument *arg = options->first;
  while (arg != NULL) {
    if (!arg->isDisabled) {
      if (strcmp(OPTION_VMODE, arg->arg) == 0) {
        printf("Using VMode %s\n", arg->value);
        LAUNCHER_OPTIONS.vmode = parseVMode(arg->value);
      } else if (strcmp(OPTION_MODE, arg->arg) == 0) {
        printf("Using mode %s\n", arg->value);
        ModeType mode = parseMode(arg->value);
        LAUNCHER_OPTIONS.mode |= mode;
        configuredLibraryModes |= mode & LUNA_LIBRARY_OPT_IN_MODES;
      } else if (strcmp(OPTION_UDPFS_IP, arg->arg) == 0) {
        printf("Using UDPFS IP %s\n", arg->value);
        strlcpy(LAUNCHER_OPTIONS.udpfsIp, arg->value, sizeof(LAUNCHER_OPTIONS.udpfsIp));
      } else if (strcmp(OPTION_RETURN_PATH, arg->arg) == 0) {
        printf("Using in-game return path %s\n", arg->value);
        strlcpy(LAUNCHER_OPTIONS.returnPath, arg->value, sizeof(LAUNCHER_OPTIONS.returnPath));
      }
    }
    arg = arg->next;
  }
  freeArgumentList(options);

  return 0;
}

// Attempts to guess device type from path
ModeType guessDeviceType(const char *path) {
  if (!strncmp(path, "mc", 2))
    return MODE_BASIC;
  if (!strncmp(path, "mmce", 4))
    return MODE_MMCE;
  if (!strncmp(path, "ata", 3))
    return MODE_ATA;
  if (!strncmp(path, "hdd", 3))
    return MODE_HDL;
  if (!strncmp(path, "udpfs", 5))
    return MODE_UDPFS;
  if (!strncmp(path, "usb", 3))
    return MODE_USB;
  if (!strncmp(path, "mx4sio", 6))
    return MODE_MX4SIO;
  if (!strncmp(path, "ilink", 5))
    return MODE_ILINK;
  if (!strncmp(path, "mass", 4))
    return MODE_BDM;
  return MODE_NONE;
}

int probePath(char *filePath, int probeAttempts) {
  // Wait for IOP to initialize device driver
  int res = 0;
  for (int attempts = 0; attempts < probeAttempts; attempts++) {
    res = open(filePath, O_RDONLY);
    if (res >= 0) {
      close(res);
      return 0;
    }
    sleep(1);
  }
  return res;
}

// Attempts to detect root device and load device drivers required for accessing CWD
// Returns root path to device ELF
char *resolveRootDevice(char *argv0) {
  char *result = strdup(argv0);
  // Load device drivers for boot path
  printf("Resolve root: argv[0] is %s, guessing device type\n", argv0);

#ifdef LUNA_EMULATOR_BUILD
  // PCSX2's direct-ELF launcher maps the ELF directory to host:, but on
  // Windows it presents argv[0] without usable path separators. Treat the
  // host device root as the current directory so adjacent config/runtime
  // files remain discoverable.
  if (!strncmp(argv0, "host:", 5)) {
    printf("Resolve root: using PCSX2 host device root\n");
    free(result);
    initModules(MODE_BASIC);
    return strdup("host:/");
  }
#endif

  ModeType device = guessDeviceType(argv0);
  if (device == MODE_BASIC) {
    printf("Resolve root: loading basic drivers\n");
    initModules(MODE_BASIC);
    return result;
  } else if (device == MODE_MMCE) {
    printf("Resolve root: loading MMCE drivers\n");
    initModules(MODE_MMCE);
    return result;
  } else if (device == MODE_HDL) {
    printf("Resolve root: loading HDD drivers\n");
    initModules(MODE_HDL);
    probePath(argv0, 10);
    return result;
  } else if (device == MODE_ATA) {
    printf("Resolve root: loading ATA drivers\n");
    initModules(MODE_ATA);
    probePath(argv0, 10);
    return result;
  } else if (device == MODE_USB) {
    printf("Resolve root: loading USB drivers\n");
    initModules(MODE_USB);
    probePath(argv0, 2);
    return result;
  } else if (device == MODE_MX4SIO) {
    printf("Resolve root: loading MX4SIO drivers\n");
    initModules(MODE_MX4SIO);
    probePath(argv0, 2);
    return result;
  } else if (device == MODE_BDM) {
    printf("Resolve root: probing root path for BDM device\n");
    initModules(MODE_ATA | MODE_USB | MODE_MX4SIO);

    if (argv0[4] == ':' || argv0[6] != '/') {
      // argv[0] is "mass:" or doesn't have a trailing slash, fix it to "mass?:/"
      int arglen = strlen(argv0) + 3;
      free(result);
      result = (char *)malloc(arglen);
      int startPos = ((argv0[5] == '/') || (argv0[5] == ':')) ? 6 : 5;
      snprintf(result, arglen, "mass?:/%s", &argv0[startPos]);
    }

    int fd = 0;
    int attempts = 0;
    for (int i = 0; i < 8; i++) {
      result[4] = '0' + i;
      printf("Resolve root: probing %s\n", result);
      if (probePath(result, 2)) {
        printf("Resolve root: failed to probe\n");
        return result; // No BDM devices were found
      }
      fd = open(result, O_RDONLY);
      if (fd >= 0) {
        printf("Resolve root: found root path\n");
        close(fd);
        return result;
      }
    }
  }

  return result;
}
