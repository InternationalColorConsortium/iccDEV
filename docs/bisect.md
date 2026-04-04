# Bisecting and Fixing Regressions in iccDEV

## Overview

This document describes the workflow for identifying, bisecting, fixing, and
confirming regressions in the iccDEV test suite. It covers the full lifecycle
from first failure observation through upstream issue filing and CI regression
test creation.

The workflow was validated across 14 regressions fixed in PR #768 (April 2026).

## Prerequisites

- clang-18 / clang++-18
- cmake 3.21+
- libxml2-dev, libtiff-dev, libjpeg-dev, libpng-dev
- ASAN + UBSAN runtime (`libclang-rt-18-dev`)
- `git bisect` familiarity

## Step 1: Reproduce the Failure

Run the test suite with sanitizer instrumentation:

```bash
cd Build
cmake Cmake \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON -DENABLE_COVERAGE=ON
make -j"$(nproc)"

export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0

# Add all tool directories to PATH
for d in Tools/*/; do export PATH="$PWD/$d:$PATH"; done

cd ../Testing
bash RunTests.sh 2>&1 | tee /tmp/runtests.log
```

Identify the specific test that fails and the error message. Note whether
the failure is:

| Type | Example |
|------|---------|
| **Validation regression** | False "invalid" warnings on known-good profiles |
| **Parse failure** | "Unable to parse element" during iccFromXml |
| **ASAN finding** | heap-buffer-overflow, use-after-free, SEGV |
| **UBSAN finding** | signed integer overflow, enum out-of-range |
| **Crash** | Exit code >= 128 |

## Step 2: Bisect the Regression

### Automated bisect

```bash
cd /path/to/iccDEV
git bisect start
git bisect bad HEAD            # current commit is broken
git bisect good <known_good>   # last known-good commit

# Automated test script
git bisect run bash -c '
  cd Build && make -j"$(nproc)" 2>/dev/null &&
  export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML &&
  export ASAN_OPTIONS=halt_on_error=1,detect_leaks=0 &&
  <test_command>
'
```

### Manual bisect

For complex failures (multi-file XML tests, directory-sensitive tools):

```bash
git bisect start
git bisect bad HEAD
git bisect good <known_good>

# At each step:
cd Build && make -j"$(nproc)" 2>/dev/null
export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML
cd ../Testing/<test_dir>
<test_command>
# If test passes: git bisect good
# If test fails:  git bisect bad
```

### Tips for finding a known-good commit

```bash
# Check release tags
git tag --sort=-creatordate | head -10

# Binary search through history
git log --oneline --since="2025-01-01" | tail -1   # start of year
git log --oneline -1 v2.3.1.5                       # release tag
```

### Working directory considerations

Some tests (e.g., CMYK-3DLUTs, LaserProjector) reference external files
relative to the XML file's directory. Always `cd` into the test directory
before running `iccFromXml`.

## Step 3: Analyze the Regression Commit

Once bisect identifies the culprit commit:

```bash
git show <commit_sha> --stat          # files changed
git show <commit_sha> -- <file>       # specific file diff
git log -1 --format="%H %s" <sha>     # one-line summary
```

Common root cause patterns found in iccDEV regressions:

| Pattern | Example | PR |
|---------|---------|-----|
| **Variable rename without updating all references** | `n` renamed to `sampleCount`, 3 comparison sites missed | #189 |
| **Case-sensitive filename on Linux** | `A0-k.txt` vs `A0-K.txt` (Windows is case-insensitive) | #205 |
| **Overly restrictive validation added** | `m_nOutput > 15` check rejects spectral CLUTs with 400+ channels | #563 |
| **Validation logic collapsed** | 3-branch validate merged into 1, losing PCS/device distinction | #708 |
| **Incorrect size comparison** | Buffer size compared against file data size (different semantics) | #351 |
| **String mismatch between reader and writer** | `icGet*Name()` returns different string than `icGet*Value()` parses | Multiple |
| **Missing XML parser flag** | Large XML files rejected without `XML_PARSE_HUGE` | Multiple |
| **Semantic mismatch in parameter passing** | Caller squares a value the callee also squares | Multiple |

## Step 4: Create the Fix

### Fix conventions

1. **Minimal patch** -- change only what is necessary to fix the regression
2. **Match existing code style** -- 2-space indent, `m_` member prefix, K&R braces
3. **No unrelated cleanup** -- resist the urge to refactor adjacent code
4. **Add a comment if the fix is non-obvious**:
   ```cpp
   // PR #768: Restore 3-branch validation -- PCS and device color spaces
   // require independent checks per ICC.1-2022-05 Table 26
   ```

### Generate the patch

```bash
# After making the fix:
git diff IccProfLib/IccTagLut.cpp > /tmp/fix-description.patch

# Or for committed changes:
git format-patch -1 HEAD
```

## Step 5: Verify the Fix

### Build and test

```bash
cd Build
rm -f CMakeCache.txt    # clear stale cmake cache (CRITICAL)
cmake Cmake \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON -DENABLE_COVERAGE=ON
make -j"$(nproc)"

# Verify ASAN is linked (must be > 0)
nm Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
```

### Run the specific failing test

```bash
export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0

# Example: NamedColor test
cd ../Testing/Named
iccFromXml NamedColor.xml NamedColor.icc
# Expected: "Profile parsed and saved correctly" with 0 ASAN/UBSAN
```

### Run the full test suite

