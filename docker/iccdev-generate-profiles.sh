#!/bin/sh
set -eu

tools_root="${1:-../Build/Tools}"
for d in "$tools_root"/*; do
  if [ -d "$d" ]; then
    tool_dir="$(realpath "$d")"
    export PATH="$tool_dir:$PATH"
  fi
done

printf '%s\n%s\n%s\n' \
  'unsigned-integer-overflow:*/IccMD5.cpp' \
  'shift-base:*/IccMD5.cpp' \
  'shift-exponent:*/IccMD5.cpp' > silence.txt

ASAN_OPTIONS='print_scariness=1:halt_on_error=1:detect_leaks=0' \
  UBSAN_OPTIONS="halt_on_error=0:suppressions=$PWD/silence.txt" \
  ./CreateAllProfiles.sh
