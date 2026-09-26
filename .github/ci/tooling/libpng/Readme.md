# Pinned libpng iCCP Fault QA

This directory provides an isolated, manually dispatched compatibility lane
for libpng iCCP parsing. It does not alter the main iccDEV build or its CTest
inventory.

The lane pins pnggroup/libpng `v1.6.58` at
`3061454d980de7d53608f594194cfac722721d2a`. The unpatched pin must reproduce
four rejected-profile retention faults before the tracked patch is applied:

- decompressed profile data beyond the ICC header size;
- an invalid Adler-32 checksum;
- a missing Adler-32 trailer and missing `Z_STREAM_END`;
- an invalid PNG iCCP chunk CRC under the default CRC policy.

After patching, all four profiles must be absent from `pngtest` output. A valid
control remains byte-identical, and `pngtest --relaxed` still retains the
bad-CRC profile because that mode explicitly selects `PNG_CRC_QUIET_USE`.

Run the proof locally with an external libpng checkout:

```bash
LIBPNG_REVISION=3061454d980de7d53608f594194cfac722721d2a
LIBPNG_TAG=v1.6.58
LIBPNG_SOURCE="$(mktemp -d /tmp/iccdev-libpng.XXXXXX)"
git -C "$LIBPNG_SOURCE" init
git -C "$LIBPNG_SOURCE" remote add origin https://github.com/pnggroup/libpng.git
git -C "$LIBPNG_SOURCE" fetch --depth=1 origin \
  "refs/tags/${LIBPNG_TAG}:refs/tags/${LIBPNG_TAG}"
git -C "$LIBPNG_SOURCE" checkout --detach "${LIBPNG_TAG}^{commit}"
test "$(git -C "$LIBPNG_SOURCE" rev-parse HEAD)" = "$LIBPNG_REVISION"

cmake -S .github/ci/tooling/libpng/qa -B /tmp/iccdev-libpng-build \
  -DLIBPNG_SOURCE_DIR="$LIBPNG_SOURCE" \
  -DICCDEV_LIBPNG_ENABLE_SANITIZERS=ON
cmake --build /tmp/iccdev-libpng-build --target pngtest --parallel
ASAN_OPTIONS=halt_on_error=1:detect_leaks=0 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  python3 .github/ci/tooling/libpng/qa/libpng_iccp_qa.py \
    --mode vulnerable \
    --pngtest /tmp/iccdev-libpng-build/libpng/pngtest \
    --profile .github/ci/tooling/libpng/fixtures/valid-hybrid-rgb.icc.b64 \
    --work-dir /tmp/iccdev-libpng-build/vulnerable-contract
for patch_file in "$PWD"/.github/ci/tooling/libpng/patches/*.patch; do
  git -C "$LIBPNG_SOURCE" apply --check "$patch_file"
  git -C "$LIBPNG_SOURCE" apply "$patch_file"
done
cmake --build /tmp/iccdev-libpng-build --target pngtest --parallel
ctest --test-dir /tmp/iccdev-libpng-build -N --no-tests=error
ctest --test-dir /tmp/iccdev-libpng-build \
  -R '^iccdev\.libpng-iccp-qa$' --output-on-failure --no-tests=error
```

Use `.github/workflows/ci-libpng-iccp-smoke.yml` for hosted validation. It
first proves the vulnerable contract, applies the tracked patch with
`git apply --check`, rebuilds under ASan+UBSan, and runs the fixed CTest.

Upstream issue assignment is pending maintainer filing. This lane is separate
from iccDEV PAWG profile handling and does not claim that pnggroup/libpng #211
covers stream completion or retention after a CRC error.
