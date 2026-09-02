# Building iccDEV

iccDEV requires C++17, CMake 3.23 or newer, and the image/XML/JSON dependencies
listed below. Maintainer-level sanitizer, Docker, and CMake policy details live in
`.github/instructions/build-system.instructions.md`.

## Compiler Requirements

| Compiler | Minimum | Recommended for full diagnostics |
|----------|---------|----------------------------------|
| GCC | 11 | 15+ |
| Clang | 10 | 14+ |
| MSVC | VS 2022 17.0 | VS 2022 17.10+ |

## Dependencies

| Platform | Packages |
|----------|----------|
| Ubuntu | `libpng-dev libjpeg-dev libtiff-dev libxml2-dev zlib1g-dev libwxgtk3.2-dev libwxgtk-media3.2-dev libwxgtk-webview3.2-dev wx-common wx3.2-headers nlohmann-json3-dev cmake make ninja-build` |
| macOS | `libpng jpeg-turbo libtiff libxml2 zlib wxwidgets nlohmann-json` |
| Windows | MSVC 2022 or 2026 with vcpkg-managed `libpng`, `libjpeg-turbo`, `libtiff`, `libxml2`, `zlib`, `wxwidgets`, `nlohmann-json` |

Thread support is provided by the platform C/C++ runtime and CMake's
`Threads::Threads` imported target; no separate Ubuntu package is required.
Maintainer sanitizer/regression containers add pinned CI-only packages such as
current Clang/LLVM runtimes, GCC, AFL++, `libssl-dev`, and GNU `time`.

Windows examples include both `cmd.exe` and PowerShell forms where shell syntax
differs. If CMake reports `No such preset`, fetch and switch to a branch that
contains the matching `Build/Cmake/CMakePresets.json` update.

Visual Studio presets use vcpkg through `VCPKG_ROOT`. For the Visual Studio
Community system vcpkg layout, set `VCPKG_ROOT` to
`C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg`, or pass
`-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake`
on the configure command line.

## Ubuntu

```bash
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libxml2-dev zlib1g-dev \
  libwxgtk3.2-dev libwxgtk-media3.2-dev libwxgtk-webview3.2-dev \
  wx-common wx3.2-headers nlohmann-json3-dev curl git make cmake \
  clang clang-tools build-essential ninja-build

git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset linux-clang -S Build/Cmake -B out/linux-clang
cmake --build out/linux-clang -j"$(nproc)"
```

## macOS

```bash
brew install nlohmann-json libxml2 zlib wxwidgets libtiff libpng jpeg-turbo
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset macos-xcode -S Build/Cmake -B out/macos-xcode
cmake --build out/macos-xcode --config Release -j"$(sysctl -n hw.ncpu)"
```

Always provide `-S Build/Cmake` and an isolated `-B out/macos-xcode`
directory. Configuring in the repository root is rejected to keep generated
Xcode files out of the source tree.

To open the generated project:

```bash
open out/macos-xcode/RefIccMAX.xcodeproj
```

For GuardMalloc/libgmalloc crash reproduction, use a non-sanitizer Debug build
and verify that the built Mach-O tools contain `LC_UUID`. Apple's dynamic loader
can abort before `main()` when `DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib`
is used with an executable that was linked without an `LC_UUID` load command.

```bash
cmake --preset macos-clang-guard-malloc -S Build/Cmake -B out/macos-clang-guard-malloc
cmake --build out/macos-clang-guard-malloc --target iccToXml -j"$(sysctl -n hw.ncpu)"
cmake --build out/macos-clang-guard-malloc --target check-macos-guard-malloc

DYLD_INSERT_LIBRARIES=/usr/lib/libgmalloc.dylib \
MallocScribble=1 \
MallocPreScribble=1 \
MallocGuardEdges=1 \
MallocStackLogging=1 \
out/macos-clang-guard-malloc/Tools/IccToXml/iccToXml input.icc output.xml
```

Do not combine GuardMalloc with AddressSanitizer, ThreadSanitizer, or
MemorySanitizer. Use ASan/UBSan builds for sanitizer attribution and use
GuardMalloc separately when checking allocator-sensitive behavior.

## Windows MSVC

```cmd
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64
cmake --build out/vs2022-x64 --config Release -- /m /maxcpucount
```

Windows presets place iccDEV `.exe` and `.dll` runtime artifacts together under
`bin` in the build tree. Run tools from that directory or by explicit path, for
example `out\vs2022-x64\bin\Release\iccToXml.exe`; no manual PATH update is
required for iccDEV project DLLs.

For a reproducible Release build and `iccBenchApply` performance report, run:

```powershell
.\.github\scripts\iccdev-windows-release-report.ps1 `
  -BuildDir out\windows-msvc-release-report `
  -Threads '1,2,4,8,16'
```

