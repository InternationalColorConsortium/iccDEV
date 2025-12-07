# Building iccDEV

## Quickstart installation: 

| Method | Command |
|--------|---------|
| **Homebrew** | `brew install iccdev` |
| **NPM** | `npm install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest bash -l` |

## Dependencies
| Dependency         | Ubuntu (apt)                     | macOS (brew)          | Windows (vcpkg)       |
|--------------------|----------------------------------|------------------------|------------------------|
| Build Tool         | cmake, make, clang               | cmake, clang           | cmake                  |
| Image Libraries    | libpng-dev, libjpeg-dev, libtiff-dev | libpng, jpeg, libtiff  | libpng, libjpeg-turbo, libtiff |
| GUI & Config       | libwxgtk3.2-dev, nlohmann-json3-dev, libxml2-dev | wxwidgets, nlohmann-json, libxml2 | wxwidgets, nlohmann-json, libxml2 |

iccDEV requires C++17 or higher to compile.


## Ubuntu GNU

```
export CXX=g++
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev/Build
sudo apt install -y libpng-dev libjpeg-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers libtiff-dev curl git make cmake clang clang-tools libxml2{-dev,} nlohmann-json3-dev build-essential
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
