# Code Review Bug Hunting

Systematic code review workflow for finding security bugs in iccDEV.
Based on analysis of 80+ upstream issues and 19 findings from manual
code review (April 2026).

## 5-Category Hunt

### Category 1: Division by Zero (CWE-369)

Search `Apply()` methods for unguarded divisors from profile data.

```bash
grep -rn 'Apply.*/' IccProfLib/ | grep -v '//' | grep -v 'test'
```

Pattern: parametric curve types 1-4 have denominators derived from
`m_Params[]` which are file-controlled. A profile with `g=0` or `a=0`
causes IEEE 754 NaN/Inf propagation.

**Key locations**:
- `CIccTagParametricCurve::Apply()` -- IccTagLut.cpp
- `CIccMpeCalculator` Divide/Modulo ops -- IccMpeCalc.cpp
- `CIccFormulaCurveSegment::Apply()` -- IccMpeBasic.cpp

**UBSAN flag gap**: `-fsanitize=undefined` does NOT catch float
division by zero (IEEE 754 defines float/0 as NaN). Must add
`-fsanitize=float-divide-by-zero` explicitly. This is why PoCs
produce silent NaN corruption (DeltaE=0) instead of UBSAN output.

Build with float sanitizer:
```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles/
CC=clang CXX=clang++ cmake Cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_TOOLS=ON \
  -DENABLE_ASAN=ON \
  -DENABLE_UBSAN=ON \
  -DENABLE_INTEGER_SANITIZER=ON \
  -DENABLE_FLOAT_SANITIZER=ON
make -j"$(nproc)"
```

### Category 2: Serialization Mismatch (CWE-345)

Audit Read/Write pairs for field order or type mismatches.

```bash
grep -rn 'Write32\|Write16\|Write8\|WriteFloat32' IccProfLib/ | \
  grep -v test | sort -t: -k1,1 -k2,2n
```

Pattern: `Read()` stores fields in member variables, `Write()` serializes
them back. If `Read()` uses `Read32()` but `Write()` uses `WriteFloat32()`,
the round-trip corrupts data.

Check: for every Read() in a tag class, verify the matching Write() uses
the same type and order.

### Category 3: Format String (CWE-134)

Search `printf`/`sprintf`/`Describe` for user-controlled format args.

```bash
grep -rn 'printf.*%' IccProfLib/ Tools/ IccXML/ | grep -v test
grep -rn 'Describe.*sDescription' IccProfLib/ | head -20
```

Pattern: `sDescription += szBuf` where `szBuf` was filled by `sprintf`
with mismatched format specifiers. Common: `%d` for `icUInt32Number`
(should be `%u`), `%s` for pointer (should be `%p`).

CodeQL query: `cpp/wrong-type-format-argument`

### Category 4: Dead Code Audit (CWE-561)

Before labeling any finding as CRITICAL, verify reachability.

```bash
# Example: is operator= ever called?
grep -rn 'operator=' IccProfLib/IccTagComposite.cpp
grep -rn '= \*\|\.operator=' IccProfLib/ Tools/ IccXML/ | grep -i tagarray
```

Pattern: iccDEV uses `NewCopy()` (calls copy constructor) for cloning.
The `operator=` may have bugs but is never called from any code path.
These findings should be LOW/informational, not CRITICAL.

### Category 5: Unchecked Return Values (CWE-252)

Search for `Read()`, `Begin()`, `fromJson()` calls where the return
value is discarded.

```bash
grep -rn 'Read(' IccProfLib/ | grep -v 'if.*Read\|!.*Read\|==.*Read'
grep -rn '\.Begin(' IccProfLib/ | grep -v 'if.*Begin\|!.*Begin'
grep -rn 'fromJson(' Tools/ IccProfLib/ | grep -v 'if.*fromJson'
```

Pattern: `Begin()` can return `false` for invalid state, but callers
proceed to `Apply()` on uninitialized data.

## PoC Synthesis Requirements

For iccRoundTrip-based PoCs (most common for Apply() bugs):

Minimal v2 RGB/XYZ-mntr ICC profile needs 9 tags:
`desc`, `cprt`, `rTRC`, `gTRC`, `bTRC`, `rXYZ`, `gXYZ`, `bXYZ`, `wtpt`

Without matrix tags (rXYZ/gXYZ/bXYZ), iccRoundTrip rejects with
"Unable to perform round trip". Use profile version 2.0.4, not 4.0.3.
Minimum size approximately 576 bytes.

## PoC Naming Convention

```
{crash_type}-{Class}-{Method}-{File}_cpp-Line{N}.icc
```

Types: `dbz` (div-by-zero), `hbo` (heap-buffer-overflow),
`sbo` (stack-buffer-overflow), `npd` (null-pointer-deref),
`ub` (undefined-behavior), `oom` (out-of-memory)

Prefer a minimal standalone PoC file that can be attached to the issue or added
to an upstream regression directory. Avoid inline generators in issues unless
the generator itself is the clearest reproduction.

## Quality Checklist

Before filing an issue:

1. [ ] Reproduced on latest master with fresh clone
2. [ ] Built with full sanitizer flags (including `-fsanitize=integer`)
3. [ ] For float bugs, added `-fsanitize=float-divide-by-zero`
4. [ ] Verified reachability from CLI tool entry point
5. [ ] PoC file in fuzz corpus with descriptive name
6. [ ] 1-liner repro command that produces sanitizer output
7. [ ] Tested the exact `wget` URL downloads correctly
8. [ ] One bug per issue

## Anti-Patterns

| # | Pattern | Consequence |
|---|---------|-------------|
| 1 | File bug without running repro | Embarrassing non-repro on reviewer VM |
| 2 | Use synthesized PoC in issue | Breaks when synthesis script changes |
| 3 | Label dead code as CRITICAL | Wastes maintainer time on unreachable paths |
| 4 | Miss `-fsanitize=integer` | UIO bugs invisible |
| 5 | Miss `-fsanitize=float-divide-by-zero` | Div-by-zero bugs produce silent NaN |
| 6 | Bundle multiple bugs in one issue | Blocks per-bug tracking and labeling |
| 7 | Include shadow byte legend | 17 lines of noise (cut unless UAF) |
| 8 | Use downstream-patched tools | Masks upstream behavior |

## CWE Quick Reference

| CWE | Name | iccDEV Hotspots |
|-----|------|----------------|
| CWE-369 | Divide by Zero | Apply() methods with file-controlled denominators |
| CWE-134 | Format String | Describe(), sprintf in tag serialization |
| CWE-345 | Insufficient Verification | Read/Write field order mismatches |
| CWE-252 | Unchecked Return | Begin(), Read(), fromJson() |
| CWE-561 | Dead Code | operator=, unused validation branches |
| CWE-190 | Integer Overflow | offset+size bounds checks |
| CWE-476 | NULL Dereference | Missing null checks after dynamic_cast |
| CWE-416 | Use After Free | AddXform ownership transfer |
| CWE-681 | Incorrect Conversion | Float-to-int casts, enum range |
| CWE-400 | Resource Consumption | Unbounded allocation from file-controlled size |

## See Also

- `prompts/file-security-issue.prompt.md` -- Issue filing format
- `prompts/bisect-regression.prompt.md` -- Bisect workflow
- `prompts/reproduce-security-issue.prompt.md` -- Triage existing advisories
- `instructions/icc-library-code.instructions.md` -- Input validation patterns
- `instructions/build-system.instructions.md` -- Sanitizer build options