```bash
cd ../Testing
bash RunTests.sh 2>&1 | tee /tmp/runtests-fixed.log

# Verify 0 ASAN + 0 UBSAN errors
grep -c "runtime error:" /tmp/runtests-fixed.log    # should be 0
grep -c "ERROR: AddressSanitizer" /tmp/runtests-fixed.log  # should be 0
```

### CRITICAL: cmake cache gotcha

If `cmake` reuses a stale cache with `ENABLE_SANITIZERS=OFF`, the build
succeeds but produces uninstrumented binaries. ASAN findings will be silently
missed. Always verify:

```bash
nm Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
# Must be > 0. If 0: rm CMakeCache.txt and reconfigure.
```

Also note that `Build/Cmake/` is git-tracked. If you `rm -rf Build/`, you
must restore the cmake sources:

```bash
git checkout -- Build/
```

## Step 6: Add CI Regression Tests

Add regression checks to `.github/workflows/ci-iccdev-tool-tests.yml` to
prevent the bug from recurring. The workflow uses a `check_asan()` function
that checks for ASAN errors only (not UBSAN) to avoid false failures from
benign upstream UBSAN noise.

### Pattern for regression sub-tests

```bash
# R1: Bug description (issue #NNN)
echo "--- R1: Description ---"
cd "$GITHUB_WORKSPACE/Testing/<dir>"
ASAN_EXIT=0
iccFromXml Input.xml /tmp/output.icc 2>&1 | tee /tmp/r1.log || true
check_asan /tmp/r1.log "R1: Description"
ASAN_EXIT=$?
# Check for specific regression indicator
if grep -q "expected success string" /tmp/r1.log; then
  echo "[PASS] R1"
else
  echo "[FAIL] R1: regression indicator found"
  ASAN_EXIT=1
fi
R1_EXIT=$ASAN_EXIT
```

### PoC files for crash regressions

Store minimal proof-of-concept files in `Testing/`:

```
Testing/poc-599-gbd-sio.icc       # GBD signed integer overflow
Testing/poc-763-cenc-huaf.icc     # cenc use-after-free
Testing/CIccToneMapFunc-*.icc     # ToneMapFunc heap OOB
```

Name PoC files with the pattern: `poc-<issue>-<short-description>.<ext>`
or the full crash-site naming: `<CrashType>-<Class>-<Method>-<File>_cpp.<ext>`

## Step 7: File Upstream

### Issue format (gold standard)

Zero prose, zero opinions, zero analysis. Pure reproduction recipe.
See [issue #753](https://github.com/InternationalColorConsortium/iccDEV/issues/753)
and [issue #744](https://github.com/InternationalColorConsortium/iccDEV/issues/744)
as examples.

```markdown
## Maintainer Repro
<UTC timestamp>

## Bisect
Date: <date of culprit commit>
Commit: <sha>
PR: #<number>

## Build Unpatched
```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV && git checkout <bad_sha>
cd Build && cmake Cmake \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
make -j$(nproc)
export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML
ASAN_OPTIONS=halt_on_error=0,detect_leaks=0 \
  Tools/IccDumpProfile/iccDumpProfile poc-file.icc ALL
```

## Unpatched Output
<verbatim error or ASAN output>

## Patch
```diff
<unified diff>
```

## Patched Output
<clean result>
```

### Commit message convention

```
Fix: <short description> (#issue)

<one-line root cause explanation>

Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>
```

## Regression Inventory (PR #768)

| # | Bug | Bisect Commit | Root Cause | CWE | Fix |
|---|-----|---------------|------------|-----|-----|
| 1 | NamedColor false validation errors | b1722f9 (#189) | Variable rename missed 3 comparison sites | - | IccArrayBasic.cpp |
| 2-3 | CMYK-3DLUTs parse failure | 4aa5857 (#205) | Case-sensitive filenames (Linux) | - | CMYK-3DLUTs.xml |
| 4 | LaserProjector spectral CLUT rejected | ba81cd9 (#563) | `m_nOutput > 15` rejects spectral CLUTs | - | IccTagLut.cpp |
| 5 | ColorantOrder false validation | c2ea9da (#708) | 3-branch logic collapsed to 1 | - | IccTagBasic.cpp |
| 6 | SparseMatrix read failure | 4876a06 (#351) | Buffer vs file size comparison | - | IccTagBasic.cpp |
| 7 | Observer name round-trip mismatch | Multiple | Reader/writer string mismatch | - | IccUtilXml.cpp |
| 8 | Large XML parse failure | Multiple | Missing XML_PARSE_HUGE flag | - | IccProfileXml.cpp, IccMpeXml.cpp |
| 9 | Spectral CLUT squared steps | Multiple | nBytesPerPoint semantic mismatch | - | IccMpeSpectral.cpp |
| 10 | cenc profile use-after-free | - | AddXform ownership semantics | CWE-416 | IccCmm.cpp |
| 11 | ToneMapFunc heap-buffer-overflow | - | m_nParameters < NumArgs() | CWE-122 | IccMpeBasic.cpp |
| 12 | GBD signed integer overflow | - | vertex vs triangle count + 32-bit overflow | CWE-190 | IccTagLut.cpp |
| 13 | icF16toF unsigned underflow | 1f0a9dd | Unsigned cast of negative exponent bias | CWE-681 | IccUtil.cpp |

## See Also

- [Build Instructions](build.md)
- [Testing Infrastructure](../README.md)
- [File a Security Issue](../.github/prompts/file-security-issue.prompt.md)
- [Reproduce a Security Issue](../.github/prompts/reproduce-security-issue.prompt.md)