The script records host and Visual Studio details, configure and build timing,
compiler warnings, project artifact counts and size, profile-generation time,
and the `iccBenchApply` suite's median/minimum/maximum throughput and scaling.
It writes JSON, Markdown, raw benchmark CSV, and command logs under the build
directory's `reports` folder. Use an otherwise idle machine for comparisons;
the report records results but does not assert a performance threshold.
If application-control policy blocks locally built unsigned executables or
dependencies, run the report on an approved Windows build host or CI runner;
do not disable the machine policy. `-ProfileRoot` can reuse a Testing tree
generated elsewhere, but it cannot bypass executable-signing policy.

Windows filesystems are case-insensitive by default, while the repository has
legacy paths that differ only by case. For a genuinely clean Git checkout, use
WSL2 storage rather than `/mnt/c` or `/mnt/e`, and preserve LF line endings:

```bash
git config --global core.autocrlf false
git clone https://github.com/InternationalColorConsortium/iccDEV.git
```

Keep Windows build output on a native Windows volume when measuring MSVC or
ClangCL; the WSL2 checkout recommendation is for collision-free Git and Unix
script work, not for benchmarking through a network-mounted source tree.

## Windows ClangCL

Use the Visual Studio LLVM toolset with the same vcpkg-managed dependencies as
the MSVC build:

```cmd
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset vs2022-clangcl-x64 -S Build/Cmake -B out/vs2022-clangcl-x64
cmake --build out/vs2022-clangcl-x64 --config Release -- /m /maxcpucount
```

The ClangCL preset uses the same `out\vs2022-clangcl-x64\bin\Release` runtime
layout as the MSVC preset.

## Windows MSVC 2026

The `vs2026-x64` preset targets the Visual Studio 18 2026 generator and v145
toolset while preserving the VS 2022 preset's vcpkg and runtime layout. CMake
3.23 or newer is required for the preset schema. Set `VCPKG_ROOT` to the
Visual Studio 2026 vcpkg installation before configuring:

```cmd
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
set "VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\2026\Community\VC\vcpkg"
cmake --preset vs2026-x64 -S Build/Cmake -B out/vs2026-x64
cmake --build out/vs2026-x64 --config Release -- /m /maxcpucount
```

## Windows MinGW UCRT64

Install MSYS2 UCRT64 packages for the selected feature set. A core command-line
tool build with `ENABLE_ICCJSON=OFF` uses GCC, CMake, Ninja, and libxml2:

`cmd.exe`:

```cmd
pacman -S --needed ^
  mingw-w64-ucrt-x86_64-gcc ^
  mingw-w64-ucrt-x86_64-cmake ^
  mingw-w64-ucrt-x86_64-ninja ^
  mingw-w64-ucrt-x86_64-make ^
  mingw-w64-ucrt-x86_64-libxml2

set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
cmake --preset mingw-x64 -S Build/Cmake -B out/mingw-x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DENABLE_TOOLS=ON ^
  -DENABLE_ICCXML=ON ^
  -DENABLE_ICCJSON=OFF ^
  -DENABLE_IMAGE_TOOLS=OFF ^
  -DENABLE_WXWIDGETS=OFF ^
  -DENABLE_CMM_TOOLS=OFF ^
  -DENABLE_IIS_TOOLS=OFF
cmake --build out/mingw-x64 --target iccDumpProfile --parallel
```

The MinGW preset writes runnable tools to `out\mingw-x64\bin`, for example
`out\mingw-x64\bin\iccDumpProfile.exe`.

PowerShell:

```powershell
pacman -S --needed `
  mingw-w64-ucrt-x86_64-gcc `
  mingw-w64-ucrt-x86_64-cmake `
  mingw-w64-ucrt-x86_64-ninja `
  mingw-w64-ucrt-x86_64-make `
  mingw-w64-ucrt-x86_64-libxml2

$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --preset mingw-x64 -S Build/Cmake -B out/mingw-x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DENABLE_TOOLS=ON `
  -DENABLE_ICCXML=ON `
  -DENABLE_ICCJSON=OFF `
  -DENABLE_IMAGE_TOOLS=OFF `
  -DENABLE_WXWIDGETS=OFF `
  -DENABLE_CMM_TOOLS=OFF `
  -DENABLE_IIS_TOOLS=OFF
