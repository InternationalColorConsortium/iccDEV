---
applyTo: "matlab/**"
---

# MATLAB/Octave MEX Bindings -- Path-Specific Instructions

## What This Is

MATLAB/Octave interface to IccProfLib via a single MEX gateway binary.
An OOP wrapper layer in the `+iccdev` package namespace provides
MATLAB-idiomatic classes on top of the MEX function calls.

## File Structure

```
matlab/
  mex/
    icc_mex.cpp           # MEX gateway (702 lines) -- action-string dispatch
  +iccdev/
    IccProfile.m          # Profile class (open, read, header, color_space)
    IccCmm.m              # Color management module wrapper
    IccApply.m            # Thread-safe per-thread apply handle
    ColorSpace.m          # Color space signature enum
    RenderingIntent.m     # Rendering intent enum
    Interpolation.m       # Interpolation method enum
    sig_to_str.m          # Signature to string utility
  examples/
    read_profile.m        # Profile reading example
    color_transform.m     # Color transform example
  build_mex.m             # Build script (auto-detects library location)
  tests/
    test_iccdev.m         # Test suite (MATLAB/Octave compatible)
  README.md               # Usage documentation
```

## Architecture

### MEX Gateway Pattern

`icc_mex.cpp` uses a single-entry-point design with action-string dispatch:

```matlab
result = icc_mex('action_name', arg1, arg2, ...);
```

Actions include: `profile_open`, `profile_read`, `profile_header`, `profile_free`,
`cmm_create`, `cmm_attach`, `cmm_begin`, `cmm_info`, `cmm_apply`, `cmm_free`,
`apply_create`, `apply_apply`, `apply_free`.

### Handle Registry

A global handle table maps integer handles to C++ object pointers:
- `profile_open` returns a handle (integer)
- Subsequent calls pass the handle to identify the object
- `profile_free` releases the handle and deletes the C++ object
- Prevents dangling pointers across MATLAB/Octave workspace operations

### OOP Layer

The `+iccdev` package provides MATLAB classes that wrap MEX calls:

```matlab
prof = iccdev.IccProfile('path/to/profile.icc');
hdr = prof.get_header();
disp(hdr.version);

cmm = iccdev.IccCmm();
cmm.attach('input.icc');
cmm.attach('output.icc');
cmm.begin();
out = cmm.apply(pixelData);
```

## Build

### MATLAB

```matlab
cd matlab
build_mex
```

`build_mex.m` searches for `IccProfLib2-static` in these directories
(relative to the build root, which defaults to `../Build`):
1. `ICCDEV_BUILD_DIR` environment variable (if set)
2. `../Build/` (default cmake build dir)
3. `../Build/IccProfLib/`
4. `../Build/lib/`
5. `../Build/Release/` and `../Build/Debug/`
6. `../Build/IccProfLib/Release/` and `../Build/IccProfLib/Debug/`

### Octave

```bash
cd matlab && octave --eval "build_mex"
```

## Test

Run from the repository root so both the package namespace and tests are on
the MATLAB/Octave path:

```matlab
addpath('matlab');
addpath('matlab/tests');
test_iccdev();
```

Or from Octave at the repository root:
```bash
octave --eval "addpath('matlab'); addpath('matlab/tests'); test_iccdev();"
```

For the extended local smoke and stress checks:

```matlab
addpath('matlab');
run_local_qa();
```

For Docker CLI interoperability:

```matlab
run_docker_qa();
```

## Dependencies

- MATLAB R2015b+ or GNU Octave 6+
- Pre-built IccProfLib2-static library (run cmake build first)
- C++ compiler supported by MATLAB's MEX infrastructure

## Common Pitfalls

- `build_mex.m` expects the iccDEV cmake build to be completed first.
  Run `cmake -S Build/Cmake -B build-matlab-full -DCMAKE_BUILD_TYPE=Release
  -DENABLE_TESTS=ON -DENABLE_TOOLS=ON -DENABLE_ICCXML=ON -DENABLE_ICCJSON=ON
  -DENABLE_IMAGE_TOOLS=ON -DENABLE_CMM_TOOLS=ON && cmake --build
  build-matlab-full -j$(nproc)` before building MEX.
- Normal MATLAB/Octave builds reject Debug-only `IccProfLib2` libraries. Use a
  Release iccDEV build, or call `build_mex('Debug', true)` only with a compatible
  debug MATLAB/Octave runtime.
- A static `IccProfLib2` built with `ICC_USE_ZLIB=ON` requires zlib at MEX link
  and load time. `build_mex.m` must read the selected build's `CMakeCache.txt`,
  link the matching vcpkg import library on Windows, and stage its runtime DLL
  beside the MEX binary.
- MATLAB package `private/` directories cannot be added to the global path.
  Native-only tests must call the hidden package bridge rather than invoking
  `icc_mex` directly from `matlab/tests`.
- The handle registry is global -- calling `clear mex` invalidates all handles.
  Always close profiles explicitly before clearing MEX state.
- MATLAB's MEX compiler may differ from the system compiler.
  Use `mex -setup C++` to configure if build fails.
- Canonical workflow and troubleshooting: `docs/matlab-bindings.md`.
- Repeatable agent workflow: `.github/skills/matlab-bindings-test/SKILL.md`.
- Keep `matlab.prj`, `matlab/resources/`, and MATLAB-generated project control
  files local-only. Review staged and untracked files before pushing.
