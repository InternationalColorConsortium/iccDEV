# hello-iccdev

Minimal standalone example that links **IccProfLib2** and **IccXML2** from the
[iccDEV](https://github.com/InternationalColorConsortium/iccDEV) project
(RefIccMAX -- the ICC reference implementation).

When **IccJSON2** (and nlohmann-json) is available, the example also
demonstrates JSON round-tripping via `CIccProfileJson::ToJson()`.

## What It Does

1. Prints the IccProfLib, IccLibXML, and IccLibJSON library version strings
2. Creates a minimal ICC display profile (sRGB, Lab PCS)
3. Round-trips the profile header to XML via `CIccProfileXml::ToXml()`
4. Round-trips the profile header to JSON via `CIccProfileJson::ToJson()` (when IccJSON2 is linked)
5. Prints a greeting: **Hello, iccDEV!**

## Prerequisites

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ compiler | C++17 support  |

## Windows vcpkg

```
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev\examples\hello-iccdev 
git checkout ci-vcpkg-ports
vcpkg install --overlay-ports=../../ports/iccdev
cmake -S . -B build-vcpkg-verify -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" "-DICCDEV_BUILD_DIR=..\..\Release"
cmake --build .\build-vcpkg-verify\ --config Release
build-vcpkg-verify\Release\hello-iccdev.exe
```

## Unix vcpkg

```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh --disableMetrics
./vcpkg integrate install
cd ..
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev/examples/hello-iccdev
../../../vcpkg/vcpkg install --overlay-ports=../../ports/iccdev
cmake -S . -B build-vcpkg-verify "-DCMAKE_TOOLCHAIN_FILE=-DCMAKE_TOOLCHAIN_FILE=..\..\..\vcpkg\scripts\buildsystems\vcpkg.cmake" -G Ninja" -G "Visual Studio 17 2022" -A x64
cmake --build build-vcpkg-verify --config Release
./build-vcpkg-verify/Release/hello-iccdev
```

### CMake

```
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev 
cmake -B unix -S Build/Cmake/
cmake --build unix
cd examples/hello-iccdev 
cmake -B unix   -DICCDEV_ROOT=$HOME/head/iccDEV   -DICCDEV_BUILD_DIR=$HOME/head/iccDEV/unix
cmake --build unix
./unix/hello-iccdev
```

### Output

```
IccProfLib version: 2.3.1.7+e280b0a
IccLibXML  version: 2.3.1.7+e280b0a
Profile spec ver:  4.00

XML round-trip OK (786 bytes)
<?xml version="1.0" encoding="UTF-8"?>
<IccProfile>
  <Header>
    <PreferredCMMType>ICCD</PreferredCMMType>
    <ProfileVersion>4.00</ProfileVersion>
  ... (truncated)

Hello, iccDEV!
```
