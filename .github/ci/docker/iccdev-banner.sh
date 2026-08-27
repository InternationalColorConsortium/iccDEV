#!/bin/sh

if [ "${ICCDEV_BANNER:-1}" = "0" ]; then
  exit 0
fi

root="${ICCDEV_ROOT:-}"
if [ -z "$root" ]; then
  for candidate in /opt/iccdev /workspace/iccDEV; do
    if [ -d "$candidate" ]; then
      root="$candidate"
      break
    fi
  done
fi
if [ -z "$root" ]; then
  root="/opt/iccdev"
fi
build_dir="${ICCDEV_BUILD_DIR:-$root/Build}"

version="${ICCDEV_VERSION:-}"
if [ -z "$version" ] && command -v iccDumpProfile >/dev/null 2>&1; then
  version="$(iccDumpProfile 2>&1 \
    | sed -n 's/^.*IccProfLib version \([^[:space:]]*\).*$/\1/p' \
    | sed -n '1p')"
fi
if [ -z "$version" ] && [ -d "$build_dir/IccProfLib" ]; then
  version="$(find "$build_dir/IccProfLib" -name 'libIccProfLib2.so.*' -type f 2>/dev/null \
    | sed -n 's/^.*libIccProfLib2\.so\.//p' \
    | sort \
    | sed -n '1p')"
fi
if [ -z "$version" ]; then
  version="unknown"
fi

profile_count=0
if [ -d "$root/Testing" ]; then
  profile_count="$(find "$root/Testing" -type f \( -name '*.icc' -o -name '*.icm' \) 2>/dev/null \
    | wc -l \
    | sed 's/[[:space:]]//g')"
fi

printf '%s\n' '============================================================'
printf '%s\n' '==== International Color Consortium | https://color.org ===='
printf '==== iccDEV v%s %s ====\n' "$version" "${ICCDEV_BUILD_LABEL:-debug-sanitizers Built for Docker}"
printf '%s\n' '============================================================'
printf '%s\n' ''
printf '%s\n' 'iccDEV provides a set of libraries and tools that allow for'
printf '%s\n' 'the interaction, manipulation, and application of ICC color'
printf '%s\n' 'management profiles.'
printf '%s\n' ''
printf '%s\n' 'URL https://github.com/InternationalColorConsortium/iccDEV'
if [ -n "${ICCDEV_IMAGE_PULL:-}" ]; then
  printf '%s\n' "$ICCDEV_IMAGE_PULL"
fi
printf '%s\n' '============================================================'
printf '%s\n' ''
printf 'Project root: %s\n' "$root"
if [ -d "$root/.git" ] && command -v git >/dev/null 2>&1; then
  branch="$(git -C "$root" rev-parse --abbrev-ref HEAD 2>/dev/null || true)"
  commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || true)"
  remote="$(git -C "$root" remote get-url origin 2>/dev/null || true)"
  if [ -n "$branch$commit" ]; then
    printf 'Git checkout: %s %s\n' "$branch" "$commit"
  fi
  if [ -n "$remote" ]; then
    printf 'Git remote: %s\n' "$remote"
  fi
fi
printf 'Reference ICC profiles present: %s\n' "$profile_count"
printf '%s\n' ''
printf '%s\n' 'The Libraries are on LD_LIBRARY_PATH and Tools are on PATH:'
printf '%s\n' ''
for dir in IccProfLib IccXML IccJSON; do
  if [ -d "$build_dir/$dir" ]; then
    find "$build_dir/$dir" -maxdepth 1 -type f \( -name '*.so*' -o -name '*.a' \) 2>/dev/null
  fi
done | sort
if [ -d "$build_dir/Tools" ]; then
  find "$build_dir/Tools" -mindepth 2 -maxdepth 2 -type f -executable 2>/dev/null | sort
fi
printf '%s\n' ''
printf '%s\n' '============================================================'
printf '%s\n' 'Example Use:'
printf '%s\n' 'iccToXml Testing/sRGB_v4_ICC_preference.icc Testing/sRGB_v4_ICC_preference.xml'
printf '%s\n' ''
printf '%s\n' 'The Testing directory contains ICC profiles.'
if command -v bash >/dev/null 2>&1; then
  printf '%s\n' ''
  printf '%s\n' 'To regenerate reference profiles:'
  printf '%s\n' 'cd Testing'
  printf '%s\n' 'bash CreateAllProfiles.sh'
fi

qa_tools=""
for tool in clang-tidy cppcheck scan-build valgrind gdb lcov gcovr shellcheck afl-fuzz; do
  if command -v "$tool" >/dev/null 2>&1; then
    qa_tools="${qa_tools}${qa_tools:+ }$tool"
  fi
done
if [ -n "$qa_tools" ]; then
  printf '%s\n' ''
  printf 'Maintainer QA tools on PATH: %s\n' "$qa_tools"
fi

if command -v iccdev-fuzz-env >/dev/null 2>&1; then
  printf '%s\n' ''
  printf '%s\n' 'AFL/CFL helper: iccdev-fuzz-env'
fi

printf '%s\n' ''
printf '%s\n' 'Open an Issue with Comments or Feedback at URL:'
printf '%s\n' 'https://github.com/InternationalColorConsortium/iccDEV/issues'
printf '%s\n' ''
printf '%s\n' '=== Thank you for using iccDEV Libraries & Tools ===='
printf '%s\n' ''
