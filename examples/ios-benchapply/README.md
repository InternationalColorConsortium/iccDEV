# ios-benchapply

Proof-of-concept iOS/iPadOS app that runs the `iccBenchApply` apply-path
benchmark primitives on device hardware. Manual-review artifact: no CTest
registration and no CI gate. Build and run by hand, then review on device.

## What it does

Builds a fixed two-link chain (the bundled `sRGB_D65_MAT.icc` matrix/curve
profile applied to itself, relative colorimetric), times
`CIccCmm::Apply` over 65536 pixels for 3 repeats, and reports median/min/max
Mpx/s plus an FNV-1a checksum of the output -- using the exact timing,
checksum, and buffer-fill primitives (`BenchTimer.h`) the desktop
`iccBenchApply` CLI uses, so the on-device number is comparable in kind, not
just in appearance, to a desktop run. See `Tools/CmdLine/IccBenchApply/Readme.md`
for what those primitives measure and how not to misread them.

This is intentionally not a full port of `iccBenchApply -suite`: the desktop
case table and `-suite`/`-perxform`/`-leaf`/`-csv` argv handling resolve a
`Testing/` tree of generated profiles that does not exist in a sandboxed
mobile app. The POC uses the one ICC profile already bundled for the
`Build/AppleMobile` core smoke app.

## Quick start from a fresh clone

From the repository root, run the helper script in this example directory. It
builds the matching `apple-ios-*-core` static library, configures the Xcode app,
and builds the app bundle.

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV
examples/ios-benchapply/build-ios.sh simulator --open
```

Use a booted simulator for a terminal smoke run:

```bash
examples/ios-benchapply/build-ios.sh simulator --run-tests
```

Use an unlocked, paired iPhone or iPad for a physical-device build. Set
`TEAM_ID` and `DEVICE_ID` from Xcode and `xcrun devicectl list devices`.
Set `BUNDLE_ID` if your Apple development team requires a custom app bundle
identifier. The helper rejects placeholder signing values before building so a
mistyped local session does not waste a full device build. Unsigned simulator
runs ignore a globally exported placeholder `BUNDLE_ID` and use the default
bundle ID instead.

```bash
TEAM_ID="$TEAM_ID" examples/ios-benchapply/build-ios.sh device --open
TEAM_ID="$TEAM_ID" DEVICE_ID="$DEVICE_ID" \
  examples/ios-benchapply/build-ios.sh device --launch
```

## Build and run on an iPhone or iPad

Use an unlocked, paired device with Developer Mode enabled and an Apple
Development signing identity in Xcode. Set your team ID and device identifier
from `xcrun devicectl list devices`. Do not commit signing credentials,
provisioning profiles, device identifiers, generated Xcode projects, or build
outputs. Override the default `org.color.iccdev.BenchApplyPOC` bundle ID with
`BUNDLE_ID` when using the helper script, or
`-DICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER=...` when configuring CMake directly.
Do not export placeholder values; use values that Xcode can sign for the
selected device.

```bash
bundle_id="${BUNDLE_ID:-org.color.iccdev.BenchApplyPOC}"

cmake --preset apple-ios-device-core -S Build/Cmake \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-device-core --config Release --parallel

cmake -S examples/ios-benchapply -B out/ios-benchapply-device -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-device-core" \
  -DICCDEV_BENCHAPPLY_BUNDLE_IDENTIFIER="$bundle_id" \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID"

xcodebuild -project out/ios-benchapply-device/IccBenchApplyPOC.xcodeproj \
  -target IccBenchApplyPOC -configuration Release -sdk iphoneos \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build

xcrun devicectl device install app --device "$DEVICE_ID" \
  "out/ios-benchapply-device/Release-iphoneos/IccBenchApplyPOC.app"
xcrun devicectl device process launch --device "$DEVICE_ID" \
  --console --terminate-existing --timeout 90 \
  "$bundle_id" --exit-after-tests
```

The app uses the ICC logo SVG requested from `https://static.color.org/img/icc-logo.f367b829dca5.svg`.
The visible header bundles a rendered PNG copy for deterministic UIKit loading,
and the iOS home screen uses a generated in-repository app icon asset.

The on-screen UI includes the ICC logo, `color.org` and iccDEV repository
links, selectors for quick, standard, and large workloads, a `Run Benchmark`
button, and a native
`Share Results` button.

The on-screen summary includes a banner with the program name, run date/time,
app version/build, IccProfLib version string including the git-derived suffix,
and iOS version. The `Share Results` button opens the native iOS share sheet
for Messages, Mail, AirDrop, and installed share extensions.

The on-screen summary and `Documents/bench-results.json` (readable via
`xcrun devicectl device copy from`) report the median/min/max Mpx/s and
checksum. `--exit-after-tests` makes the process exit 0/1 for automation; a
normal launch (without that argument) leaves the report visible on screen.

The same sequence is wrapped by:

```bash
TEAM_ID="$TEAM_ID" DEVICE_ID="$DEVICE_ID" \
  examples/ios-benchapply/build-ios.sh device --run-tests
```

## Build and run on the simulator (no signing needed)

```bash
cmake --preset apple-ios-simulator-core -S Build/Cmake \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-simulator-core --config Release --parallel

cmake -S examples/ios-benchapply -B out/ios-benchapply-sim -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-simulator-core"

xcodebuild -project out/ios-benchapply-sim/IccBenchApplyPOC.xcodeproj \
  -target IccBenchApplyPOC -configuration Release -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
```

Install and launch the built `.app` in a simulator with `xcrun simctl` to
review the same report without a physical device.

The same sequence is wrapped by:

```bash
examples/ios-benchapply/build-ios.sh simulator --run-tests
```

## Scope and gaps

- One fixed chain and one matrix/curve profile. The UI can vary pixel/repeat
  budget, but it is not the desktop
  `-suite` table, not `-perxform`/`-leaf` breakdowns, not `-csv`.
- No CTest registration and no CI gate; this is reviewed manually on device.
- Universal `TARGETED_DEVICE_FAMILY "1,2"`: one build targets both iPhone and
  iPad. tvOS/watchOS are out of scope for this POC.
