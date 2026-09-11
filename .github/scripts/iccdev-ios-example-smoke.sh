#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Run the three manual iOS example apps on one disposable simulator.
set -euo pipefail

if [[ $# -gt 1 || ( $# -eq 1 && "$1" != "release" && "$1" != "sanitizers" ) ]]; then
  echo "Usage: $0 [release|sanitizers]" >&2
  exit 2
fi

mode="${1:-sanitizers}"
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
evidence_dir="$repo_root/out/ios-example-smoke-${mode}"
rm -rf "$evidence_dir"
mkdir -p "$evidence_dir"

xcrun simctl list runtimes --json > "$evidence_dir/runtimes.json"
if ! jq -e '
    any(.runtimes[];
      .isAvailable and
      (.identifier | startswith("com.apple.CoreSimulator.SimRuntime.iOS-")))
  ' "$evidence_dir/runtimes.json" >/dev/null; then
  if [[ "${ICCDEV_APPLE_DOWNLOAD_RUNTIME:-0}" != 1 ]]; then
    echo "No available iOS simulator runtime." >&2
    exit 1
  fi
  xcodebuild -downloadPlatform iOS
  xcrun simctl list runtimes --json > "$evidence_dir/runtimes.json"
fi

selection="$(jq -er '
  [.runtimes[] |
    select(.isAvailable and
      (.identifier | startswith("com.apple.CoreSimulator.SimRuntime.iOS-")))]
  | sort_by(.version | split(".") | map(tonumber)) | last
  | select(. != null)
  | [.identifier,
     (.supportedDeviceTypes |
       map(select(.productFamily == "iPhone")) | first.identifier)]
  | select(all(.[]; type == "string" and length > 0)) | @tsv
' "$evidence_dir/runtimes.json")"
IFS=$'\t' read -r runtime device_type <<< "$selection"

simulator=""
booted=0
cleanup() {
  status=$?
  trap - EXIT
  if [[ -n "$simulator" ]]; then
    if [[ "$booted" == 1 ]] && ! xcrun simctl shutdown "$simulator"; then
      echo "Failed to shut down the test simulator: $simulator" >&2
      status=1
    fi
    if ! xcrun simctl delete "$simulator"; then
      echo "Failed to delete the test simulator: $simulator" >&2
      status=1
    fi
  fi
  exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

simulator="$(xcrun simctl create "iccDEV-ios-examples-$$" "$device_type" "$runtime")"
xcrun simctl boot "$simulator"
booted=1
xcrun simctl bootstatus "$simulator" -b

build_config=Release
sanitizers=OFF
if [[ "$mode" == "sanitizers" ]]; then
  build_config=Debug
  sanitizers=ON
fi

examples=(ios-apply-preview ios-benchapply ios-clut-editor)
for example in "${examples[@]}"; do
  log="$evidence_dir/${example}.log"
  BUILD_CONFIG="$build_config" SANITIZERS="$sanitizers" \
    SIMULATOR_UDID="$simulator" \
    "$repo_root/examples/${example}/build-ios.sh" simulator --run-tests \
    > "$log" 2>&1
  grep -F 'TESTS PASS' "$log"
  if grep -E 'dyld\[[0-9]+\]: Library not loaded|AddressSanitizer:|UndefinedBehaviorSanitizer:|runtime error:' \
      "$log" >/dev/null; then
    echo "${example} emitted a loader or sanitizer diagnostic." >&2
    sed -n '1,160p' "$log"
    exit 1
  fi
  if grep -E 'warning:' "$log" |
      grep -Ev 'warning: not stripping binary because it is signed: .*/libclang_rt\.asan_[^/]+_dynamic\.dylib \(in target ' \
      > "$evidence_dir/${example}-warnings.log"; then
    echo "${example} emitted an actionable build warning." >&2
    sed -n '1,160p' "$evidence_dir/${example}-warnings.log"
    exit 1
  fi
done

printf 'iOS example %s smoke passed using %s\n' "$mode" "$runtime"
