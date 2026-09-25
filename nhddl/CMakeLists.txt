# LUNA modifications: Danny Nunez (dnunezx) 2026
cmake_minimum_required(VERSION 3.13)

include($ENV{PS2SDK}/ps2dev.cmake)

project(luna)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Executable name
set(EE_BIN "luna_unc.elf")
set(EE_BIN_PKD "luna.elf")
# Enable debug output if requested
option(ENABLE_PRINTF "Enable printf debugging output" ON)
option(LUNA_GLASS_UI "Build the production LUNA glass interface" ON)
option(LUNA_EMULATOR_BUILD "Enable behavior used only by emulator test builds" OFF)

# Source files
set(SOURCES
    src/main.c
    src/common.c
    src/options.c
    src/favorites.c
    src/neutrino.c
    src/target.c
    src/forwarder.c

    # UI
    src/ui/gui.c
    src/ui/graphics.c
    src/ui/handoff.c
    src/ui/ambient.c
  src/ui/navigation.c
  src/ui/view_state.c
  src/ui/art_cache.c
    src/ui/view_classic.c
    src/ui/view_collection.c
  src/ui/view_grid.c
  src/ui/view_orbit.c
    src/ui/args.c
    src/ui/pad.c

    # Devices
    src/devices/init.c
    src/devices/devices.c
    src/devices/mmce.c
    src/devices/udpfs.c
    src/devices/bdm.c
    src/devices/iso.c
    src/devices/hdl.c
    src/devices/title_id.c
)
include(irx.cmake)

# Get git version for build info
find_package(Git QUIET)
if(DEFINED LUNA_RELEASE_VERSION AND NOT LUNA_RELEASE_VERSION STREQUAL "")
  set(GIT_VERSION "${LUNA_RELEASE_VERSION}")
elseif(GIT_FOUND)
  execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --always --dirty --tags --exclude nightly
        OUTPUT_VARIABLE GIT_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
else()
  set(GIT_VERSION "unknown")
endif()

# Set compiler flags
add_definitions(-DGIT_VERSION=\"${GIT_VERSION}\")

if(ENABLE_PRINTF)
  add_definitions(-DENABLE_PRINTF)
endif()

if(LUNA_GLASS_UI)
  add_definitions(-DLUNA_GLASS_UI)
endif()

if(LUNA_EMULATOR_BUILD)
  add_definitions(-DLUNA_EMULATOR_BUILD)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/assets/ambient.wav
                 ${CMAKE_CURRENT_BINARY_DIR}/ambient.wav COPYONLY)
endif()

# Create the main executable
add_executable(${EE_BIN} ${SOURCES})

# Include directories
target_include_directories(${EE_BIN}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Custom linker script
separate_arguments(LINKER_FLAGS UNIX_COMMAND ${CMAKE_EXE_LINKER_FLAGS})
list(REMOVE_ITEM LINKER_FLAGS "-T$ENV{PS2SDK}/ee/startup/linkfile")
list(JOIN LINKER_FLAGS " " CMAKE_EXE_LINKER_FLAGS)
target_link_options(${EE_BIN}
    PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/linkfile
)

# Link libraries
target_link_libraries(${EE_BIN}
    pad
    patches
    fileXio
    libdebug.a
    gskit_toolkit
    gskit
    dmakit
    png
    z
    tiff
    audsrv
)

# Set link flags for PS2
set_target_properties(${EE_BIN} PROPERTIES
    LINK_FLAGS "-s"
)

# Create packed binary
add_custom_command(
    TARGET ${EE_BIN}
    POST_BUILD
    COMMAND ${PS2DEV}/bin/ps2-packer
            ${EE_BIN}
            ${EE_BIN_PKD}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Packing ${EE_BIN} as ${EE_BIN_PKD}"
)
