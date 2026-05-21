# Building iccDEV

iccDEV requires C++17, CMake 3.18 or newer, and the image/XML/JSON dependencies
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
| Ubuntu | `libpng-dev libjpeg-dev libtiff-dev libxml2-dev libwxgtk3.2-dev libwxgtk-media3.2-dev libwxgtk-webview3.2-dev wx-common wx3.2-headers nlohmann-json3-dev cmake make ninja-build` |
| macOS | `libpng jpeg-turbo libtiff libxml2 wxwidgets nlohmann-json` |
| Windows | MSVC 2022 with vcpkg-managed `libpng`, `libjpeg-turbo`, `libtiff`, `libxml2`, `wxwidgets`, `nlohmann-json` |

Thread support is provided by the platform C/C++ runtime and CMake's
`Threads::Threads` imported target; no separate Ubuntu package is required.
Maintainer sanitizer/regression containers add pinned CI-only packages such as
current Clang/LLVM runtimes, GCC, AFL++, `libssl-dev`, and GNU `time`.

Windows examples include both `cmd.exe` and PowerShell forms where shell syntax
differs. If CMake reports `No such preset`, fetch and switch to a branch that
contains the matching `Build/Cmake/CMakePresets.json` update.

## Ubuntu

```bash
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libxml2-dev \
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
brew install nlohmann-json libxml2 wxwidgets libtiff libpng jpeg-turbo
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset macos-xcode -S Build/Cmake -B out/macos-xcode
cmake --build out/macos-xcode --config Release -j"$(sysctl -n hw.ncpu)"
```

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

The reusable tool-test workflow can also run opt-in all-tool profiling. Dispatch
`ci-pr-action` with `run_tool_flamegraphs=true` to include the sanitized
FlameGraph manifest in the job summary. Each profiled tool records status,
sample count, unknown folded-frame count, SVG availability, and skip reason.
On successful runs, the workflow uploads an `iccdev-developer-report-<BuildType>`
artifact with `index.html`, CTest outputs, hybrid timing data when requested,
and FlameGraph data/SVGs when profiling was enabled. The artifact upload uses
the reviewed sanitized developer-report governance exception.

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
| `Dockerfile` | Ubuntu release/runtime image for `ghcr.io/internationalcolorconsortium/iccdev`. | Validate with a local Docker build and tool smoke test before maintainer publishing. |
| `Dockerfile.nixos` | NixOS/scratch runtime image and dependency-closure check. | Validate locally with a Docker build, runtime closure check, and secret scan before maintainer publishing. |
| `Dockerfile.ci-regression` | Pinned Ubuntu maintainer image for `ci-regression-checks`, with Clang/LLVM 22 defaults, GCC 15.2+, sanitizer, debugger, fuzzing, git, curl, and GitHub CLI tooling. | Validate locally with a no-cache Docker build and toolchain smoke tests before maintainer publishing; consumer workflows select the published tag. |

Before using a branch-specific regression image, maintainers should publish it
through the maintainer-controlled container release path, record the branch or
SHA tag, then pass that tag to `ci-iccdev-tool-tests.yml`.

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
For a complete consuming-project example, see the
[`examples/hello-iccdev` README](https://github.com/InternationalColorConsortium/iccDEV/blob/master/examples/hello-iccdev/README.md).

```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)
```
