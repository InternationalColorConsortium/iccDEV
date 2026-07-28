#!/bin/sh
set -eu

tools_root="${1:-../Build/Tools}"
for d in "$tools_root"/*; do
  if [ -d "$d" ]; then
    tool_dir="$(realpath "$d")"
    export PATH="$tool_dir:$PATH"
  fi
done

printf '%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n' \
  'unsigned-integer-overflow:*/IccMD5.cpp' \
  'shift-base:*/IccMD5.cpp' \
  'shift-exponent:*/IccMD5.cpp' \
  '# Recoverable runtime UBSAN noise from libstdc++ implementation internals.' \
  '# Fatal IntegerSanitizer builds require the compile-time ignorelist instead.' \
  'unsigned-integer-overflow:*/include/c++/*/bits/basic_string.h' \
  'unsigned-integer-overflow:*/include/c++/*/bits/basic_string.tcc' \
  'unsigned-integer-overflow:*/include/c++/*/bits/stl_bvector.h' \
  'unsigned-integer-overflow:*/include/c++/*/bits/stl_uninitialized.h' > silence.txt

ASAN_OPTIONS='print_scariness=1:halt_on_error=1:detect_leaks=0' \
  UBSAN_OPTIONS="halt_on_error=0:suppressions=$PWD/silence.txt" \
  ./CreateAllProfiles.sh
