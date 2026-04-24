# Building iccDEV

iccDEV requires C++17 or higher to compile.

## Compiler Requirements

| Compiler | Minimum (build succeeds) | Recommended (full diagnostics) | Notes |
|----------|--------------------------|--------------------------------|-------|
| **GCC**   | 11 | **15** | Ubuntu 26.04+, or `ppa:ubuntu-toolchain-r/test` on older releases |
| **Clang** | 10 | **14** | Ubuntu 24.04+, macOS Xcode 15+ |
| **MSVC**  | VS 2022 17.0 | VS 2022 17.10 (19.40) | Windows |

The build enables a base hardening warning set (`-Wall`, `-Wextra`,
`-Wformat-security`, `-Wnon-virtual-dtor`, `-Wuninitialized`,
`-fstack-protector-strong`) on every supported compiler.

When building with **GCC 15+ or Clang 14+**, an additional strict tier turns on
automatically: `-Werror=uninitialized`, `-Wshadow`, `-Wnull-dereference`,
`-Wundef`, `-Wpointer-arith`, plus GCC's `-Wlogical-op`. Older toolchains keep
the base set and skip the strict tier so they continue to build cleanly. CI
exercises GCC 15 (ubuntu:26.04 docker image) and clang-18 (Tool Tests
workflow); contributors should match one of these locally before opening a PR
to see the same diagnostics CI sees. CMake prints the detected tier:

```
-- Linux: strict warnings ENABLED (GNU 15.1.0)
```

Optional opt-in extra strictness (gated by CMake options, OFF by default):

| Option | Effect |
|--------|--------|
| `-DENABLE_STRICT_EFFC=ON`      | `-Weffc++` (Effective C++ guidelines) |
| `-DENABLE_DOUBLE_PROMOTION=ON` | `-Wdouble-promotion` |

## Required libraries

| Platform          | Libraries                                                                 |
|-------------------|---------------------------------------------------------------------------|
| **macOS**         | libpng, jpeg-turbo, libtiff, libxml2, wxwidgets, nlohmann-json                  |
| **Windows**       | libpng, libjpeg-turbo, libtiff, libxml2, wxwidgets, nlohmann-json         |
| **Linux (Ubuntu)** | libpng-dev, libjpeg-dev, libtiff-dev, libxml2-dev, wxwidgets*, nlohmann-json |

## Ubuntu

\* **Note:** On Ubuntu, `wxwidgets` is installed via distribution-specific development packages  
(e.g. `libwxgtk3.2-dev`). Refer to the `apt install` command below for the exact package names.


```
export CXX=clang++   # or: export CC=gcc-15 CXX=g++-15
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers curl git make cmake clang{,-tools} libxml2{,-dev} nlohmann-json3-dev build-essential ninja-build
# For the strict-warning tier on Ubuntu <26.04:
#   sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test && sudo apt install -y g++-15 gcc-15
cmake --preset linux-clang -S Build/Cmake -B out/linux-clang
cmake --build out/linux-clang -j"$(nproc)"
```

## macOS

```
export CXX=clang++
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg-turbo
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
cmake --preset macos-xcode -S Build/Cmake -B out/macos-xcode
cmake --build out/macos-xcode --config Release -j"$(sysctl -n hw.ncpu)"
```

## Windows MSVC

```
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
cmake --preset vs2022-x64 -S Build/Cmake -B out/vs2022-x64
cmake --build out/vs2022-x64 --config Release -- /m /maxcpucount
```

## vcpkg Port (Core Libraries & Tools)

A vcpkg overlay port is provided in `ports/iccdev/` for consuming iccDEV as a
static library dependency across Windows, Linux, and macOS.

For full documentation — install commands, feature flags, CMake integration,
local source mode, platform notes, and troubleshooting — see
**[examples/hello-iccdev](../examples/hello-iccdev/README.md)**.

### Example Project vcpkg

```
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev\examples\hello-iccdev 
git checkout ci-vcpkg-ports
vcpkg install --overlay-ports=../../ports/iccdev
cmake -S . -B build-vcpkg-verify -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DICCDEV_BUILD_DIR=..\..\Release"
cmake --build .\build-vcpkg-verify\ --config Release
build-vcpkg-verify\Release\hello-iccdev.exe
```

### CMake Usage

```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE RefIccMAX::IccProfLib2-static)
```
