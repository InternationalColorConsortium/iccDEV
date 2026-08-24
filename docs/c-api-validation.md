# C Validation API

`IccProfLib/IccCValidation.h` is the stable C-only validation surface for
callers that cannot include C++ headers or link to C++ symbols. It includes
only `<stddef.h>` and exports one unmangled function:

```c
icc_validation_status icc_validate_profile(
  const unsigned char *icc_data,
  size_t icc_size,
  char *report,
  size_t report_size);
```

`report` is optional. If it is non-NULL and `report_size` is non-zero, the
function always NUL-terminates it; a short buffer receives a truncated report.
The input remains owned by the caller and must stay valid for the duration of
the call.

| Status | Meaning |
|--------|---------|
| `ICC_VALIDATION_OK` | Profile conforms to the specification. |
| `ICC_VALIDATION_WARNING` | Profile conforms with concerns. |
| `ICC_VALIDATION_NON_COMPLIANT` | Profile does not conform but may be usable. |
| `ICC_VALIDATION_CRITICAL_ERROR` | Profile cannot be read or used. |
| `ICC_VALIDATION_INVALID_ARGUMENT` | The input buffer is NULL, empty, or exceeds the ICC size limit. |
| `ICC_VALIDATION_INTERNAL_ERROR` | Validation could not finish internally. |

## Runtime loading

The API is intended for runtime loading as well as normal shared-library use.
The CTest regression compiles a C99 executable, resolves
`icc_validate_profile` through `dlopen` on Unix or `LoadLibrary` on Windows,
and validates a tracked profile plus invalid-input behavior:

```bash
cmake -S Build/Cmake -B build -DENABLE_TESTS=ON -DENABLE_SHARED_LIBS=ON -DENABLE_STATIC_LIBS=OFF
cmake --build build --target iccCValidationDlopenTest --parallel "$(nproc)"
ctest --test-dir build -R '^iccdev\.c-validation-dlopen$' --output-on-failure --no-tests=error
```

Runtime loading requires a shared build. Static-library consumers can call the
same C API directly after linking the static IccProfLib target.

## Example and Review

`examples/c-validation-dlopen/` is a standalone C99 program that demonstrates
the header, runtime symbol lookup, and output. Its README includes a complete
build command and expected output.

Review changes to this ABI with the following invariants:

- the public header stays C99-compatible and includes no IccProfLib or C++
  header;
- the export remains named `icc_validate_profile`;
- invalid pointers and sizes are rejected before parsing;
- every non-empty report buffer is NUL-terminated; and
- the C `dlopen` CTest and standalone example both build without linking to
  IccProfLib.
