# iccBenchApply Throughput Harness Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build `iccBenchApply`, a command-line tool that measures pixels/second through an ICC profile chain at three granularities and emits an output checksum per case, so the optimisation work that follows can be shown fast *and* numerically equivalent.

**Architecture:** A normal `Tools/CmdLine` tool linking `IccProfLib` alone, split across three files: a timing/checksum helper, a case table with profile-path resolution, and a `main` that parses a profile chain in the same encoded-intent syntax `iccApplyToLink` uses. Measurement reaches into the resolved transform chain through public API only — `CIccCmm::IterateXforms`, `CIccXform::GetXformType`, `CIccXform::GetNewApply` — so no library change is needed. Registered as one record-only CTest test.

**Tech Stack:** C++17, CMake, CTest, IccProfLib. No new third-party dependencies. `<chrono>` and `<thread>` from the standard library.

**Spec:** `docs/superpowers/specs/2026-08-20-apply-throughput-harness-design.md`

## Global Constraints

- **No `IccProfLib/` changes.** This branch must be a clean measurement baseline. If a task appears to need a library edit, stop and report it — that is a spec problem, not an implementation decision.
- **Link `${TARGET_LIB_ICCPROFLIB}` only.** Never `${TARGET_LIB_ICCCONNECT}`, IccXML, or IccJSON. `Build/Cmake/Testing/CMakeLists.txt:56-79` documents why: a target linking IccXML/IccJSON *and* static IccProfLib gets two copies of the library's globals.
- **C++17.** Already the project standard (`Build/Cmake/CMakeLists.txt:226`). Use `target_compile_features(<t> PRIVATE cxx_std_17)`.
- **Branch:** `bench/throughput-harness`. Already created; the spec is committed on it at `6da03802`.
- **Never assert a timing threshold.** Anywhere. CI records only.
- **Copyright header:** every new `.cpp`/`.h` copies the header block from `Tools/CmdLine/IccApplyToLink/iccApplyToLink.cpp:1-70` verbatim, changing only the `File:` and `Contains:` lines.
- **Profile paths are resolved at runtime against two roots**, never hardcoded to one. `Build/Cmake/Testing/CMakeLists.txt:1746` records that the POSIX profile fixture writes into the source tree and the Windows one into the build tree.

## Build and test commands

Substitute your platform's preset (`docs/build.md:48,99,134` list them):

```bash
# Configure once
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64      # Windows
cmake --preset linux-clang -S Build/Cmake -B out/linux-clang     # Linux

# Build just this tool (fast loop)
cmake --build out/vs2022-x64 --config Release --target iccBenchApply

# Run the one CTest case this plan adds
ctest --test-dir out/vs2022-x64 -C Release -R apply-throughput --output-on-failure
```

`$BUILD` below means your build directory, `$TOOLS` means `$BUILD/Tools` (`Build/Cmake/Testing/CMakeLists.txt:10`).

## Testing approach — read this before Task 1

**This repository has no unit-test framework.** There is no gtest, no Catch2, no
pytest. The two established idioms are:

1. A standalone binary under `.github/ci/regression/*.cpp` that returns non-zero
   on failure, registered with `iccdev_add_regression_executable` +
   `add_test`. Sixty-odd of these exist.
2. A shell script under `.github/scripts/` registered with
   `iccdev_add_script_test` (`Build/Cmake/Testing/CMakeLists.txt:5275`).

So "write the failing test" in the tasks below means **a shell command whose
output you assert on**, not a test-framework case. The tool also self-asserts:
`-suite` checks its own checksum determinism and thread invariance and returns
non-zero if they break. That is deliberate — it makes the CTest registration in
Task 8 a one-liner instead of a second program.

Where a task's test is a shell assertion, the plan gives the exact command and
the exact expected output. Run it and read it; do not assume.

## File structure

| File | Responsibility |
|---|---|
| `Tools/CmdLine/IccBenchApply/BenchTimer.h` | Timing, statistics, checksum, buffer fill. No IccProfLib types beyond `icFloatNumber`. Header-only. |
| `Tools/CmdLine/IccBenchApply/BenchCases.h` | Declares `BenchChainLink`, `BenchCase`, the built-in table accessor, and profile-path resolution. |
| `Tools/CmdLine/IccBenchApply/BenchCases.cpp` | The case table data and the two-root path resolver. |
| `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp` | CLI parsing, chain construction, the three measurement tiers, output formatting, `main`. |
| `Tools/CmdLine/IccBenchApply/Readme.md` | Usage, and the two caveats from the spec (per-xform shares are attribution not decomposition; checksums only compare across matching compiler flags). |
| `Testing/V2/v2GrayTRC.xml` | New test asset. The only source in the tree that produces a `CIccXformMonochrome`. |
| `Build/Cmake/Tools/IccBenchApply/CMakeLists.txt` | Target definition. |
| `Build/Cmake/CMakeLists.txt` | +1 `ADD_SUBDIRECTORY` near `:1749`. |
| `Build/Cmake/Testing/CMakeLists.txt` | CTest registration. |
| `Testing/CreateAllProfiles.sh` / `.bat` | +1 `iccFromXml` line each, in the V2 section. |

Splitting across four files rather than one follows repo precedent —
`Tools/CmdLine/IccProfilePlot` has six `.cpp` files, `IccApplyProfiles` has two.

---

### Task 1: The `v2GrayTRC.xml` test asset

The monochrome case has no source profile in the tree. `Testing/Display/GrayGSDF.xml`
is `link` class with `DataColourSpace` and `PCS` both `GRAY`, so it produces a
devicelink and never instantiates `CIccXformMonochrome`. This task creates the
missing asset. It is first because nothing else needs it, and it is verifiable
with no C++ written.

**Files:**
- Create: `Testing/V2/v2GrayTRC.xml`
- Modify: `Testing/CreateAllProfiles.sh:305-313` (the V2 section)
- Modify: `Testing/CreateAllProfiles.bat` (its V2 section)

**Interfaces:**
- Consumes: nothing.
- Produces: `Testing/V2/v2GrayTRC.icc` after the profile fixture runs. Task 7's
  case table references it by the relative path `V2/v2GrayTRC.icc`.

- [ ] **Step 1: Write the failing test**

```bash
cd Testing/V2 && iccFromXml v2GrayTRC.xml v2GrayTRC.icc
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `iccFromXml` reports it cannot open `v2GrayTRC.xml`, exit non-zero.
If `iccFromXml` is not on PATH, use `$TOOLS/IccFromXml/iccFromXml`.

- [ ] **Step 3: Create the asset**

Modelled on `Testing/V2/v2RgbMatrixTRC.xml`, reduced to the single grey channel.
`grayTRCTag` is what `CIccXformMonochrome::GetCurve` looks up. The curve value
`563` is the u8Fixed8 encoding of gamma 2.2, matching the sibling v2 fixtures.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType>ICCD</PreferredCMMType>
    <ProfileVersion>2.10</ProfileVersion>
    <ProfileDeviceClass>mntr</ProfileDeviceClass>
    <DataColourSpace>GRAY</DataColourSpace>
    <PCS>XYZ </PCS>
    <CreationDateTime>2026-08-20T00:00:00</CreationDateTime>
    <ProfileFlags EmbeddedInFile="false" UseWithEmbeddedDataOnly="false"/>
    <DeviceAttributes ReflectiveOrTransparency="reflective" GlossyOrMatte="glossy" MediaPolarity="positive" MediaColour="colour"/>
    <RenderingIntent>Perceptual</RenderingIntent>
    <PCSIlluminant>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </PCSIlluminant>
    <ProfileCreator>ICCD</ProfileCreator>
  </Header>
  <Tags>
    <grayTRCTag> <curveType>
      <Curve>
         563
      </Curve>
    </curveType> </grayTRCTag>

    <profileDescriptionTag> <textDescriptionType>
      <TextData><![CDATA[iccDEV v2 grayscale display profile (grayTRC)]]></TextData>
    </textDescriptionType> </profileDescriptionTag>

    <copyrightTag> <textType>
      <TextData><![CDATA[Copyright (C) 2026 The International Color Consortium. Generated fixture, no measured data.]]></TextData>
    </textType> </copyrightTag>

    <mediaWhitePointTag> <XYZArrayType>
      <XYZNumber X="0.964202880859" Y="1.000000000000" Z="0.824905395508"/>
    </XYZArrayType> </mediaWhitePointTag>

  </Tags>
</IccProfile>
```

- [ ] **Step 4: Run the test to verify it passes, and verify the shape**

```bash
cd Testing/V2 && iccFromXml v2GrayTRC.xml v2GrayTRC.icc && \
  ../iccDumpProfile.exe v2GrayTRC.icc | grep -E "Data Color Space|PCS Color Space|Profile Class|grayTRC"
```