cmake --build out/mingw-x64 --target iccDumpProfile --parallel
```

Install `mingw-w64-ucrt-x86_64-nlohmann-json` and set
`-DENABLE_ICCJSON=ON` when building `IccJSON2`, `IccConnect2`,
`iccToJson` / `iccFromJson`, or the JSON runtime-configuration tools
(`iccApplyNamedCmm`, `iccApplyProfiles`, and `iccApplySearch`).

For a dependency-light local compiler sanity check, use the static core preset.
It disables XML and image tools, but still builds the core library, JSON library,
IccConnect, JSON CLI tools, and the IccConnect threaded CMM regression target:

`cmd.exe`:

```cmd
set PATH=C:\msys64\ucrt64\bin;C:\msys64\usr\bin;%PATH%
cmake --preset mingw-core-x64 -S Build/Cmake -B out/mingw-core-x64
cmake --build out/mingw-core-x64 --parallel
ctest --test-dir out/mingw-core-x64 -R "iccconnect|icc-dump-profile-smoke" --output-on-failure --no-tests=error
```

PowerShell:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;C:\msys64\usr\bin;' + $env:PATH
cmake --preset mingw-core-x64 -S Build/Cmake -B out/mingw-core-x64
cmake --build out/mingw-core-x64 --parallel
ctest --test-dir out/mingw-core-x64 -R "iccconnect|icc-dump-profile-smoke" --output-on-failure --no-tests=error
```

## CTest Tool Suites

Enable both tools and tests to expose the script-backed tool suites through
CTest:

```bash
cmake -S Build/Cmake -B build \
  -DENABLE_TOOLS=ON \
  -DENABLE_TESTS=ON \
  -DENABLE_WXWIDGETS=OFF
cmake --build build --parallel "$(nproc)"
ctest --test-dir build -N --no-tests=error
cmake --build build --target build-test-binaries --parallel "$(nproc)"
ctest --test-dir build --output-on-failure --no-tests=error
```

The default `all` build excludes CTest-only regression helper binaries. The
`build-test-binaries` target builds those helpers for filtered CTest runs. The
`check` target runs the same CTest suite after building tool and test
dependencies:

```bash
cmake --build build --target check
```

For Visual Studio builds, pass the configuration to CTest:

```cmd
ctest --test-dir out/vs2022-x64 -C Release -N --no-tests=error
ctest --test-dir out/vs2022-x64 -C Release --output-on-failure --no-tests=error
cmake --build out/vs2022-x64 --config Release --target check
```

See [CTest tool suites](ctest.md) for the registered tests, fixtures, logs, and
add-test process.

QA flag evidence builds are enabled by `ICCDEV_ENABLE_QA_FLAGS=ON`. Convenience
presets turn on tests, tools, zlib-backed compressed tag support
(`ICC_USE_ZLIB=ON`), and QA evidence hooks for the common local toolchains:

```bash
cmake --preset linux-clang-qa-flags -S Build/Cmake -B out/linux-clang-qa-flags
cmake --build out/linux-clang-qa-flags --parallel "$(nproc)"
ctest --test-dir out/linux-clang-qa-flags -R "^iccdev\.qa-target-flags$" --output-on-failure --no-tests=error

cmake --preset linux-clang-qa-sanitizers -S Build/Cmake -B out/linux-clang-qa-sanitizers
```

QA builds also enable `ICCDEV_ENABLE_STRICT_WARNINGS` by default so CMake, not
workflow command lines, owns maintainer warning policy. The strict tier adds
`-Wpedantic -Werror` plus compiler-specific diagnostics: GCC treats type-limit
and attribute diagnostics as errors, while Clang treats the matching
tautological type-limit and attribute diagnostics as errors. Pass
`-DICCDEV_ENABLE_STRICT_WARNINGS=OFF` to collect QA evidence without promoting
warnings to errors. GCC and Clang QA builds also add debug symbols and frame
pointers so Release and Debug reports can be symbolized consistently.
On ELF platforms, verify the compression dependency with `ldd TOOL_PATH |
grep -E 'libz|zlib'`; `liblzma` is a separate LibXml2 dependency and is
expected only for XML-linked tools.

For local FlameGraph capture of the hybrid `iccApplyProfiles` path, use the
profiling helper after building tools:

```bash
ICCDEV_TOOLS_DIR=$PWD/build/Tools ICCDEV_TESTING_DIR=$PWD/Testing ICCDEV_PROFILE_MODE=record ICCDEV_FLAMEGRAPH_DIR=/tmp/FlameGraph .github/scripts/iccdev-hybrid-applyprofiles-profile.sh
```

The helper defaults to user-space samples, DWARF call graphs, a higher sample
frequency, and a minimum sample threshold so short or kernel-heavy captures do
not produce misleading SVGs with large `unknown` blocks.

