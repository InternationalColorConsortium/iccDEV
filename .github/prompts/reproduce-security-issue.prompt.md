# Reproduce a Security Advisory

Use this prompt when triaging an iccDEV security advisory (GHSA/CVE) or
investigating a crash reported against an ICC profile or image file.

## Step 1: Identify the Advisory

```bash
# List all advisories
gh api --paginate "repos/InternationalColorConsortium/iccDEV/security-advisories" \
  --jq '.[] | "\(.ghsa_id) \(.cve_id // "no-CVE") \(.summary)"'

# Get details for a specific advisory
gh api "repos/InternationalColorConsortium/iccDEV/security-advisories/GHSA-xxxx-xxxx-xxxx"
```

## Step 2: Build with Sanitizers

```bash
cd Build
cmake Cmake \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON \
  -DENABLE_COVERAGE=ON
make -j"$(nproc)"
```

## Step 3: Reproduce

```bash
# Set sanitizer options for catch-and-continue (full crash chain)
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1
export LD_LIBRARY_PATH=IccProfLib:IccXML

# Test with DumpProfile (most common crash surface)
Tools/IccDumpProfile/iccDumpProfile poc-file.icc ALL

# Test with ToXml (XML serialization crashes)
Tools/IccToXml/iccToXml poc-file.icc /dev/null

# Test with RoundTrip (CMM Apply crashes)
Tools/IccRoundTrip/iccRoundTrip poc-file.icc

# For TIFF-embedded ICC crashes
Tools/IccTiffDump/iccTiffDump poc-file.tif

# For multi-profile workflows
Tools/IccApplyNamedCmm/iccApplyNamedCmm -cfg config.json
```

## Step 4: Classify the Finding

Read the ASAN/UBSAN stack trace — frames #2-#3 identify the bug location:

| ASAN Error Type | CWE | Severity |
|-----------------|-----|----------|
| heap-buffer-overflow | CWE-122 | CRITICAL |
| stack-buffer-overflow | CWE-121 | CRITICAL |
| use-after-free | CWE-416 | CRITICAL |
| alloc-dealloc-mismatch | CWE-762 | HIGH |
| null-pointer-deref (SEGV) | CWE-476 | HIGH |
| runtime error: signed integer overflow | CWE-190 | HIGH |
| runtime error: outside range of representable values | CWE-681 | HIGH |
| runtime error: member access within null pointer | CWE-476 | HIGH |

## Step 5: Determine Root Cause

Key questions:
1. Which `Read()` function loaded the malformed data without validation?
2. Which field from the file controlled the crash (size, offset, count, enum)?
3. Is the fix in `Read()` (input validation) or `Describe()`/`Apply()` (defensive check)?

Common patterns:
- **Uncapped allocation**: `new T[fileControlledCount]` without size check
- **Missing null termination**: Fixed-size string fields read without `[N-1]='\0'`
- **NaN bypass**: `if(v<0)...if(v>1)` doesn't catch NaN (IEEE 754)
- **Enum out-of-range**: `(icSomeEnum)rawUint32` without range validation
- **Offset overflow**: `offset + size` wraps past `profileSize`

## Step 6: Create Minimal PoC

```bash
# If the crash file is large, minimize it
# For LibFuzzer crash files:
cfl/bin/icc_dump_fuzzer -minimize_crash=1 -exact_artifact_path=minimized.icc crash-file

# For manual minimization:
# Copy the file, zero out non-essential tags, verify crash still reproduces
```

## Step 7: File Upstream

Create an issue with:
1. Clear title: `[CWE-NNN] Type: Location in File.cpp:LineN`
2. ASAN/UBSAN output (verbatim)
3. Minimal PoC file attached (rename `.icc` → `.icc.txt` for GitHub)
4. One-liner reproduction command
5. Suggested fix (if known)

```bash
# Example upstream report format:
# Title: [CWE-122] Heap-buffer-overflow in CIccTagColorantTable::Describe()
# Body: Steps + ASAN output + PoC file + fix suggestion
gh issue create --repo InternationalColorConsortium/iccDEV \
  --title "[CWE-122] HBO in CIccTagColorantTable::Describe()" \
  --body-file /tmp/issue-body.md
```

## Exit Code Reference

| Exit Code | Meaning | Is it a crash? |
|-----------|---------|----------------|
| 0 | Success | No |
| 1-127 | Graceful rejection | **No** |
| 134 | SIGABRT (ASan abort) | Yes |
| 136 | SIGFPE | Yes |
| 137 | SIGKILL (OOM) | Yes (resource exhaustion) |
| 139 | SIGSEGV | Yes |
