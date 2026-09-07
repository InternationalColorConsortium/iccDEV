#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Run the native core host on a disposable iOS/watchOS simulator. A fresh
# persisted report and console sentinel, not simctl's exit code alone, decide
# success. The missing-fixture control must fail before restoring a passing run.
set -euo pipefail

if [[ $# -gt 1 ]]; then
  echo "Usage: $0 [ios|watchos]" >&2
  exit 2
fi
platform="${1:-ios}"
case "$platform" in
  ios) system=iOS; sdk=iphonesimulator; family=iPhone; deployment=17.0 ;;
  watchos) system=watchOS; sdk=watchsimulator; family='Apple Watch'; deployment=10.0 ;;
  *) echo "Usage: $0 [ios|watchos]" >&2; exit 2 ;;
esac
repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"
core="out/apple-${platform}-simulator-core"
build="out/apple-${platform}-simulator-smoke"
mkdir -p "$build"

xcrun simctl list runtimes --json > "$build/runtimes.json"
if ! jq -e --arg prefix "com.apple.CoreSimulator.SimRuntime.${system}-" \
    'any(.runtimes[]; .isAvailable and (.identifier | startswith($prefix)))' \
    "$build/runtimes.json" >/dev/null; then
  if [[ "${ICCDEV_APPLE_DOWNLOAD_RUNTIME:-0}" != 1 ]]; then
    echo "No available ${system} simulator runtime. Install it with: xcodebuild -downloadPlatform ${system}" >&2
    exit 1
  fi
  xcodebuild -downloadPlatform "$system"
  xcrun simctl list runtimes --json > "$build/runtimes.json"
fi
selection="$(jq -er --arg prefix "com.apple.CoreSimulator.SimRuntime.${system}-" \
  --arg family "$family" '
    [.runtimes[] | select(.isAvailable and (.identifier | startswith($prefix)))]
    | sort_by(.version | split(".") | map(tonumber)) | last
    | select(. != null)
    | [.identifier, (.supportedDeviceTypes | map(select(.productFamily == $family)) | first.identifier)]
    | select(all(.[]; type == "string" and length > 0)) | @tsv
  ' "$build/runtimes.json")"
IFS=$'\t' read -r runtime device_type <<< "$selection"

cmake --preset "apple-${platform}-simulator-core" -S Build/Cmake \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment"
cmake --build "$core" --parallel "$(sysctl -n hw.ncpu)"
cmake -S Build/AppleMobile -B "$build" -G Xcode \
  -DCMAKE_SYSTEM_NAME="$system" -DCMAKE_OSX_SYSROOT="$sdk" \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment" \
  -DRefIccMAX_DIR="$repo_root/$core"
xcodebuild -quiet -project "$build/IccDevCoreSmoke.xcodeproj" \
  -target IccDevCoreSmoke -configuration Release -sdk "$sdk" CODE_SIGNING_ALLOWED=NO build
app="$repo_root/$build/Release-${sdk}/IccDevCoreSmoke.app"
bundle="$(plutil -extract CFBundleIdentifier raw -o - "$app/Info.plist")"

simulator=""
launch_pid=""
booted=0
cleanup() {
  status=$?
  trap - EXIT
  if [[ -n "$launch_pid" ]] && kill -0 "$launch_pid" 2>/dev/null; then
    if ! kill -TERM "$launch_pid"; then
      echo "Unable to terminate simulator launch process ${launch_pid}." >&2
      status=1
    fi
  fi
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
simulator="$(xcrun simctl create "iccDEV-${platform}-smoke-$$" "$device_type" "$runtime")"
xcrun simctl boot "$simulator"
booted=1
xcrun simctl bootstatus "$simulator" -b
xcrun simctl install "$simulator" "$app"
data=""
installed_app=""
refresh_app_containers() {
  data="$(xcrun simctl get_app_container "$simulator" "$bundle" data)"
  installed_app="$(xcrun simctl get_app_container "$simulator" "$bundle" app)"
}
refresh_app_containers

run_case() {
  local name="$1" expected="$2"
  local launch_status=0 timed_out=0
  local result_file="$data/Documents/results.json"
  rm -f "$result_file"
  xcrun simctl launch --console-pty --terminate-running-process \
    "$simulator" "$bundle" --exit-after-tests > "$build/${name}.log" 2>&1 &
  launch_pid=$!
  for ((second = 0; second < 120; ++second)); do
    if ! kill -0 "$launch_pid" 2>/dev/null; then
      break
    fi
    sleep 1
  done
  if kill -0 "$launch_pid" 2>/dev/null; then
    timed_out=1
    kill -TERM "$launch_pid"
    sleep 1
    if kill -0 "$launch_pid" 2>/dev/null; then
      kill -KILL "$launch_pid"
    fi
  fi
  wait "$launch_pid" || launch_status=$?
  launch_pid=""
  if [[ "$timed_out" == 1 || ( "$launch_status" != 0 && "$expected" == true ) ]]; then
    echo "Simulator case ${name} failed (exit=${launch_status}, timeout=${timed_out})." >&2
    sed -n '1,120p' "$build/${name}.log"
    return 1
  fi
  if [[ ! -s "$result_file" ]]; then
    echo "Simulator case ${name} did not persist results at ${result_file}." >&2
    sed -n '1,120p' "$build/${name}.log"
    return 1
  fi
  cp "$result_file" "$build/${name}.json"
  if ! jq -e --argjson expected "$expected" '
    .passed == $expected and (.tests | length > 0) and
    (if $expected then all(.tests[]; .passed == true)
     else any(.tests[]; .test == "Bundled RGB fixture exists" and .passed == false) end)
  ' "$build/${name}.json" >/dev/null; then
    sed -n '1,120p' "$build/${name}.log"
    return 1
  fi
  if [[ "$expected" == true ]]; then
    grep -F 'ICCDEV_DEVICE_TESTS PASS (' "$build/${name}.log"
  else
    grep -F 'ICCDEV_DEVICE_TESTS FAIL (' "$build/${name}.log"
  fi
}

run_case positive true
mv "$installed_app/sRGB_D65_MAT.icc" "$installed_app/sRGB_D65_MAT.icc.disabled"
run_case missing-fixture false
xcrun simctl install "$simulator" "$app"
refresh_app_containers
run_case restored true
printf '%s simulator core smoke passed using %s\n' "$system" "$runtime"
