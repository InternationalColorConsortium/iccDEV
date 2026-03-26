---
applyTo: "IccProfLib/**,IccXML/**,Tools/**"
---

# ICC Library Code — Auto-Loaded Instructions

## Input Validation Rules

ICC profiles are **untrusted binary input**. Every value read from a profile
controls memory allocation, loop iteration, or buffer access.

### Mandatory Checks

1. **Size before allocation**: Validate `count * elementSize` doesn't overflow
   before `new[]` or `malloc()`. Use `SIZE_MAX / elementSize` guard.

2. **Offset + size within file**: `offset + size <= profileSize` with overflow
   check: `offset > profileSize || size > profileSize - offset`.

3. **Loop bounds from file**: Any loop count from file data must be capped.
   Named colors: ≤ 65535. Channels: ≤ 16. Tags: ≤ 1000.

4. **Enum range**: Cast from `icUInt32Number` to enum MUST validate range
   before use. Out-of-range → UBSan finding.

5. **Float-to-int**: Check `isfinite()` AND range (INT_MIN..INT_MAX) before
   `(int)` cast. NaN bypasses `if(v < 0)` clamp patterns (IEEE 754).

6. **String termination**: Fixed-size name fields (e.g., 32-byte
   `icColorantTableEntry.name`) may lack null terminator. Always enforce
   `name[sizeof(name)-1] = '\0'` after Read().

## Error Handling

- Return `false` or error code on failure — **never throw exceptions**
- `Read()` must validate ALL fields before storing — partial reads leave
  dangling state that `Describe()` will crash on
- `Begin()` can return `false` — callers MUST check before `Apply()`

## Sanitizer Compatibility

Code must be clean under:
- AddressSanitizer (`-fsanitize=address`)
- UndefinedBehaviorSanitizer (`-fsanitize=undefined`)
- MemorySanitizer (`-fsanitize=memory`) — Clang only
- ThreadSanitizer (`-fsanitize=thread`) — conflicts with ASan

### Common UBSAN Triggers
- `(int)floatValue` where float is NaN, Inf, or out-of-range
- `enumValue` assigned from raw uint32 without range check
- `signed integer overflow` in size calculations
- `shift exponent` too large in bit operations