Expected, all four lines present:
```
Data Color Space:   GrayData
PCS Color Space:    XYZData
Profile Class:      Display
```
plus a `grayTRC` entry in the tag list. **`Profile Class` must be `Display`, not
`Link`** — that is the whole point of the asset. If it says `Link`, the
`ProfileDeviceClass` is wrong.

- [ ] **Step 5: Wire it into both generation scripts**

In `Testing/CreateAllProfiles.sh`, inside the V2 block, after the
`v2RgbMatrixTRC.xml` line at `:312`:

```sh
	iccFromXml v2GrayTRC.xml     v2GrayTRC.icc
```

Add the equivalent line to the V2 section of `Testing/CreateAllProfiles.bat`,
matching that file's existing call style exactly (read the neighbouring lines
first — the `.bat` does not use the same quoting as the `.sh`).

Extend the explanatory comment at `Testing/CreateAllProfiles.sh:299-304`. It
currently says "These three cover the v2 shapes that path depends on" and
enumerates them; it becomes four, and the reason for the fourth is that
`CIccXformMonochrome` had no coverage of any kind:

```sh
# Issue #1883: before these, the corpus held no ICC v2 profile at all [...]
# These four cover the v2 shapes that path depends on: lut16Type ('mft2', which
# is what selects UseLegacyPCS), lut8Type ('mft1'), the matrix/TRC form most real
# v2 display profiles take, and grayTRC -- the last added because no profile in
# the tree produced a CIccXformMonochrome at all, so that xform's apply path was
# exercised by nothing. GrayGSDF is link class Gray->Gray and does not reach it.
```

- [ ] **Step 6: Verify the script path works end to end**

```bash
cd Testing && ./CreateAllProfiles.sh clean && ./CreateAllProfiles.sh 2>&1 | grep -i graytrc
ls -la Testing/V2/v2GrayTRC.icc
```
Expected: the `iccFromXml v2GrayTRC.xml` line echoes (the block runs under
`set -x`), and the `.icc` exists.

- [ ] **Step 7: Commit**

```bash
git add Testing/V2/v2GrayTRC.xml Testing/CreateAllProfiles.sh Testing/CreateAllProfiles.bat
git commit -m "test(bench): add a v2 grayTRC fixture

No profile in the tree produced a CIccXformMonochrome. The one Gray profile,
Display/GrayGSDF, is link class with Gray for both device space and PCS, so it
builds a devicelink and never reaches that xform -- leaving its apply path
exercised by nothing.

Adds the missing shape as a fourth Testing/V2 fixture and wires it into both
generation scripts.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Tool skeleton, CMake wiring, and chain parsing

Deliverable: `iccBenchApply 1 <profile> 1` builds a CMM, calls `Begin()`, prints
the resolved transform chain, and exits 0. No timing yet.

**Files:**
- Create: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`
- Create: `Build/Cmake/Tools/IccBenchApply/CMakeLists.txt`
- Modify: `Build/Cmake/CMakeLists.txt` (one line near `:1749`)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `static bool ParseIntArg(const char*, int, int, int&)` and the chain
  loop shape. Later tasks add functions to this file; they rely on `main` having
  already produced a `CIccCmm*` that has been `Begin()`-ed, and on these two
  file-scope globals being set by argument parsing:
  ```cpp
  static icUInt32Number g_nPixels  = 1048576;
  static int            g_nRepeats = 7;
  ```

- [ ] **Step 1: Write the failing test**

```bash
cmake --build $BUILD --config Release --target iccBenchApply
```

- [ ] **Step 2: Run it to verify it fails**

Expected: CMake errors with `No TARGET named "iccBenchApply"` (or
`ninja: unknown target`). The target does not exist yet.

- [ ] **Step 3: Write the CMakeLists**

`Build/Cmake/Tools/IccBenchApply/CMakeLists.txt` — a copy of
`Build/Cmake/Tools/IccApplyToLink/CMakeLists.txt` with the names changed. Keep
the `IF(NOT TARGET ...)` guard; the repo relies on it.

```cmake
# IccBenchApply CMakeLists.txt | iccDEV Project
# Copyright (c) 2026 The International Color Consortium. All rights reserved.

IF(NOT TARGET iccBenchApply)
  SET(SRC_PATH ../../../..)
  SET(SOURCES
    ${SRC_PATH}/Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
  )
  SET(TARGET_NAME iccBenchApply)

  ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
  TARGET_COMPILE_FEATURES(${TARGET_NAME} PRIVATE cxx_std_17)
  TARGET_LINK_LIBRARIES(${TARGET_NAME} ${TARGET_LIB_ICCPROFLIB})

  FIND_PACKAGE(Threads REQUIRED)
  TARGET_LINK_LIBRARIES(${TARGET_NAME} Threads::Threads)

  IF(ENABLE_INSTALL_RIM)
    INSTALL(TARGETS ${TARGET_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR})
  ENDIF()
ELSE()
  MESSAGE(WARNING "Target iccBenchApply already exists. Skipping duplicate addition.")
ENDIF()
```

`Threads::Threads` is linked now rather than in Task 5 so the target's link line
never changes again. `BenchCases.cpp` joins `SOURCES` in Task 7.

In `Build/Cmake/CMakeLists.txt`, immediately after the
`ADD_SUBDIRECTORY(Tools/IccRoundTrip)` line at `:1749`:

```cmake
  ADD_SUBDIRECTORY(Tools/IccBenchApply)
```

- [ ] **Step 4: Write the tool skeleton**

`Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`. Copy the copyright header block
from `Tools/CmdLine/IccApplyToLink/iccApplyToLink.cpp:1-70` verbatim, changing
only `File:` and `Contains:`. Then:

