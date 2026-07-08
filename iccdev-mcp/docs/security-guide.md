# Security Guide

Vulnerability taxonomy and security analysis patterns for ICC color
profiles processed through iccdev-mcp.

> **Source**: 113 security advisories against iccDEV (RefIccMAX),
> covering 84 CVEs and 29 pending GHSAs across 46 distinct CWE types.
> CVSS range 3.3-9.8, mean 7.0.

---

## Threat Model

ICC profiles are binary files parsed by color management systems in
web browsers, image editors, print workflows, and operating systems.
A malicious ICC profile can be delivered via:

| Vector | Attack Surface | Exposure |
|--------|---------------|----------|
| Web (img/css/pdf) | Browser ICC parser | AV:N |
| Email attachment | Image viewer ICC parser | AV:N |
| Print job | Print spooler ICC parser | AV:N |
| File system | OS color management (ColorSync, ICM) | AV:L |
| TIFF/JPEG/PNG | Embedded ICC in image container | AV:N |

---

## Vulnerability Classes

### Memory Corruption (72% of advisories)

The dominant vulnerability class. File-controlled values flow into
allocation sizes, loop bounds, array indices, and pointer arithmetic
without validation.

| CWE | Name | Count | Severity | Example |
|-----|------|-------|----------|---------|
| CWE-122 | Heap Buffer Overflow | 19 | HIGH | Tag data exceeding allocated buffer |
| CWE-125 | Out-of-Bounds Read | 14 | HIGH | Reading past tag data boundaries |
| CWE-787 | Out-of-Bounds Write | 8 | CRITICAL | Writing past CLUT allocation |
| CWE-121 | Stack Buffer Overflow | 5 | HIGH | Fixed-size buffers in Describe() |
| CWE-119 | Buffer Overflow (generic) | 4 | HIGH | Improper memory boundary checks |
| CWE-416 | Use After Free | 3 | CRITICAL | AddXform ownership transfer |

**Detection with iccdev-mcp**: The `dump_profile` tool with ASAN-instrumented
iccDEV binaries will crash with diagnostic output on memory corruption.

### Input Validation (54 advisories)

CWE-20 is tagged on nearly half of all advisories. File-controlled values
are trusted without bounds checking.

| Pattern | CWE | Description |
|---------|-----|-------------|
| Unchecked allocation size | CWE-190 | `size * count` overflows uint32 |
| Missing bounds check | CWE-20 | Array index from file data |
| Enum value from file | CWE-681 | Cast to enum without range check |
| Type signature trust | CWE-843 | Tag type sig used as vtable selector |

**Detection with iccdev-mcp**: The `inspect_header` tool validates
header field ranges. Use `profile_to_xml` to inspect all tag data.

### Type Confusion (7 advisories)

ICC tag type signatures read from the file are trusted as valid enum
values. An attacker can craft a profile with a valid tag signature but
an unexpected type signature, causing incorrect vtable dispatch.

| Component | Trigger | Impact |
|-----------|---------|--------|
| `CIccTag::Create()` | Malformed type signature | Wrong tag class instantiated |
| `CIccMultiProcessElement` | Invalid element signature | Wrong MPE handler |
| Tag array elements | Mixed type signatures | Type-confused access |

### Resource Exhaustion (DoS)

| CWE | Name | Pattern |
|-----|------|---------|
| CWE-400 | Uncontrolled Resource Consumption | File-controlled allocation size |
| CWE-674 | Uncontrolled Recursion | Self-referencing calculator elements |
| CWE-835 | Infinite Loop | Loop bound from file data |
| CWE-789 | Memory Allocation with Excessive Size | `nChannels * gridPoints^nInputs` |

**Detection with iccdev-mcp**: The CLI tool wrappers enforce timeouts
(default 60s). Resource exhaustion manifests as timeout rather than crash.

### Numeric Errors

| CWE | Name | Pattern |
|-----|------|---------|
| CWE-681 | Incorrect Conversion | float-to-int with NaN/Inf |
| CWE-369 | Divide by Zero | Zero denominators from file |
| CWE-190 | Integer Overflow | `offset + size` wraps uint32 |
| CWE-191 | Integer Underflow | `exponent - bias` unsigned underflow |

---

## Component Attack Surface

### By iccDEV Component

| Component | Advisories | Primary CWEs | iccdev-mcp Tools |
|-----------|-----------|-------------|-------------------|
| IccProfLib | 45 | CWE-122, 125, 787 | `dump_profile`, `inspect_header` |
| IccCmm | 18 | CWE-416, 476, 674 | `apply_profiles`, `apply_named_cmm` |
| IccMpeCalc | 15 | CWE-674, 681, 190 | `apply_profiles` (MPE path) |
| IccTagLut | 12 | CWE-122, 787, 400 | `round_trip_test` |
| IccLibXML | 8 | CWE-20, 125, 787 | `profile_to_xml`, `xml_to_profile` |
| CLI Tools | 5 | CWE-190, 121 | All CLI-backed tools |

### By Profile Feature

