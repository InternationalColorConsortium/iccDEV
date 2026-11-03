# CodeQL Dismissed-Alert Audit

This audit reviews the hosted CodeQL findings that inherited dismissal state
from the retired broad numeric rules. It is evidence for maintainer triage, not
a request to reopen every historical alert.

## Inventory and decision

The 2026-11-03 GitHub API snapshot contained:

| Retired rule ID | Dismissed | False positive | Won't fix | Decision |
|---|---:|---:|---:|---|
| `iccdev/division-by-zero-profile` | 112 | 87 | 25 | Do not bulk reopen |
| `iccdev/float-to-int-cast` | 72 | 69 | 3 | Reopen #1086-#1088, or link them to the replacement v2 findings |

All 25 DBZ `won't fix` alerts are the repeated `m_PCSOffset[n] /
m_PCSScale[n]` family in `IccCmm.cpp`. At the alert revision
`8688572104b4ce337013ce3c95feb5abee9fdf58`, every group immediately calls
`CheckForInvalidPCSScale()` and returns `icCmmStatBadXform` before division.
Those dismissals remain supported.

The three float-cast `won't fix` alerts were different. Alerts
[#1088](https://github.com/InternationalColorConsortium/iccDEV/security/code-scanning/1088),
[#1087](https://github.com/InternationalColorConsortium/iccDEV/security/code-scanning/1087),
and [#1086](https://github.com/InternationalColorConsortium/iccDEV/security/code-scanning/1086)
were dismissed because `pos` is clamped to `[0,1]`. For NaN, both `pos < 0`
and `pos > 1` are false. NaN therefore reached the conversions in
`CIccTagSparseMatrixArray::Interpolate`, `CIccTagFixedNum::Interpolate`, and
`CIccTagNum::Interpolate`. These three dismissals should be reversed or retained
only with a link to the replacement `iccdev/float-to-int-cast-v2` alerts and
this fix.

## Stratified sample

The audit reviewed every `won't fix` alert plus representative false positives
from distinct suppression families:

| Alerts sampled | Pattern | Result |
|---|---|---|
| DBZ #2021, #2027, #2030, #2033, #2042 and the other 20 `won't fix` alerts | Adjacent `CheckForInvalidPCSScale()` fail-closed guard | Dismissal supported |
| DBZ #2140-#2141 | Ternary `m_cc == 0 ? 0 : y / m_cc` | Dismissal supported |
| DBZ #2133-#2134 | Earlier finite/nonzero `srcScale` guard controls both divisions | Dismissal supported |
| DBZ #2066-#2069 | Denominator is compile-time constant `0.85f` | Dismissal supported |
| DBZ #1998 | Output-channel zero rejected while reading the owning object | Dismissal supported |
| Float #1096-#1100 | `isfinite` plus lower/upper CLUT coordinate clamps | Dismissal supported |
| Float #2317-#2318 | `!(value > 0)` deliberately rejects NaN and nonpositive values | Dismissal supported |
| Float #1139-#1141 | Template branch casts NaN only for floating-point instantiations | Dismissal supported |
| Float #2192 | `isfinite` plus `[0,1]` clamps precede the tint-percent cast | Dismissal supported |
| Float #1086-#1088 | Two relational clamps with no finiteness rejection | Dismissal contradicted by UBSan |

The restored rules use versioned IDs so these results do not silently inherit
the old rules' dismissal state:

- `iccdev/division-by-zero-profile-v2`
- `iccdev/float-to-int-cast-v2`

## Executable proof

The public-API regression `.github/ci/regression/tag-interpolate-nonfinite.cpp`
passes NaN through all three tag types. On the unpatched source, UBSan reported:

```text
IccTagBasic.cpp:5924:41: runtime error: nan is outside the range of representable values of type 'unsigned int'
IccTagBasic.cpp:6321:41: runtime error: nan is outside the range of representable values of type 'unsigned int'
IccTagBasic.cpp:6927:41: runtime error: nan is outside the range of representable values of type 'unsigned int'
```

A stock-command-line reproduction also reaches the fixed-number specialization.
From an unpatched checkout whose iccDEV tools and libraries were built with
UBSan float-cast instrumentation, run this as one shell command:

```bash
sed 's#<float32NumberType> <Data>#<s15Fixed16ArrayType> <Array>#; s#</Data> </float32NumberType>#</Array> </s15Fixed16ArrayType>#' Testing/CalcTest/calcExercizeOps.xml >/tmp/iccdev-cli-nan-s15.xml && iccFromXml /tmp/iccdev-cli-nan-s15.xml /tmp/iccdev-cli-nan-s15.icc && UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 iccApplyNamedCmm Testing/CalcTest/rgbExercise8bit.txt 0 1 /tmp/iccdev-cli-nan-s15.icc 1
```

The existing calculator fixture executes `NaN tint{tintElem}`. The text
substitution changes only the tint element from a floating array to an
integer-backed `s15Fixed16ArrayType`; `iccFromXml` creates the profile and
`iccApplyNamedCmm` reaches `CIccTagFixedNum::Interpolate`. The audited
historical build reported the float-to-unsigned conversion at
`IccTagBasic.cpp:6113`; the corresponding line is 6321 near the current
branch. An uninstrumented build can complete silently because this is
undefined behavior, so it is not suitable as negative evidence. Repeating the
same two stock-tool commands against the fixed ASan/UBSan build completed with
no sanitizer diagnostic and exit status zero.

Clang commonly links its sanitizer runtimes statically, so an `ldd` search for
`libubsan` or `clang_rt` can be empty even for an instrumented executable. The
audited build was verified from its compile/link commands and by finding
`__ubsan_handle_float_cast_overflow` in the executable with `nm -an`. When
multiple builds exist, invoke
`Build/Tools/IccFromXml/iccFromXml` and
`Build/Tools/IccApplyNamedCmm/iccApplyNamedCmm` directly instead of relying on
`PATH`.

The allocation regression
`.github/ci/regression/mpecalc-apply-allocation-failure.cpp` rejects the exact
one-entry sub-apply `calloc` through GNU `--wrap`. Before the fix, ASan/UBSan
reported a store through null in `CIccMpeCalculator::GetNewApply()` at the
first `pApply->m_SubElem[i]` assignment. The fixed method deletes the partial
apply object and returns null.

Run the fixed regressions from the repository root:

```bash
cmake -S Build/Cmake -B /tmp/iccdev-codeql-poc -G Ninja \
  -DENABLE_TESTS=ON -DICC_JSON_ORDERED=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined,float-cast-overflow' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined,float-cast-overflow' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined,float-cast-overflow' \
  -DCMAKE_SHARED_LINKER_FLAGS='-fsanitize=address,undefined,float-cast-overflow'
cmake --build /tmp/iccdev-codeql-poc --target \
  iccTagInterpolateNonfiniteTest iccMpeCalcApplyAllocationFailureTest -j 8
ctest --test-dir /tmp/iccdev-codeql-poc --output-on-failure \
  -R '^iccdev\.(tag-interpolate-nonfinite|mpecalc-apply-allocation-failure)$'
```

For a red-side reproduction, create a worktree at
`ff74b49fbcea4272d37e42402c5a1aa162c15dce`, apply only the two regression
sources and their CMake registration from the fixed revision, then run the same
commands. The interpolation test emits all three UBSan diagnostics; the
calculator test terminates at the null write.

## Same-database CodeQL A/B

The pre-fix CodeQL 2.27.4 database was kept constant while only the queries
changed:

| Query state | Raw custom findings | Interpretation |
|---|---:|---|
| Previous custom suite | 20 | 11 broad allocation uses, 4 layout-guarded loops, 3 catch-protected factories, 1 structurally bounded cast, 1 stock-query duplicate |
| Refined suite on the same pre-fix database | 4 | Three dismissed-alert NaN casts and one calculator allocation null write |

The four-row fingerprint manifest SHA-256 was
`ac0479d766a3db604ff8eb840da3ffc71aabf6f12aa839f0ab06d929e26b9904`.
After rebuilding the database from the fixed source with CodeQL 2.27.4, the
custom and JSON suites both had zero findings. The repository-filtered stock
suite was narrowed from 249 reviewed findings to 18 retained security prompts,
and the complete 18-row fingerprint manifest SHA-256 was
`96af8576decc1f26a4837ca11d599c45faab1b5acf6f2d5dc474cca4e6e1b9c0`.
Hosted CI must be run manually and compared by SARIF category, CodeQL version,
raw count, and fingerprint digest; a clean Security UI alone is not sufficient
because dismissed alerts are hidden there.
