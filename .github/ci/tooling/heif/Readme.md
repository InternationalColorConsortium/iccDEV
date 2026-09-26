# HEIF ICC Carrier QA

`iccHeifDump` enumerates `colr` properties through the Nokia HEIF reader,
reports embedded ICC sizes and SHA-256 digests, and optionally extracts the
profiles. The QA wrapper pins Nokia HEIF v3.7.1 commit
`503194eb85e13434b54797bab9d82ad7f88fd35b`; Homebrew `libheif` exposes a
different API and is not a substitute for this dependency.

## Local build and CTest

Run from the iccDEV repository root:

```bash
HEIF_REVISION=503194eb85e13434b54797bab9d82ad7f88fd35b
HEIF_TAG=v3.7.1
HEIF_SOURCE="$(mktemp -d /tmp/iccdev-nokia-heif.XXXXXX)"
git -C "$HEIF_SOURCE" init
git -C "$HEIF_SOURCE" remote add origin https://github.com/nokiatech/heif.git
git -C "$HEIF_SOURCE" fetch --depth=1 origin \
  "refs/tags/${HEIF_TAG}:refs/tags/${HEIF_TAG}"
git -C "$HEIF_SOURCE" checkout --detach "${HEIF_TAG}^{commit}"
test "$(git -C "$HEIF_SOURCE" rev-parse HEAD)" = "$HEIF_REVISION"
for patch_file in "$PWD"/.github/ci/tooling/heif/patchs/*.patch; do
  git -C "$HEIF_SOURCE" apply --check "$patch_file"
  git -C "$HEIF_SOURCE" apply "$patch_file"
done

cmake -S .github/ci/tooling/heif/qa -B /tmp/iccdev-heif-qa \
  -DHEIF_SOURCE_DIR="$HEIF_SOURCE" \
  -DICCDEV_HEIF_ENABLE_SANITIZERS=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -Wno-deprecated
cmake --build /tmp/iccdev-heif-qa --target iccHeifDump --parallel
ctest --test-dir /tmp/iccdev-heif-qa -N --no-tests=error
ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir /tmp/iccdev-heif-qa \
    -R '^iccdev\.heif-carrier-qa$' \
    --output-on-failure \
    --no-tests=error
```

The CTest runs all 16 tracked carriers, valid and invalid property probes, CLI
error controls, and a byte-for-byte extraction check. It treats exit values
128 and above or any ASan/UBSan diagnostic as a failure.

The manually dispatched `Nokia HEIF ICC carrier smoke` workflow in
`.github/workflows/ci-nokia-heif-icc-smoke.yml` runs this standalone build in
the unified regression image. It is intentionally separate from the general
iccDEV pull-request and tool-test workflows.

## Known upstream patches

`overflow-meta-largesize.heic` exposes signed overflow in the pinned Nokia
reader when it adds a box start offset to an `INT64_MAX` `largesize`. The
tracked patch rejects that box using subtraction-based bounds checks before
the addition can occur. Keep the patch dry-run and sanitizer carrier test
together when changing the upstream revision.

The patch stack also provides Nokia headers with their direct `<cstdint>`
dependency and checks that decoded unsigned durations fit the reader's signed
timestamp type. The dedicated GCC smoke treats compiler warnings and sanitizer
findings as blocking.
