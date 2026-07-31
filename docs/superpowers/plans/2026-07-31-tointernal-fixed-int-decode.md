# ToInternalEncoding Fixed-Integer Decode Fix — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `CIccCmm::ToInternalEncoding` correctly *decode* fixed-integer (8-bit / 16-bit / 16-bit-v2) color samples into the internal representation, so it is the true inverse of `FromInternalEncoding` for every color space (including 7CLR), instead of saturating everything to full.

**Architecture:** The float overload of `ToInternalEncoding` currently re-encodes its input through `icFtoU8`/`icFtoU16`, which clamp to [0,1] and so saturate any real 0–255 / 0–65535 sample. Add two file-local "decode an already-encoded integer sample" helpers and swap them in across the Lab, XYZ, and default/CLR branches. Correctness is pinned by a new round-trip regression test asserting `From(To(x)) == x`.

**Tech Stack:** C++ (IccProfLib), CMake + CTest regression harness under `.github/ci/regression/`, MSVC / VS2022 build in `out/vs2022-x64`.

## Global Constraints

- Do **not** modify `CIccCmm::FromInternalEncoding` — it is the reference of truth for the round-trip invariant.
- Correctness definition: for every (space, encoding) pair supported, `From(To(encoded)) == encoded` within half an LSB, and the fix touches only the erroneous `icFtoU8`/`icFtoU16` calls plus (for XYZ) whatever the round-trip demands.
- Leave the RGB/CMYK special-cases in the `icUInt8Number*` / `icUInt16Number*` integer overloads in place (redundant after this fix but intentionally untouched to minimize diff).
- New regression test follows the existing pattern: a self-contained `.cpp` under `.github/ci/regression/`, linked against `${ICCDEV_TEST_LIB_ICCPROFLIB}`, wired via `iccdev_add_regression_executable`, asserting on computed values only (no exported library data).
- `<cmath>` and `std::isnan` are already used in `IccCmm.cpp` (see the Lab From-branch), so no new include is required.

**Reference — build & run a single regression test on this machine:**
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
(`cmake --build` re-runs CMake configuration automatically when `CMakeLists.txt` changes. Use `ctest`, not the raw exe — it applies the Windows DLL `PATH` via `ENVIRONMENT_MODIFICATION`.)

---

### Task 1: Round-trip test + fix the default/CLR branch (RGB, CMYK, 7CLR)

