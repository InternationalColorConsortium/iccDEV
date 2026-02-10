# Building iccDEV

iccDEV requires C++17 or higher to compile.

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
export CXX=clang++
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev/Build
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers curl git make cmake clang{,-tools} libxml2{,-dev} nlohmann-json3-dev build-essential
cmake Cmake
make -j"$(nproc)"

```

## macOS

```
export CXX=clang++
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg-turbo
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

### Reporting Build Issues

Before submitting a PR for build failures:

1. Test local environment:
   - Build in a clean container/VM
   - Open an Issue first
   - Utilize Testing Scripts
     - [Unix](https://github.com/InternationalColorConsortium/iccDEV/blob/research/contrib/HelperScripts/unix-issue-template.sh)
       ```
       /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/InternationalColorConsortium/iccDEV/refs/heads/research/contrib/HelperScripts/unix-pr-review.sh)"
       ```
     - [Windows](https://raw.githubusercontent.com/InternationalColorConsortium/iccDEV/refs/heads/research/contrib/HelperScripts/windows-pr-review.ps1)
       ```
       iex (iwr -Uri "https://raw.githubusercontent.com/InternationalColorConsortium/iccDEV/refs/heads/research/contrib/HelperScripts/windows-issue-template.ps1").Content
       ```

2. Open or Update an Issue with the script output.

3. [CI](https://github.com/InternationalColorConsortium/iccDEV/actions/workflows/ci-latest-release.yml) provides the latest builds for macOS, Windows & Linux.
