# IccDEVCmm

`IccDEVCmm.dll` is the iccDEV Windows Image Color Management (ICM) color
management module. It exports the legacy Windows CMM entry points from
`Icm.h`, including `CMGetInfo`, `CMIsProfileValid`,
`CMCreateMultiProfileTransform`, `CMTranslateColors`, and
`CMDeleteTransform`.

The CMM identifier is `ICCD` (`0x49434344`).

## Build

Configure and build the Windows CMake tree with tools and tests enabled:

```powershell
cmake -S Build\Cmake -B build -DENABLE_TOOLS=ON -DENABLE_TESTS=ON -Wno-dev
cmake --build build --config Debug --target RefIccMAX
```

The Debug DLL is written to:

```text
build\Tools\IccDEVCmm\x64\Debug\IccDEVCmm.dll
```

## Local smoke test

Run the direct DLL smoke test without registering the CMM globally:

```powershell
Tools\Winnt\IccDEVCmm\tests\Run-IccDEVCmmSmoke.ps1 -RepoRoot (Get-Location).Path
```

The test loads `IccDEVCmm.dll`, verifies required exports, checks the `ICCD`
identity, validates `Testing\sRGB_v4_ICC_preference.icc`, creates an RGB
transform, translates one color, and deletes the transform.

The same coverage is registered with CTest as:

```powershell
ctest --test-dir build -C Debug -R "^iccdev\.windows-iccdevcmm-smoke$" --output-on-failure --no-tests=error
```

## Optional Windows registration

Most development and CI testing should use the direct smoke test above. To test
Windows ICM dispatch explicitly, register the CMM with Windows:

```cpp
RegisterCMM(NULL, 0x49434344, L"C:\\path\\to\\IccDEVCmm.dll");
```

Then exercise normal ICM APIs such as `CreateColorTransform`,
`TranslateColors`, and `DeleteColorTransform` with ICC profiles whose CMM
signature selects `ICCD`.

Unregister the CMM after system-level testing:

```cpp
UnregisterCMM(NULL, 0x49434344);
```

## Runtime dependencies

For Debug builds, the process loading `IccDEVCmm.dll` must be able to find:

- `IccProfLib2d.dll`
- MSVC Debug CRT DLLs
- `mscms.dll` from Windows

CTest configures the required `PATH` entries automatically through
`Build\Cmake\Testing\WindowsRuntimePaths.cmake`.