```cpp
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "IccCmm.h"
#include "IccDefs.h"
#include "IccProfLibVer.h"
#include "IccUtil.h"

static icUInt32Number g_nPixels  = 1048576;
static int            g_nRepeats = 7;

static void Usage()
{
  printf("iccBenchApply built with IccProfLib version " ICCPROFLIBVER "\n\n");
  printf("Usage: iccBenchApply {options} interpolation"
         " {profile_path rendering_intent {-PCC pcc_path}}...\n\n");
  printf("  interpolation      0 = Linear, 1 = Tetrahedral\n");
  printf("  rendering_intent   0..3, plus +1000 / +10000 modifiers"
         " (as iccApplyToLink)\n\n");
  printf("  -pixels N          pixels per buffer      (default 1048576)\n");
  printf("  -repeats N         timed repeats per case (default 7)\n");
  printf("\nNo timing threshold is ever asserted. This tool records.\n");
}

// Verbatim from iccApplyToLink.cpp:820 -- same parsing contract, so the two
// tools accept and reject exactly the same argument strings.
static bool ParseIntArg(const char *arg, int minValue, int maxValue, int &value)
{
  char *end = NULL;
  long parsed;

  if (!arg || !*arg)
    return false;

  errno = 0;
  parsed = strtol(arg, &end, 10);

  if (errno == ERANGE || end == arg || *end != '\0' ||
      parsed < minValue || parsed > maxValue ||
      parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }

  value = (int)parsed;
  return true;
}

// Names for the icXformType values in IccCmm.h:177-187. PCS is the row this
// whole harness exists to make visible, so it is named explicitly rather than
// falling through to "Unknown".
static const char *XformTypeName(icXformType t)
{
  switch (t) {
    case icXformTypeMatrixTRC:   return "MatrixTRC";
    case icXformType3DLut:       return "3DLut";
    case icXformType4DLut:       return "4DLut";
    case icXformTypeNDLut:       return "NDLut";
    case icXformTypeNamedColor:  return "NamedColor";
    case icXformTypeMpe:         return "Mpe";
    case icXformTypeMonochrome:  return "Monochrome";
    case icXformTypePCS:         return "PCS";
    default:                     return "Unknown";
  }
}

class CChainReporter : public IXformIterator
{
public:
  virtual void iterate(const CIccXform *pXform)
  {
    if (!pXform)
      return;
    printf("  %2d  %-12s\n", (int)m_types.size(),
           XformTypeName(pXform->GetXformType()));
    m_types.push_back(pXform->GetXformType());
  }

  std::vector<icXformType> m_types;
};

int main(int argc, const char *argv[])
{
  if (argc < 3) {
    Usage();
    return 1;
  }

  int nArg = 1;

  while (nArg < argc && argv[nArg][0] == '-') {
    if (!stricmp(argv[nArg], "-pixels")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1 << 26, n)) {
        printf("Invalid -pixels value: expected 1..67108864\n");
        return 1;
      }
      g_nPixels = (icUInt32Number)n;
      nArg += 2;
    }
    else if (!stricmp(argv[nArg], "-repeats")) {
      int n;
      if (nArg + 1 >= argc || !ParseIntArg(argv[nArg + 1], 1, 1000, n)) {
        printf("Invalid -repeats value: expected 1..1000\n");
        return 1;
      }
      g_nRepeats = n;
      nArg += 2;
    }
    else {
      printf("Unknown option '%s'\n", argv[nArg]);
      Usage();
      return 1;
    }
  }

  int nInterp;
  if (nArg >= argc || !ParseIntArg(argv[nArg], 0, 1, nInterp)) {
    printf("Invalid interpolation: expected 0 (linear) or 1 (tetrahedral)\n");
    return 1;
  }
  nArg++;

  if (nArg >= argc) {
    printf("No profiles given: a chain needs at least one"
           " 'profile_path rendering_intent' pair\n");
    return 1;
  }

  CIccCmm theCmm(icSigUnknownData, icSigUnknownData, true);
  int nProfiles = 0;

  while (nArg < argc) {
    const char *szProfile = argv[nArg];

    if (nArg + 1 >= argc) {
      printf("Profile '%s' has no rendering intent\n", szProfile);
      return 1;
    }

    int nEncoded;
    if (!ParseIntArg(argv[nArg + 1], INT_MIN, INT_MAX, nEncoded)) {
      printf("Invalid rendering intent '%s': expected an integer code\n",
             argv[nArg + 1]);
      return 1;
    }
    nArg += 2;

    // Same decode as iccApplyToLink.cpp:1054-1059.
    bool bUseSubProfile = (nEncoded / 1000) > 0;
    int nIntent = nEncoded % 1000;
    nIntent = nIntent % 100;
    int nType = abs(nIntent) / 10;
    nIntent = nIntent % 10;

    bool bUseD2BxB2DxTags = true;
    if (nType == 1) {
      nType = 0;
      bUseD2BxB2DxTags = false;
    }

    if (nIntent < (int)icPerceptual || nIntent > (int)icAbsoluteColorimetric) {
      printf("Invalid rendering intent '%s': decoded intent out of range\n",
             argv[nArg - 1]);
      return 1;
    }

    CIccProfile *pPccProfile = NULL;
    if (nArg + 1 < argc && !stricmp(argv[nArg], "-PCC")) {
      pPccProfile = ReadIccProfile(argv[nArg + 1]);
      if (!pPccProfile) {
        printf("Unable to read PCC profile '%s'\n", argv[nArg + 1]);
        return 1;
      }
      nArg += 2;
    }

    icStatusCMM stat = theCmm.AddXform(szProfile,
                                       (icRenderingIntent)nIntent,
                                       nInterp ? icInterpTetrahedral
                                               : icInterpLinear,
                                       pPccProfile,
                                       (icXformLutType)nType,
                                       bUseD2BxB2DxTags,
                                       NULL,
                                       bUseSubProfile);
    if (stat != icCmmStatOk) {
      printf("Unable to add '%s' to the chain: %s\n",
             szProfile, CIccCmm::GetStatusText(stat));
      return 1;
    }
    nProfiles++;
  }

  icStatusCMM stat = theCmm.Begin();
  if (stat != icCmmStatOk) {
    printf("Begin() failed: %s\n", CIccCmm::GetStatusText(stat));
    return 1;
  }

  printf("chain: %d profile(s), %d source sample(s) -> %d destination sample(s)\n",
         nProfiles, (int)theCmm.GetSourceSamples(),
         (int)theCmm.GetDestSamples());
  printf("resolved transforms:\n");
  CChainReporter reporter;
  theCmm.IterateXforms(&reporter);

  return 0;
}
```

Both API facts this code depends on are confirmed, so use them as written:

1. **`CIccCmm::GetStatusText`** is `static const icChar *GetStatusText(icStatusCMM)`,
   public at `IccProfLib/IccCmm.h:1868`.
2. **`stricmp`** resolves via `IccProfLib/IccProfLibConf.h:154`
   (`#define stricmp strcasecmp` on non-Windows), which `IccCmm.h` pulls in. No
   extra include needed.

- [ ] **Step 5: Reconfigure, build, and run the test**

```bash
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64   # picks up the new subdirectory
cmake --build out/vs2022-x64 --config Release --target iccBenchApply
```
Expected: builds clean.

```bash
$TOOLS/IccBenchApply/iccBenchApply 1 Testing/sRGB_v4_ICC_preference.icc 1
```
Expected output shape:
```
chain: 1 profile(s), 3 source sample(s) -> 3 destination sample(s)
resolved transforms:
   0  3DLut
```

`sRGB_v4_ICC_preference.icc` is tracked, so this works with no profile fixture.

- [ ] **Step 6: Verify the argument errors actually fire**

```bash
$TOOLS/IccBenchApply/iccBenchApply 1 Testing/sRGB_v4_ICC_preference.icc     ; echo "rc=$?"
$TOOLS/IccBenchApply/iccBenchApply 9 Testing/sRGB_v4_ICC_preference.icc 1   ; echo "rc=$?"
$TOOLS/IccBenchApply/iccBenchApply 1 no/such/file.icc 1                     ; echo "rc=$?"
```
Expected: `rc=1` for all three, with the specific message each case prints —
"has no rendering intent", "Invalid interpolation", "Unable to add". A chain
given explicitly on the command line must fail loudly; `SKIP` semantics arrive
in Task 7 and apply only to `-suite`.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/iccBenchApply.cpp \
        Build/Cmake/Tools/IccBenchApply/CMakeLists.txt \
        Build/Cmake/CMakeLists.txt
git commit -m "feat(bench): add iccBenchApply skeleton with chain parsing

Takes a profile chain in the same encoded-intent syntax iccApplyToLink parses,
builds and Begin()s a CIccCmm, and reports the resolved transform list via the
public CIccCmm::IterateXforms hook. No measurement yet.

Links IccProfLib alone. iccApplyNamedCmm's CIccCfgProfileSequence would have
given the chain parser for free but lives in IccConnect.h, which would pull
IccConnect and IccJSON into a binary whose purpose is measuring IccProfLib.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Pixel buffer, checksum, and tier 1 timing

Deliverable: the tool reports median Mpx/s for the whole chain plus a checksum,
and the checksum is identical across runs.

**Files:**
- Create: `Tools/CmdLine/IccBenchApply/BenchTimer.h`
- Modify: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`

**Interfaces:**
- Consumes: `g_nPixels`, `g_nRepeats` from Task 2.
- Produces, all used by Tasks 4-7:
  ```cpp
  struct BenchStats {
    double medianMpxPerSec;
    double minMpxPerSec;
    double maxMpxPerSec;
  };
  BenchStats     icBenchRun(const std::function<void()> &fn,
                            icUInt32Number nUnits, int nRepeats);
  icUInt32Number icBenchChecksum(const icFloatNumber *pData, size_t nFloats);
  void           icBenchFill(icFloatNumber *pBuf, icUInt32Number nPixels,
                             icUInt16Number nSamples, icUInt32Number nSeed);
  ```

- [ ] **Step 1: Write the failing test**

```bash
A=$($TOOLS/IccBenchApply/iccBenchApply -pixels 65536 -repeats 3 1 Testing/sRGB_v4_ICC_preference.icc 1 | grep checksum)
B=$($TOOLS/IccBenchApply/iccBenchApply -pixels 65536 -repeats 3 1 Testing/sRGB_v4_ICC_preference.icc 1 | grep checksum)
echo "A=[$A] B=[$B]"; [ -n "$A" ] && [ "$A" = "$B" ] && echo DETERMINISTIC || echo FAIL
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `FAIL`, because nothing prints a `checksum` line yet, so `$A` is empty.
The `[ -n "$A" ]` guard is there on purpose — without it, two empty strings
compare equal and the test would pass vacuously.

- [ ] **Step 3: Write `BenchTimer.h`**

Copyright header first, then:

