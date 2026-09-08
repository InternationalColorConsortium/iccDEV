# ios-clut-editor

Proof-of-concept iOS/iPadOS app for live profile and 3D CLUT editing. It lets
a reviewer select an image from the photo library or use the generated default
image, applies a bundled ICC profile chain through `CIccCmm`, applies an
editable 3D LUT, and displays source, managed, edited, and amplified-delta
panels on device. Manual-review artifact: no CTest registration and no CI gate.

## What it does

The app builds this fixed profile chain before applying the editable CLUT:

```
sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc
```

The live edit controls build an in-memory RGB 3D LUT with adjustable grid size,
interpolation mode, exposure, contrast, saturation, and warm/cool bias. The
output report records mean channel delta, max channel delta, and an FNV-1a
checksum of the edited preview bytes. `Documents/clut-editor-report.json` is
removed before each write so a failed run cannot leave stale validation output.

The UI reuses the current iOS example visual shell and ICC assets: ICC logo
header, color.org and repository links, brand-blue primary action, iPhone
stacked layout, iPad dashboard layout, native share sheet, and the same app
icon style with a CLUT-specific source label.

This is intentionally not a full mobile profile editor. It does not write ICC
profiles, edit arbitrary tags, embed an output profile in image files, parse
TIFF input, or expose the desktop JSON configuration pipeline. It is a small
on-device proof of concept for the profile-apply plus editable-CLUT UX.

## Quick start from a fresh clone

From the repository root, run the helper script in this example directory. It
builds the matching `apple-ios-*-core` static library, configures the Xcode app,
and builds the app bundle.

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV
examples/ios-clut-editor/build-ios.sh simulator --open
```

Use a booted simulator for a terminal smoke run:

```bash
examples/ios-clut-editor/build-ios.sh simulator --run-tests
```

Use an unlocked, paired iPhone or iPad for a physical-device build. Set
`TEAM_ID` and `DEVICE_ID` from Xcode and `xcrun devicectl list devices`.
Set `BUNDLE_ID` if your Apple development team requires a custom app bundle
identifier.

```bash
TEAM_ID=ABCDE12345 examples/ios-clut-editor/build-ios.sh device --open
TEAM_ID=ABCDE12345 DEVICE_ID=<udid> \
  examples/ios-clut-editor/build-ios.sh device --launch
```

## Build and run on an iPhone or iPad

Use an unlocked, paired device with Developer Mode enabled and an Apple
Development signing identity in Xcode. Set your team ID and device identifier
from `xcrun devicectl list devices`. Do not commit signing credentials,
provisioning profiles, device identifiers, generated Xcode projects, or build
outputs. Override the default `org.color.iccdev.ClutEditorPOC` bundle ID with
`BUNDLE_ID` when using the helper script, or
`-DICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER=...` when configuring CMake directly.

```bash
bundle_id="${BUNDLE_ID:-org.color.iccdev.ClutEditorPOC}"

cmake --preset apple-ios-device-core -S Build/Cmake \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-device-core --config Release --parallel

cmake -S examples/ios-clut-editor -B out/ios-clut-editor-device -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-device-core" \
  -DICCDEV_CLUTEDITOR_BUNDLE_IDENTIFIER="$bundle_id" \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID"

xcodebuild -project out/ios-clut-editor-device/IccClutEditorPOC.xcodeproj \
  -target IccClutEditorPOC -configuration Release -sdk iphoneos \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build

xcrun devicectl device install app --device "$DEVICE_ID" \
  "out/ios-clut-editor-device/Release-iphoneos/IccClutEditorPOC.app"
xcrun devicectl device process launch --device "$DEVICE_ID" \
  --console --terminate-existing --timeout 90 \
  "$bundle_id" --exit-after-tests
```

Launch without `--exit-after-tests` for hands-on visual review. The UI includes
size, CLUT grid, interpolation, exposure, contrast, saturation, warm/cool,
image-selection, reset-default, source/managed/edited/delta panels, ICC and
repository links, and a native share sheet.

The same sequence is wrapped by:

```bash
TEAM_ID=ABCDE12345 DEVICE_ID=<udid> \
  examples/ios-clut-editor/build-ios.sh device --run-tests
```

## Build and run on the simulator

```bash
cmake --preset apple-ios-simulator-core -S Build/Cmake \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-simulator-core --config Release --parallel

cmake -S examples/ios-clut-editor -B out/ios-clut-editor-sim -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-simulator-core"

xcodebuild -project out/ios-clut-editor-sim/IccClutEditorPOC.xcodeproj \
  -target IccClutEditorPOC -configuration Release -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
```

Install and launch the built `.app` in a simulator with `xcrun simctl` to
review the same editor without a physical device.

The same sequence is wrapped by:

```bash
examples/ios-clut-editor/build-ios.sh simulator --run-tests
```

## Scope and gaps

- One fixed source-to-destination RGB profile chain.
- Generated default image or photo-library image selection.
- In-memory RGB 3D LUT only; no serialized ICC output profile.
- No TIFF/libtiff dependency, JSON config, threaded CMM, or embedded-profile
  output.
- Universal `TARGETED_DEVICE_FAMILY "1,2"`: one build targets both iPhone and
  iPad. tvOS/watchOS are out of scope for this POC.
