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
# -fsanitize=undefined. The unified Dockerfile builds clang with
# ENABLE_SANITIZERS=ON, which CMake expands to include integer.
#
# The runtime list is appropriate because ci-regression passes
# SANITIZER_RECOVER=ON, allowing the suppressions to apply.
#
# Reading the file also stops this script overwriting a tracked source file as a
# side effect, which the unified Dockerfile corrects after profile generation.
silence_file="$PWD/silence.txt"
if [ ! -r "$silence_file" ]; then
  echo "ERROR: UBSAN suppression list not found: $silence_file" >&2
  exit 1
fi

ASAN_OPTIONS='print_scariness=1:halt_on_error=1:detect_leaks=0' \
  UBSAN_OPTIONS="halt_on_error=0:suppressions=$silence_file" \
  ./CreateAllProfiles.sh
