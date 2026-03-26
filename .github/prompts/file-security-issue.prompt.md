# File a Security Issue on iccDEV

## When to Use

You have found a bug via sanitizer output (ASAN, UBSAN), fuzzer crash, or manual
testing and need to open an issue on `InternationalColorConsortium/iccDEV`.

## Issue Title Convention

```
{CrashType} in {Function}() at {File}:{Line}
```

**Crash type abbreviations** (used in titles and PoC filenames):

| Abbreviation | ASAN/UBSAN Signal | Example Title |
|-------------|-------------------|---------------|
| `HBO` | heap-buffer-overflow | `HBO in CIccApplyCmmSearch::costFunc() at IccCmmSearch.cpp:112` |
| `SBO` | stack-buffer-overflow | `SBO in icFixXml() at IccUtilXml.cpp:314` |
| `NPD` | null-pointer-dereference | `NPD in CIccTagLut16::Write() at IccTagLut.cpp:5361` |
| `UB` | undefined-behavior | `UB at IccUtilXml.cpp:1539` |
| `UAF` | use-after-free | `UAF in CIccTagArray::Cleanup() at IccTagComposite.cpp:1523` |
| `SEGV` | SIGSEGV | `SEGV in CIccCLUT::Interp3d() at IccTagLut.cpp:2400` |
| `OOM` | out-of-memory | `OOM in CIccTagNamedColor2::Read() at IccTagBasic.cpp:8200` |
| `SO` | stack-overflow | `SO in CIccTagStruct::Read() recursive at IccTagComposite.cpp` |

**Title rules**:
- Derive the crash type and location from `SUMMARY:` line of ASAN/UBSAN output
- Use the **iccDEV source path** (e.g., `IccTagLut.cpp`), not the full build path
- Include the line number when ASAN/UBSAN provides one
- For UB, `at {File}:{Line}` is acceptable without a function name
- Keep titles under 80 characters

## Required Body Structure