**Files:**
- Create: `.github/ci/regression/tointernal-fixedint-roundtrip.cpp`
- Modify: `Build/Cmake/Testing/CMakeLists.txt` (add a test function ~after line 410, and a call to it in the invocation list ~line 2881)
- Modify: `IccProfLib/IccCmm.cpp` (add two helpers above the float `ToInternalEncoding` near line 9656; fix the `default:` branch's `icEncode8Bit` and `icEncode16Bit`/`icEncode16BitV2` cases, ~lines 9820-9835)

**Interfaces:**
- Consumes: `CIccCmm::ToInternalEncoding(icColorSpaceSignature, icFloatColorEncoding, icFloatNumber* pInternal, const icFloatNumber* pData, bool bClip)` and `CIccCmm::FromInternalEncoding(icColorSpaceSignature, icFloatColorEncoding, icFloatNumber* pData, const icFloatNumber* pInternal, bool bClip)` — both public static in [IccCmm.h:1801-1816](../../../IccProfLib/IccCmm.h).
- Produces (for later tasks): file-local `static icUInt8Number icToU8Sample(icFloatNumber v)` and `static icUInt16Number icToU16Sample(icFloatNumber v)` in `IccCmm.cpp`; the test's `checkRoundTrip(label, space, enc, encoded[], n)` helper that later tasks extend.

- [ ] **Step 1: Write the failing test**

Create `.github/ci/regression/tointernal-fixedint-roundtrip.cpp`:

```cpp
/*
    File:       tointernal-fixedint-roundtrip.cpp

    Contains:   CTest regression for CIccCmm::ToInternalEncoding fixed-integer
                decode.

    ToInternalEncoding must be the inverse of FromInternalEncoding: given an
    already-encoded fixed-integer sample it must decode back to the internal
    0-1 representation.  It used to re-encode the input through icFtoU8/icFtoU16
    (which clamp to [0,1]), so any 0..255 / 0..65535 sample saturated to full
    and the round trip was destroyed for every space except RGB/CMYK (which the
    integer overloads special-case with an explicit /N).

    This helper asserts From(To(encoded)) == encoded (within half an LSB) for
    the default/CLR path (RGB, CMYK, 7CLR), the Lab path and the XYZ path across
    the 8-bit / 16-bit / 16-bit-v2 encodings, and that two distinct 7CLR samples
    decode to distinct internal values.

    Exit codes:
      0 - all round trips and the distinctness check passed
      1 - a round trip diverged
*/

#include "IccCmm.h"

#include <cmath>
#include <cstdio>

#ifdef USEICCDEVNAMESPACE
using namespace iccDEV;
#endif

static int g_failures = 0;

static const char* encName(icFloatColorEncoding e)
{
  switch (e) {
    case icEncode8Bit:    return "8Bit";
    case icEncode16Bit:   return "16Bit";
    case icEncode16BitV2: return "16BitV2";
    default:              return "?";
  }
}

// From(To(encoded)) must reproduce every encoded sample to within half an LSB.
static void checkRoundTrip(const char* label, icColorSpaceSignature space,
                           icFloatColorEncoding enc,
                           const icFloatNumber* encoded, int n)
{
  icFloatNumber internal[16];
  icFloatNumber back[16];

  if (CIccCmm::ToInternalEncoding(space, enc, internal, encoded, false) != icCmmStatOk) {
    std::printf("FAIL %s/%s: ToInternalEncoding returned error\n", label, encName(enc));
    g_failures++;
    return;
  }
  if (CIccCmm::FromInternalEncoding(space, enc, back, internal, false) != icCmmStatOk) {
    std::printf("FAIL %s/%s: FromInternalEncoding returned error\n", label, encName(enc));
    g_failures++;
    return;
  }
  for (int i = 0; i < n; i++) {
    if (std::fabs(back[i] - encoded[i]) > 0.5f) {
      std::printf("FAIL %s/%s ch%d: encoded %.3f -> internal %.6f -> re-encoded %.3f\n",
                  label, encName(enc), i, encoded[i], internal[i], back[i]);
      g_failures++;
    }
  }
}

// Two distinct encoded 7CLR samples must decode to distinct internal values.
// Pre-fix both saturate to (1,1,1,1,1,1,1) and this check fails.
static void check7clrDistinct()
{
  const icFloatNumber a8[7] = { 64, 64, 64, 64, 64, 64, 64 };
  const icFloatNumber b8[7] = { 200, 200, 200, 200, 200, 200, 200 };
  icFloatNumber ia[7], ib[7];

  if (CIccCmm::ToInternalEncoding(icSig7colorData, icEncode8Bit, ia, a8, false) != icCmmStatOk ||
      CIccCmm::ToInternalEncoding(icSig7colorData, icEncode8Bit, ib, b8, false) != icCmmStatOk) {
    std::printf("FAIL 7CLR distinct: ToInternalEncoding returned error\n");
    g_failures++;
    return;
  }
  bool differ = false;
  for (int i = 0; i < 7; i++)
    if (std::fabs(ia[i] - ib[i]) > 0.01f)
      differ = true;
  if (!differ) {
    std::printf("FAIL 7CLR distinct: 64s and 200s decoded to the same internal value\n");
    g_failures++;
  }
}

int main()
{
  // default/CLR device path (these signatures have no dedicated case in the
  // float overload, so they exercise the default: branch).
  const icFloatNumber rgb8[3]   = { 0.0f, 128.0f, 255.0f };
  const icFloatNumber rgb16[3]  = { 0.0f, 32768.0f, 65535.0f };
  checkRoundTrip("RGB",  icSigRgbData,     icEncode8Bit,    rgb8, 3);
  checkRoundTrip("RGB",  icSigRgbData,     icEncode16Bit,   rgb16, 3);
  checkRoundTrip("RGB",  icSigRgbData,     icEncode16BitV2, rgb16, 3);

  const icFloatNumber cmyk8[4]  = { 0.0f, 64.0f, 128.0f, 255.0f };
  const icFloatNumber cmyk16[4] = { 0.0f, 16384.0f, 32768.0f, 65535.0f };
  checkRoundTrip("CMYK", icSigCmykData,    icEncode8Bit,    cmyk8, 4);
  checkRoundTrip("CMYK", icSigCmykData,    icEncode16Bit,   cmyk16, 4);
  checkRoundTrip("CMYK", icSigCmykData,    icEncode16BitV2, cmyk16, 4);

  const icFloatNumber clr7_8[7]  = { 0.0f, 64.0f, 128.0f, 200.0f, 255.0f, 32.0f, 96.0f };
  const icFloatNumber clr7_16[7] = { 0.0f, 16384.0f, 32768.0f, 50000.0f, 65535.0f, 8192.0f, 24576.0f };
  checkRoundTrip("7CLR", icSig7colorData,  icEncode8Bit,    clr7_8, 7);
  checkRoundTrip("7CLR", icSig7colorData,  icEncode16Bit,   clr7_16, 7);
  checkRoundTrip("7CLR", icSig7colorData,  icEncode16BitV2, clr7_16, 7);

  check7clrDistinct();

  if (g_failures) {
    std::printf("tointernal-fixedint-roundtrip: %d failure(s)\n", g_failures);
    return 1;
  }
  std::printf("tointernal-fixedint-roundtrip: OK\n");
  return 0;
}
```

- [ ] **Step 2: Wire the test into CMake**

In `Build/Cmake/Testing/CMakeLists.txt`, add this function immediately after the `iccdev_add_pawg_q4_xyz_pcs_decode_test` function definition (it ends near line 410):

```cmake
function(iccdev_add_tointernal_fixedint_roundtrip_test)
  if(NOT TARGET "${TARGET_LIB_ICCPROFLIB}")
    return()
  endif()

  iccdev_add_regression_executable(iccToInternalFixedIntRoundtripTest
    "${ICCDEV_REPO_ROOT}/.github/ci/regression/tointernal-fixedint-roundtrip.cpp"
  )
  target_link_libraries(iccToInternalFixedIntRoundtripTest PRIVATE ${ICCDEV_TEST_LIB_ICCPROFLIB})
  add_dependencies(check iccToInternalFixedIntRoundtripTest)

  add_test(
    NAME iccdev.tointernal-fixedint-roundtrip
    COMMAND "$<TARGET_FILE:iccToInternalFixedIntRoundtripTest>"
  )
  set_tests_properties(iccdev.tointernal-fixedint-roundtrip PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 60
    LABELS "iccdev;encoding;regression"
  )
  if(WIN32)
    set(_tointernal_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccToInternalFixedIntRoundtripTest>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _tointernal_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.tointernal-fixedint-roundtrip PROPERTIES
      ENVIRONMENT_MODIFICATION "${_tointernal_env_mods}"
    )
  endif()
endfunction()
```

Then add its call in the invocation list next to the other `iccdev_add_*` calls (near line 2881, after `iccdev_add_pawg_q4_xyz_pcs_decode_test()`):

```cmake
  iccdev_add_tointernal_fixedint_roundtrip_test()
```

- [ ] **Step 3: Build and run to verify it FAILS**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: FAIL. Output shows `FAIL RGB/8Bit`, `FAIL CMYK/8Bit`, `FAIL 7CLR/8Bit` (and the 16-bit variants), plus `FAIL 7CLR distinct`, because the default branch re-encodes 0–255/0–65535 and saturates.

- [ ] **Step 4: Add the decode helpers**

In `IccProfLib/IccCmm.cpp`, immediately above the float `ToInternalEncoding` definition (the doc comment near line 9643 / the function at 9656), add:

```cpp
// Decode one already-encoded fixed-integer sample carried in a float into its
// icUIntN value.  Unlike icFtoU8/icFtoU16 this does NOT pre-clamp to [0,1]: the
// argument is a sample in 0..255 / 0..65535, not a normalized float being
// quantized.  Clamps to the encoding range and rounds; NaN maps to 0.
static icUInt8Number icToU8Sample(icFloatNumber v)
{
  if (std::isnan(v) || v <= 0.0f)
    return 0;
  if (v >= 255.0f)
    return 255;
  return (icUInt8Number)icRoundOffset(v);
}

static icUInt16Number icToU16Sample(icFloatNumber v)
{
  if (std::isnan(v) || v <= 0.0f)
    return 0;
  if (v >= 65535.0f)
    return 65535;
  return (icUInt16Number)icRoundOffset(v);
}
```

- [ ] **Step 5: Fix the default/CLR branch**

In the float `ToInternalEncoding`, `default:` case, replace the `icEncode8Bit` body (~line 9820):

```cpp
        case icEncode8Bit:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icU8toF(icToU8Sample(pInput[i]));
            }
            break;
          }
```

and the `icEncode16Bit` / `icEncode16BitV2` body (~line 9828):

```cpp
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            for(i=0; i<nSamples; i++) {
              pInput[i] = icU16toF(icToU16Sample(pInput[i]));
            }
            break;
          }
```

(Only the inner `icFtoU8`/`icFtoU16` changed to `icToU8Sample`/`icToU16Sample`.)

- [ ] **Step 6: Build and run to verify default/CLR passes**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: PASS. All RGB/CMYK/7CLR cases and the 7CLR distinctness check now pass. (Lab and XYZ are not yet in the test.)

- [ ] **Step 7: Commit**

```bash
git add .github/ci/regression/tointernal-fixedint-roundtrip.cpp Build/Cmake/Testing/CMakeLists.txt IccProfLib/IccCmm.cpp
git commit -m "$(cat <<'EOF'
fix: decode fixed-int samples in ToInternalEncoding default branch

Replace icFtoU8/icFtoU16 (which clamp to [0,1] and saturate real 0-255/
0-65535 samples) with icToU8Sample/icToU16Sample so the default/CLR path
decodes 8/16/16V2 input instead of re-encoding it. 7CLR and other CLR
spaces now round-trip. Adds a From-To round-trip regression test.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 2: Fix the Lab branch

**Files:**
- Modify: `.github/ci/regression/tointernal-fixedint-roundtrip.cpp` (add Lab cases to `main`)
- Modify: `IccProfLib/IccCmm.cpp` (Lab branch `icEncode8Bit`, `icEncode16Bit`, `icEncode16BitV2`, ~lines 9702-9726)

**Interfaces:**
- Consumes: `checkRoundTrip` and `icToU8Sample`/`icToU16Sample` from Task 1.
- Produces: nothing new for later tasks.

- [ ] **Step 1: Extend the test with Lab cases**

In `main()`, after the 7CLR block and before `check7clrDistinct();`, add:

```cpp
  // Lab: 8-bit encodes L as 0..255 and a*/b* via icABtoU8 (0..255); 16-bit and
  // 16-bit-v2 encode the normalized PCS channels as 0..65535.
  const icFloatNumber lab8[3]  = { 128.0f, 100.0f, 160.0f };
  const icFloatNumber lab16[3] = { 32768.0f, 25600.0f, 40960.0f };
  checkRoundTrip("Lab",  icSigLabData, icEncode8Bit,    lab8, 3);
  checkRoundTrip("Lab",  icSigLabData, icEncode16Bit,   lab16, 3);
  checkRoundTrip("Lab",  icSigLabData, icEncode16BitV2, lab16, 3);
```

- [ ] **Step 2: Build and run to verify Lab FAILS**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: FAIL on `Lab/8Bit`, `Lab/16Bit`, `Lab/16BitV2` (the Lab branch still calls `icFtoU8`/`icFtoU16`). RGB/CMYK/7CLR still pass.

- [ ] **Step 3: Fix the Lab branch**

In the float `ToInternalEncoding`, `case icSigLabData:` — replace the `icEncode8Bit` body (~line 9702):

```cpp
        case icEncode8Bit:
          {
            pInput[0] = icU8toF(icToU8Sample(pInput[0]))*100.0f;
            pInput[1] = icU8toAB(icToU8Sample(pInput[1]));
            pInput[2] = icU8toAB(icToU8Sample(pInput[2]));

            icLabToPcs(pInput);
            break;
          }
```

the `icEncode16Bit` body (~line 9711):

```cpp
        case icEncode16Bit:
          {
            pInput[0] = icU16toF(icToU16Sample(pInput[0]));
            pInput[1] = icU16toF(icToU16Sample(pInput[1]));
            pInput[2] = icU16toF(icToU16Sample(pInput[2]));
            break;
          }
```

and the `icEncode16BitV2` body (~line 9718):

```cpp
        case icEncode16BitV2:
          {
            pInput[0] = icU16toF(icToU16Sample(pInput[0]));
            pInput[1] = icU16toF(icToU16Sample(pInput[1]));
            pInput[2] = icU16toF(icToU16Sample(pInput[2]));

            CIccPCSUtil::Lab2ToLab4(pInput, pInput);
            break;
          }
```

(Only the inner `icFtoU8`/`icFtoU16` changed; `*100.0f`, `icU8toAB`, `icLabToPcs`, `Lab2ToLab4` are preserved.)

- [ ] **Step 4: Build and run to verify Lab passes**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: PASS for all RGB/CMYK/7CLR/Lab cases.

- [ ] **Step 5: Commit**

```bash
git add .github/ci/regression/tointernal-fixedint-roundtrip.cpp IccProfLib/IccCmm.cpp
git commit -m "$(cat <<'EOF'
fix: decode fixed-int samples in ToInternalEncoding Lab branch

Swap icFtoU8/icFtoU16 for icToU8Sample/icToU16Sample in the Lab 8/16/16V2
cases so encoded Lab samples decode instead of saturating. Extends the
round-trip regression test to cover Lab.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 3: Fix the XYZ branch

**Files:**
- Modify: `.github/ci/regression/tointernal-fixedint-roundtrip.cpp` (add XYZ cases to `main`)
- Modify: `IccProfLib/IccCmm.cpp` (XYZ branch `icEncode16Bit`/`icEncode16BitV2`, ~lines 9759-9766)

**Interfaces:**
- Consumes: `checkRoundTrip` and `icToU16Sample` from Task 1.
- Produces: nothing new for later tasks.

- [ ] **Step 1: Extend the test with XYZ cases**

In `main()`, after the Lab block, add (XYZ has no 8-bit case in these functions — 16-bit only):

```cpp
  // XYZ: 16-bit / 16-bit-v2 encode each channel as a u1Fixed15 value (0..65535,
  // 0x8000 == 1.0).
  const icFloatNumber xyz16[3] = { 32768.0f, 16384.0f, 49152.0f };
  checkRoundTrip("XYZ",  icSigXYZData, icEncode16Bit,   xyz16, 3);
  checkRoundTrip("XYZ",  icSigXYZData, icEncode16BitV2, xyz16, 3);
```

- [ ] **Step 2: Build and run to verify XYZ FAILS**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: FAIL on `XYZ/16Bit`, `XYZ/16BitV2` (the input is clamped by `icFtoU16` to [0,1] before the `u1Fixed15` cast, so 32768 → 1.0 → 65535).

- [ ] **Step 3: Apply the sample-decode swap for XYZ**

In the float `ToInternalEncoding`, `case icSigXYZData:`, replace the `icEncode16Bit`/`icEncode16BitV2` body (~line 9759):

```cpp
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            pInput[0] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[0]));
            pInput[1] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[1]));
            pInput[2] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[2]));
            break;
          }
```

- [ ] **Step 4: Run — expect XYZ still FAILS (missing PCS conversion)**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: still FAIL on XYZ. Reason: `FromInternalEncoding(XYZ,16Bit)` does `icXyzFromPcs` then `icDtoUSF` ([IccCmm.cpp:10068-10076](../../../IccProfLib/IccCmm.cpp)), so the decoder must invert both — it currently does `icUSFtoD` (u1Fixed15 → XYZ) but never converts XYZ → PCS, so `From(To(x)) != x`.

- [ ] **Step 5: Add the missing `icXyzToPcs` to complete the inverse**

Update the same XYZ case to convert back to PCS after decoding the samples:

```cpp
        case icEncode16Bit:
        case icEncode16BitV2:
          {
            pInput[0] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[0]));
            pInput[1] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[1]));
            pInput[2] = icUSFtoD((icU1Fixed15Number)icToU16Sample(pInput[2]));
            icXyzToPcs(pInput);
            break;
          }
```

- [ ] **Step 6: Build and run to verify XYZ passes**

Run:
```bash
cmake --build out/vs2022-x64 --config Release --target iccToInternalFixedIntRoundtripTest
ctest --test-dir out/vs2022-x64 -C Release -R tointernal-fixedint --output-on-failure
```
Expected: PASS. The whole test (`tointernal-fixedint-roundtrip: OK`) is green.

- [ ] **Step 7: Commit**

```bash
git add .github/ci/regression/tointernal-fixedint-roundtrip.cpp IccProfLib/IccCmm.cpp
git commit -m "$(cat <<'EOF'
fix: decode fixed-int samples in ToInternalEncoding XYZ branch

Swap icFtoU16 for icToU16Sample in the XYZ 16/16V2 case and add the
icXyzToPcs step so the decoder is the true inverse of FromInternalEncoding
(which does icXyzFromPcs + icDtoUSF). Extends the round-trip test to XYZ.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

### Task 4: Fix the shipped example and audit broader regressions

**Files:**
- Modify: `docs/Testing/json-configs/applynamedcmm-8bit-encoding.json`

**Interfaces:**
- Consumes: the corrected `ToInternalEncoding` from Tasks 1-3.
- Produces: nothing.

- [ ] **Step 1: Confirm the example now decodes correctly**

The example already carries 0–255 values with `"encoding": "8Bit"`; after the fix they decode properly instead of saturating. First build the tool, then run the example:

```bash
cmake --build out/vs2022-x64 --config Release --target iccApplyNamedCmm
./out/vs2022-x64/bin/Release/iccApplyNamedCmm.exe -cfg docs/Testing/json-configs/applynamedcmm-8bit-encoding.json | tail -7
```
Expected: rows are now distinct and non-saturated — in particular the `128,77,51` row and the `128,128,128` row produce **different** output (pre-fix both produced the sRGB white point `0.9504 1.0000 1.0889`).

- [ ] **Step 2: Make the example self-documenting**

Edit `docs/Testing/json-configs/applynamedcmm-8bit-encoding.json` — add a `"comment"` field inside the `"colorData"` object documenting the contract, so the file no longer reads as if arbitrary 0–255 values were fine only by luck:

```json
    "comment": "8Bit encoding: integer sample values 0-255 per channel, decoded as value/255.",
```
(Place it as the first key inside `"colorData"`. The JSON parser ignores unknown keys — verify by re-running the command from Step 1 and confirming identical output.)

- [ ] **Step 3: Audit for baselines capturing the old saturated output**

Run:
```bash
grep -rn "icEncode8Bit\|\"8Bit\"\|8bit-encoding" .github/scripts docs/Testing --include=*.sh --include=*.json | grep -iv "16bit"
```
Expected: no regression script pins a stored expected output for `applynamedcmm-8bit-encoding.json` (confirmed during planning — it is a standalone example, not wired into a diff test). If this surface has grown a baseline since, regenerate it from the tool output in Step 1 and note the change in the commit message. If nothing is found, this step is a no-op.

- [ ] **Step 4: Run the JSON config regression suite to confirm nothing else regressed**

Run:
```bash
ctest --test-dir out/vs2022-x64 -C Release -R "json|namedcmm|applysearch" --output-on-failure
```
Expected: PASS. Any failure here means a test captured the old fixed-int behavior — investigate and regenerate its baseline, calling it out explicitly.

- [ ] **Step 5: Commit**

```bash
git add docs/Testing/json-configs/applynamedcmm-8bit-encoding.json
git commit -m "$(cat <<'EOF'
docs: correct the 8-bit colorData example after decode fix

The example fed 0-255 values with "8Bit" and previously produced saturated
output. With ToInternalEncoding fixed it now decodes correctly; document
the 0-255 contract in the config.

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Self-Review

**Spec coverage:**
- "Fix all fixed-int decode paths (8Bit/16Bit/16BitV2 across Lab/XYZ/default)" → Tasks 1 (default), 2 (Lab), 3 (XYZ). ✓
- "Correctness via round-trip invariant `From∘To == identity`" → `checkRoundTrip` in Task 1, extended in 2/3. ✓
- "7CLR saturation regression (distinct inputs → distinct outputs)" → `check7clrDistinct` in Task 1. ✓
- "Two file-local helpers `icToU8Sample`/`icToU16Sample` without the 0–1 pre-clamp" → Task 1 Step 4. ✓
- "XYZ specifics dictated by the round-trip test" → Task 3 Steps 3-5 (swap, observe failure, add `icXyzToPcs`). ✓
- "Leave RGB/CMYK integer-overload special-cases in place" → Global Constraints; no task touches them. ✓
- "Fix the shipped example; audit baselines" → Task 4. ✓
- "Do not modify FromInternalEncoding" → Global Constraints; no task touches it. ✓

**Placeholder scan:** No TBD/TODO; every code and CMake step contains literal content. ✓

**Type consistency:** `icToU8Sample`/`icToU16Sample` signatures identical everywhere used; `checkRoundTrip(const char*, icColorSpaceSignature, icFloatColorEncoding, const icFloatNumber*, int)` consistent across tasks; `ToInternalEncoding`/`FromInternalEncoding` calls match the public static signatures in [IccCmm.h:1801-1816](../../../IccProfLib/IccCmm.h). ✓