```cpp
#ifndef _BENCHTIMER_H
#define _BENCHTIMER_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <vector>

#include "IccDefs.h"

struct BenchStats {
  double medianMpxPerSec;
  double minMpxPerSec;
  double maxMpxPerSec;
};

// One warm-up pass, then nRepeats timed passes; report the median.
//
// Median rather than mean because runner noise is one-sided: interrupts,
// contention, and frequency scaling only ever make a pass slower, never faster.
// A mean is dragged down by any single hostile pass, and a min reports a
// best case the caller will not see again.
inline BenchStats icBenchRun(const std::function<void()> &fn,
                             icUInt32Number nUnits, int nRepeats)
{
  fn();  // warm-up: first-touch page faults and cold caches land here

  std::vector<double> rates;
  rates.reserve((size_t)nRepeats);

  for (int i = 0; i < nRepeats; i++) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();

    const double secs =
      std::chrono::duration<double>(t1 - t0).count();
    rates.push_back(secs > 0.0 ? ((double)nUnits / secs) / 1.0e6 : 0.0);
  }

  std::sort(rates.begin(), rates.end());

  BenchStats rv;
  rv.minMpxPerSec    = rates.front();
  rv.maxMpxPerSec    = rates.back();
  rv.medianMpxPerSec = rates[rates.size() / 2];
  return rv;
}

// FNV-1a over the raw float bytes.
//
// Hashing the bit patterns, not decimal renderings, makes this exact -- which
// is the point: it is the equivalence oracle for the optimisation branches that
// follow. It also means the harness consumes its own output, so the compiler
// cannot elide the apply loop as dead code.
//
// Exactness cuts both ways: this is also sensitive to legitimate floating-point
// reassociation, so a checksum only compares meaningfully between builds made
// with the same compiler flags. Readme.md states this.
inline icUInt32Number icBenchChecksum(const icFloatNumber *pData, size_t nFloats)
{
  const unsigned char *p = (const unsigned char *)pData;
  const size_t nBytes = nFloats * sizeof(icFloatNumber);

  icUInt32Number h = 2166136261u;
  for (size_t i = 0; i < nBytes; i++) {
    h ^= (icUInt32Number)p[i];
    h *= 16777619u;
  }
  return h;
}

// Deterministic fill. Rows 0..n-2 are in-gamut; the final row is pathological.
//
// The pathological row is deliberate. Several of the checks this harness exists
// to measure concern clamping and non-finite handling -- ClutUnitClip, the
// isfinite tests in the interpolators and the sampled curves. A happy-path-only
// buffer would leave them unexercised, and would let a wrong hoist pass the
// checksum oracle unnoticed.
inline void icBenchFill(icFloatNumber *pBuf, icUInt32Number nPixels,
                        icUInt16Number nSamples, icUInt32Number nSeed)
{
  icUInt32Number state = nSeed ? nSeed : 1u;
  const icUInt32Number nNormal = nPixels ? nPixels - 1 : 0;

  for (icUInt32Number i = 0; i < nNormal; i++) {
    for (icUInt16Number s = 0; s < nSamples; s++) {
      state = state * 1664525u + 1013904223u;  // Numerical Recipes LCG
      pBuf[(size_t)i * nSamples + s] =
        (icFloatNumber)((state >> 8) & 0xFFFFFF) / (icFloatNumber)0xFFFFFF;
    }
  }

  if (nPixels) {
    static const icFloatNumber pathological[6] = {
      (icFloatNumber)-1.0,
      (icFloatNumber) 2.0,
      (icFloatNumber) 0.0,
      (icFloatNumber) 1.0,
      std::numeric_limits<icFloatNumber>::infinity(),
      -std::numeric_limits<icFloatNumber>::infinity(),
    };
    icFloatNumber *pLast = pBuf + (size_t)nNormal * nSamples;
    for (icUInt16Number s = 0; s < nSamples; s++) {
      pLast[s] = (s == nSamples - 1)
                   ? std::numeric_limits<icFloatNumber>::quiet_NaN()
                   : pathological[s % 6];
    }
  }
}

#endif // _BENCHTIMER_H
```

Add `#include <limits>` to that include list — `std::numeric_limits` is used and
the list above omits it. (Catching this now is cheaper than catching it from a
compiler error.)

- [ ] **Step 4: Wire tier 1 into `main`**

Add `#include "BenchTimer.h"` and, replacing the `IterateXforms` block's
trailing `return 0;`:

```cpp
  const icUInt16Number nSrc = theCmm.GetSourceSamples();
  const icUInt16Number nDst = theCmm.GetDestSamples();

  if (!nSrc || !nDst) {
    printf("Chain reports %d source and %d destination samples;"
           " nothing to measure\n", (int)nSrc, (int)nDst);
    return 1;
  }

  std::vector<icFloatNumber> src((size_t)g_nPixels * nSrc);
  std::vector<icFloatNumber> dst((size_t)g_nPixels * nDst);

  icBenchFill(src.data(), g_nPixels, nSrc, 20260820u);

  const BenchStats st = icBenchRun(
    [&]() { theCmm.Apply(dst.data(), src.data(), g_nPixels); },
    g_nPixels, g_nRepeats);

  const icUInt32Number sum = icBenchChecksum(dst.data(), dst.size());

  printf("\n  %-20s %9s %9s %9s  %-10s\n",
         "case", "Mpx/s", "min", "max", "checksum");
  printf("  %-20s %9.2f %9.2f %9.2f  0x%08x\n",
         "chain", st.medianMpxPerSec, st.minMpxPerSec, st.maxMpxPerSec, sum);

  return 0;
```

`std::vector` rather than `new[]` so a throwing allocation is not silently
leaked; the buffers are large and allocation failure is a real outcome.

- [ ] **Step 5: Run the test to verify it passes**

Rebuild, then re-run the Step 1 command.
Expected: `DETERMINISTIC`, and the table shows a plausible non-zero Mpx/s.

- [ ] **Step 6: Verify the pathological row did not poison everything**

```bash
$TOOLS/IccBenchApply/iccBenchApply -pixels 65536 -repeats 3 1 Testing/sRGB_v4_ICC_preference.icc 1
```
Expected: a finite Mpx/s and a checksum. If Mpx/s is `0.00` or the run hangs,
the pathological row has hit an unclamped path — that is a genuine library
finding, so **report it rather than removing the row**.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/BenchTimer.h Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
git commit -m "feat(bench): add timing, checksum, and whole-chain measurement

Reports median Mpx/s over the whole chain with a checksum of the destination
buffer. Median because runner noise is one-sided -- a mean is dragged down by
any single hostile pass.

The checksum hashes float bit patterns, so it is exact: it is the equivalence
oracle the optimisation branches need, and it stops the compiler eliding the
apply loop. The input buffer's final row is deliberately pathological
(out-of-range, +/-inf, NaN) so the clamp and non-finite paths those branches
touch are actually exercised.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Per-xform breakdown, including PCS

Deliverable: `-perxform` prints one row per xform in the resolved chain, PCS
rows included.

**Files:**
- Modify: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`

**Interfaces:**
- Consumes: `BenchStats`, `icBenchRun`, `icBenchFill` from Task 3; `XformTypeName`
  and `CChainReporter` from Task 2.
- Produces: nothing later tasks depend on.

- [ ] **Step 1: Write the failing test**

```bash
$TOOLS/IccBenchApply/iccBenchApply -perxform -pixels 65536 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1 | grep -c "attribution"
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `0` — and note the tool currently *rejects* `-perxform` as an unknown
option with `rc=1`, which is also a correct failure for this step.

- [ ] **Step 3: Collect xform pointers instead of only their types**

`CChainReporter` in Task 2 stores types. Extend it to keep the pointers, which
is what timing needs. `IterateXforms` hands out `const CIccXform*`, but
`GetNewApply` is non-const, so store a non-const pointer obtained by cast. Add
a comment stating why the cast is sound:

```cpp
class CChainReporter : public IXformIterator
{
public:
  virtual void iterate(const CIccXform *pXform)
  {
    if (!pXform)
      return;
    // IterateXforms hands out const pointers, but GetNewApply() is non-const
    // and allocates a per-apply object without mutating the xform's own state.
    // The CMM owns these and outlives this tool's use of them.
    m_xforms.push_back(const_cast<CIccXform *>(pXform));
  }

  std::vector<CIccXform *> m_xforms;
};
```

Update the Task 2 printing loop to iterate `m_xforms` after collection rather
than printing inside `iterate`, so collection and reporting are separable.

- [ ] **Step 4: Add the per-xform pass**

Add a `-perxform` flag (`static bool g_bPerXform = false;`) to the option loop
in `main`, then after the tier 1 table:

