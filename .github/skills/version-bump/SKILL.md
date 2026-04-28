---
name: version-bump
description: >
  Synchronize iccDEV version references after a release version bump.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# Version Bump

Use this skill when updating the iccDEV release version.

## Version Locations

| File | Symbol or content |
|------|-------------------|
| `IccProfLib/IccProfLibVer.h` | `ICCPROFLIBVER` |
| `IccXML/IccLibXML/IccLibXMLVer.h` | `ICCLIBXMLVER` |
| `IccXML/IccLibXML/IccLibXMLVer.h` | `ICCPROFLIBLIBXMLVER` |
| `Build/Cmake/CMakeLists.txt` | patch version variable and comment |
| `.github/copilot-instructions.md` | version or release note references, if present |
| `.github/instructions/build-system.instructions.md` | version references, if present |

Do not hardcode versions in `.in` templates that use CMake substitution.

## Workflow

1. Update all active version references.
2. Search for the old version across source, CMake, docs, XML, JSON, and CI.
3. Build and verify the version in generated binaries or package metadata.
4. Update release-facing docs only when they contain explicit version text.

## Verification

```bash
grep -rn 'OLD_VERSION' --include='*.cpp' --include='*.h' --include='*.txt' --include='*.cmake' --include='*.md' --include='*.xml' --include='*.json' --include='*.yml' --include='*.yaml' | grep -v '.git/'
```

## References

- `../../prompts/version-bump.prompt.md`
- `../../../docs/build.md`
- `../../../docs/install.md`
