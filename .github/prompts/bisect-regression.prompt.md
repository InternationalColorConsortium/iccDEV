# Bisect and Fix a Regression

Use this prompt when a test that previously passed starts failing on
the master branch, or when RunTests.sh produces new warnings, errors,
ASAN findings, or UBSAN findings that were not present before.

## Workflow Overview

1. **Reproduce** -- confirm the failure with sanitizer-instrumented build
2. **Bisect** -- `git bisect` to find the culprit commit
3. **Analyze** -- read the diff, identify the root cause pattern
4. **Fix** -- minimal patch, matching existing code style
5. **Verify** -- full RunTests.sh with 0 ASAN + 0 UBSAN
6. **Regress** -- add CI regression test to prevent recurrence
7. **Report** -- file upstream issue with gold-standard format

## Reproduce

```bash
cd Build
rm -f CMakeCache.txt
CC=clang CXX=clang++ \
  CXXFLAGS="-fsanitize=address,undefined,integer -fno-omit-frame-pointer -g -O1" \
  LDFLAGS="-fsanitize=address,undefined,integer" \
  cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
make -j"$(nproc)"

# CRITICAL: Verify ASAN is linked (must be > 0)
nm Tools/IccDumpProfile/iccDumpProfile | grep -c __asan

export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0

# Run the failing test
cd ../Testing/<test_dir>
<failing_command> 2>&1 | tee /tmp/repro.log
```

### cmake cache gotcha

cmake retains `ENABLE_SANITIZERS=OFF` from prior builds silently.
Always delete `CMakeCache.txt` and verify `nm | grep __asan > 0`.
Also: `Build/Cmake/` is git-tracked; `rm -rf Build/` requires
`git checkout -- Build/` to restore cmake sources.

## Bisect

```bash
git bisect start
git bisect bad HEAD
git bisect good <known_good_tag_or_sha>

# Automated:
git bisect run bash -c '
  cd Build && make -j"$(nproc)" 2>/dev/null &&
  export LD_LIBRARY_PATH=$PWD/IccProfLib:$PWD/IccXML &&
  export ASAN_OPTIONS=halt_on_error=1,detect_leaks=0 &&
  <test_command>
'

# Manual: build, test, then `git bisect good` or `git bisect bad`
git bisect reset   # when done
```

### Finding a known-good commit

```bash
git tag --sort=-creatordate | head -10
git log --oneline --since="2025-01-01" | tail -1
```

## Common Root Cause Patterns

| Pattern | How to Spot |
|---------|-------------|
| Variable rename missed references | diff shows rename but `grep` finds old name still used |
| Case-sensitive filename (Linux vs Windows) | `ls` shows different case than XML references |
| Overly restrictive validation | New `if (x > N) return false` on a value that legitimately exceeds N |
| Validation branches collapsed | Multi-branch `if/else if` merged into single branch |
| Incorrect size/offset comparison | Buffer capacity compared against file stream position |
| Reader/writer string mismatch | `icGet*Name()` output differs from `icGet*Value()` parser |
| Missing parser flag | `xmlReadFile()` without `XML_PARSE_HUGE` for large files |
| Unsigned underflow | `(icUInt32Number)(x) - bias` wraps when `x < bias` |
| Unsigned overflow in bounds check | `offset + size` wraps past `profileSize` -- use `size > limit \|\| offset > limit - size` |
| Parameter semantic mismatch | Caller computes `steps * bytesPerSample`, callee also multiplies |

## JSON/Config Parser Regressions

Latest branch: `bisect-60bbb8c-json`.

Fail closed for JSON parser/config helpers:
- Never ignore a nested `ParseJson()` false return.
- Never truncate inline CLUT or spectral arrays; reject wrong counts/types.
- Never skip failed struct members and return success.
- Reset all stale fields in `reset()` and honor `fromJson(..., true)`.
- Fixed-size array helpers must require exactly `n` numeric values.

Focused gate:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing \
  LD_LIBRARY_PATH=$PWD/Build/IccProfLib:$PWD/Build/IccXML:$PWD/Build/IccJSON \
  .github/scripts/iccdev-json-parser-regression-tests.sh
```

## Fix Conventions

- **Minimal patch**: change only what fixes the regression
- **Match existing style**: 2-space indent, `m_` prefix, K&R braces
- **No unrelated cleanup**: resist adjacent refactoring
- **Comment non-obvious fixes**: cite the PR/issue number
- **Generate patch**: `git diff > /tmp/fix.patch`

## Verify

```bash
# Rebuild
cd Build && make -j"$(nproc)"

# Run specific test
cd ../Testing/<dir>
<test_command>  # expected: success, 0 ASAN/UBSAN

# Run full suite
cd ../Testing
bash RunTests.sh 2>&1 | tee /tmp/runtests-fixed.log
grep -c "runtime error:" /tmp/runtests-fixed.log     # must be 0
grep -c "ERROR: AddressSanitizer" /tmp/runtests-fixed.log  # must be 0
```

## Add CI Regression Test

Add to `.github/workflows/ci-iccdev-tool-tests.yml` Test 18 section:

```bash
echo "--- RN: Bug description (#issue) ---"
cd "$GITHUB_WORKSPACE/Testing/<dir>"
ASAN_EXIT=0
<test_command> 2>&1 | tee /tmp/rN.log || true
check_asan /tmp/rN.log "RN: Description"
ASAN_EXIT=$?
if grep -q "expected output" /tmp/rN.log; then
  echo "[PASS] RN"
else
  echo "[FAIL] RN: regression"
  ASAN_EXIT=1
fi
RN_EXIT=$ASAN_EXIT
```

For crash-based regressions, add a PoC file to `Testing/`:
`poc-<issue>-<description>.icc`

## File Upstream Issue

Use the gold-standard format (zero prose, pure reproduction recipe):

```markdown
## Maintainer Repro
<UTC timestamp>

## Bisect
Commit: <sha>

## Build Unpatched
<exact commands>

## Unpatched Output
<verbatim error>

## Patch
<inline diff>

## Patched Output
<clean result>
```

Reference examples:
[#753](https://github.com/InternationalColorConsortium/iccDEV/issues/753),
[#744](https://github.com/InternationalColorConsortium/iccDEV/issues/744),
[#752](https://github.com/InternationalColorConsortium/iccDEV/issues/752)

## UBSAN vs ASAN Triage

- **ASAN** (heap/stack overflow, UAF, SEGV): Always a real bug. Fix immediately.
- **UBSAN** (signed overflow, enum range, NaN cast): May be benign in some
  contexts (e.g., MD5 relies on unsigned wrap). Check if the specific UBSAN
  finding is in the known-benign list:
  - `IccMD5.cpp:185` -- unsigned overflow in MD5 (by design)
  - `iccDumpProfile.cpp:327` -- tool-level conversion (cosmetic)

CI regression tests use `check_asan()` (ASAN-only) to avoid false failures
from benign upstream UBSAN.

## Exit Code Reference

| Code | Meaning | Is it a bug? |
|------|---------|-------------|
| 0 | Success | No |
| 1 | Validation error / graceful rejection | Not a crash |
| 134 | SIGABRT (ASAN abort) | Yes |
| 139 | SIGSEGV | Yes |

## See Also

- `docs/bisect.md` -- Full reference with regression inventory
- `prompts/reproduce-security-issue.prompt.md` -- Security-specific triage
- `prompts/file-security-issue.prompt.md` -- Upstream issue filing format
- `instructions/testing.instructions.md` -- Test infrastructure details
