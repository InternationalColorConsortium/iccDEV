# Building iccDEV

iccDEV requires C++17, CMake 3.21 or newer, and the image/XML/JSON dependencies
listed below. Maintainer-level sanitizer and CMake policy details live in
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

## Windows MSVC

```cmd
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64
cmake --build out/vs2022-x64 --config Release -- /m /maxcpucount
```

## Windows ClangCL

Use the Visual Studio LLVM toolset with the same vcpkg-managed dependencies as
the MSVC build:

```cmd
git clone https://github.com/InternationalColorConsortium/iccDEV.git iccdev
cd iccdev
cmake --preset vs2022-clangcl-x64 -S Build/Cmake -B out/vs2022-clangcl-x64
cmake --build out/vs2022-clangcl-x64 --config Release -- /m /maxcpucount
```

## Windows MinGW UCRT64

Install MSYS2 UCRT64 packages for the selected feature set. A core command-line
tool build uses GCC, CMake, Ninja, libxml2, and nlohmann-json:

`cmd.exe`:

```cmd
pacman -S --needed ^
  mingw-w64-ucrt-x86_64-gcc ^
  mingw-w64-ucrt-x86_64-cmake ^
  mingw-w64-ucrt-x86_64-ninja ^
  mingw-w64-ucrt-x86_64-make ^
  mingw-w64-ucrt-x86_64-libxml2 ^
  mingw-w64-ucrt-x86_64-nlohmann-json

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

PowerShell:

```powershell
pacman -S --needed `
  mingw-w64-ucrt-x86_64-gcc `
  mingw-w64-ucrt-x86_64-cmake `
  mingw-w64-ucrt-x86_64-ninja `
  mingw-w64-ucrt-x86_64-make `
  mingw-w64-ucrt-x86_64-libxml2 `
  mingw-w64-ucrt-x86_64-nlohmann-json

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
ctest --test-dir build --output-on-failure --no-tests=error
```

The `check` target runs the same CTest suite after building tool dependencies:

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

## vcpkg Consumers

The `ports/iccdev/` overlay port builds core static libraries and CLI tools.
For a complete consuming-project example, see
[`examples/hello-iccdev`](../examples/hello-iccdev/README.md).

```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)
```
