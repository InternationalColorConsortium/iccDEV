# Version Bump Workflow -- iccDEV (RefIccMAX)

## When to Use

Use this prompt when bumping the iccDEV version number (e.g., 2.3.1.7 -> 2.3.1.8).

## Version Location Map (6 locations, 5 files)

| # | File | Symbol | Type |
|---|------|--------|------|
| 1 | `IccProfLib/IccProfLibVer.h` | `ICCPROFLIBVER` | Hardcoded string |
| 2 | `IccXML/IccLibXML/IccLibXMLVer.h` | `ICCLIBXMLVER` | Hardcoded string |
| 3 | `IccXML/IccLibXML/IccLibXMLVer.h` | `ICCPROFLIBLIBXMLVER` | Hardcoded string |
| 4 | `Build/Cmake/CMakeLists.txt` | `PATCH_VERSION` + comment | CMake variable |
| 5 | `.github/copilot-instructions.md` | Version string in header | Documentation |
| 6 | `.github/instructions/build-system.instructions.md` | Version string | Documentation |

## Files That Do NOT Need Updating

These `.in` template files use CMake `@VAR@` substitution and inherit the
version from `CMakeLists.txt` at configure time:

- `IccProfLib/IccProfLibVer.h.in` -- uses `@ICCPROFLIB_VERSION_STRING@`
- `IccXML/IccLibXML/IccLibXMLVer.h.in` -- uses `@ICCLIBXML_VERSION_STRING@`
- `Build/Cmake/RefIccMAXConfig.cmake.in` -- uses `@REFICCMAX_PATCH_VERSION@` etc.
- `Build/Cmake/RefIccMAXUninstall.cmake.in` -- no version references

## Step-by-Step Procedure

### 1. Update version strings

Replace OLD_VERSION with NEW_VERSION in all 6 locations listed above.

For CMakeLists.txt, update both the PATCH_VERSION number and the comment:
```
# Version components (4-part version: X.Y.Z.NEW)
set(${PROJECT_UP_NAME}_PATCH_VERSION NEW)
```

### 2. Verify no stale references

```bash
grep -rn 'OLD_VERSION' \
  --include='*.cpp' --include='*.h' --include='*.txt' \
  --include='*.cmake' --include='*.md' --include='*.xml' \
  --include='*.json' --include='*.yml' --include='*.yaml' \
  | grep -v '.git/'
```

Must return 0 hits. If any remain, update them.

### 3. Verify .in templates use substitution (not hardcoded)

```bash
grep -n 'OLD_VERSION' IccProfLib/IccProfLibVer.h.in \
  IccXML/IccLibXML/IccLibXMLVer.h.in \
  Build/Cmake/RefIccMAXConfig.cmake.in
```

Must return 0 hits. These files should only contain `@VAR@` placeholders.

### 4. Build and verify

```bash
mkdir -p Build && cd Build
cmake ../Build/Cmake -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Verify version in built library
strings IccProfLib/libIccProfLib2.so.* | grep 'NEW_VERSION'
```

### 5. Commit

```bash
git add -A
git commit -m "Update to Version X.Y.Z.W"
git push origin BRANCH
```

## Version Numbering Convention

iccDEV uses 4-part versioning: `MAJOR.MINOR.MICRO.PATCH`

- **MAJOR** (2): ICC specification generation (v2 = ICC.2/iccMAX era)
- **MINOR** (3): Significant feature additions
- **MICRO** (1): Minor feature or structural changes
- **PATCH** (N): Bug fixes, security patches, maintenance

The PATCH number increments for each release containing bug fixes or
security hardening. Security fixes from CFL patches, CI warning fixes,
and upstream issue resolutions all warrant a PATCH bump.

## Downstream Notice

If downstream projects vendor or package iccDEV, mention the version bump in the
release notes and point them to the updated tag or commit. Keep downstream
repository-specific sync steps out of upstream iccDEV documentation.