The reusable `ci-regression-checks` workflow can also run opt-in all-tool
profiling. Dispatch it with `run_tool_flamegraphs=true`; optionally set
`flamegraph_repeat` and `flamegraph_timeout` for the capture envelope. On a
successful run, download `iccdev-developer-report-<BuildType>` and open its root
`index.html`: it embeds the FlameGraph dashboard when profiling was requested.
Each manifest row records status, sample count, unknown folded-frame count, SVG
availability, and a skip or failure reason. A runner without usable `perf`
truthfully produces `SKIP` or `PERF_FAIL` rows and no SVGs; it is not a visual
capture. The artifact also contains CTest outputs, QA target-flag evidence,
optional hybrid timing data, and all FlameGraph capture data. The artifact
upload uses the reviewed sanitized developer-report governance exception.

```cmd
set VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg
cmake --preset vs2022-x64-qa-flags -S Build/Cmake -B out/vs2022-x64-qa-flags
cmake --preset vs2022-clangcl-x64-qa-flags -S Build/Cmake -B out/vs2022-clangcl-x64-qa-flags
cmake --preset mingw-x64-qa-flags -S Build/Cmake -B out/mingw-x64-qa-flags
```

### Linux AVX2 and AVX-512 CLUT build

Linux and WSL2 can use paired Clang presets for portable, AVX2, and AVX-512
CLUT comparisons. The presets hold the compiler, Release configuration, QA
flags, enabled tools and tests, disabled wxWidgets UI, and disabled AVX2
diagnostics constant:

```bash
cmake --preset linux-clang-clut-baseline-qa-flags \
  -S Build/Cmake -B out/clut-baseline
cmake --preset linux-clang-clut-avx2-qa-flags \
  -S Build/Cmake -B out/clut-avx2
cmake --preset linux-clang-clut-avx512-qa-flags \
  -S Build/Cmake -B out/clut-avx512

cmake --build out/clut-baseline --parallel "$(nproc)"
cmake --build out/clut-avx2 --parallel "$(nproc)"
cmake --build out/clut-avx512 --parallel "$(nproc)"
```

The AVX-512 preset inherits the AVX2 preset so AVX2 remains the intermediate
runtime fallback. The focused CLUT test uses distinct values at every grid
point so its expected output checks both corner selection and interpolation
weights. Its eight-, nine-, eleven-, and fourteen-output fixtures validate
fallback behavior, while the fifteen-output fixture covers a full AVX2 vector
plus a masked tail and the corresponding AVX-512 path.

To inspect AVX2 dispatch or set source breakpoints, add
`-DICC_AVX2_CLUT_DEBUG=ON`. This emits the CPU decision, output channel count,
corner offsets, interpolation weights, and per-kernel elapsed nanoseconds
through `IccSignatureUtils.h`. It is intentionally diagnostic-only: trace
logging and timing are compiled out when the option is `OFF`, so do not use a
debug-trace build for throughput benchmarks.

### Windows AVX2 CLUT build

`vs2022-clangcl-x64-avx2-qa-flags` enables the optional AVX2 3D CLUT
interpolation implementation. It compiles the AVX2 code in a separate
translation unit and dispatches only after CPUID and XGETBV verify OS AVX
state, so the resulting binary retains the scalar/SSE fallback on other x64
CPUs.

```cmd
set VCPKG_ROOT=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg
cmake --preset vs2022-clangcl-x64-clut-baseline-qa-flags -S Build/Cmake -B out/vs2022-clangcl-x64-clut-baseline-qa-flags
cmake --preset vs2022-clangcl-x64-avx2-qa-flags -S Build/Cmake -B out/vs2022-clangcl-x64-avx2-qa-flags
cmake --build out/vs2022-clangcl-x64-clut-baseline-qa-flags --config Release --target IccProfLib2 -- /m /maxcpucount
cmake --build out/vs2022-clangcl-x64-avx2-qa-flags --config Release -- /m /maxcpucount
ctest --test-dir out/vs2022-clangcl-x64-avx2-qa-flags -C Release --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
ctest --test-dir out/vs2022-clangcl-x64-avx2-qa-flags -C Release --output-on-failure --no-tests=error
```

The Visual Studio presets run vcpkg manifest installation during
configuration, so separate `vcpkg integrate install` and `vcpkg install`
commands are not required. The dedicated CLUT comparison presets suppress the
repository's existing Microsoft CRT compatibility deprecations; other Windows
presets continue to report them.

The preset is intentionally opt-in. Enable `ICCDEV_ENABLE_AVX2=ON` only for
x86-64 compiler targets that accept the required ISA flag; CMake validates the
compiler flag during configuration. Native MSVC reports an explicit skip and
keeps SSE2 because its measured AVX2 path was slower; use ClangCL on Windows.
The focused regression converts valid 3D
RGB-to-8CLR, RGB-to-9CLR, RGB-to-BCLR, RGB-to-ECLR, and RGB-to-FCLR ICC v5
fixtures and verifies their expected CLUT outputs. On an AVX2-capable x86-64
host, the fifteen-output fixture enters the AVX2 helper; eight, nine, eleven,
and fourteen outputs validate SSE2 fallback.

