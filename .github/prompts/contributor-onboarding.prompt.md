# Contributor Onboarding

Use this prompt when new to the iccDEV project. It covers repo structure,
build setup, PR workflow, and key concepts.

## Repository Map

```
iccDEV/
├── IccProfLib/           83K LOC — Core ICC profile library (C++17)
│   ├── IccProfile.h/cpp  — CIccProfile: read, write, validate profiles
│   ├── IccCmm.h/cpp      — CIccCmm: Color Management Module pipeline
│   ├── IccTagBasic.h/cpp  — 40+ CIccTag subclasses (all tag types)
│   ├── IccTagLut.h/cpp    — LUT/CLUT/curve tags
│   ├── IccMpeBasic.h/cpp  — Multi-Processing Elements (MPE)
│   ├── IccMpeCalc.h/cpp   — Calculator elements (operators, variables)
│   ├── IccIO.h/cpp        — Binary I/O abstraction
│   └── IccUtil.h/cpp      — Utility functions, fixed-point math
│
├── IccXML/               14K LOC — XML serialization extension
│   ├── IccLibXML/         — CIccProfileXml, CIccTagXml* (libxml2-based)
│   └── CmdLine/           — IccToXml and IccFromXml source
│
├── Tools/CmdLine/         9K LOC — 13 CLI tools
│   ├── IccDumpProfile/    — Profile structure dump + validation
│   ├── IccToXml/          — Binary ICC → XML conversion
│   ├── IccFromXml/        — XML → Binary ICC conversion
│   ├── IccRoundTrip/      — A2B/B2A round-trip evaluation
│   ├── IccApplyNamedCmm/  — Named color CMM application
│   ├── IccApplyProfiles/  — Multi-profile color transform
│   ├── IccApplySearch/    — Profile search/optimization
│   ├── IccApplyToLink/    — DeviceLink profile creation
│   ├── IccFromCube/       — .cube LUT → ICC profile
│   ├── IccTiffDump/       — TIFF ICC profile extraction
│   ├── IccJpegDump/       — JPEG ICC profile extraction
│   ├── IccPngDump/        — PNG ICC profile extraction
│   ├── IccSpecSepToTiff/  — Spectral separation → TIFF
│   └── IccV5DspObsToV4Dsp/ — v5 Display → v4 conversion
│
├── Build/Cmake/           — CMakeLists.txt + CMakePresets.json
├── Testing/               — 14 test profile directories + scripts
├── docs/                  — index.md, build.md, install.md
├── .github/
│   ├── workflows/         — 17 CI workflows
│   ├── scripts/           — sanitize-sed.sh, sanitize.ps1
│   ├── instructions/      — Auto-loaded copilot instructions
│   └── prompts/           — Reusable task prompts
└── README.md              — Quickstart (Homebrew, NPM, Docker, source)
```

## First Build

### Linux/macOS
```bash
# Install dependencies
sudo apt install -y libpng-dev libjpeg-dev libtiff-dev libxml2-dev \
  nlohmann-json3-dev cmake clang build-essential       # Ubuntu
# or: brew install libpng nlohmann-json libxml2 libtiff jpeg   # macOS

# Build
cd Build && cmake Cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
make -j"$(nproc)"

# Verify tools
ls Tools/*/icc* | head -5
echo "---"
LD_LIBRARY_PATH=IccProfLib:IccXML \
  Tools/IccDumpProfile/iccDumpProfile ../Testing/Display/sRGB_D65_MAT.icc
```

### Windows
```powershell
vcpkg integrate install && vcpkg install
cmake --preset vs2022-x64 -S Build/Cmake -B build
cmake --build build --config Release -- /m
```

## First Test Run

```bash
# Generate test profiles from XML definitions
cd Testing && ./CreateAllProfiles.sh

# Run the full test suite
./RunTests.sh

# Quick validation of a single profile
LD_LIBRARY_PATH=../Build/IccProfLib:../Build/IccXML \
  ../Build/Tools/IccDumpProfile/iccDumpProfile -v Display/sRGB_D65_MAT.icc ALL
```

## Key Concepts

### CIccProfile
The central class. Reads/writes binary ICC profiles. Tag access via
`FindTag(icSigXxx)` returns `CIccTag*` subclasses.

### CIccCmm (Color Management Module)
Pipeline for applying color transforms. Add profiles with `AddXform()`,
call `Begin()`, then `Apply()` per pixel.

### CIccTag Hierarchy
40+ tag type classes inheriting from `CIccTag`. Each implements:
- `Read(CIccIO*)` — parse from binary
- `Write(CIccIO*)` — serialize to binary
- `Describe(std::string&)` — human-readable dump
- `Validate(...)` — structural validation

### XML Round-Trip
`CIccProfileXml::SaveXml()` and `CIccProfileXml::LoadXml()` convert between
binary ICC and XML. This is the foundation for test profile generation
(XML → binary via `iccFromXml`).

### Multi-Processing Elements (MPE)
v5/iccMAX extension. Chain of elements (curves, matrices, CLUTs, calculators)
that define complex color transforms. Each `CIccMultiProcessElement` has
`Begin()` and `Apply()` methods.

## PR Workflow

1. **Fork and branch**: `git checkout -b feature/my-change`
2. **Build**: `cd Build && cmake Cmake ... && make -j"$(nproc)"`
3. **Test**: `cd Testing && ./CreateAllProfiles.sh && ./RunTests.sh`
4. **Sanitizer check**: rebuild with `-DENABLE_SANITIZERS=ON`, re-run tests
5. **Push and PR**: target `master`, clear description, reference any issues
6. **CI gates**: all 5 PR workflows must pass (action, unix, unix-sb, win, lint)
7. **Review**: requires Committer approval. Sign the CLA if first contribution.

## Coding Standards

- C++17, 2-space indent, K&R braces, `m_` prefix for members
- Return-value error handling (no C++ exceptions)
- ICC copyright + BSD 3-Clause header on all new files
- Match existing patterns in the file you're modifying
- Zero compiler warnings on all platforms

## Sanitizer Compatibility

When writing new code, ensure it works under:
- **ASan**: No heap-buffer-overflow, use-after-free, double-free
- **UBSan**: No signed overflow, enum out-of-range, NaN-to-int casts
- **TSan**: No data races (if threading is involved)
- **MSan**: No uninitialized memory reads

Common pitfalls:
- `new T[fileSize]` without bounds check → ASan heap-buffer-overflow
- `(int)floatValue` without NaN/Inf check → UBSan
- `strlen(fixedBuffer)` without null termination → ASan read overflow
- `offset + size` integer overflow → missed bounds check

## Key Documentation

| Document | Location |
|----------|----------|
| Build instructions | `docs/build.md` |
| Installation methods | `docs/install.md` |
| Project overview (all tools) | `docs/index.md` |
| ICC specification reference | `.github/instructions/icc-specification.instructions.md` |
| CMake options | `.github/copilot-instructions.md` → CMake Options table |
| Workflow governance | `.github/instructions/workflow-governance.instructions.md` |
| Security advisories | `github.com/InternationalColorConsortium/iccDEV/security/advisories` |
