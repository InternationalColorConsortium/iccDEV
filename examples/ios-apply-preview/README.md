# ios-apply-preview

Proof-of-concept iOS/iPadOS app inspired by `iccApplyProfiles`. It generates
an RGB image in memory, applies the bundled `sRGB_D65_MAT.icc` profile through
`CIccCmm`, and displays source, applied, and amplified delta panels on device.
Manual-review artifact: no CTest registration and no CI gate.

## What it does

Builds a fixed two-link chain:

```
sRGB_D65_MAT.icc -> sRGB_D65_MAT.icc
```

The app renders a generated RGB ramp and swatch grid, applies the profile
chain with relative colorimetric intent, and reports mean channel delta, max
channel delta, and an FNV-1a checksum of the applied preview bytes. A native
`Share Report` button sends the text report to Messages, Mail, AirDrop, or an
installed share extension.

This is intentionally not a full mobile port of the desktop
`iccApplyProfiles` CLI. It does not parse TIFF files, write TIFF output, embed
profiles, or consume JSON configuration. It is a visual CMM apply preview that
keeps the first mobile example limited to IccProfLib and UIKit.

## Build and run on an iPhone or iPad

Use an unlocked, paired device with Developer Mode enabled and an Apple
Development signing identity in Xcode. Set your team ID and device identifier
from `xcrun devicectl list devices`. Do not commit signing credentials,
provisioning profiles, device identifiers, generated Xcode projects, or build
outputs.

```bash
cmake --preset apple-ios-device-core -S Build/Cmake \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-device-core --config Release --parallel

cmake -S examples/ios-apply-preview -B out/ios-apply-preview-device -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-device-core" \
  -DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM="$TEAM_ID"

xcodebuild -project out/ios-apply-preview-device/IccApplyPreviewPOC.xcodeproj \
  -target IccApplyPreviewPOC -configuration Release -sdk iphoneos \
  -allowProvisioningUpdates -allowProvisioningDeviceRegistration build

xcrun devicectl device install app --device "$DEVICE_ID" \
  "out/ios-apply-preview-device/Release-iphoneos/IccApplyPreviewPOC.app"
xcrun devicectl device process launch --device "$DEVICE_ID" \
  --console --terminate-existing --timeout 90 \
  org.color.iccdev.ApplyPreviewPOC --exit-after-tests
```

Launch without `--exit-after-tests` for hands-on visual review on iPhone or
iPad. The UI includes size and interpolation selectors, source/applied/delta
image panels, ICC and repository links, and a native share sheet.

## Build and run on the simulator

```bash
cmake --preset apple-ios-simulator-core -S Build/Cmake \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0
cmake --build out/apple-ios-simulator-core --config Release --parallel

cmake -S examples/ios-apply-preview -B out/ios-apply-preview-sim -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DRefIccMAX_DIR="$PWD/out/apple-ios-simulator-core"

xcodebuild -project out/ios-apply-preview-sim/IccApplyPreviewPOC.xcodeproj \
  -target IccApplyPreviewPOC -configuration Release -sdk iphonesimulator \
  CODE_SIGNING_ALLOWED=NO build
```

Install and launch the built `.app` in a simulator with `xcrun simctl` to
review the same visual panels without a physical device.

## Scope and gaps

- One generated image and one bundled matrix/curve profile chain.
- No TIFF/libtiff dependency, image-file picker, JSON config, threaded CMM, or
  embedded-profile output.
- Universal `TARGETED_DEVICE_FAMILY "1,2"`: one build targets both iPhone and
  iPad. tvOS/watchOS are out of scope for this POC.
