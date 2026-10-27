---
name: version-bump
description: >
  Synchronize iccDEV version references after a release version bump.
allowed-tools:
  - bash
  - read
  - write
  - grep
  - glob
  - shell(git:*)
---

# Version Bump

Use this skill when updating the iccDEV release version.

## Version Locations

| File | Symbol or content |
|------|-------------------|
| `Build/Cmake/CMakeLists.txt` | `REFICCMAX_*_VERSION` variables |
| `IccProfLib/IccProfLibVer.h` | `ICCPROFLIBVER` |
| `vcpkg.json`, `Build/Cmake/vcpkg.json` | package version |
| `ports/iccdev/vcpkg.json` | package version and reset `port-version` |
| `examples/hello-iccdev/vcpkg.json` | `iccdev` override version |
| `.github/workflows/ci-vcpkg-ports.yml` | generated manifest override version |
| `.github/ci/doxygen/Doxyfile` | `PROJECT_NUMBER` |
| `.github/instructions/build-system.instructions.md` | project version |
| `examples/hello-iccdev/README.md` | versioned sample output |

Do not hardcode versions in generated XML, JSON, or Connect version headers.
`IccXML/IccLibXML/IccLibXMLVer.h.in`,
`IccJSON/IccLibJSON/IccLibJSONVer.h.in`, and
`IccConnect/IccLibConnect/IccLibConnectVer.h.in` use CMake substitution and
inherit the version from `Build/Cmake/CMakeLists.txt`.

## Workflow

1. Update all active version references.
2. Search all tracked files for the old version and review every match. Preserve
   clearly dated historical plans and captured output as historical evidence.
3. Build and verify the version in generated binaries or package metadata.
4. Update release-facing docs only when they contain explicit version text.
5. Commit the release references, then update `REF` and `SHA512` together in
   `ports/iccdev/portfile.cmake` to pin that immutable commit. Verify the
   downloaded archive checksum and keep the source-pin refresh in a second
   commit so the pinned commit does not refer to itself.

## Verification

```bash
git grep -n -F 'OLD_VERSION'
```

## References

- `../../prompts/version-bump.prompt.md`
- `../../../docs/build.md`
- `../../../docs/install.md`