```cpp
  if (g_bPerXform) {
    printf("\n  per-xform breakdown:\n");
    printf("  %3s %-12s %9s %7s\n", "#", "type", "Mpx/s", "share");

    // Two buffers, ping-ponged: each xform reads what the previous produced.
    // Sized to the widest sample count any stage uses, so no stage can write
    // past the end of a buffer sized for a narrower neighbour.
    icUInt16Number nMax = nSrc > nDst ? nSrc : nDst;
    for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
      icUInt16Number n = reporter.m_xforms[i]->GetNumDstSamples();
      if (n > nMax)
        nMax = n;
    }

    std::vector<icFloatNumber> a((size_t)g_nPixels * nMax);
    std::vector<icFloatNumber> b((size_t)g_nPixels * nMax);
    icBenchFill(a.data(), g_nPixels, nSrc, 20260820u);

    std::vector<double> secs;
    icFloatNumber *pIn = a.data(), *pOut = b.data();

    for (size_t i = 0; i < reporter.m_xforms.size(); i++) {
      CIccXform *pXform = reporter.m_xforms[i];

      icStatusCMM xstat = icCmmStatOk;
      CIccApplyXform *pApply = pXform->GetNewApply(xstat);
      if (!pApply || xstat != icCmmStatOk) {
        printf("  %3d %-12s   (no apply object; skipped)\n",
               (int)i, XformTypeName(pXform->GetXformType()));
        secs.push_back(0.0);
        delete pApply;
        continue;
      }

      const icUInt16Number nIn  = pXform->GetNumSrcSamples();
      const icUInt16Number nOut = pXform->GetNumDstSamples();

      const BenchStats xs = icBenchRun(
        [&]() {
          const icFloatNumber *s = pIn;
          icFloatNumber *d = pOut;
          for (icUInt32Number k = 0; k < g_nPixels; k++, s += nIn, d += nOut)
            pXform->Apply(pApply, d, s);
        },
        g_nPixels, g_nRepeats);

      secs.push_back(xs.medianMpxPerSec > 0.0 ? 1.0 / xs.medianMpxPerSec : 0.0);
      printf("  %3d %-12s %9.2f", (int)i,
             XformTypeName(pXform->GetXformType()), xs.medianMpxPerSec);
      printf("\n");

      delete pApply;
      icFloatNumber *t = pIn; pIn = pOut; pOut = t;
    }

    double total = 0.0;
    for (size_t i = 0; i < secs.size(); i++)
      total += secs[i];
    if (total > 0.0) {
      printf("\n  shares: ");
      for (size_t i = 0; i < secs.size(); i++)
        printf("%s#%d %.0f%%", i ? ", " : "", (int)i, 100.0 * secs[i] / total);
      printf("\n");
    }

    printf("\n  note: per-xform timing defeats the chunked-apply cache locality\n"
           "        that the chain number measures (IccCmm.cpp:8671), so these\n"
           "        rows sum to more than it. They are for attribution, not a\n"
           "        decomposition of the chain figure.\n");
  }
```

Two API facts are confirmed; one assumption is genuinely yours to check:

1. **`CIccXform::Apply` is public** — `IccProfLib/IccCmm.h:448`, inside the
   `public:` block opened at `:389` (`protected:` does not start until `:516`).
   `ApplyN` sits beside it at `:449`. No friend declaration needed, which matters
   because adding one would violate the no-library-changes constraint.
2. **`GetNumSrcSamples` / `GetNumDstSamples`** are the accessors `CIccXform::ApplyN`
   itself uses (`IccCmm.cpp:1706-1712`).
3. **The ping-pong assumes each xform's source width equals the previous xform's
   destination width.** That should hold for a chain the CMM built, but it is an
   assumption rather than something the headers guarantee. If a run shows
   garbage, print `nIn`/`nOut` per stage and check this before changing anything
   else.

- [ ] **Step 5: Run the test to verify it passes**

```bash
$TOOLS/IccBenchApply/iccBenchApply -perxform -pixels 65536 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1
```
Expected: a `per-xform breakdown` table, a `shares:` line, and the
`attribution, not a decomposition` note. The Step 1 `grep -c` now returns `1`.

- [ ] **Step 6: Verify a PCS row appears in a two-profile chain**

A single profile has no inter-profile PCS step; two do. This is the check that
the tier does what it was built for.

```bash
$TOOLS/IccBenchApply/iccBenchApply -perxform -pixels 65536 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1 \
  Testing/ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc 1 | grep -E "PCS|type"
```
Expected: at least one row whose type column reads `PCS`. **If no PCS row
appears, stop and report it** — either `Begin()` optimised the step out as an
identity (which is legitimate and interesting), or `IterateXforms` does not
expose PCS xforms and the spec's constraint 1 is wrong. Both are findings worth
surfacing, not working around.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
git commit -m "feat(bench): add per-xform breakdown including PCS steps

Times each xform in the resolved chain individually, via the public
GetNewApply()/Apply() pair, so the CIccPcsXform entries Begin() inserts between
profiles become visible. Those steps are where the sparse-matrix and icXYZtoLab
findings live and they are invisible in the whole-chain number.

The report carries its own caveat: per-xform timing defeats the chunked-apply
cache locality the chain figure measures, so the rows sum to more than it. They
attribute cost; they do not decompose the chain number.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: Threading axis with checksum invariance

Deliverable: `-threads 1,8` reports throughput and scaling per thread count and
**fails** if the checksum differs between them.

**Files:**
- Modify: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`

**Interfaces:**
- Consumes: everything from Task 3.
- Produces: nothing later tasks depend on, but Task 7's `-suite` calls the same
  code path, so keep the per-thread-count loop in a function:
  ```cpp
  static bool RunThreadSweep(CIccCmm &cmm, const std::vector<int> &threads,
                             const char *szCaseName);
  ```
  Returns `false` if any checksum mismatched.

- [ ] **Step 1: Write the failing test**

```bash
$TOOLS/IccBenchApply/iccBenchApply -threads 1,2 -pixels 262144 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1; echo "rc=$?"
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `Unknown option '-threads'` and `rc=1`.

- [ ] **Step 3: Parse the thread list**

Add to the option loop. A comma list, each entry 1..1024:

```cpp
    else if (!stricmp(argv[nArg], "-threads")) {
      if (nArg + 1 >= argc) {
        printf("-threads needs a comma-separated list, e.g. 1,2,8\n");
        return 1;
      }
      g_threads.clear();
      const char *p = argv[nArg + 1];
      std::string tok;
      while (true) {
        if (*p == ',' || *p == '\0') {
          int n;
          if (!ParseIntArg(tok.c_str(), 1, 1024, n)) {
            printf("Invalid thread count '%s': expected 1..1024\n", tok.c_str());
            return 1;
          }
          g_threads.push_back(n);
          tok.clear();
          if (!*p)
            break;
        }
        else {
          tok.push_back(*p);
        }
        p++;
      }
      nArg += 2;
    }
```

with `static std::vector<int> g_threads;` at file scope, defaulted to `{1}` in
`main` if left empty after parsing.

- [ ] **Step 4: Write `RunThreadSweep`**

```cpp
// Runs the chain at each requested thread count.
//
// The checksum must be identical at every count. That is the assertion, not a
// nicety: it is what catches precomputed state parked on a shared CIccXform or
// CIccPcsStep when it belonged in the per-thread apply object. A hoist that
// looks correct single-threaded and races under load fails here.
static bool RunThreadSweep(CIccCmm &cmm, const std::vector<int> &threads,
                           const char *szCaseName)
{
  const icUInt16Number nSrc = cmm.GetSourceSamples();
  const icUInt16Number nDst = cmm.GetDestSamples();

  std::vector<icFloatNumber> src((size_t)g_nPixels * nSrc);
  std::vector<icFloatNumber> dst((size_t)g_nPixels * nDst);
  icBenchFill(src.data(), g_nPixels, nSrc, 20260820u);

  bool bOk = true;
  icUInt32Number sumFirst = 0;
  double rateFirst = 0.0;

  for (size_t i = 0; i < threads.size(); i++) {
    const int n = threads[i];

    BenchStats st;
    icUInt32Number sum;

    if (n == 1) {
      st = icBenchRun(
        [&]() { cmm.Apply(dst.data(), src.data(), g_nPixels); },
        g_nPixels, g_nRepeats);
      sum = icBenchChecksum(dst.data(), dst.size());
    }
    else {
      // Attach() with bDeleteCmm=false: this tool owns theCmm and reuses it
      // across thread counts, so the wrapper must not take ownership.
      CIccThreadedCmm *pT = CIccThreadedCmm::Attach(&cmm, n, false);
      if (!pT) {
        printf("  %-16s t=%-4d  (Attach failed; skipped)\n", szCaseName, n);
        continue;
      }
      st = icBenchRun(
        [&]() { pT->Apply(dst.data(), src.data(), g_nPixels); },
        g_nPixels, g_nRepeats);
      sum = icBenchChecksum(dst.data(), dst.size());
      delete pT;
    }

    if (!i) {
      sumFirst  = sum;
      rateFirst = st.medianMpxPerSec;
    }

    const bool bMatch = (sum == sumFirst);
    if (!bMatch)
      bOk = false;

    printf("  %-16s t=%-4d %9.2f %7.2fx  0x%08x  %s\n",
           i ? "" : szCaseName, n, st.medianMpxPerSec,
           rateFirst > 0.0 ? st.medianMpxPerSec / rateFirst : 0.0,
           sum, bMatch ? "OK" : "CHECKSUM MISMATCH");
  }

  if (!bOk) {
    printf("\n  FAIL: %s produced different output at different thread counts.\n"
           "        Per-thread state is being shared. This is a correctness\n"
           "        defect, not a performance one.\n", szCaseName);
  }
  return bOk;
}
```