| Feature | Risk Level | Attack Patterns |
|---------|-----------|-----------------|
| Calculator (`calc`) | CRITICAL | Recursion, stack overflow, NaN casts |
| CLUT tables | HIGH | OOM via dimension overflow, OOB access |
| Named colors | HIGH | Heap overflow in color name parsing |
| Spectral data | HIGH | Channel count/float16 confusion |
| Tag aliasing | MEDIUM | Shared offset causing double-free |
| String tags | MEDIUM | Unbounded string length |
| Matrix/TRC | LOW | Limited attack surface |

---

## Exploit Primitives

Known exploit primitives from the 113 advisories:

### Heap Shaping

CLUT allocation sizes are file-controlled. An attacker can:
1. Allocate a precisely-sized heap chunk via grid dimensions
2. Overflow into adjacent allocations via malformed CLUT data
3. Achieve controlled write via LUT interpolation index overflow

### Use-After-Free

The CMM `AddXform` function transfers profile ownership. Crafted
profiles with `cenc` (color encoding) class trigger premature
destruction of the profile while transforms still reference it.

### Stack Overflow

`Describe()` methods use fixed-size stack buffers. Profiles with
deeply nested structures or large parameter counts overflow these
buffers during text formatting.

### Integer Overflow

`offset + size` bounds checks wrap on uint32 when both values are
large. The resulting small sum passes the `<= profile_size` check,
enabling OOB access at the original offset.

---

## Using iccdev-mcp for Security Analysis

### Quick Triage

```python
# 1. Inspect header for anomalies
result = inspect_header(path="suspicious.icc")
# Check: version, device_class, color_space, profile_size

# 2. Dump full profile with validation
result = dump_profile(path="suspicious.icc")
# ASAN-instrumented binary will report memory issues

# 3. Convert to XML for manual inspection
result = profile_to_xml(input_path="suspicious.icc")
# Examine tag structure, element counts, data values

# 4. Test round-trip fidelity
result = round_trip_test(path="suspicious.icc")
# Exercises both AToB and BToA transform paths
```

### Image ICC Extraction

```python
# Extract ICC from container formats
tiff_result = tiff_dump(path="image.tiff")
jpeg_result = jpeg_dump(path="photo.jpg")
png_result  = png_dump(path="image.png")

# Then analyze the extracted profile
header = inspect_header(path="extracted.icc")
```

### Known-Bad Profile Testing

Test against known CVE proof-of-concept profiles to verify detection:

| CVE | Profile Type | Primary Bug |
|-----|-------------|-------------|
| CVE-2022-26730 | ColorSync | Heap buffer overflow |
| CVE-2023-46602 | iccDEV | Tag validation bypass |
| CVE-2024-38427 | iccDEV | Integer overflow |

---

## CodeQL Security Queries

The `.github/codeql-queries/iccdev-mcp/` directory contains 6 custom CodeQL
queries targeting iccdev-mcp-specific security patterns:

| Query | CWE | What It Detects |
|-------|-----|-----------------|
| subprocess-command-injection | CWE-078 | MCP params flowing to subprocess |
| profile-path-traversal | CWE-022 | Profile paths bypassing validation |
| api-path-traversal | CWE-022 | REST API path traversal |
| unsafe-env-server-bind | CWE-015 | Env vars controlling server bind |
| unvalidated-file-upload | CWE-434 | Upload without size/type checks |
| missing-output-sanitization | CWE-116 | Direct subprocess bypassing wrapper |

Run:
```bash
gh codeql pack install .github/codeql-queries/iccdev-mcp
gh codeql database create /tmp/codeql-db --language=python \
  --source-root=iccdev-mcp --overwrite
gh codeql database analyze /tmp/codeql-db --format=sarif-latest \
  --output=/tmp/results.sarif --threads=0 \
  .github/codeql-queries/iccdev-mcp/iccdev-mcp-security-suite.qls
```

---

## ASAN/UBSAN Build for Security Testing

For maximum vulnerability detection, build iccDEV with full
sanitizer instrumentation:

```bash
cd iccDEV/Build
cmake -S Cmake -B . \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON \
  -DCMAKE_CXX_FLAGS="-g3 -O0 -fsanitize=address,undefined,integer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined,integer"
make -j$(nproc)
```

Then set `ICCDEV_TOOLS_DIR` to point to the instrumented binaries:

```bash
export ICCDEV_TOOLS_DIR=$(pwd)/Tools
export LD_LIBRARY_PATH=$(pwd)/IccProfLib:$(pwd)/IccXML
```

### Sanitizer Environment

```bash
# Recommended ASAN settings
export ASAN_OPTIONS=detect_leaks=0,halt_on_error=0

# Suppress profraw when not collecting coverage
export LLVM_PROFILE_FILE=/dev/null
```

---

## References

- [ICC.1-2022-05](https://www.color.org/specification/ICC.1-2022-05.pdf) -- ICC v4.4 specification
- [ICC.2-2023](https://www.color.org/specification/ICC.2-2023.pdf) -- ICC v5/iccMAX specification
- [iccDEV Advisories](https://github.com/InternationalColorConsortium/iccDEV/security/advisories) -- Full advisory list
- [iccDEV Repository](https://github.com/InternationalColorConsortium/iccDEV) -- Source code