For an interleaved Release comparison across every supported output count:

```powershell
.github\scripts\iccdev-windows-clut-avx2-benchmark.ps1 `
  -BaselineBuildDir out\vs2022-clangcl-x64-clut-baseline-qa-flags `
  -Avx2BuildDir out\vs2022-clangcl-x64-avx2-qa-flags `
  -Iterations 20000000 -Repetitions 11 `
  -AffinityCpu 30 `
  -OutputPath out\clut-avx2-benchmark.tsv
```

Every result row must report `output_vector_match=True`. The helper compares
the raw float bits for each output lane; a rounded aggregate checksum is not a
sufficient correctness check. Keep both benchmark builds on ClangCL; comparing
native MSVC against ClangCL also measures a compiler change.

For a Windows debugging session, use the dedicated diagnostics preset:

```cmd
cmake --preset vs2022-clangcl-x64-avx2-diagnostics -S Build/Cmake -B out/vs2022-clangcl-x64-avx2-diagnostics
cmake --build out/vs2022-clangcl-x64-avx2-diagnostics --config Debug -- /m /maxcpucount
ctest --test-dir out/vs2022-clangcl-x64-avx2-diagnostics -C Debug --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
```

Set source breakpoints in
`IccTraceAvx2ClutDispatch()` or `IccTraceAvx2ClutKernel()` in
`IccSignatureUtils.h` to inspect the selected path and kernel inputs.

### Windows AVX-512 CLUT build

`vs2022-clangcl-x64-avx512-qa-flags` adds the experimental AVX-512
implementation while retaining AVX2 and scalar/SSE fallbacks. The AVX-512
translation unit is compiled separately, and runtime dispatch requires CPUID
AVX-512F support plus XGETBV confirmation that XMM, YMM, opmask, and ZMM state
are enabled by the operating system.

```cmd
cmake --preset vs2022-clangcl-x64-avx512-qa-flags -S Build/Cmake -B out/vs2022-clangcl-x64-avx512-qa-flags
cmake --build out/vs2022-clangcl-x64-avx512-qa-flags --config Release -- /m /maxcpucount
ctest --test-dir out/vs2022-clangcl-x64-avx512-qa-flags -C Release --output-on-failure --no-tests=error -R "^iccdev\.clut-eight-output-regression$"
ctest --test-dir out/vs2022-clangcl-x64-avx512-qa-flags -C Release --output-on-failure --no-tests=error
```

`ICCDEV_ENABLE_AVX512=ON` is independently opt-in and defaults to `OFF`.
The Windows preset also enables AVX2 so AVX2-capable systems retain that
intermediate fallback when AVX-512 is unavailable.

## Runtime packaging

Top-level CMake builds can also emit runtime packages with CPack without changing
the legacy `dist-bin` archive flow:

```bash
cmake -S Build/Cmake -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON -DENABLE_SHARED_LIBS=ON
cmake --build build --parallel "$(nproc)"
cmake --build build --target package-sha256
```

The `runtime` install component includes command-line tools, shared runtime
libraries, package notes, the project license, a version manifest, and a generated
tool catalog. Development headers, CMake package exports, and static archives are
installed by the `dev` component. Linux builds generate ZIP packages by default
and add DEB or RPM packages when the corresponding local CPack backend is
available. The `ci-latest-release` workflow runs the Linux CPack runtime-package
smoke and uploads the `reficcmax-runtime-packages-linux` artifact.

## Shared library exports

`ENABLE_SHARED_LIBS` defaults to `ON`, so a default Windows build links against
`IccProfLib2.dll`. Its exports come from CMake's `WINDOWS_EXPORT_ALL_SYMBOLS`
rather than from `__declspec` annotations: MSVC is deliberately excluded from the
`ICCPROFLIBDLL_EXPORTS` definition in `Build/Cmake/IccProfLib/CMakeLists.txt`.
That arrangement dates from #764, and the same file records why hidden visibility
is not used on GCC/Clang -- `ICCPROFLIB_API` annotations are incomplete
(~308 partial uses across 43 headers) and `IccXML` has none at all.

### Exported global data (#1888, fixed in #2219)

One consequence of that arrangement cost several CI rounds before it was fixed:

> `WINDOWS_EXPORT_ALL_SYMBOLS` auto-exports **functions**. Exported global
> **variables** still require explicit `dllexport`/`dllimport`. IccProfLib
> carried none, so referencing one from outside the library failed to link on
> Windows shared builds with `LNK2019`/`LNK1120`, while every function in the
> same header linked normally.

Linux and macOS have no import-library model and were never affected, so this
never appeared in a local pre-flight on those platforms.

Eight globals are involved: `g_pIccMatrixSolver` and `g_pIccMatrixInverter`
(`IccSolve.h`), and `icD50XYZ`, `icD50XYZxx` and the four `icMsgValidate*`
message prefixes (`IccUtil.h`). They now carry `ICCPROFLIB_DATA_API`, a macro
kept deliberately separate from `ICCPROFLIB_API` so real `dllexport`/`dllimport`
could be added without flipping the ~308 incomplete `ICCPROFLIB_API` annotations
and changing how every class and function is exported. The shared IccProfLib
target defines `ICCPROFLIBDLL_DATA_EXPORTS` `PRIVATE` and
`ICCPROFLIBDLL_DATA_IMPORTS` `INTERFACE`, so anything linking it — in-tree tools
and `find_package()` consumers alike — sees `dllimport`. The static target
deliberately gets neither and keeps a plain `extern`.

Nothing special is needed to consume these symbols now. Link
`RefIccMAX::IccProfLib2` and reference them; the workarounds the tree used to
carry are gone (#2154):

- **Tools** — `iccDumpProfile`, `iccProfilePlot` and `wxProfileDump` linked
  `IccProfLib2-static` on Windows shared builds; all three now link
  `${TARGET_LIB_ICCPROFLIB}` on every platform.
- **Regression executables** — the `${ICCDEV_TEST_LIB_ICCPROFLIB}` indirection
  that applied the same fallback is retired; tests link
  `${TARGET_LIB_ICCPROFLIB}` directly.
- **Consumers that also link `IccXML`/`IccJson`** could never use that fallback
  anyway, since those libraries link `IccProfLib` `PUBLIC` and a static copy
  would load the library twice in one process. That constraint is moot now.

One case still needs a line of CMake: a **hand-built imported target**, which
inherits nothing. `install(EXPORT)` carries the `INTERFACE` definition, and
`Build/Cmake/Modules/FindRefIccMAX.cmake` and `examples/hello-iccdev` set it
explicitly for the same reason — but a consumer that constructs its own
`IMPORTED` target from a found library path must add
`INTERFACE_COMPILE_DEFINITIONS "ICCPROFLIBDLL_DATA_IMPORTS"` to it, on the
shared target only. Without it the macro compiles empty and Windows is back to
`LNK2019`.

Two CTests hold the fix in place, and they fail differently on purpose. An
in-tree target that cannot link takes the whole Windows build down rather than
turning one test red, so `iccdev.proflib-exported-data-linkage` (in-tree, all
platforms) pins the linkage and the literal values, while
`iccdev.proflib-exported-data-dll-linkage` configures and compiles a consumer
**out of tree** at test time and reports a lost annotation as a single failing
test with the `LNK2019` in its log.

A third test, `iccdev.installed-package-consumer`, covers the same annotation
one step further out — on the *installed* package rather than the build tree.
It stages an install into a temporary prefix and then builds and runs three
consumers against it: `find_package(RefIccMAX CONFIG)`, `find_package(RefIccMAX
MODULE)` through `FindRefIccMAX.cmake`, and `examples/hello-iccdev` itself. A
fourth arm repeats MODULE mode with a target-less `RefIccMAXConfig.cmake`
planted earlier on `CMAKE_PREFIX_PATH`, the shape every install before #712 has.
Each
arm pins the library it resolved back to the staged prefix, so a machine with
iccDEV installed system-wide cannot quietly satisfy the test with that copy
instead. The MODULE arm additionally asserts the imported target still carries
`ICCPROFLIBDLL_DATA_IMPORTS`, which no compile or link step would notice off
Windows because `IccProfLibConf.h`'s non-PC branch never reads the macro. The
arms are skipped, with a logged reason, on sanitizer builds (the out-of-tree
consumers are not instrumented) and the MODULE arms on static-only builds.

`IccUtil.h` also declared `icInfo` until #1897, but that one had no definition
anywhere in the tree and so failed to link on every platform — a dangling
declaration rather than an export problem. It is gone; construct a `CIccInfo`
where one is needed. The `iccdev.proflib-exported-global-definitions` CTest
checks every `ICCPROFLIB_API extern` **and** `ICCPROFLIB_DATA_API extern`
declaration in these headers — in both macro orderings — against the built
library's symbol table, so a declaration added without a definition fails CI
rather than reaching a consumer. Annotate a new exported *variable*
`ICCPROFLIB_DATA_API`, not `ICCPROFLIB_API`: both are audited, but only the
first exports data on Windows.

`IccSetMatrixSolver()` and `IccSetMatrixInverter()` remain the supported way to
install a custom solver: they are functions, and they do not depend on the
caller writing through an imported pointer.

See `.github/ci/regression/README.md` for the test-side rules.

## Namespace wrapping (known defect)

`ENABLE_USEICCDEVNAMESPACE` defaults to `OFF`
(`Build/Cmake/CMakeLists.txt:565`). **Turning it `ON` does not produce a
library.** Configure succeeds and prints `>>> Namespace wrapping enabled
(iccDEV)`, then `IccProfLib2` fails to compile; no tool or test is reached.
This is tracked as #2152 and is documented here because the option looks
supported at configure time.

```bash
cmake -S Build/Cmake -B build-ns -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_USEICCDEVNAMESPACE=ON -DENABLE_TOOLS=OFF -DENABLE_TESTS=OFF \
  -DENABLE_WXWIDGETS=OFF -DENABLE_SHARED_LIBS=ON -DENABLE_STATIC_LIBS=OFF