Add `#include "IccCmmThread.h"`. The ownership question is settled, so pass
`false` as written: `Attach(CIccCmm*, int nThreads=0, bool bDeleteCmm=true)` is
declared at `IccProfLib/IccCmmThread.h:153`, and `bDeleteCmm` genuinely governs
ownership — `~CIccThreadedCmm` does `if (m_bDeleteCmm) delete m_pCmm;`
(`IccCmmThread.cpp:256-260`), and the comment at `:280` reads "if true, pCmm is
owned and deleted with this object". Since this tool reuses one `theCmm` across
every thread count, `false` is required; `true` would free it after the first
non-serial iteration and the next one would use freed memory.

Call it from `main` in place of the Task 3 inline timing, and make `main` return
`1` when it returns `false`.

- [ ] **Step 5: Run the test to verify it passes**

```bash
$TOOLS/IccBenchApply/iccBenchApply -threads 1,2 -pixels 262144 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1; echo "rc=$?"
```
Expected: two rows, both ending `OK`, `rc=0`. The `t=2` scaling figure may be
below `2.00x` — that is fine and expected; only the checksum is asserted.

- [ ] **Step 6: Verify the mismatch path actually reports**

The failure branch is the most important code in this task and the least likely
to run, so prove it works. Temporarily corrupt one byte of `dst` between the two
counts:

```cpp
      // TEMPORARY -- verify the mismatch path reports. Remove before commit.
      if (n != 1) dst[0] += (icFloatNumber)1.0;
```

Rebuild, re-run Step 5. Expected: `CHECKSUM MISMATCH`, the `FAIL:` block, and
`rc=1`. **Then remove the two lines and rebuild**, and re-run Step 5 to confirm
`OK` and `rc=0` again. Do not commit with the temporary lines present —
`git diff --cached` before committing.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
git commit -m "feat(bench): sweep thread counts and assert checksum invariance

Runs each chain at every requested thread count via CIccThreadedCmm::Attach and
requires the output checksum to be identical across all of them.

That assertion is the point. The optimisation work this harness precedes moves
precomputed state into Begin(), and the rule is that anything written during
Apply() belongs in the per-thread apply object rather than the shared xform. A
hoist that breaks the rule can look correct single-threaded; this catches it,
and reports it as the correctness defect it is rather than a slow case.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Isolated leaf-function timing

Deliverable: `-leaf` reports Mvalues/s for `CIccTagCurve::Apply` and the CLUT
interpolators, reached through public tag accessors.

**Files:**
- Modify: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`

**Interfaces:**
- Consumes: `icBenchRun`, `icBenchFill` from Task 3.
- Produces: nothing later tasks depend on.

- [ ] **Step 1: Write the failing test**

```bash
$TOOLS/IccBenchApply/iccBenchApply -leaf -pixels 65536 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1 | grep -c "Mval/s"
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `Unknown option '-leaf'`, `rc=1`, grep count `0`.

- [ ] **Step 3: Implement the leaf pass**

The route in is `CIccProfile::FindTag`, then `CIccMBB::GetCLUT()` and
`GetCurvesA()` — both public at `IccProfLib/IccTagLut.h:495-496`.

```cpp
// Times the hot leaf functions in isolation.
//
// Reported in Mval/s, not Mpx/s, and printed under its own heading: these
// numbers are not comparable to the chain figure. Isolation changes cache
// behaviour, so a win measured here may not survive in a real chain -- which is
// exactly why the chain tier exists. This tier answers "did the change to this
// function work", not "does it matter".
static void RunLeaf(const char *szProfilePath)
{
  CIccProfile *pProfile = ReadIccProfile(szProfilePath);
  if (!pProfile) {
    printf("  (leaf: cannot read '%s')\n", szProfilePath);
    return;
  }

  printf("\n  isolated leaf functions (%s):\n", szProfilePath);
  printf("  %-28s %9s\n", "function", "Mval/s");

  // A2B0 is where a LUT-based profile keeps its device-to-PCS transform.
  CIccTag *pTag = pProfile->FindTag(icSigAToB0Tag);
  CIccMBB *pMBB = NULL;
  if (pTag) {
    switch (pTag->GetType()) {
      case icSigLut8Type:
      case icSigLut16Type:
      case icSigLutAtoBType:
        pMBB = (CIccMBB *)pTag;
        break;
      default:
        break;
    }
  }

  if (!pMBB) {
    printf("  (no A2B0 LUT tag; nothing to time)\n");
    delete pProfile;
    return;
  }

  // No pMBB->Begin() here: CIccMBB does not declare Begin() and neither does
  // CIccTag, so that call would not compile. The curve and the CLUT each carry
  // their own Begin() and are initialised individually below -- which is all
  // this tier needs, since it drives them directly rather than through the tag.

  LPIccCurve *pCurves = pMBB->GetCurvesA();
  if (pCurves && pCurves[0]) {
    CIccCurve *pCurve = pCurves[0];
    pCurve->Begin();

    std::vector<icFloatNumber> vals(g_nPixels);
    icBenchFill(vals.data(), g_nPixels, 1, 20260820u);
    volatile icFloatNumber sink = 0;

    const BenchStats cs = icBenchRun(
      [&]() {
        icFloatNumber acc = 0;
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          acc += pCurve->Apply(vals[k]);
        sink = acc;   // consume, so the loop is not elided
      },
      g_nPixels, g_nRepeats);

    printf("  %-28s %9.2f\n", "CIccCurve::Apply", cs.medianMpxPerSec);
  }

  CIccCLUT *pCLUT = pMBB->GetCLUT();
  if (pCLUT && pMBB->InputChannels() == 3) {
    pCLUT->Begin();

    std::vector<icFloatNumber> in((size_t)g_nPixels * 3);
    std::vector<icFloatNumber> out((size_t)g_nPixels * pMBB->OutputChannels());
    icBenchFill(in.data(), g_nPixels, 3, 20260820u);

    const icUInt16Number nOut = pMBB->OutputChannels();

    const BenchStats t3 = icBenchRun(
      [&]() {
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          pCLUT->Interp3dTetra(&out[(size_t)k * nOut], &in[(size_t)k * 3]);
      },
      g_nPixels, g_nRepeats);
    printf("  %-28s %9.2f\n", "CIccCLUT::Interp3dTetra", t3.medianMpxPerSec);

    const BenchStats l3 = icBenchRun(
      [&]() {
        for (icUInt32Number k = 0; k < g_nPixels; k++)
          pCLUT->Interp3d(&out[(size_t)k * nOut], &in[(size_t)k * 3]);
      },
      g_nPixels, g_nRepeats);
    printf("  %-28s %9.2f\n", "CIccCLUT::Interp3d", l3.medianMpxPerSec);
  }

  printf("  note: isolated figures are not comparable to the chain number;\n"
         "        isolation changes cache behaviour.\n");

  delete pProfile;
}
```

Every API fact here is confirmed; use them as written:

- `CIccMBB::InputChannels()` and `OutputChannels()` are public at
  `IccProfLib/IccTagLut.h:471-472` (the `public:` block opens at `:452`). Both
  return `icUInt8Number`, so assigning to an `icUInt16Number` widens safely.
- The tag enums are `icSigLut16Type` (`'mft2'`), `icSigLut8Type` (`'mft1'`), and
  `icSigLutAtoBType` (`'mAB '`) — `IccProfLib/icProfileHeader.h:558-560`.
- `CIccCurve::Begin()` is public and virtual at `IccTagLut.h:96`, overridden by
  `CIccTagCurve::Begin()` at `:162`. `CIccCLUT::Begin()` is public at `:376`.

One real limitation to keep in mind rather than fix: `CIccCLUT::Begin()` returns
`void` (`IccTagLut.cpp:2424`), so it cannot report failure, and a malformed CLUT
can still misbehave after it. That is finding A1's subject and is deliberately
out of scope here. Guard on `pMBB->InputChannels() == 3` as written and do not
attempt N-D — see the accepted gap in the spec.

- [ ] **Step 4: Add the flag and call it**

`static bool g_bLeaf = false;` plus an option-loop branch, and in `main` after
the other tiers, call `RunLeaf` on the **first** profile in the chain. Record
the first profile path in a local while parsing the chain.

- [ ] **Step 5: Run the test to verify it passes**

