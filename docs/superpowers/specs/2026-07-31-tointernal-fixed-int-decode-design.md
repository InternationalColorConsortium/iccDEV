# Fix `ToInternalEncoding` fixed-integer decode

**Date:** 2026-07-31
**Branch:** `fix/tointernal-fixed-int-decode`
**Status:** Approved design

## Problem

`CIccCmm::ToInternalEncoding` (the `icFloatColorEncoding` float overload in
[IccProfLib/IccCmm.cpp](../../../IccProfLib/IccCmm.cpp)) is supposed to **decode**
color data that is already in a stated encoding into the CMM's internal 0–1
(PCS-normalized) representation. It is the inverse of `FromInternalEncoding`.

For every fixed-integer encoding it instead performs a **re-encode round-trip**
on its input:

| Branch | Current code |
|---|---|
| default/CLR 8-bit | `icU8toF(icFtoU8(x))` |
| default/CLR 16-bit / 16BitV2 | `icU16toF(icFtoU16(x))` |
| Lab 8-bit | `icU8toF(icFtoU8(L))*100`, `icU8toAB(icFtoU8(a))`, `icU8toAB(icFtoU8(b))` |
| Lab 16-bit / 16BitV2 | `icU16toF(icFtoU16(L/a/b))` |
| XYZ 16-bit / 16BitV2 | `icUSFtoD((icU1Fixed15Number)icFtoU16(x))` |

`icFtoU8` / `icFtoU16` ([IccProfLib/IccUtil.cpp](../../../IccProfLib/IccUtil.cpp))
clamp their argument to **[0.0, 1.0]** before scaling by 255 / 65535. So any
already-encoded sample ≥ 1 saturates to the maximum. `icFtoU8(128.0)` → clamp to
`1.0` → `255`; `icU8toF(255)` → `1.0`.

### Consequences

- `From∘To` is **not** identity: internal `0.5` → `FromInternalEncoding(8Bit)` →
  `128` → `ToInternalEncoding(8Bit)` → `1.0`.
- Any input in true 0–255 / 0–65535 range collapses to full: every non-zero
  8-bit channel becomes `1.0`.
- **RGB and CMYK escaped the bug** only because the *integer* overloads
  (`icUInt8Number*` / `icUInt16Number*`) special-case them with an explicit
  `/255` / `/65535` ([IccCmm.cpp:9865](../../../IccProfLib/IccCmm.cpp),
  [IccCmm.cpp:9918](../../../IccProfLib/IccCmm.cpp)). Their proven-correct
  `/N` behavior is also evidence of the intended contract.
- **7CLR and every other non-RGB/CMYK space** hit the bug through both entry
  points: the JSON float path in the apply tools, and the integer overloads'
  `default` fallback, which converts bytes to float and calls the buggy float
  overload with `icEncode8Bit` / `icEncode16Bit`.

### Reproduction (this session)

7CLR input through the Epson profile, `encoding: "8Bit"`, distinct 0–255 rows:

```
(128,64,32,0,200,0,0) -> Lab (8.7220, 6.0300, 1.0439)
(255,10, 5,0,240,0,0) -> Lab (8.7220, 6.0300, 1.0439)   <-- identical: both saturate to (1,1,0,0,1,0,0)
```

## Correctness definition (backbone of the fix)

The fix is defined by an invariant, not by matching conventions case by case:

- **`From∘To == identity`** on valid encoded samples, and
- **`To∘From == identity`** on internal values,

for every (space, encoding) pair the functions support, within the encoding's
precision. Each corrected case is whatever makes its existing
`FromInternalEncoding` counterpart a true inverse — nothing more.

## Change

In [IccProfLib/IccCmm.cpp](../../../IccProfLib/IccCmm.cpp) add two file-local
helpers that decode an already-encoded integer sample carried in a float —
clamp to the encoding range and round, **without** the erroneous 0–1 pre-clamp:

```cpp
static icUInt8Number  icToU8Sample(icFloatNumber v);   // NaN/<0 -> 0, >255 -> 255, else round
static icUInt16Number icToU16Sample(icFloatNumber v);  // NaN/<0 -> 0, >65535 -> 65535, else round
```

Then in the `ToInternalEncoding` float overload, replace the inner `icFtoU8(x)`
/ `icFtoU16(x)` with `icToU8Sample(x)` / `icToU16Sample(x)` in every fixed-int
case, keeping all surrounding decode logic unchanged (`icU8toF`, `icU8toAB`,
`*100`, `icUSFtoD`, `Lab2ToLab4`, `icLabToPcs`, etc.).

Branches touched: **Lab** (8Bit), **XYZ** (16Bit/16BitV2), **default/CLR**
(8Bit, 16Bit, 16BitV2).

The XYZ `u1Fixed15` path (and whether an `icXyzToPcs` step is missing) is
dictated by the round-trip test — not guessed.

### Deliberately unchanged

- The RGB/CMYK special-cases in the integer overloads stay. They become
  redundant once the `default` fallback is correct, but are harmless; leaving
  them minimizes the diff. Called out here so a reviewer knows it is intentional.
- `FromInternalEncoding` is the reference of truth and is not modified.

## Tests (TDD — written first, must fail before the fix)

Following the existing regression pattern under
[.github/ci/regression/](../../../.github/ci/regression/) with CMake wiring in
[Build/Cmake/Testing/CMakeLists.txt](../../../Build/Cmake/Testing/CMakeLists.txt):

1. **Round-trip invariant test.** For a representative set of spaces — including
   **7CLR** — plus RGB, CMYK, Lab, XYZ, and encodings 8Bit / 16Bit / 16BitV2:
   assert `From(To(x)) ≈ x` and `To(From(v)) ≈ v` within encoding precision.
2. **7CLR saturation regression.** The concrete session case: distinct 0–255
   inputs must map to distinct internal values (they currently collapse).

## Collateral

- **Fix the shipped example**
  [docs/Testing/json-configs/applynamedcmm-8bit-encoding.json](../../Testing/json-configs/applynamedcmm-8bit-encoding.json).
  Its 0–255 values become correct after the fix; update it (and any captured
  baseline) so it demonstrates real 8-bit decoding rather than saturated output.
- **Audit existing baselines.** Any json-config regression that captured the old
  (saturated) fixed-int output has its expected output regenerated, with the
  behavior change called out in the commit message.

## Out of scope

- `FromInternalEncoding`.
- The search cost-function investigation from earlier this session.
- The unrelated uncommitted `IccProfLib/IccProfile.cpp` edit.