cmake --build build-ns -j4
# Clang 18.1.3: 70 errors, first at IccProfLib/IccCAM.cpp:81:
#   use of undeclared identifier 'CIccCamConverter';
#   did you mean 'iccDEV::CIccCamConverter'?
# GCC 13.3.0: 925 errors, same first site:
#   'CIccCamConverter' has not been declared
```

The two error counts are for the same tree and the same first failure. Take
that as a warning about counts: the number depends on how far a compiler
carries on after the first unresolved scope, so it measures diagnostic
recovery, not how much work is left.

The option is not broken so much as **half-applied**. It defines
`USEICCDEVNAMESPACE=1` (`Build/Cmake/CMakeLists.txt:709-711`), which opens
`namespace iccDEV` in `IccProfLib/IccProfLibConf.h:67` and closes it at `:200`.
Every other file must opt in with its own `#ifdef USEICCDEVNAMESPACE` block,
and many never did:

| Location | Files with no guard |
|----------|---------------------|
| `IccProfLib/*.cpp` | 16 of 39 |
| `IccProfLib/*.h` | 13 of 47 |
| `IccXML/IccLibXML/*.cpp` | 2 of 7 |
| `IccXML/IccLibXML/*.h` | 4 of 8 |
| `IccConnect/IccLibConnect` | none - fully guarded |

