# C Validation Runtime-Loading Example

This C99 program includes only `IccCValidation.h`, loads IccProfLib at runtime,
and resolves the unmangled `icc_validate_profile` symbol.

## Build and Run

From an iccDEV checkout with a shared build:

```bash
cmake -S Build/Cmake -B build -DENABLE_SHARED_LIBS=ON -DENABLE_STATIC_LIBS=OFF
cmake --build build --target IccProfLib2 --parallel "$(nproc)"
cmake -S examples/c-validation-dlopen -B examples/c-validation-dlopen/build -DICCDEV_ROOT="$PWD"
cmake --build examples/c-validation-dlopen/build --parallel "$(nproc)"
examples/c-validation-dlopen/build/icc-validate-dlopen build/IccProfLib/libIccProfLib2d.so Testing/sRGB_v4_ICC_preference.icc
```

The library suffix and build directory can vary by generator and configuration.
On Windows, pass `IccProfLib2.dll` from the selected configuration directory.

## Expected Output

The tracked profile is valid, so it returns zero and has an empty report:

```text
validation: OK (0)
report: none
```
