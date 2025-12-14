# Building iccDEV

## Quickstart installation: 

| Method | Command |
|--------|---------|
| **Homebrew** | `brew install iccdev` |
| **NPM** | `npm install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest bash -l` |

## Required libraries

| Platform          | Libraries                                                                 |
|-------------------|---------------------------------------------------------------------------|
| **macOS**         | libpng, jpeg, libtiff, libxml2, wxwidgets, nlohmann-json                  |
| **Windows**       | libpng, libjpeg-turbo, libtiff, libxml2, wxwidgets, nlohmann-json         |
| **Linux (Ubuntu)** | libpng-dev, libjpeg-dev, libtiff-dev, libxml2-dev, wxwidgets*, nlohmann-json |

\* **Note:** On Ubuntu, `wxwidgets` is installed via distribution-specific development packages  
(e.g. `libwxgtk3.2-dev`). Refer to the `apt install` command below for the exact package names.

iccDEV requires C++17 or higher to compile.


## Ubuntu GNU

```
export CXX=g++
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev/Build
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers curl git make cmake clang{,-tools} libxml2{,-dev} nlohmann-json3-dev build-essential
cmake Cmake
make -j"$(nproc)"

```

## macOS Clang

```
export CXX=clang++
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
cmake -G "Xcode" Build/Cmake
xcodebuild -project RefIccMAX.xcodeproj
open RefIccMAX.xcodeproj
```

## Windows MSVC

```
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
vcpkg integrate install
vcpkg install
cmake --preset vs2022-x64 -B . -S Build/Cmake
cmake --build . -- /m /maxcpucount
```

---