So a header that wraps declares `iccDEV::CIccFoo` while its unwrapped `.cpp`
defines a global `CIccFoo`, and the definition matches no declaration.
`IccCAM.h` (guards at `:74` and `:159`) against `IccCAM.cpp` (zero occurrences)
is the cleanest instance, and is where the build stops first. The build never
reaches `IccXML`, whose own gaps are listed above for completeness.

Finishing the rollout would be a per-file decision, not a sweep, and the
per-file question is open rather than settled. Not every unguarded header wants
the guard: `IccProfLibVer.h` holds a single version macro, and
`icProfileHeader.h` holds the on-disk ICC structures and signature enums in
deliberately C-style form. Whether headers of that kind stay outside the
namespace by design is part of what #2152 asks.

Two traps are worth knowing before spending time here:

> **A single-header syntax probe will not scope this.** `IccCmm.h` *has* the
> guard, yet `clang++ -fsyntax-only -DUSEICCDEVNAMESPACE=1` on a file that
> includes only it still errors, because it pulls in unguarded siblings such as
> `IccTagEmbedIcc.h`. Such a probe reflects the include graph, not the work
> remaining. Only a full library build says anything trustworthy.

> **This is not the same defect as the export-annotation gap** described in the
> section above, even though #176 is titled `Known Defect |
> ENABLE_USEICCDEVNAMESPACE=ON`. That issue's body is about the vcpkg port
> building static libraries only and shared builds failing for want of symbol
> exports, and the #764/#784/#823/#966/#1193 history sits with it on the shared
> library and visibility side. Namespace scoping is a separate incomplete
> rollout that fails under both Clang and GCC on Linux before any linking
> happens. Both are incomplete; they are unrelated defects.

Test sources are written as though the option worked: 25 of the 91 regression
sources under `.github/ci/regression/` carry `#ifdef USEICCDEVNAMESPACE`
blocks, as does all of `IccConnect`. Those paths are currently unexercised,
since no build can enable them.

Measured on `master` `054eb006`, Ubuntu 24.04 (WSL2), Unix Makefiles, Release,
with both Clang 18.1.3 and GCC 13.3.0.

## Instrumentation Builds

Use CMake options instead of hand-written sanitizer flags. Clean the cache when
changing compiler or instrumentation mode, and use `CC=clang` plus `CXX=clang++`
for environment compiler selection.

```bash
# Security repro default: ASan + UBSan + IntSan + float checks, no coverage
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_ASAN=ON -DENABLE_UBSAN=ON -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON
make -j"$(nproc)"

# Coverage build; keep separate from sanitizer reproductions
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_COVERAGE=ON
make -j"$(nproc)"

# Profiling build
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_PROFILING=ON
make -j"$(nproc)"
```