```bash
$TOOLS/IccBenchApply/iccBenchApply -leaf -pixels 65536 -repeats 3 1 \
  Testing/sRGB_v4_ICC_preference.icc 1
```
Expected: an `isolated leaf functions` heading with a `Mval/s` column and at
least the `Interp3dTetra` and `Interp3d` rows. `sRGB_v4_ICC_preference.icc` is
RGB→Lab with an A2B0, so both should appear.

- [ ] **Step 6: Sanity-check the two interpolators differ**

Tetrahedral and linear do different amounts of work, so identical figures to two
decimals suggest the wrong function is being timed twice. Compare the two rows;
if they match exactly, re-read the lambdas before continuing.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/iccBenchApply.cpp
git commit -m "feat(bench): add isolated leaf-function timing

Times CIccCurve::Apply and the 3D CLUT interpolators directly, through the
public CIccMBB::GetCLUT()/GetCurvesA() accessors -- no friend declarations and
no library change.

Reported in Mval/s under its own heading because these figures are not
comparable to the chain number: isolation changes cache behaviour, so a win here
may not survive in a real chain. This tier answers whether a change to a given
function worked; the chain tier answers whether it matters.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: The `-suite` case table, CSV output, and the `Begin()` idempotency check

Deliverable: `-suite` runs the eight built-in cases with `SKIP` for missing
profiles, `-csv` emits machine-readable rows, and one case exercises a second
`Begin()`.

**Files:**
- Create: `Tools/CmdLine/IccBenchApply/BenchCases.h`
- Create: `Tools/CmdLine/IccBenchApply/BenchCases.cpp`
- Modify: `Tools/CmdLine/IccBenchApply/iccBenchApply.cpp`
- Modify: `Build/Cmake/Tools/IccBenchApply/CMakeLists.txt` (add `BenchCases.cpp`)

**Interfaces:**
- Consumes: `RunThreadSweep` from Task 5, timing helpers from Task 3.
- Produces:
  ```cpp
  struct BenchChainLink { std::string profile; int encodedIntent; };
  struct BenchCase {
    std::string name;
    int interpolation;
    std::vector<BenchChainLink> chain;
  };
  const std::vector<BenchCase> &icBenchBuiltinCases();
  bool icBenchResolveProfile(const std::string &rel, std::string &abs);
  void icBenchSetRoots(const char *szSourceRoot, const char *szBuildRoot);
  ```

- [ ] **Step 1: Write the failing test**

```bash
$TOOLS/IccBenchApply/iccBenchApply -suite -pixels 65536 -repeats 3 -csv; echo "rc=$?"
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `Unknown option '-suite'`, `rc=1`.

- [ ] **Step 3: Write `BenchCases.h`**

Copyright header, then the three structs and three function declarations from
the Interfaces block above, each with a one-line comment. Guard with
`#ifndef _BENCHCASES_H`.

- [ ] **Step 4: Write `BenchCases.cpp`**

The table. Every path is relative to `Testing/`; the resolver handles the roots.

```cpp
// The built-in case table.
//
// Chains, not single profiles, because not every profile round-trips: the
// six-channel SpecRef profile is scnr class and can only appear first, as can
// the v2 grayTRC fixture.
//
// Intent 1 is relative colorimetric throughout except pcs-abs, which uses 3
// (absolute) deliberately: absolute forces the AdjustPCS/CheckSrcAbs path, so
// the pcs-abs minus pcs-rel delta isolates PCS-adjustment cost.
static const struct { const char *name; int interp; const char *chain; }
kCases[] = {
  { "matrix-trc",   1, "V2/v2RgbMatrixTRC.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "lut-3d-tetra", 1, "V2/v2RgbLut8.icc:1|V2/v2CmykLut16.icc:1" },
  { "lut-6ch",      1, "SpecRef/SixChanCameraRef.icc:1|V2/v2CmykLut16.icc:1" },
  { "monochrome",   1, "V2/v2GrayTRC.icc:1|V2/v2RgbMatrixTRC.icc:1" },
  { "mpe-calc",     1, "Calc/srgbCalcTest.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "mpe-tonemap",  1, "Display/Rec2100HlgFull.icc:1|ApplyDataFiles/test-profiles/sRGB_D65_MAT.icc:1" },
  { "pcs-rel",      1, "V2/v2RgbLut8.icc:1|V2/v2CmykLut16.icc:1" },
  { "pcs-abs",      1, "V2/v2RgbLut8.icc:3|V2/v2CmykLut16.icc:3" },
};
```

Note `lut-6ch` is named for `Interp6d`, which is what six input channels
actually reach — **not** `InterpND`. The spec records that as an accepted gap;
do not rename this case to suggest N-D coverage it does not have.

Parse that `path:intent|path:intent` form into `BenchCase` in
`icBenchBuiltinCases()` on first call, into a function-local static.

The resolver tries both roots, because the POSIX profile fixture writes into the
source tree and the Windows one into the build tree
(`Build/Cmake/Testing/CMakeLists.txt:1746`):

```cpp
// Two roots, not one. iccdev.create-profiles writes generated profiles into the
// source tree on POSIX and iccdev.windows-create-profiles writes them into the
// build tree, so a single hardcoded root works on exactly one platform.
bool icBenchResolveProfile(const std::string &rel, std::string &abs)
{
  const char *roots[2] = { g_szSourceRoot, g_szBuildRoot };
  for (int i = 0; i < 2; i++) {
    if (!roots[i] || !*roots[i])
      continue;
    std::string cand = std::string(roots[i]) + "/Testing/" + rel;
    FILE *f = fopen(cand.c_str(), "rb");
    if (f) {
      fclose(f);
      abs = cand;
      return true;
    }
  }
  return false;
}
```

Set the roots from `ICCDEV_BENCH_SOURCE_ROOT` / `ICCDEV_BENCH_BUILD_ROOT`
environment variables if present, defaulting to `.` and `.` — Task 8 sets them
from CMake.

- [ ] **Step 5: Wire `-suite` and `-csv` into `main`**

For each case: resolve every profile; if any is missing, print
`SKIP <name>: <path> not found` and continue **without** setting a failure flag.
Otherwise build the chain, `Begin()`, and run `RunThreadSweep`.

Then the idempotency check, which the spec requires (constraint 6):

```cpp
    // CIccCmm::Begin() returns early when m_pApply is set -- the idempotency
    // contract e4e05c3a restored for #1940. The optimisation branches add
    // Begin()-time precomputation, so a second Begin() must stay a no-op both
    // in status and in output.
    if (!strcmp(pCase->name.c_str(), "lut-3d-tetra")) {
      icStatusCMM again = theCmm.Begin();
      if (again != icCmmStatOk) {
        printf("  FAIL: second Begin() on %s reported %d, expected icCmmStatOk\n",
               pCase->name.c_str(), (int)again);
        bAllOk = false;
      }
      else {
        theCmm.Apply(dst.data(), src.data(), g_nPixels);
        if (icBenchChecksum(dst.data(), dst.size()) != sumBefore) {
          printf("  FAIL: second Begin() on %s changed the output\n",
                 pCase->name.c_str());
          bAllOk = false;
        }
      }
    }
```

`-csv` switches every row to `case,threads,mpx_per_sec,min,max,checksum,status`
with a single header line. `main` returns non-zero only on a real failure — a
`SKIP` is exit 0.

Add `BenchCases.cpp` to `SOURCES` in the tool's CMakeLists.

- [ ] **Step 6: Run the test to verify it passes**

```bash
cd Testing && ./CreateAllProfiles.sh >/dev/null 2>&1; cd ..
$TOOLS/IccBenchApply/iccBenchApply -suite -pixels 65536 -repeats 3; echo "rc=$?"
```
Expected: eight case rows, `rc=0`. `monochrome` must **not** be `SKIP` — Task 1
created its profile. If it is, the resolver or the generation wiring is wrong.

Then confirm skip semantics without profiles:

```bash
cd Testing && ./CreateAllProfiles.sh clean >/dev/null 2>&1; cd ..
$TOOLS/IccBenchApply/iccBenchApply -suite -pixels 65536 -repeats 3; echo "rc=$?"
```
Expected: several `SKIP` lines, `matrix-trc` still running (its second profile is
tracked), and `rc=0`. Regenerate afterwards.

- [ ] **Step 7: Commit**

