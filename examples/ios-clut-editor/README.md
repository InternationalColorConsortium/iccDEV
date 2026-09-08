# ios-clut-editor

Proof-of-concept iOS/iPadOS and Mac Catalyst app for live profile and 3D CLUT
editing. It lets a reviewer select an image from the photo library or use the
generated default image, applies a bundled ICC profile chain through `CIccCmm`,
applies an editable 3D LUT, and displays source, managed, edited, and
amplified-delta panels on device or on an Apple Silicon Mac. Manual-review
artifact: no CTest registration and no CI gate.

## What it does

The app builds this fixed profile chain before applying the editable CLUT:

```
sRGB_v4_ICC_preference.icc -> sRGB_D65_MAT.icc
```

The live edit controls build an in-memory RGB 3D LUT with adjustable grid size,
interpolation mode, exposure, contrast, saturation, and warm/cool bias. The
output report records mean channel delta, max channel delta, and an FNV-1a
checksum of the edited preview bytes. `Documents/clut-editor-report.json` is
removed before the latest accepted render writes its report so a failed or stale
background run cannot leave misleading validation output.

The UI reuses the current iOS example visual shell and ICC assets: ICC logo
header, color.org and repository links, brand-blue primary action, compact
iPhone preview grid, iPad/Mac dashboard layout, educational light/dark/system
appearance toggle, native share sheets, and the same app icon style with a
CLUT-specific source label.

This is intentionally not a full mobile profile editor. It does not write ICC
profiles, edit arbitrary tags, embed an output profile in image files, parse
TIFF input, or expose the desktop JSON configuration pipeline. It is a small
on-device proof of concept for the profile-apply plus editable-CLUT UX.

## Quick start from a fresh clone

From the repository root, run the helper script in this example directory. It
builds the matching iOS or Mac Catalyst static library, configures the Xcode
app, and builds the app bundle.

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV
examples/ios-clut-editor/build-ios.sh simulator --open
```

Use a booted simulator for a terminal smoke run:

```bash
examples/ios-clut-editor/build-ios.sh simulator --run-tests
```

Run the same UIKit app as a Mac Catalyst app on an Apple Silicon Mac:

```bash
examples/ios-clut-editor/build-ios.sh maccatalyst --run-tests
examples/ios-clut-editor/build-ios.sh maccatalyst --launch
```

Use an unlocked, paired iPhone or iPad for a physical-device build. Set
`TEAM_ID` and `DEVICE_ID` from Xcode and `xcrun devicectl list devices`.
Set `BUNDLE_ID` if your Apple development team requires a custom app bundle
identifier. The helper rejects placeholder signing values before building so a
mistyped local session does not waste a full device build. Unsigned simulator
and Mac Catalyst runs ignore a globally exported placeholder `BUNDLE_ID` and
use the default bundle ID instead.

```bash
TEAM_ID="$TEAM_ID" examples/ios-clut-editor/build-ios.sh device --open
TEAM_ID="$TEAM_ID" DEVICE_ID="$DEVICE_ID" \
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
Do not export the placeholder values shown below; use values that Xcode can
sign for the selected device.

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
image-selection, reset-default, light/dark/system appearance switching,
source/managed/edited/delta panels, ICC and repository links, a native report
share sheet, and an edited-image export share sheet for Files, Photos, AirDrop,
and installed image extensions. iPhone keeps the same controls but uses a
half-height logo/banner, shorter action labels, and shorter two-column preview
rows so sliders and images fit with less scrolling; iPad and Mac Catalyst keep
the wide dashboard layout when the current window is wide enough, and compact
Split View or resized Catalyst windows keep the overflowing controls in a
scrollable compact layout. The UI stays intentionally simple and educational for
students and younger Apple developers learning colorimetry concepts.

The same sequence is wrapped by:

```bash
TEAM_ID="$TEAM_ID" DEVICE_ID="$DEVICE_ID" \
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

## Build and run on Apple Silicon Mac

The Mac path builds the same Objective-C++ UIKit app for Mac Catalyst and links
it against a matching Mac Catalyst `IccProfLib2-static` archive. It does not
reuse an iOS device or simulator archive.

```bash
examples/ios-clut-editor/build-ios.sh maccatalyst --run-tests
examples/ios-clut-editor/build-ios.sh maccatalyst --launch
```

The direct sequence is:

```bash
cmake -S Build/Cmake -B out/apple-maccatalyst-core -G Xcode \
  -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_SHARED_LIBS=OFF \
  -DENABLE_STATIC_LIBS=ON -DENABLE_TOOLS=OFF -DENABLE_TESTS=OFF \
  -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=OFF -DICC_USE_ZLIB=OFF \
  -DENABLE_IMAGE_TOOLS=OFF -DENABLE_WXWIDGETS=OFF \
  -DENABLE_CMM_TOOLS=OFF -DENABLE_IIS_TOOLS=OFF \
  -DCMAKE_XCODE_ATTRIBUTE_SUPPORTS_MACCATALYST=YES \
  -DCMAKE_XCODE_ATTRIBUTE_DERIVE_MACCATALYST_PRODUCT_BUNDLE_IDENTIFIER=NO \
  -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=17.0 \
  -DCMAKE_XCODE_ATTRIBUTE_MACOSX_DEPLOYMENT_TARGET=14.0
xcodebuild -project out/apple-maccatalyst-core/RefIccMAX.xcodeproj \
  -scheme IccProfLib2-static -configuration Release -sdk macosx \
  -destination 'generic/platform=macOS,variant=Mac Catalyst' clean build

cmake -S examples/ios-clut-editor -B out/ios-clut-editor-maccatalyst \
  -G Xcode -DCMAKE_OSX_SYSROOT=macosx -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DRefIccMAX_DIR="$PWD/out/apple-maccatalyst-core" \
  -DICCDEV_CLUTEDITOR_ENABLE_MACCATALYST=ON
xcodebuild -project out/ios-clut-editor-maccatalyst/IccClutEditorPOC.xcodeproj \
  -scheme IccClutEditorPOC -configuration Release -sdk macosx \
  -destination 'generic/platform=macOS,variant=Mac Catalyst' \
  CODE_SIGNING_ALLOWED=NO build
app_path=out/ios-clut-editor-maccatalyst/Release-maccatalyst/IccClutEditorPOC.app
if [ ! -d "$app_path" ]; then
  app_path=out/ios-clut-editor-maccatalyst/Release/IccClutEditorPOC.app
fi
"$app_path/Contents/MacOS/IccClutEditorPOC" --exit-after-tests
```

## Scope and gaps

- One fixed source-to-destination RGB profile chain.
- Generated default image or photo-library image selection.
- In-memory RGB 3D LUT only; no serialized ICC output profile.
- No TIFF/libtiff dependency, JSON config, threaded CMM, or embedded-profile
  output.
- Universal `TARGETED_DEVICE_FAMILY "1,2"` for iPhone and iPad, plus a Mac
  Catalyst helper target for Apple Silicon Macs. tvOS/watchOS are out of scope
  for this POC.
