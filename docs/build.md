# Building iccDEV

## Quickstart installation: 

`brew install iccdev` or `npm install iccdev`

## Dependencies
| Dependency         | Ubuntu (apt)                     | macOS (brew)          | Windows (vcpkg)       |
|--------------------|----------------------------------|------------------------|------------------------|
| Build Tool         | cmake, make, clang               | cmake, clang           | cmake                  |
| Image Libraries    | libpng-dev, libjpeg-dev, libtiff6| libpng, jpeg, libtiff  | libpng, libjpeg-turbo, libtiff |
| GUI & Config       | libwxgtk3.2-dev, nlohmann-json3-dev, libxml2-dev | wxwidgets, nlohmann-json, libxml2 | wxwidgets, nlohmann-json, libxml2 |


## Ubuntu GNU

```
export CXX=g++
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev/Build
sudo apt install -y libpng-dev libjpeg-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers libtiff6 curl git make cmake clang clang-tools libxml2{-dev,} nlohmann-json3-dev build-essential
cmake Cmake
make -j$(nproc)
```

## macOS Clang

```
export CXX=clang++
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev
cmake -G "Xcode" Build/Cmake
xcodebuild -project RefIccMAX.xcodeproj
open RefIccMAX.xcodeproj
```

## Windows MSVC

```
git clone https://github.com/InternationalColorConsortium/iccdev.git
cd iccdev
vcpkg integrate install
vcpkg install
cmake --preset vs2022-x64 -B . -S Build/Cmake
cmake --build . -- /m /maxcpucount
devenv RefIccMAX.sln
```

---
