# IccDEVCmm

`IccDEVCmm.dll` is the iccDEV Windows Image Color Management (ICM) color
management module. It exports the legacy Windows CMM entry points from
`Icm.h`, including `CMGetInfo`, `CMIsProfileValid`,
`CMCreateMultiProfileTransform`, `CMTranslateColors`, and
`CMDeleteTransform`.

The CMM identifier is `ICCD` (`0x49434344`).

The current global transform-handle table has no synchronization. Concurrent
creation, translation, and deletion are not validated for this DLL; evaluate
system ICM use in controlled single-threaded tests until that lifecycle is
hardened.

## Build

Configure and build the Windows CMake tree with tools and tests enabled:

```powershell
$env:VCPKG_ROOT = 'C:\path\to\vcpkg'
cmake --preset vs2022-x64 -S Build\Cmake -B build -DENABLE_TOOLS=ON -DENABLE_TESTS=ON
cmake --build build --config Debug --target RefIccMAX iccDEVCmmSmokeTest
```

The `vs2022-clangcl-x64` preset also builds this target. The unified Windows
runtime layout writes the Debug DLL to:

```text
build\bin\Debug\IccDEVCmm.dll
```

The manual `ci-latest-release` workflow builds and smoke-tests the Release DLL.
Its Windows archive places `IccDEVCmm.dll` at the bundle root for runtime
loading and under `IccDEVCmm/` with this README.

## Local smoke test

Run the direct DLL smoke test without registering the CMM globally:

```powershell
Tools\Winnt\IccDEVCmm\tests\Run-IccDEVCmmSmoke.ps1 `
  -RepoRoot (Get-Location).Path -BuildDirectory build -Configuration Debug
```

`-BuildDirectory` accepts an absolute path or a path relative to `-RepoRoot`.
The helper also accepts `-DllPath` for a copied DLL. Its default build directory
is `msvc` for existing callers. It finds the CMake-built smoke executable and
the vcpkg runtime directory recorded in the build cache; an older MSVC build
without the CMake test target can still compile the standalone smoke executable.

The test loads `IccDEVCmm.dll`, verifies required exports, checks the `ICCD`
identity, validates `Testing\sRGB_v4_ICC_preference.icc`, creates an RGB
transform, translates one color, and deletes the transform. When the
named-color fixture is present, it also checks name/index conversion and
invalid-input handling.

The same coverage is registered with CTest as:

```powershell
ctest --test-dir build -C Debug -R "^iccdev\.windows-iccdevcmm-smoke$" --output-on-failure --no-tests=error
```

## CMake runtime install and uninstall

Build all Release runtime targets before installing the `runtime` component.
Use an isolated prefix so the install manifest can be checked before removal:

```powershell
cmake --build build --config Release
$prefix = Join-Path (Get-Location).Path 'out\iccdev-cmm-install'
cmake --install build --config Release --component runtime --prefix $prefix
```

The install includes `bin\IccDEVCmm.dll`, `IccProfLib2.dll`, the required
runtime DLLs, this README, and `share\doc\RefIccMAX\TOOL-CATALOG.md`. To smoke
the installed DLLs without registering the CMM, temporarily put the built test
executable beside them:

```powershell
$smoke = Join-Path $prefix 'bin\iccDEVCmmSmokeTest.exe'
Copy-Item -LiteralPath 'build\bin\Release\iccDEVCmmSmokeTest.exe' -Destination $smoke
Tools\Winnt\IccDEVCmm\tests\Run-IccDEVCmmSmoke.ps1 `
  -RepoRoot (Get-Location).Path -BuildDirectory build -Configuration Release `
  -DllPath (Join-Path $prefix 'bin\IccDEVCmm.dll')
