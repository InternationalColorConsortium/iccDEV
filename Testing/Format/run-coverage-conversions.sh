#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Convert the hand-authored XML and JSON coverage pairs in this directory.
# Set ICCDEV_TOOLS_DIR to the build's Tools directory, or put both CLIs on PATH.

set -euo pipefail

tools_dir="${ICCDEV_TOOLS_DIR:-}"
if [[ -n "$tools_dir" ]]; then
  if [[ ! -d "$tools_dir" ]]; then
    printf 'Tools directory does not exist: %s\n' "$tools_dir" >&2
    exit 2
  fi
  tools_dir="$(cd -- "$tools_dir" && pwd -P)"
fi

format_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
cd -- "$format_dir"

resolve_tool() {
  local binary="$1" target_dir="$2" candidate
  if [[ -n "$tools_dir" ]]; then
    for candidate in "$tools_dir/$target_dir/$binary" "$tools_dir/$binary"; do
      if [[ -x "$candidate" && ! -d "$candidate" ]]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
    printf 'Required tool not found under %s: %s\n' "$tools_dir" "$binary" >&2
    return 2
  fi
  if ! command -v "$binary"; then
    printf 'Required tool is not on PATH: %s\n' "$binary" >&2
    return 2
  fi
}

json_tool="$(resolve_tool iccFromJson IccFromJson)"
xml_tool="$(resolve_tool iccFromXml IccFromXml)"

names=(
  acs-bookends-coverage
  compressed-text-coverage
  format-feature-coverage
  sequence-metadata-coverage
  metadata-numeric-coverage
  dict-localization-coverage
  response-curve-coverage
)

for name in "${names[@]}"; do
  for ext in json xml; do
    input="$name.$ext"
    if [[ ! -s "$input" ]]; then
      printf 'Missing or empty input: %s\n' "$input" >&2
      exit 2
    fi
  done
done

precision_name=u16fixed16-xml-precision-coverage
if [[ ! -s "$precision_name.xml" ]]; then
  printf 'Missing or empty input: %s\n' "$precision_name.xml" >&2
  exit 2
fi

convert() {
  local binary="$1" input="$2" output="$3"
  rm -f -- "$output"
  printf '%s %s %s\n' "$(basename -- "$binary")" "$input" "$output"
  if ! "$binary" "$input" "$output"; then
    rm -f -- "$output"
    printf 'Conversion failed: %s\n' "$input" >&2
    return 1
  fi
  if [[ ! -s "$output" ]]; then
    rm -f -- "$output"
    printf 'Conversion produced no ICC profile: %s\n' "$input" >&2
    return 1
  fi
}

for name in "${names[@]}"; do
  convert "$json_tool" "$name.json" "$name.json.icc"
  convert "$xml_tool" "$name.xml" "$name.xml.icc"
done
convert "$xml_tool" "$precision_name.xml" "$precision_name.xml.icc"

printf 'Converted %d authored pairs and 1 XML-only precision fixture (%d ICC profiles).\n' \
  "${#names[@]}" "$((2 * ${#names[@]} + 1))"
