# ci-shared-exports Log Audit - 2026-04-30

## Source Run

- Run: `25146690868`
- Workflow: `ci-shared-exports`
- Event: `push`
- Branch: `master`
- SHA: `fede75adad6f1011dbb9254c42ccf35a53c43ab5`
- Conclusion: `success`

## Hidden Diagnostics Found

| Area | Runtime signal | Fix or gate |
|------|----------------|-------------|
| Install manifest | `IccProfLibVer.h` installed twice | Install only the generated version header. |
| Uninstall | `-- File ".../IccProfLibVer.h" does not exist.` | Capture install/uninstall logs and fail on missing-file diagnostics. |
| Housekeeping | Same generated header path uninstalled twice | Fail on duplicate `install_manifest.txt` entries. |
| Linux cache | apt `lock` and `partial` paths could not be archived | Cache only user-writable ccache paths. |
| macOS dependencies | Homebrew emitted already-installed annotations, including one `##[error]` | Check `brew list --formula` before installing. |
| Windows CMake | Hosted runner did not match Community-only MSBuild path probe | Probe Enterprise, Professional, Community, and BuildTools paths. |
| Windows vcpkg | vcpkg ignored mismatched `VCPKG_ROOT` | Export the workflow-owned vcpkg root. |
| Windows vcpkg CRT | Static triplet linked `/MD` and `/MDd` | Pass `CMAKE_MSVC_RUNTIME_LIBRARY` from `VCPKG_CRT_LINKAGE`. |
| Windows smoke | `hello-iccdev` ran only for `Debug` | Run the smoke test for all build types with the correct DLL suffix. |
| Artifact download | `digest-mismatch: error` plus Node `DEP0005` warning | Monitor action behavior; no source fix identified in this pass. |

## Audit Command

```bash
RUN_ID=25146690868
REPO=InternationalColorConsortium/iccDEV
OUT="/tmp/iccdev-run-${RUN_ID}-logs.zip"
DIR="/tmp/iccdev-run-${RUN_ID}-logs"
rm -rf "$DIR" "$OUT"
gh api "/repos/${REPO}/actions/runs/${RUN_ID}/logs" > "$OUT"
mkdir -p "$DIR"
unzip -q "$OUT" -d "$DIR"
rg -n '##\[error\]|##\[warning\]|::error|::warning|CMake Warning|File ".+" does not exist|Cannot open: Permission denied|post-build check|DEP0005|digest-mismatch' "$DIR" | grep -v $'\033\[36;1m'
```

## Local Validation Used

```bash
python3 -c "import yaml; [yaml.safe_load(open(p)) for p in ['.github/workflows/ci-shared-exports.yml','.github/workflows/ci-pr-lint.yml']]; print('YAML parse OK')"
git diff --check -- .github/workflows/ci-shared-exports.yml .github/workflows/ci-pr-lint.yml Build/Cmake/CMakeLists.txt Build/Cmake/IccProfLib/CMakeLists.txt ports/iccdev/portfile.cmake
actionlint -no-color -ignore 'SC2129|SC2034|SC2221|SC2222|SC2038|SC2155|SC2086' .github/workflows/ci-shared-exports.yml .github/workflows/ci-pr-lint.yml
python3 .github/scripts/audit-workflow-permissions.py --workflows-dir .github/workflows --format shell
.github/scripts/run-codeql-local.sh --custom-only
```

The local Linux install/uninstall repro also verified that duplicate manifest
entries were gone and no missing-file uninstall diagnostic remained.

## Follow-Up

The Windows vcpkg CRT fix, Windows vcpkg root export, and all-build-type Windows
smoke changes require GitHub-hosted Windows runners for final proof.
