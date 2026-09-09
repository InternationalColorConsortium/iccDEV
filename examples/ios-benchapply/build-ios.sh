#!/usr/bin/env bash
# Copyright (c) 2026 International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
#
# Configure and build the ios-benchapply Xcode app from a fresh iccDEV clone.

set -euo pipefail

usage() {
  cat <<'USAGE'
Usage:
  examples/ios-benchapply/build-ios.sh [simulator|device] [options]

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
                  org.color.iccdev.BenchApplyPOC.

Examples:
  examples/ios-benchapply/build-ios.sh simulator --open
  examples/ios-benchapply/build-ios.sh simulator --run-tests
  TEAM_ID="$TEAM_ID" examples/ios-benchapply/build-ios.sh device --open
  TEAM_ID="$TEAM_ID" DEVICE_ID="$DEVICE_ID" examples/ios-benchapply/build-ios.sh device --launch
USAGE
}

is_placeholder_team_id() {
  case "$1" in
    ABCDE12345|XXXXXXXXXX|YOURTEAMID|YOUR_TEAM_ID|TEAM_ID|DEVELOPMENT_TEAM|\
      "<team-id>"|"<TEAM_ID>"|"<team_id>"|"<your-team-id>"|"<YOUR_TEAM_ID>"|\
      *[Yy][Oo][Uu][Rr][Tt][Ee][Aa][Mm]*|\
      *[Yy][Oo][Uu][Rr][_-][Tt][Ee][Aa][Mm]*|\
      *[Pp][Ll][Aa][Cc][Ee][Hh][Oo][Ll][Dd][Ee][Rr]*)
      return 0
      ;;
  esac
  return 1
}

is_placeholder_bundle_id() {
  case "$1" in
    BUNDLE_ID|YOURBUNDLEID|YOUR_BUNDLE_ID|"<bundle-id>"|"<BUNDLE_ID>"|\
      "<bundle_id>"|"<your-bundle-id>"|"<YOUR_BUNDLE_ID>"|com.yourteam.*|\
      *[Yy][Oo][Uu][Rr][Tt][Ee][Aa][Mm]*|\
      *[Yy][Oo][Uu][Rr][_-][Tt][Ee][Aa][Mm]*|\
      *[Yy][Oo][Uu][Rr][Bb][Uu][Nn][Dd][Ll][Ee]*|\
      *[Yy][Oo][Uu][Rr][_-][Bb][Uu][Nn][Dd][Ll][Ee]*|\
      com.example.*|org.example.*|\
      *[Pp][Ll][Aa][Cc][Ee][Hh][Oo][Ll][Dd][Ee][Rr]*)
      return 0
      ;;
  esac
  return 1
}

target="simulator"
if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
  usage
  exit 0
fi
if [[ "${1:-}" != "" && "${1:-}" != --* ]]; then
  target="$1"
  shift
fi
if [[ "$target" == "sim" ]]; then
  target="simulator"
fi
if [[ "$target" == "iphone" || "$target" == "ipad" ]]; then
  target="device"
fi
case "$target" in
  simulator|device)
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
default_bundle_id="org.color.iccdev.BenchApplyPOC"
bundle_id="${BUNDLE_ID:-$default_bundle_id}"
bundle_id_is_placeholder=0
if is_placeholder_bundle_id "$bundle_id"; then
  bundle_id_is_placeholder=1
fi
if [[ "$target" != "device" && "$bundle_id_is_placeholder" -eq 1 ]]; then
  echo "warning: ignoring placeholder BUNDLE_ID for unsigned $target build" >&2
  bundle_id="$default_bundle_id"
fi

if [[ "$target" == "simulator" ]]; then
  core_preset="apple-ios-simulator-core"
  core_dir="$repo_root/out/apple-ios-simulator-core"
  app_build_dir="$repo_root/out/ios-benchapply-sim"
  sdk="iphonesimulator"
  sign_args=(CODE_SIGNING_ALLOWED=NO)
else
  core_preset="apple-ios-device-core"
  core_dir="$repo_root/out/apple-ios-device-core"
  app_build_dir="$repo_root/out/ios-benchapply-device"
  sdk="iphoneos"
  sign_args=(-allowProvisioningUpdates -allowProvisioningDeviceRegistration)
  if [[ -z "${TEAM_ID:-}" ]]; then
    echo "error: TEAM_ID is required for device builds" >&2
    echo "hint: list devices with: xcrun devicectl list devices" >&2
    exit 2
  fi
  if is_placeholder_team_id "$TEAM_ID"; then
    echo "error: TEAM_ID still contains a placeholder value" >&2
    echo "hint: use the Apple Development team shown by Xcode signing" >&2
    exit 2
  fi
  if [[ "$bundle_id_is_placeholder" -eq 1 ]]; then
    echo "error: BUNDLE_ID still contains a placeholder value" >&2
    echo "hint: omit BUNDLE_ID for the default or use a registered app ID" >&2
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
  -DICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER="$bundle_id"
)
if [[ "$target" == "device" ]]; then
  app_config_args+=("-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=$TEAM_ID")
fi
cmake "${app_config_args[@]}"

xcodebuild -project "$app_build_dir/IccBenchApplyPOC.xcodeproj" \
  -target IccBenchApplyPOC \
  -configuration "$configuration" \
  -sdk "$sdk" \
  "${sign_args[@]}" \
  build

app_path="$app_build_dir/$configuration-$sdk/IccBenchApplyPOC.app"
if [[ "$open_project" -eq 1 ]]; then
  open "$app_build_dir/IccBenchApplyPOC.xcodeproj"
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
