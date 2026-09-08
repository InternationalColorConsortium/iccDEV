#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Configure and build the ios-clut-editor Xcode app from a fresh iccDEV clone.

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  examples/ios-clut-editor/build-ios.sh [simulator|device] [options]

Targets:
  simulator   Build the iOS simulator app with code signing disabled (default).
  device      Build the iPhone/iPad app. Requires TEAM_ID for command-line build.

Options:
  --open       Open the generated Xcode project after building.
  --run-tests  Install and launch with --exit-after-tests.
  --launch     Install and launch for visual review.
  --help       Show this help.

Environment:
  TEAM_ID         Apple development team ID for device builds.
  DEVICE_ID       Device identifier for device --run-tests or --launch.
  SIMULATOR_UDID  Simulator UDID for simulator --run-tests or --launch.
                  Defaults to "booted".
  BUILD_CONFIG    Xcode build configuration. Defaults to Release.
  DEPLOYMENT      iOS deployment target for the core and app. Defaults to 17.0.
  ARCH            Target architecture. Defaults to arm64.
  BUNDLE_ID       App bundle identifier. Defaults to
                  org.color.iccdev.ClutEditorPOC.

Examples:
  examples/ios-clut-editor/build-ios.sh simulator --open
  examples/ios-clut-editor/build-ios.sh simulator --run-tests
  TEAM_ID=ABCDE12345 examples/ios-clut-editor/build-ios.sh device --open
  TEAM_ID=ABCDE12345 DEVICE_ID=<udid> examples/ios-clut-editor/build-ios.sh device --launch
USAGE
}

target="${1:-simulator}"
if [[ "$target" == "--help" || "$target" == "-h" ]]; then
  usage
  exit 0
fi
if [[ "$target" == "sim" ]]; then
  target="simulator"
fi
if [[ "$target" == "iphone" || "$target" == "ipad" ]]; then
  target="device"
fi
case "$target" in
  simulator|device)
    shift || true
    ;;
  *)
    echo "error: first argument must be simulator or device" >&2
    usage >&2
    exit 2
    ;;
esac

open_project=0
launch_mode=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --open)
      open_project=1
      ;;
    --run-tests)
      launch_mode="tests"
      ;;
    --launch)
      launch_mode="visual"
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "error: unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/../.." && pwd)"
configuration="${BUILD_CONFIG:-Release}"
deployment="${DEPLOYMENT:-17.0}"
arch="${ARCH:-arm64}"
bundle_id="${BUNDLE_ID:-org.color.iccdev.ClutEditorPOC}"

if [[ "$target" == "simulator" ]]; then
  core_preset="apple-ios-simulator-core"
  core_dir="$repo_root/out/apple-ios-simulator-core"
  app_build_dir="$repo_root/out/ios-clut-editor-sim"
  sdk="iphonesimulator"
  sign_args=(CODE_SIGNING_ALLOWED=NO)
else
  core_preset="apple-ios-device-core"
  core_dir="$repo_root/out/apple-ios-device-core"
  app_build_dir="$repo_root/out/ios-clut-editor-device"
  sdk="iphoneos"
  sign_args=(-allowProvisioningUpdates -allowProvisioningDeviceRegistration)
  if [[ -z "${TEAM_ID:-}" ]]; then
    echo "error: TEAM_ID is required for device builds" >&2
    echo "hint: list devices with: xcrun devicectl list devices" >&2
    exit 2
  fi
fi

cmake --preset "$core_preset" -S "$repo_root/Build/Cmake" \
  -DCMAKE_OSX_ARCHITECTURES="$arch" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment"
cmake --build "$core_dir" --config "$configuration" --parallel

app_config_args=(
  -S "$script_dir"
  -B "$app_build_dir"
  -G Xcode
  -DCMAKE_SYSTEM_NAME=iOS
  -DCMAKE_OSX_SYSROOT="$sdk"
  -DCMAKE_OSX_ARCHITECTURES="$arch"
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$deployment"
  -DRefIccMAX_DIR="$core_dir"
  -DICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER="$bundle_id"
)
if [[ "$target" == "device" ]]; then
  app_config_args+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$TEAM_ID")
fi
cmake "${app_config_args[@]}"

xcodebuild -project "$app_build_dir/IccClutEditorPOC.xcodeproj" \
  -target IccClutEditorPOC \
  -configuration "$configuration" \
  -sdk "$sdk" \
  "${sign_args[@]}" \
  build

app_path="$app_build_dir/$configuration-$sdk/IccClutEditorPOC.app"
if [[ "$open_project" -eq 1 ]]; then
  open "$app_build_dir/IccClutEditorPOC.xcodeproj"
fi

if [[ -n "$launch_mode" ]]; then
  app_args=()
  if [[ "$launch_mode" == "tests" ]]; then
    app_args=(--exit-after-tests)
  fi
  if [[ "$target" == "simulator" ]]; then
    simulator="${SIMULATOR_UDID:-booted}"
    xcrun simctl install "$simulator" "$app_path"
    xcrun simctl launch --console-pty --terminate-running-process \
      "$simulator" "$bundle_id" "${app_args[@]}"
  else
    if [[ -z "${DEVICE_ID:-}" ]]; then
      echo "error: DEVICE_ID is required for device launch" >&2
      echo "hint: list devices with: xcrun devicectl list devices" >&2
      exit 2
    fi
    xcrun devicectl device install app --device "$DEVICE_ID" "$app_path"
    xcrun devicectl device process launch --device "$DEVICE_ID" \
      --console --terminate-existing --timeout 90 "$bundle_id" "${app_args[@]}"
  fi
fi

echo "Built $app_path"
