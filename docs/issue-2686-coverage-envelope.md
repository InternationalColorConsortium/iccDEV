# Issue 2686 Coverage Envelope

The detector envelope was measured on 2026-09-23 at the unfixed base revision
`805b316c1bf250232fbfea19f17a35aa7f0de43c`. The fix verification below was
measured from branch `ci-qa-issue-2686` after applying the issue #2686 patch.
The finding originated in ClusterFuzzLite run 35942315860 at
`af9dd29d959fec17b7d1b17e78a8f279ba671c78`.

Artifact:

- Name: `crash-6e1c8815748271d3bc28f0b9fd22d4ba01606999`
- Size: 1,876 bytes
- SHA-256: `bc25d19cbe27872e6640482105c5c2d3a0054fa33cf840f3cd7e5acc7587a4f2`
- Download: `gh run download 35942315860 --repo InternationalColorConsortium/iccDEV --name crashes-icc_profilevisualize_fuzzer --dir out/issue-2686`

## Root cause

Commit `7f25ed0a` moved the one-entry `curveType` gamma conversion from
`CIccTagCurve::Apply()` into `CIccTagCurve::Begin()` and stored it in
`m_fGamma`. Constructors, copy, assignment, `Read()`, and `SetGamma()` did not
initialize or refresh that member. Profile visualization reads nested LUT
curves and applies them without a `Begin()` call, passing the indeterminate
gamma to `pow()`.

Initializing the cache is insufficient. `operator[]` and `GetData()` expose
mutable curve samples, so a gamma cached by `Begin()` can become stale. The fix
removes `m_fGamma` and derives the exponent from the current sample in
`Apply()`, restoring the pre-`7f25ed0a` public behavior.

## Minimal serialized reproduction

The checked-in XML fixture converts to a 584-byte ICC profile and reaches the
same `CIccTagCurve::Apply()` path through shipped tools. Its SHA-256 is
`89abd0241b53d5b84606c2a451b13f67e6b63942e36ae3fd066724874c8cb642`; the
generated ICC SHA-256 is
`47e5356bc7de265a61ed912431926c74dc1e44587f00806adc8a5cf9a04b8c1e`.

```bash
cd Build
./Tools/IccFromXml/iccFromXml ../.github/ci/regression/issue-2686-curve-gamma.xml issue-2686.icc -noid
MSAN_OPTIONS='halt_on_error=1:exit_code=86:origin_history_size=7' ./Tools/IccProfilePlot/iccProfilePlot issue-2686.icc graph curve:B2A1:B:2
```

The second command exits 86 under MSan at the unfixed base. It exits 0 on the
patched MSan and TSan builds and describes the curve as `Y = X ^ 2.000031`.
The paired CTest registrations preserve the XML producer-to-ICC consumer
dependency; the commands above remain the minimal copy-and-paste reproduction.

## Detector envelope before the fix

| Lane or consumer | Exit | Finding |
| --- | ---: | --- |
| ClusterFuzzLite MSan `icc_profilevisualize_fuzzer` | 77 | Yes: uninitialized value reaches `pow()` from `CIccTagCurve::Apply()` |
| Valgrind Memcheck, same one-input harness | 99 | Yes: same `pow()`/`Apply()` stack and heap origin |
| Valgrind Memcheck, `iccProfilePlot graph curve:B2A1:B:2` | 99 | Yes |
| Valgrind Memcheck, other five enumerated graph IDs | 0 | No: those curves do not carry the one-entry trigger |
| Valgrind Memcheck, legacy `iccProfileVisualize` | 99 | Yes |
| Valgrind Memcheck, `iccProfileVisualizePlot` | 99 | Yes |
| ASan+UBSan, same one-input harness | 0 | No: neither detector tracks initializedness |
| TSan, same one-input harness | 0 | No: no data race |
| Helgrind, harness and triggering ProfilePlot graph | 0 | No: no lock or race defect |
| DRD, harness and triggering ProfilePlot graph | 0 | No: no thread-synchronization defect |
| Memcheck, `iccDumpProfile ... ALL` on the issue #2686 artifact | 0 | No: parses and dumps but does not apply the curve |
| Memcheck, `iccProfilePlot ... list` | 0 | No: enumerates descriptors but does not render them |
| Memcheck, `iccToXml` | 1 | No Valgrind error; tool rejects this malformed profile |
| Memcheck, `iccRoundTrip` | 255 | No Valgrind error; tool rejects this profile |

The original CI matrix independently passed the address and undefined lanes
and failed only the memory lane. Local one-input ASan+UBSan and TSan runs agree
with that result.

A native MSan harness linked to the host's uninstrumented libstdc++ reports an
earlier unrelated initializedness warning in `CIccProfile::GetTag()`. The
ClusterFuzzLite MSan result is authoritative for the full harness because its
instrumented runtime reaches the issue stack. The focused API binary and the
fully instrumented XML-to-ProfilePlot path avoid that host-runtime boundary and
pass under local MSan after the fix.

## Fix verification

| Verification | Result |
| --- | --- |
| New regression source linked to the unfixed Debug library | Expected fail, exit 1 with six failed checks |
| `iccdev.curve-apply-gamma` normal Debug CTest | Pass |
| Same CTest under Clang 22 MSan with origin tracking | Pass, exit 0 |
| XML -> ICC -> `iccProfilePlot` under Clang 22 MSan with instrumented dependencies | Pass, exit 0 |
| XML -> ICC -> `iccProfilePlot` under Clang 22 TSan | Pass, exit 0 |
| Neighboring `iccdev.curve-setsize-contract` CTest | Pass |
| Neighboring `iccdev.curve-gamma-u8fixed8` CTest | Pass |
| ASan+UBSan one-input `icc_profilevisualize_fuzzer` replay | Clean, exit 0 |
| Memcheck one-input `iccProfilePlot` triggering graph replay | Clean, exit 0 |
| Memcheck legacy `iccProfileVisualize` replay | Clean, exit 0 |
| Memcheck `iccProfileVisualizePlot` replay | Clean, exit 0 |
| Memcheck focused CTest binary | Clean, exit 0 |

The new CTest is deterministic without a sanitizer: after `Begin()`, it mutates
the gamma through both `operator[]` and `GetData()` and requires `Apply()` to
observe the current value. It also covers parsed, copied, assigned,
`SetGamma()`, and constructed one-entry curves.