```bash
git add Tools/CmdLine/IccBenchApply/BenchCases.h \
        Tools/CmdLine/IccBenchApply/BenchCases.cpp \
        Tools/CmdLine/IccBenchApply/iccBenchApply.cpp \
        Build/Cmake/Tools/IccBenchApply/CMakeLists.txt
git commit -m "feat(bench): add the built-in case table, CSV output, and a Begin() re-entry check

Eight cases, each a chain rather than a single profile because not every profile
round-trips -- the six-channel SpecRef profile and the grayTRC fixture are both
input-only and can appear only first.

Profiles resolve against both the source and build trees: the POSIX profile
fixture writes generated profiles into one and the Windows fixture into the
other, so a single root works on exactly one platform. A missing profile is a
SKIP with a reason, not a failure, so a build without ENABLE_ICCXML still runs
the cases whose profiles are tracked.

One case Begin()s twice and requires both the status and the output checksum to
be unchanged, holding the idempotency contract e4e05c3a restored for #1940 --
the contract the Begin()-time precomputation that follows could quietly break.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: CTest registration and Readme

Deliverable: `ctest -R apply-throughput` passes, `check` includes it, `check-fast`
skips it.

**Files:**
- Create: `Tools/CmdLine/IccBenchApply/Readme.md`
- Modify: `Build/Cmake/Testing/CMakeLists.txt`

**Interfaces:**
- Consumes: the `iccBenchApply` target and its `-suite -csv` interface.
- Produces: nothing.

- [ ] **Step 1: Write the failing test**

```bash
ctest --test-dir $BUILD -C Release -R apply-throughput --output-on-failure; echo "rc=$?"
```

- [ ] **Step 2: Run it to verify it fails**

Expected: `No tests were found!!!` and non-zero `rc`.

- [ ] **Step 3: Register the test**

Append to `Build/Cmake/Testing/CMakeLists.txt`, modelled on the
`iccdev.iccconnect-threaded-cmm` block at `:143-186` — including its Windows
`ENVIRONMENT_MODIFICATION` handling, which is what makes a DLL build find
`IccProfLib2.dll` at test time.

```cmake
# Throughput harness. Records; never asserts a timing threshold -- shared CI
# runners vary 20-30% between runs on the same commit, so a threshold would
# flake. The test passes when every case either measured or reported SKIP.
#
# The 'slow' label puts it on the existing opt-out path: check-fast (:88)
# excludes 'slow' while check (:29) excludes only 'known-red', so the full
# suite runs it and the fast loop does not.
if(TARGET iccBenchApply)
  add_dependencies(build-test-binaries iccBenchApply)

  if(WIN32)
    add_test(
      NAME iccdev.apply-throughput
      COMMAND "$<TARGET_FILE:iccBenchApply>" -suite -csv -pixels 65536 -repeats 3
    )
  else()
    add_test(
      NAME iccdev.apply-throughput
      COMMAND
        "${CMAKE_COMMAND}" -E env
        ${ICCDEV_TEST_ENV}
        "$<TARGET_FILE:iccBenchApply>" -suite -csv -pixels 65536 -repeats 3
    )
  endif()

  set_tests_properties(iccdev.apply-throughput PROPERTIES
    WORKING_DIRECTORY "${ICCDEV_REPO_ROOT}"
    TIMEOUT 300
    LABELS "iccdev;iccprofLib;benchmark;throughput;slow"
    FIXTURES_REQUIRED iccdev_profiles
    ENVIRONMENT "ICCDEV_BENCH_SOURCE_ROOT=${ICCDEV_REPO_ROOT};ICCDEV_BENCH_BUILD_ROOT=${CMAKE_BINARY_DIR}"
  )

  if(WIN32)
    set(_bench_windows_env_mods
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:iccBenchApply>"
      "PATH=path_list_prepend:$<TARGET_FILE_DIR:${TARGET_LIB_ICCPROFLIB}>"
    )
    foreach(_runtime_path IN LISTS ICCDEV_WINDOWS_RUNTIME_PATHS)
      list(APPEND _bench_windows_env_mods
        "PATH=path_list_prepend:${_runtime_path}")
    endforeach()
    set_tests_properties(iccdev.apply-throughput PROPERTIES
      ENVIRONMENT_MODIFICATION "${_bench_windows_env_mods}"
    )
  endif()
endif()
```

`ENVIRONMENT` and `ENVIRONMENT_MODIFICATION` are separate properties, so setting
both is safe — verify with `ctest -N -R apply-throughput` that the test is listed
before running it.

- [ ] **Step 4: Run the test to verify it passes**

```bash
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64
cmake --build out/vs2022-x64 --config Release --target iccBenchApply
ctest --test-dir out/vs2022-x64 -C Release -R apply-throughput --output-on-failure; echo "rc=$?"
```
Expected: `1 test passed`, `rc=0`, and the CSV rows visible in the output.

- [ ] **Step 5: Verify the label wiring both ways**

```bash
ctest --test-dir $BUILD -C Release -N -R apply-throughput
ctest --test-dir $BUILD -C Release -N --label-exclude slow -R apply-throughput
```
Expected: the first lists the test; the second lists **no** tests. That is the
`check` / `check-fast` split working. If the second still lists it, the `slow`
label did not take.

- [ ] **Step 6: Write the Readme**

`Tools/CmdLine/IccBenchApply/Readme.md`, following the style of
`Tools/CmdLine/IccRoundTrip/Readme.md`. It must cover usage, the case table, and
these four things, because each is a way to misread the output:

1. **Per-xform shares attribute cost; they do not decompose the chain figure.**
   Per-xform timing defeats the chunked-apply cache locality at
   `IccCmm.cpp:8671`, so the rows sum to more than the chain number.
2. **Checksums only compare across builds with matching compiler flags.** The
   hash is over float bit patterns, so legitimate FP reassociation moves it. A
   diff means investigate, not regress.
3. **`lut-6ch` measures `Interp6d`, not `InterpND`.** `CIccCLUT` dispatches 5 and
   6 input channels to dedicated routines. A true `InterpND` case needs a
   ≥7-channel profile and none is tracked; that is a known gap, deferred.
4. **No timing threshold is asserted anywhere.** CI records. Numbers that decide
   anything should be taken locally on an otherwise idle machine.

- [ ] **Step 7: Commit**

```bash
git add Build/Cmake/Testing/CMakeLists.txt Tools/CmdLine/IccBenchApply/Readme.md
git commit -m "test(bench): register the throughput harness with CTest

Record-only: the test passes when every case either measured or reported SKIP,
and no timing threshold is asserted anywhere. Shared CI runners vary 20-30%
between runs on the same commit, so a threshold would flake without catching
anything a local run would not.

Labelled 'slow' to use the existing opt-out path rather than invent one --
check-fast excludes 'slow', check excludes only 'known-red' -- so the full suite
runs it and the fast loop does not.

Readme records the four ways to misread the output: per-xform shares attribute
cost rather than decomposing the chain figure, checksums only compare across
matching compiler flags, lut-6ch measures Interp6d and not InterpND, and nothing
here gates on time.

Co-Authored-By: Claude Opus 5 (1M context) <noreply@anthropic.com>"
```

---

## Self-review notes

Checked against the spec:

- **Every spec section maps to a task.** Files → Tasks 2/7/8; CLI contract →
  Tasks 2/5/6/7; tier 1 → Task 3; tier 2 → Task 4; tier 3 → Task 6; threading →
  Task 5; checksum oracle → Task 3; input buffer → Task 3; methodology → Task 3;
  new test asset → Task 1; case table → Task 7; CTest integration → Task 8;
  error handling → Tasks 2 and 7; "testing the harness itself" → the determinism
  check in Task 3 Step 1, thread invariance in Task 5, and `Begin()` re-entry in
  Task 7.
- **One spec item deliberately not implemented:** the spec's "known-answer case"
  cross-checked against `iccApplyToLink`. Those two tools apply different
  encodings, so agreement would require replicating `iccApplyToLink`'s encoding
  conversion and would test that replication rather than the harness. The three
  checks above cover the same risk — that the harness silently changes what it
  measures. Flagging rather than silently dropping.
- **API facts were resolved rather than left as homework.** A first draft of this
  plan asked the implementer to verify `GetStatusText`, `CIccXform::Apply`
  accessibility, `Attach`'s third parameter, the `CIccMBB` accessors, and the LUT
  tag enum spellings. All are now stated with citations. Resolving them found a
  real defect in the draft: Task 6 called `pMBB->Begin()`, and neither `CIccMBB`
  nor `CIccTag` declares `Begin()`, so that line would not have compiled. It is
  removed, with a comment explaining why, and the curve and CLUT are initialised
  individually instead.
- **Two assumptions remain genuinely open**, and are flagged as such rather than
  asserted: the per-xform ping-pong buffer assumes adjacent sample widths match
  (Task 4), and whether `IterateXforms` exposes PCS xforms is checked
  empirically in Task 4 Step 6 with instructions to report either outcome rather
  than work around it.
- **Type consistency:** `BenchStats`, `icBenchRun`, `icBenchChecksum`,
  `icBenchFill`, `RunThreadSweep`, `icBenchResolveProfile` are spelled
  identically at every use.