Every issue MUST contain these sections in order. The gold standard is
[#700](https://github.com/InternationalColorConsortium/iccDEV/issues/700).

### Section 1: Maintainer Repro Header

```markdown
## Maintainer Repro

{YYYY-MM-DD HH:MM:SS} UTC
```

Use the UTC timestamp of when you confirmed the reproduction. This establishes
a forensic timeline.

### Section 2: Git State

```markdown
## Git

{commit_hash} (HEAD -> {branch}, origin/{branch})
```

Pin the **exact commit** you tested against. This is mandatory — the bug may
already be fixed on a newer commit, or a regression introduced between versions.
Get it from `git --no-pager log --oneline -1`.

### Section 3: PoC Replay

Numbered steps that anyone can copy-paste to reproduce:

```markdown
## PoC Replay

Step 1. wget https://github.com/xsscx/fuzz/raw/refs/heads/master/graphics/icc/{poc_file}

Step 2. ASAN_OPTIONS=print_scariness=1:halt_on_error=0:detect_leaks=0 {tool} {poc_file} {args}
```

**Rules for PoC Replay**:
- Every PoC file MUST be downloadable via `wget` or `curl` (hosted on GitHub or CDN)
- Use `ASAN_OPTIONS=print_scariness=1:halt_on_error=0:detect_leaks=0` for crash reproduction
- Use `ASAN_OPTIONS=print_scariness=1` for single-crash analysis
- Include exact tool name and arguments — no ambiguity
- If the tool requires building from a specific branch (e.g., `cfl`), include the full
  `git clone → cmake → make → run` sequence (see Gold Standard below)
- If multiple prerequisite patches are needed, list them as numbered sub-steps with diffs

### Section 4: PoC Output

```markdown
## PoC Output

\```
{verbatim ASAN/UBSAN output}
\```
```

**Rules for PoC Output**:
- Include the **complete** sanitizer output — never summarize or truncate
- The `SUMMARY:` line must be present (it names the crash type and location)
- Include the shadow byte legend if ASAN provides it
- Include `SCARINESS:` score when present (ASAN `print_scariness=1`)
- Do NOT strip frame numbers or stack traces

### Section 5 (Gold Standard): Patch

Only include when you have a fix:

```markdown
## Patch

\```diff
diff --git a/IccProfLib/IccCmmSearch.cpp b/IccProfLib/IccCmmSearch.cpp
index 9b8a2e3..a021cb8 100644
--- a/IccProfLib/IccCmmSearch.cpp
+++ b/IccProfLib/IccCmmSearch.cpp
@@ -78,6 +78,8 @@
   m_nApply = pCmm->m_pcc.size();
   if (!m_nApply)
     m_nApply = 1;
+  if (m_nApply > pCmm->m_dst_to_mid.size())
+    m_nApply = (size_t)pCmm->m_dst_to_mid.size();
\```
```

### Section 6 (Gold Standard): Patched Output

Prove the fix works:

```markdown
## Patched Output

\```
{clean tool output after applying the patch — exit 0, no ASAN/UBSAN}
\```
```

### Section 7 (When Applicable): Won't Fix

If the behavior is spec-defined or intentional:

```markdown
## Won't Fix

[RFC 1321](https://www.ietf.org/rfc/rfc1321.txt)
```

Cite the specification that makes the behavior correct/expected.

## Complete Examples by Severity

### Critical — Write Overflow (SCARINESS ≥ 40)

**Reference**: [#624](https://github.com/InternationalColorConsortium/iccDEV/issues/624)

```markdown
# SBO in icFixXml() at IccUtilXml.cpp:314

## Maintainer Repro

2026-02-27 23:37:07 UTC

SCARINESS: 55 (7-byte-write-stack-buffer-overflow)

### Git

186bba0 (HEAD -> master, origin/master)

## PoC Replay

Step 1. wget https://github.com/xsscx/fuzz/raw/refs/heads/master/graphics/icc/sbo-icFixXml-IccUtilXml_cpp-Line314.icc

Step 2. ASAN_OPTIONS=print_scariness=1 iccToXml sbo-icFixXml-IccUtilXml_cpp-Line314.icc foo.bar

## PoC Output

{full ASAN output with SUMMARY line}
```

Labels: `Bug`, `libFuzzer`, `Triaged`

### Moderate — Read Overflow (SCARINESS 20–39)

**Reference**: [#651](https://github.com/InternationalColorConsortium/iccDEV/issues/651)

```markdown
# HBO in icCurvesFromXml() at IccTagXml.cpp:3330

## Maintainer Repro

2026-03-07 17:50:49 UTC

### Git

5f7e03a (HEAD -> master, origin/master)

## PoC Replay

Step 1. wget https://github.com/xsscx/fuzz/raw/refs/heads/master/xml/icc/hbo-icCurvesFromXml-IccTagXml_cpp-Line333.xml

Step 2. ASAN_OPTIONS=print_scariness=1:halt_on_error=0 iccFromXml hbo-icCurvesFromXml-IccTagXml_cpp-Line333.xml foo.bar

## PoC Expected Output

{full ASAN output}
```

Labels: `Bug`, `libFuzzer`, `Triaged`

### Low — UB / Benign

**Reference**: [#722](https://github.com/InternationalColorConsortium/iccDEV/issues/722)

```markdown
# UB at IccUtilXml.cpp:1539

## Maintainer Repro

2026-03-25 23:24:00 UTC

## Git

9e0b03c (HEAD -> master, origin/master)

Step 1. curl -O https://xss.cx/2026/03/25/img/BeyondRGB_CM_1774467526.tiff

Step 2. iccTiffDump BeyondRGB_CM_1774467526.tiff BeyondRGB_CM_1774467526.tiff.icc

Step 3. iccToXml BeyondRGB_CM_1774467526.tiff.icc foo.bar

## PoC Output

{UBSAN output}
```

Labels: `Triaged`

### Won't Fix — Spec-Defined Behavior

**Reference**: [#729](https://github.com/InternationalColorConsortium/iccDEV/issues/729)

```markdown
# UB in IccMD5.cpp

## Maintainer Repro

2026-03-26 02:18:39 UTC

## Git

85663e4 (HEAD -> master, origin/master)

{clone + build + repro commands}

## PoC Output

{UBSAN output}

## Won't Fix

[RFC 1321](https://www.ietf.org/rfc/rfc1321.txt)
```

Labels: `wontfix`, `OOS`

### Gold Standard — Full Patch Cycle

**Reference**: [#700](https://github.com/InternationalColorConsortium/iccDEV/issues/700)

Includes everything above PLUS:
1. Prerequisite patches listed as numbered diffs (if the fix depends on prior patches)
2. Full `git clone → build → reproduce` command block (self-contained)
3. `## Patch` with unified diff
4. `## Patched Output` proving clean exit after applying the fix

This is the target quality for every issue. At minimum, provide sections 1–4.

## SCARINESS Score Reference

ASAN assigns a `SCARINESS` score reflecting exploitability:

| Score Range | Severity | Typical Cause |
|-------------|----------|---------------|
| 40+ | **Critical** | Write overflow (stack or heap), use-after-free write |
| 20–39 | **Moderate** | Read overflow, large read OOB, container overflow |
| 10–19 | **Low** | Small read OOB, partially addressable access |
| N/A (UBSAN) | **Varies** | Unsigned overflow may be benign; signed overflow is UB |

Enable with `ASAN_OPTIONS=print_scariness=1`.

## Labels

| Label | When to Apply |
|-------|---------------|
| `Bug` | All confirmed bugs |
| `libFuzzer` | Found via LibFuzzer/AFL++/fuzzing |
| `Triaged` | Maintainer has confirmed reproduction |
| `wontfix` | Behavior is intentional or spec-required |
| `OOS` | Out of scope for this project |

## PoC File Naming Convention

Name PoC files to encode the crash signature:

```
{crash_type}-{Class}-{Method}-{File}_cpp-Line{N}.{ext}
```

Examples:
- `hbo-CTiffImg-ReadLine-TiffImg_cpp-Line370.tiff`
- `sbo-icFixXml-IccUtilXml_cpp-Line314.icc`
- `npd-CIccTagLut16-Write-IccTagLut_cpp-Line5361.icc`
- `ub-runtime-error-type-confusion-CIccMpeCalculator.icc`

For CVE PoCs: `cve-{YYYY}-{NNNNN}-{description}-variant-{NNN}.icc`

## PoC File Hosting

GitHub does not allow `.icc` file attachments. Options:
1. **Rename to `.icc.txt`** and attach to the issue, with a note to rename back
2. **Add PR** `https://github.com/xsscx/fuzz/` under the appropriate subdirectory
3. **Host on** `CDN` with a direct download URL
4. For TIFF/XML/JPEG/PNG PoCs, GitHub allows direct attachment

## Agent Checklist Before Filing

1. [ ] **Reproduced 3× deterministically** — not a flaky crash
2. [ ] **Tested against latest master** — `git pull && rebuild`
3. [ ] **Title follows convention** — `{Type} in {Function}() at {File}:{Line}`
4. [ ] **UTC timestamp present** in Maintainer Repro
5. [ ] **Git commit hash present** — exact source state pinned
6. [ ] **PoC file downloadable** — `wget`/`curl` URL verified
7. [ ] **Reproduction command is copy-paste ready** — includes ASAN_OPTIONS
8. [ ] **Sanitizer output is verbatim** — not summarized, not truncated
9. [ ] **SUMMARY line present** in output block
10. [ ] **Labels assigned** — at minimum `Bug` + `Triaged`
11. [ ] **Assignee set** if known (typically `ChrisCoxArt` for library bugs)

## Common Mistakes

| Mistake | Why It's Wrong | Fix |
|---------|---------------|-----|
| Summarizing ASAN output | Loses frame numbers, shadow bytes, allocation site | Paste verbatim |
| Missing git commit hash | Can't determine if already fixed | Always include `git log -1` |
| Using relative paths in repro | Others can't reproduce | Use `wget` URLs for PoC files |
| Filing without testing latest master | May duplicate a fixed issue | `git pull && rebuild && retest` |
| Omitting ASAN_OPTIONS in repro command | Others get different output | Include exact env vars |
| Using tool from `cfl/` (patched) instead of upstream | Masks real upstream behavior | Always use `iccDEV/Build/Tools/` |
| Filing exit code 1 as a crash | Exit 1 = graceful rejection, not a bug | Only file exit 128+ as crashes |
| Truncating shadow byte legend | Loses exploitability context | Include full ASAN output |

## Build Command for Reproduction

Standard ASAN+UBSAN build for issue reproduction:

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV/Build
cmake Cmake -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-g -fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DENABLE_TOOLS=ON -DENABLE_ICCXML=ON
make -j"$(nproc)"
cd ..

# Add tools to PATH
for d in Build/Tools/*; do [ -d "$d" ] && export PATH="$(realpath "$d"):$PATH"; done
export LD_LIBRARY_PATH="Build/IccProfLib:Build/IccXML/IccLibXML"

# Reproduce
ASAN_OPTIONS=print_scariness=1:halt_on_error=0:detect_leaks=0 \
  {tool} {poc_file} {args}
```

## See Also

- `prompts/reproduce-security-issue.prompt.md` — Triage workflow for existing advisories
- `instructions/icc-library-code.instructions.md` — Input validation patterns
- [GHSA advisories](https://github.com/InternationalColorConsortium/iccDEV/security/advisories) — Published advisories
