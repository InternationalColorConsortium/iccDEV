#!/usr/bin/env bash
#------------------------------------------------------------------------------
# Restore metadata lost by GitHub artifact transport before release packaging.
#------------------------------------------------------------------------------

set -euo pipefail

usage() {
  printf 'Usage: %s <artifact-directory>\n' "$(basename "$0")" >&2
}

if [ "$#" -ne 1 ]; then
  usage
  exit 2
fi

artifact_dir="$1"
if [ ! -d "$artifact_dir" ]; then
  printf 'ERROR: artifact directory not found: %s\n' "$artifact_dir" >&2
  exit 1
fi

artifact_dir="$(cd "$artifact_dir" && pwd -P)"

for tool in file readelf; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    printf 'ERROR: required tool not found: %s\n' "$tool" >&2
    exit 1
  fi
done

if ! command -v patchelf >/dev/null 2>&1; then
  printf 'ERROR: required tool not found: patchelf\n' >&2
  exit 1
fi

elf_exec_restored=0
macho_exec_restored=0
script_exec_restored=0
elf_rpath_updated=0
absolute_runtime_paths=0

has_dynamic_deps() {
  local path="$1"
  readelf -W -d "$path" 2>/dev/null | grep -q '(NEEDED)'
}

has_absolute_runtime_path() {
  local path="$1"
  readelf -W -d "$path" 2>/dev/null |
    awk -F'[][]' '/RPATH|RUNPATH/{print $2}' |
    tr ':' '\n' |
    grep -Eq '^/|^[A-Za-z]:/'
}

while IFS= read -r -d '' path; do
  rel="${path#"$artifact_dir"/}"
  file_type="$(file -b "$path")"
  base="$(basename "$path")"

  if grep -q 'ELF' <<< "$file_type"; then
    if grep -q 'executable' <<< "$file_type"; then
      chmod 0755 "$path"
      elf_exec_restored=$((elf_exec_restored + 1))
    fi

    if has_dynamic_deps "$path"; then
      patchelf --set-rpath '$ORIGIN' "$path"
      elf_rpath_updated=$((elf_rpath_updated + 1))
    fi

    if has_absolute_runtime_path "$path"; then
      printf 'ERROR: %s still contains an absolute ELF runtime path\n' "$rel" >&2
      absolute_runtime_paths=$((absolute_runtime_paths + 1))
    fi
    continue
  fi

  if grep -q 'Mach-O .*executable' <<< "$file_type"; then
    chmod 0755 "$path"
    macho_exec_restored=$((macho_exec_restored + 1))
    continue
  fi

  if [[ "$base" == *.sh || "$base" == "path.sh" ]]; then
    chmod 0755 "$path"
    script_exec_restored=$((script_exec_restored + 1))
  fi
done < <(find "$artifact_dir" -type f -print0 | sort -z)

printf 'Restored ELF executable modes: %s\n' "$elf_exec_restored"
printf 'Restored Mach-O executable modes: %s\n' "$macho_exec_restored"
printf 'Restored shell script modes: %s\n' "$script_exec_restored"
printf 'Normalized ELF runtime paths: %s\n' "$elf_rpath_updated"

if [ "$absolute_runtime_paths" -ne 0 ]; then
  printf 'ERROR: absolute runtime paths remain: %s\n' "$absolute_runtime_paths" >&2
  exit 1
fi
