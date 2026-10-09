#!/bin/sh
# Original LUNA code: Danny Nunez (dnunezx) 2026
# Run in a PS2SDK container with the complete LUNA checkout at /workspace.
set -eu

: "${PS2SDK:?PS2SDK must point to the PS2SDK installation}"
workspace="${LUNA_WORKSPACE:-/workspace}"
frontend="${LUNA_FRONTEND:-$workspace/nhddl}"
core="$workspace/psxcore"
build_dir="${LUNA_BUILD_DIR:-$frontend/build-psxcore}"
emulator_build="${LUNA_EMULATOR_OPTION:-ON}"
development="${LUNA_PSXCORE_DEVELOPMENT:-OFF}"
runtime_elf="${LUNA_PSXCORE_RUNTIME_ELF:-}"

if [ ! -f "$core/Makefile" ]; then
  echo "Initialize the private psxcore submodule before building." >&2
  exit 1
fi
mkdir -p "$build_dir"
# Only a completed build publishes provenance; failed updates retain no success stamp.
rm -f "$build_dir/psxcore-build.txt" "$build_dir/psxcore-build.sha256"
if [ -n "$runtime_elf" ] && [ "$development" != ON ]; then
  echo "An external reference ELF requires LUNA_PSXCORE_DEVELOPMENT=ON." >&2
  exit 1
fi
core_git() { git -c safe.directory="$core" -C "$core" "$@"; }
core_revision="$(core_git rev-parse HEAD)"
core_status="$(core_git status --porcelain)"
if [ -n "$core_status" ]; then
  echo "PSXCore has source changes; build from a clean, pinned revision." >&2
  exit 1
fi
frontend_revision="$(git -c safe.directory="$frontend" -C "$frontend" rev-parse HEAD)"
frontend_status="$(git -c safe.directory="$frontend" -C "$frontend" status --porcelain)"
frontend_dirty=no
if [ -n "$frontend_status" ]; then
  frontend_dirty=yes
fi
if [ ! -e "$PS2SDK/ps2dev.cmake" ]; then
  ln -s ../share/ps2dev.cmake "$PS2SDK/ps2dev.cmake"
fi
if [ ! -f "$PS2SDK/ports/lib/libtiff.a" ] && [ -f "$frontend/build-emulator/libtiff.a" ]; then
  cp "$frontend/build-emulator/libtiff.a" "$PS2SDK/ports/lib/libtiff.a"
fi
sh "$workspace/tools/bootstrap-ps2-libtiff.sh"

# Consume PSXCore's standalone ELF without editing its runtime or SDK modules.
# A supplied ELF allows direct and frontend tests to use exactly the same bytes.
runtime_source=external-reference-unverified
if [ -z "$runtime_elf" ]; then
  make -C "$core" runtime
  runtime_elf="$core/build/runtime/psxcore-runtime-bootstrap.elf"
  runtime_source=matching-checkout
fi
if [ ! -f "$runtime_elf" ]; then
  echo "Standalone PSXCore ELF is missing: $runtime_elf" >&2
  exit 1
fi
cmake -S "$frontend" -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLUNA_EMULATOR_BUILD="$emulator_build" \
  -DLUNA_ENABLE_PSXCORE=ON \
  -DLUNA_PSXCORE_DEVELOPMENT="$development" \
  -DLUNA_PSXCORE_SOURCE_DIR="$core"
cmake --build "$build_dir" --parallel 2
cp "$runtime_elf" "$build_dir/psxcore-runtime-bootstrap.elf"
sha256sum "$build_dir/psxcore-runtime-bootstrap.elf" > "$build_dir/psxcore-runtime-bootstrap.sha256"
final_revision="$(core_git rev-parse HEAD)"
final_status="$(core_git status --porcelain)"
if [ "$final_revision" != "$core_revision" ] || [ -n "$final_status" ]; then
  echo "PSXCore changed during the build; rebuild before using these outputs." >&2
  exit 1
fi
compiler_version="$("$PS2DEV/ee/bin/mips64r5900el-ps2-elf-gcc" -dumpfullversion)"
(
  cd "$build_dir"
  sha256sum luna.elf luna_unc.elf psxcore/libpsxcore.a psxcore-runtime-bootstrap.elf > psxcore-build.sha256
  {
    printf 'psxcore_revision=%s\n' "$core_revision"
    printf 'runtime_source=%s\n' "$runtime_source"
    printf 'frontend_revision=%s\nfrontend_dirty=%s\n' "$frontend_revision" "$frontend_dirty"
    printf 'emulator_build=%s\ndevelopment=%s\n' "$emulator_build" "$development"
    sdk_revision="$(git -c safe.directory="$PS2SDK" -C "$PS2SDK" rev-parse HEAD 2>/dev/null || printf unavailable)"
    printf 'ps2sdk_revision=%s\n' "$sdk_revision"
    printf 'build_image=%s\nbuild_image_id=%s\n' "${LUNA_BUILD_IMAGE:-unspecified}" "${LUNA_BUILD_IMAGE_ID:-unspecified}"
    printf 'ee_compiler_version=%s\n' "$compiler_version"
    printf 'built_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'verification=build-only-gameplay-pending\n'
  } > psxcore-build.txt.tmp
  mv psxcore-build.txt.tmp psxcore-build.txt
)
echo "Built LUNA and PSXCore $core_revision ($runtime_source); see $build_dir/psxcore-build.txt"
