# Pinned OpenImageIO ICC, EXIF, and JPEG2000 QA

This directory provides the OpenImageIO compatibility lane for iccDEV issue
#2657. It is isolated from the main iccDEV build and CTest inventory.

The lane pins AcademySoftwareFoundation/OpenImageIO commit
`8004015ace460bf7e9019514f6d8c6c677e6e7ae`. Before patching, the QA driver
requires these confirmed outcomes:

- a big-endian EXIF write performs a misaligned typed access under UBSan;
- a scalar Canon MakerNote value aborts because an element count is treated as
  a byte count, while an indexed value is written in the wrong byte order;
- strict JPEG input accepts an ICC profile whose header size exceeds its data;
- an `mluc` description reads its record and string from the following tag;
- a small JPEG2000 encoder failure is reported as a successful write; and
- every supported `jpeg2000:ProgressionOrder` value reaches OpenJPEG with an
  invalid empty progression string and terminates on its assertion.

The tracked patch makes the EXIF endian conversion alignment-safe, preserves
Canon element sizes and requested byte order, propagates strict JPEG and
JPEG2000 failures, bounds `mluc` parsing to the declared tag, reads the string
progression attribute through `ParamValue`, and makes the CPRL branch
reachable. The fixed contract also requires all five progression orders to
produce readable files and an Adobe RGB profile to survive a JP2 round-trip
byte-for-byte. The latter is the positive control for OpenImageIO issue #4608
and its merged fix, PR #5419; it is not presented as a new patch here.

OpenImageIO issue assignment for the six newly tracked faults is pending. The
iccDEV reproduction and CI contract is registered under #2657; update this
document and `.github/ci/regression/README.md` when upstream identifiers exist.

## Local build and CTest

Run from the iccDEV repository root with an external pinned checkout:

```bash
set -euo pipefail
OIIO_REVISION=8004015ace460bf7e9019514f6d8c6c677e6e7ae
OIIO_SOURCE="$(mktemp -d /tmp/iccdev-openimageio.XXXXXX)"
git -C "$OIIO_SOURCE" init
git -C "$OIIO_SOURCE" remote add origin \
  https://github.com/AcademySoftwareFoundation/OpenImageIO.git
git -C "$OIIO_SOURCE" fetch --depth=1 origin "$OIIO_REVISION"
git -C "$OIIO_SOURCE" checkout --detach FETCH_HEAD
test "$(git -C "$OIIO_SOURCE" rev-parse HEAD)" = "$OIIO_REVISION"
git -C "$OIIO_SOURCE" apply --check \
  .github/ci/tooling/openimageio/bootstrap-patches/openimageio-yaml-cpp-cstdint.patch
git -C "$OIIO_SOURCE" apply \
  .github/ci/tooling/openimageio/bootstrap-patches/openimageio-yaml-cpp-cstdint.patch

OIIO_BUILD=/tmp/iccdev-openimageio-build
OIIO_QA=/tmp/iccdev-openimageio-qa
CMAKE_BUILD_PARALLEL_LEVEL="$(nproc)"
export CMAKE_BUILD_PARALLEL_LEVEL
mkdir -p "$OIIO_BUILD" "$OIIO_QA"
cmake -S "$OIIO_SOURCE" -B "$OIIO_BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DOpenImageIO_BUILD_MISSING_DEPS='required;OpenJPEG' \
  -DSANITIZE=address,undefined \
  -DUSE_PYTHON=OFF \
  -DUSE_QT=OFF
cmake --build "$OIIO_BUILD" --target oiiotool iinfo \
  --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"

ASAN_OPTIONS=halt_on_error=1:abort_on_error=1:detect_leaks=0 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 .github/ci/tooling/openimageio/qa/openimageio_icc_qa.py \
    --mode vulnerable \
    --oiiotool "$OIIO_BUILD/bin/oiiotool" \
    --iinfo "$OIIO_BUILD/bin/iinfo" \
    --source-dir "$OIIO_SOURCE" \
    --work-dir "$OIIO_BUILD/vulnerable-contract"

for patch_file in "$PWD"/.github/ci/tooling/openimageio/patches/*.patch; do
  git -C "$OIIO_SOURCE" apply --check "$patch_file"
  git -C "$OIIO_SOURCE" apply "$patch_file"
done
cmake --build "$OIIO_BUILD" --target oiiotool iinfo \
  --parallel "$CMAKE_BUILD_PARALLEL_LEVEL"

cmake -S .github/ci/tooling/openimageio/qa \
  -B "$OIIO_QA" \
  -DOPENIMAGEIO_SOURCE_DIR="$OIIO_SOURCE" \
  -DOPENIMAGEIO_BUILD_DIR="$OIIO_BUILD"
ctest --test-dir "$OIIO_QA" -N --no-tests=error
ctest --test-dir "$OIIO_QA" \
  -R '^iccdev\.openimageio-icc-qa$' \
  --output-on-failure \
  --no-tests=error
```

The OpenImageIO configure must find JPEG, PNG, and OpenJPEG 2.5.4. The QA
driver deliberately fails its vulnerable contract if the JPEG2000 plugin is
missing, preventing absent optional coverage from looking green.

Use `.github/workflows/ci-openimageio-icc-smoke.yml` for hosted validation.
It resolves the unified iccDEV image to an immutable digest, applies the
build-only yaml-cpp compatibility patch, proves the vulnerable contract,
applies the behavioral patch with `git apply --check`, rebuilds under
ASan+UBSan, and runs the fixed CTest.