For ThreadSanitizer and MemorySanitizer, use one sanitizer family per build:

```bash
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TSAN=ON
cd Build && rm -rf CMakeCache.txt CMakeFiles && CC=clang CXX=clang++ cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_MSAN=ON
```

Do not enable coverage while reproducing sanitizer findings; coverage
instrumentation can change optimizer and sanitizer behavior enough to mask a
bug. Maintainer-level details live in
`.github/instructions/build-system.instructions.md`.

Preset equivalents are available for the same modes:

```bash
cmake --preset linux-clang-sanitizers -S Build/Cmake -B out/linux-clang-sanitizers
cmake --preset linux-clang-ubsan-int-float -S Build/Cmake -B out/linux-clang-ubsan-int-float
cmake --preset linux-clang-tsan -S Build/Cmake -B out/linux-clang-tsan
cmake --preset linux-clang-msan -S Build/Cmake -B out/linux-clang-msan
cmake --preset linux-clang-coverage -S Build/Cmake -B out/linux-clang-coverage
cmake --preset linux-clang-profiling -S Build/Cmake -B out/linux-clang-profiling
cmake --preset macos-clang-sanitizers -S Build/Cmake -B out/macos-clang-sanitizers
cmake --preset macos-clang-guard-malloc -S Build/Cmake -B out/macos-clang-guard-malloc
```

## Maintainer Dockerfiles

`Dockerfile*` files are maintainer-owned release and CI infrastructure. General
source builds should use the platform package lists above; only maintainers
should change container package pins, published image tags, or GHCR workflows.

| File | Maintainer purpose | Publish/validation path |
|------|--------------------|-------------------------|
| `Dockerfile` | Pinned Ubuntu unified image for runtime, MCP, and maintainer checks, with Clang/LLVM 22 defaults, a Clang 21 pair for the packaged AFL++ LLVM plugin, GCC 15.2+, sanitizer, debugger, fuzzing, git, curl, and GitHub CLI tooling. | Validate locally with a no-cache Docker build and toolchain smoke tests before maintainer publishing; AFL wrapper changes also need the `docs/afl-fuzzing.md` container bootstrap probe; consumer workflows select `latest`, an immutable SHA, or a release tag. |

For reproducible maintainer checks, pass the immutable SHA tag to
`ci-iccdev-tool-tests.yml`; use `latest` only for the current `master`
baseline and a release tag for a released image.

## Python Wheel Validation

The `python/` package supports local editable installs, source distributions,
platform wheels, and cibuildwheel matrix validation:

```bash
python -m pip install -e "./python[dev]"
python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m "not parity"
ICCDEV_BUILD_DIR=/path/to/Build python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m parity
cd python
python -m build
python -m twine check dist/*.whl dist/*.tar.gz
python -m cibuildwheel --print-build-identifiers --platform linux
python -m cibuildwheel --print-build-identifiers --platform macos
python -m cibuildwheel --print-build-identifiers --platform windows
```

On Windows, run from a Developer Command Prompt or another shell where `cl.exe`
is already available. The package build reuses that environment instead of
launching another `vcvarsall.bat`, which avoids failures from very long
developer `PATH` values. Native-backed parity tests use the built iccDEV tools
for ProfileLib, XML, and JSON parity; cibuildwheel keeps using the lightweight
`not parity` subset so isolated wheel tests do not require repository build
artifacts.
For Python packaging PR gates, merge criteria, and production PyPI release
steps, see [Python packaging PR, merge, and production release](python-packaging-release.md).

## vcpkg Consumers

The `ports/iccdev/` overlay port builds core static libraries and CLI tools.
The pinned vcpkg registry baseline is
`eae1680538b86f962455c27abca2aad0dc304a4d`. Keep the root and
`Build/Cmake` manifests, the `hello-iccdev` registry configuration, and both
vcpkg CI bootstrap paths synchronized when updating it. The overlay port
manifest intentionally has no registry baseline.
For a complete consuming-project example, see the
[`examples/hello-iccdev` README](https://github.com/InternationalColorConsortium/iccDEV/blob/master/examples/hello-iccdev/README.md).

```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)
```

Use `CONFIG` mode for a static-only install, as above.
`Build/Cmake/Modules/FindRefIccMAX.cmake` reports not-found for one on purpose:
a static archive carries none of its own dependencies, and which ones the
consumer has to repeat depends on the options the install was built with
(`ICC_USE_ZLIB`, `ENABLE_ICCXML`) — something a hand-written find module cannot
see. The `CONFIG` package is generated from the build that produced it and
carries them exactly.