Remove-Item -LiteralPath $smoke
```

The smoke executable is a test artifact and is not part of the runtime install.
Before uninstalling, reinstall to the same prefix to refresh
`build\install_manifest_runtime.txt`. CPack or another install can replace that
build-tree manifest. Check that every manifest path is within the intended
prefix, then remove only the recorded runtime files:

```powershell
cmake --install build --config Release --component runtime --prefix $prefix
$prefix = [IO.Path]::GetFullPath($prefix).TrimEnd('\')
$installed = @(Get-Content -LiteralPath 'build\install_manifest_runtime.txt' |
  ForEach-Object { [IO.Path]::GetFullPath($_.Replace('/', '\')) })
if (-not $installed.Count -or @($installed | Where-Object {
  -not $_.StartsWith("$prefix\", [StringComparison]::OrdinalIgnoreCase)
}).Count) { throw 'Runtime install manifest is outside the intended prefix' }
cmake --build build --config Release --target uninstall-runtime
```

`uninstall-runtime` removes manifest-listed files; empty directories may remain.
It does not change Windows CMM registration.

## File-stage use and removal

The Windows release archive places `IccDEVCmm.dll` beside `IccProfLib2.dll`
and the other runtime DLLs at the archive root. Use that root DLL path for
direct loading or registration so its dependencies are beside it. A separate
`IccDEVCmm/` folder contains another copy of the CMM DLL and this README; that
copy needs the same dependencies on the process DLL search path.

For an isolated local smoke test, stage the release DLLs with the CMake-built
test executable, then pass the staged CMM path:

```powershell
$bundle = 'C:\path\to\expanded\iccdev-windows-msvc'
$stage = Join-Path $env:TEMP 'iccdev-cmm-file-stage'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item -Path (Join-Path $bundle '*.dll') -Destination $stage
Copy-Item -LiteralPath 'build\bin\Release\iccDEVCmmSmokeTest.exe' -Destination $stage
Tools\Winnt\IccDEVCmm\tests\Run-IccDEVCmmSmoke.ps1 `
  -RepoRoot (Get-Location).Path -BuildDirectory build -Configuration Release `
  -DllPath (Join-Path $stage 'IccDEVCmm.dll')
```

The smoke executable must be beside the staged DLL to keep the test's runtime
search confined to the stage and the normal Windows DLL directories. Remove the
files you staged after testing:

```powershell
Get-ChildItem -LiteralPath $stage -File | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
Remove-Item -LiteralPath $stage
```

This checks direct `LoadLibraryW` use of the CMM. System registration and ICM
host dispatch require the separate check below.

## Optional Windows registration and host selection

Most development and CI testing should use the direct smoke test above. To test
Windows ICM dispatch explicitly, first copy the archive-root DLLs together to
a stable directory and keep them there until after unregistering. The process
calling `RegisterCMMW` and every later ICM host process must be able to resolve
`IccProfLib2.dll`, `z.dll`, and other runtime dependencies. CMake install and
CPack do not configure a system-wide DLL search path. Adding the package `bin`
directory with `SetDllDirectoryW` affects only the calling process.

Run a program with administrator privileges that registers the CMM using the
absolute path to that directory's `IccDEVCmm.dll`:

```cpp
if (!RegisterCMMW(NULL, 0x49434344,
                  L"C:\\path\\to\\stable-directory\\IccDEVCmm.dll")) {
    // Report GetLastError() and stop.
}
```

Then call `SelectCMM(0x49434344)` and exercise normal ICM APIs such as
`CreateMultiProfileTransform`, `GetCMMInfo`, `TranslateColors`, and
`DeleteColorTransform`. Local Windows QA confirmed this path with the package
`bin` directory on the test process's DLL search path. A clean child process
without that directory failed `SelectCMM` with `ERROR_INVALID_CMM` (2010), so
registration alone does not make the archive usable by arbitrary host processes.

Run the unregister call with administrator privileges before deleting the DLL
or its dependencies:

```cpp
if (!UnregisterCMMW(NULL, 0x49434344)) {
    // Report GetLastError() and stop.
}
```

Then remove only files you copied for this test. The direct smoke helper above
does not register or unregister a CMM.

## Runtime dependencies

The process loading `IccDEVCmm.dll` must be able to find `IccProfLib2.dll`
(or `IccProfLib2d.dll` for Debug), its vcpkg runtime DLLs, the matching MSVC
runtime, and Windows `mscms.dll`. Debug builds also require the MSVC Debug CRT.

CTest configures the required `PATH` entries automatically through
`Build\Cmake\Testing\WindowsRuntimePaths.cmake`.
