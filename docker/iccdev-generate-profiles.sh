#!/bin/sh
set -eu

tools_root="${1:-../Build/Tools}"
for d in "$tools_root"/*; do
  if [ -d "$d" ]; then
    tool_dir="$(realpath "$d")"
    export PATH="$tool_dir:$PATH"
  fi
done

# Use the suppression list that is tracked in Testing/ instead of regenerating a
# copy of it here. Every caller runs with Testing/ as the working directory --
# the default tools_root above is written relative to it -- so the tracked file
# is always alongside. The copy this replaced had drifted two entries short of
# the tracked list: bits/vector.tcc and ext/string_conversions.h were absent.
#
# Those entries only matter where the integer sanitizer is on, because
# unsigned-integer-overflow belongs to -fsanitize=integer and not to
# -fsanitize=undefined. Of the three callers, two qualify: Dockerfile.ci-regression
# builds clang with ENABLE_SANITIZERS=ON, which CMake expands to include integer,
# and Dockerfile.nixos sets ENABLE_INTEGER_SANITIZER=ON outright. Only the plain
# Dockerfile is exempt, because it is gcc and CMake skips IntSan there.
#
# The runtime list this points at is the right venue only where the build also
# allows recovery -- ci-regression passes SANITIZER_RECOVER=ON, so suppressions
# apply. Dockerfile.nixos leaves it OFF and so compiles in
# -fno-sanitize-recover=integer, which no runtime suppression can override; that
# configuration needs the compile-time -DUBSAN_IGNORELIST= instead, as the header
# of .github/ci/ubsan-ignorelist.txt already says.
#
# Reading the file also stops this script overwriting a tracked source file as a
# side effect, which Dockerfile.ci-regression previously had to undo.
silence_file="$PWD/silence.txt"
if [ ! -r "$silence_file" ]; then
  echo "ERROR: UBSAN suppression list not found: $silence_file" >&2
  exit 1
fi

ASAN_OPTIONS='print_scariness=1:halt_on_error=1:detect_leaks=0' \
  UBSAN_OPTIONS="halt_on_error=0:suppressions=$silence_file" \
  ./CreateAllProfiles.sh
