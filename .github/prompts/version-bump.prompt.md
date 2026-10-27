# Version Bump Workflow -- iccDEV (RefIccMAX)

## When to Use

Use this prompt when bumping the iccDEV version number (`OLD_VERSION` -> `NEW_VERSION`).

## Version Location Map

| File | Symbol or content | Type |
|------|-------------------|------|
| `Build/Cmake/CMakeLists.txt` | `REFICCMAX_*_VERSION` variables | CMake variables |
| `IccProfLib/IccProfLibVer.h` | `ICCPROFLIBVER`, when intentionally maintained as a source header | Source header |
| `vcpkg.json`, `Build/Cmake/vcpkg.json` | Package version | vcpkg manifests |
| `ports/iccdev/vcpkg.json` | Package version; reset `port-version` | Overlay port manifest |
| `examples/hello-iccdev/vcpkg.json` | `iccdev` override version | Example manifest |
| `.github/workflows/ci-vcpkg-ports.yml` | Generated manifest override version | CI validation |
| `.github/ci/doxygen/Doxyfile` | `PROJECT_NUMBER` | Documentation metadata |
| `.github/instructions/build-system.instructions.md` | Project version | Agent documentation |
| `examples/hello-iccdev/README.md` | Versioned sample output | User documentation |

After committing these release references, update `REF` and `SHA512` in
`ports/iccdev/portfile.cmake` together so non-local vcpkg consumers resolve the
new version commit. Verify the downloaded archive checksum before recording it.

## Files That Do NOT Need Updating

These `.in` template files use CMake `@VAR@` substitution and inherit the
version from `CMakeLists.txt` at configure time:

- `IccProfLib/IccProfLibVer.h.in` -- uses `@ICCPROFLIB_VERSION_STRING@`
- `IccXML/IccLibXML/IccLibXMLVer.h.in` -- uses `@ICCLIBXML_VERSION_STRING@`
- `IccJSON/IccLibJSON/IccLibJSONVer.h.in` -- uses `@ICCLIBJSON_VERSION_STRING@`
- `IccConnect/IccLibConnect/IccLibConnectVer.h.in` -- uses `@ICCLIBCONNECT_VERSION_STRING@`
- `Build/Cmake/RefIccMAXConfig.cmake.in` -- uses `@REFICCMAX_PATCH_VERSION@` etc.
- `Build/Cmake/RefIccMAXUninstall.cmake.in` -- no version references

## Step-by-Step Procedure

### 1. Update version strings

Replace OLD_VERSION with NEW_VERSION in the active locations listed above.
Review every tracked match instead of assuming this table is exhaustive. Leave
clearly dated historical plans and captured command output unchanged.

For CMakeLists.txt, update both the PATCH_VERSION number and the comment:
```
# Version components (4-part version: X.Y.Z.NEW)
set(${PROJECT_UP_NAME}_PATCH_VERSION NEW)
```

### 2. Verify no stale references

```bash
git grep -n -F 'OLD_VERSION'
```

Update active references. Any remaining hit must be reviewed and documented as
historical evidence rather than silently ignored.

### 3. Verify .in templates use substitution (not hardcoded)

```bash
grep -n 'OLD_VERSION' IccProfLib/IccProfLibVer.h.in \
  IccXML/IccLibXML/IccLibXMLVer.h.in \
  IccJSON/IccLibJSON/IccLibJSONVer.h.in \
  IccConnect/IccLibConnect/IccLibConnectVer.h.in \
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

### 5. Commit the version references

Commit the version references before calculating the overlay source pin:

```bash
git add <version-reference-files>
git commit -m "Update to Version X.Y.Z.W"
```

### 6. Refresh the overlay source pin

Calculate the SHA-512 of the version commit's GitHub source archive, verify the
download succeeded, and update both `REF` and `SHA512` in
`ports/iccdev/portfile.cmake`. The separate pin commit keeps the source archive
immutable and prevents the pinned commit from referring to itself.

### 7. Commit the source pin and push

```bash
git add ports/iccdev/portfile.cmake
git commit -m "Sync vcpkg port for version X.Y.Z.W"
git push origin BRANCH
```

## Version Numbering Convention

iccDEV uses 4-part versioning: `MAJOR.MINOR.MICRO.PATCH`

- **MAJOR** (2): ICC specification generation (v2 = ICC.2/iccMAX era)
- **MINOR** (3): Significant feature additions
- **MICRO** (2): Minor feature or structural changes
- **PATCH** (N): Bug fixes, security patches, maintenance

The PATCH number increments for each release containing bug fixes or
security hardening. Security fixes from CFL patches, CI warning fixes,
and upstream issue resolutions all warrant a PATCH bump.

## Downstream Notice

If downstream projects vendor or package iccDEV, mention the version bump in the
release notes and point them to the updated tag or commit. Keep downstream
repository-specific sync steps out of upstream iccDEV documentation.
