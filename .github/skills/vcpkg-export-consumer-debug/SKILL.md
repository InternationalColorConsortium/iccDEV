---
name: vcpkg-export-consumer-debug
description: >
  Debug iccDEV vcpkg, install/export, uninstall, and packaged consumer
  failures, especially Windows static CRT and path quoting regressions.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
  - shell(gh:*)
---

# vcpkg Export Consumer Debug

Use this skill when a review or workflow log mentions `ci-vcpkg-ports`,
`ci-shared-exports`, `examples/hello-iccdev`, install manifests, uninstall,
or packaged CMake consumers.

## Staleness Check

1. Compare the tested ref with the current PR head:

```bash
gh pr view <PR> --repo InternationalColorConsortium/iccDEV \
  --json headRefOid,headRefName,baseRefName,statusCheckRollup
git rev-parse HEAD
```

2. If the review SHA is not current, do not discard the finding. Re-check the
   current head for the exact source pattern or failing workflow path.
3. Treat the review as still actionable when the same source pattern remains
   present on the current head.

## Common Windows Failures

### Static vcpkg Consumer Uses the Wrong CRT

Signal:

```text
MT_StaticRelease from Icc*2-static.lib conflicts with MD_DynamicRelease
LNK1169
```

Cause: the `x64-windows-static` packaged libraries were built with `/MT`, but
the consumer executable used the default `/MD` runtime.

Fix: when configuring the packaged Windows static consumer, pass the matching
runtime explicitly:

```powershell
cmake -S "examples\hello-iccdev" -B $exampleBuild `
  -G "Visual Studio 17 2022" `
  -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" `
  -DVCPKG_MANIFEST_MODE=OFF `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static `
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
```

### Source Uninstall Breaks on CMake Path Spaces

Signal:

```text
cmake --build ... --target uninstall --config Release
C:/Program Files/CMake/bin/cmake.exe is split at the space
```

Cause: generated uninstall scripts used unquoted `@CMAKE_COMMAND@`.

Fix:

```cmake
execute_process(
    COMMAND "@CMAKE_COMMAND@" -E remove "$ENV{DESTDIR}${file}"
)
```

## Validation

Run local checks before pushing:

```bash
git diff --check -- \
  Build/Cmake/RefIccMAXUninstall.cmake.in \
  .github/workflows/ci-shared-exports.yml \
  .github/skills/vcpkg-export-consumer-debug/SKILL.md

python3 -c "import yaml; yaml.safe_load(open('.github/workflows/ci-shared-exports.yml')); print('YAML parse OK')"
actionlint -no-color .github/workflows/ci-shared-exports.yml
file Build/Cmake/RefIccMAXUninstall.cmake.in .github/skills/vcpkg-export-consumer-debug/SKILL.md
```

Final proof for these failures requires a GitHub-hosted Windows runner. Monitor
the source install/uninstall job and the packaged `hello-iccdev` consumer job
after pushing.

## References

- `../../workflows/ci-shared-exports.yml`
- `../../../Build/Cmake/RefIccMAXUninstall.cmake.in`
- `../../../examples/hello-iccdev/CMakeLists.txt`
- `../../../ports/iccdev/portfile.cmake`
