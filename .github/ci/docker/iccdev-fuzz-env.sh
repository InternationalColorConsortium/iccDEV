#!/bin/sh
###############################################################################
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# This source file is licensed under the BSD 3-Clause "New" or "Revised"
# License used by ICC software projects.
#
# Report maintainer AFL/CFL fuzzing helpers available in the regression image.
###############################################################################

set -eu

root="${ICCDEV_ROOT:-/workspace/iccDEV}"
build_dir="${ICCDEV_BUILD_DIR:-/workspace/build}"

printf '%s\n' 'iccDEV AFL/CFL maintainer environment'
printf 'Project root: %s\n' "$root"
printf 'Build dir: %s\n' "$build_dir"
printf '\n'

printf '%s\n' 'Toolchain:'
for tool in clang clang++ clang-21 clang++-21 llvm-symbolizer afl-fuzz afl-showmap afl-clang-fast afl-clang-fast++; do
  if command -v "$tool" >/dev/null 2>&1; then
    printf '  %-16s %s\n' "$tool" "$(command -v "$tool")"
  else
    printf '  %-16s missing\n' "$tool"
  fi
done
printf '\n'

printf '%s\n' 'Source mode: upstream checkout without local patches'
printf 'Source revision: %s\n' "${ICCDEV_SOURCE_REVISION:-unknown}"
printf '\n'

printf '%s\n' 'Examples:'
printf '%s\n' '  .github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump'
printf '%s\n' '  .github/ci/cfl/build.sh --targets dump,toxml,fromxml,tojson,fromjson,roundtrip --seconds 30'
printf '\n'

printf '%s\n' 'Note: ci-afl-smoke rebuilds AFL++ dev wrappers against LLVM 22 before'
printf '%s\n' 'running AFL instrumentation. The packaged AFL++ tools in this image have'
printf '%s\n' 'their matching LLVM 21 compiler runtime for lightweight local smoke checks.'
