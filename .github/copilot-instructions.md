# Copilot Instructions for iccDEV

## Project Overview
iccDEV is an open source set of libraries and tools for interaction, manipulation, and application of ICC-based color management profiles. The project is maintained by the International Color Consortium (ICC) and uses the BSD 3-Clause License.

## Code Style Guidelines

### Indentation and Formatting
- Use **2 space indentation**, no tabs
- Use **K&R brace style**
- Aim for zero compiler warnings and static analysis warnings across all platforms

### Naming Conventions
- Prefix class/struct members with `m_` (e.g., `m_variableName`)
- No uniform convention for general variables - match nearby code
- Use descriptive names

### Code Organization
- Multiple classes per file, grouped by functionality
- Use header guards in all header files
- Minimize pollution of the `std` namespace
- Const correctness: make inputs const when possible, class functions const when appropriate

### Language Features
- **Error handling**: Use manual return values, NOT exceptions (this is the existing pattern)
- **Containers**: Prefer STL containers, but the codebase historically uses raw pointers
- **Templates**: Currently minimal use. Ensure new templates are readable
- **Namespaces**: Not currently using namespaces (work in progress)
- **C++ Standard**: Requires C++17 or higher

### Comments
- No consistent style exists - match nearby code
- Don't over-comment obvious code

## Build System

### Primary Build Tool
- CMake-based build system located in `Build/Cmake/`
- Supports multiple platforms: Linux, macOS, Windows

## Required libraries

| Platform          | Libraries                                                                 |
|-------------------|---------------------------------------------------------------------------|
| **macOS**         | libpng, jpeg, libtiff, libxml2, wxwidgets, nlohmann-json                  |
| **Windows**       | libpng, libjpeg-turbo, libtiff, libxml2, wxwidgets, nlohmann-json         |
| **Linux (Ubuntu)** | libpng-dev, libjpeg-dev, libtiff-dev, libxml2-dev, wxwidgets*, nlohmann-json |

\* **Note:** On Ubuntu, `wxwidgets` is installed via distribution-specific development packages  
(e.g. `libwxgtk3.2-dev`). Refer to the `apt install` command below for the exact package names.

### Build Commands
#### Ubuntu GNU

```
export CXX=g++
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev/Build
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libwxgtk3.2-dev libwxgtk-{media,webview}3.2-dev wx-common wx3.2-headers curl git make cmake clang{,-tools} libxml2{,-dev} nlohmann-json3-dev build-essential
cmake Cmake
make -j"$(nproc)"

```

#### macOS Clang

```
export CXX=clang++
brew install libpng nlohmann-json libxml2 wxwidgets libtiff jpeg
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
cmake -G "Xcode" Build/Cmake
xcodebuild -project RefIccMAX.xcodeproj
open RefIccMAX.xcodeproj
```

#### Windows MSVC

```
git clone https://github.com/InternationalColorConsortium/iccdev.git iccdev
cd iccdev
vcpkg integrate install
vcpkg install
cmake --preset vs2022-x64 -B . -S Build/Cmake
cmake --build . -- /m /maxcpucount
```

## Testing
- Test scripts located in `Testing/` directory
- Run tests using `Testing/RunTests.sh` (Unix) or `Testing/RunTests.bat` (Windows)
- Profile creation: `Testing/CreateAllProfiles.sh` (Unix) or `Testing/CreateAllProfiles.bat` (Windows)
- Test various ICC profile operations and transformations

## Security Practices
- Report security issues via GitHub Security Advisory
- All new source files must begin with ICC Copyright notice and BSD 3-Clause License
- Follow secure coding practices
- Validate all inputs, especially when processing ICC profiles

## Legal and Licensing

### Copyright Notice
All new source files must begin with the ICC Copyright notice and include or reference the BSD 3-Clause "New" or "Revised" License.

### Contributor License Agreement
Contributors must sign the ICC Contributor License Agreement (CLA) before code can be merged.

## Pull Request Process
1. Create a topic branch: `feature/<your-feature>` or `bugfix/<your-fix>`
2. Make focused changes related to the topic
3. Ensure code compiles and tests pass
4. Follow existing code style and conventions
5. Create pull request with clear description
6. Address review feedback
7. Requires Committer approval before merge

## Project Structure
- `IccProfLib/` - Core ICC profile library
- `IccXML/` - XML handling for ICC profiles
- `Tools/` - Command-line tools for profile manipulation
- `Build/` - Build system files (CMake, Xcode)
- `Testing/` - Test files and scripts
- `docs/` - Documentation

## Key Considerations
- This is an older codebase - consistency is valued over perfection
- Match existing patterns when adding new code
- Focus on cross-platform compatibility (Linux, macOS, Windows)
- Performance matters for profile processing
- Maintain compatibility with ICC specifications