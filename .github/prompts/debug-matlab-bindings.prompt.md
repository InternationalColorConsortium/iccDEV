# Debug MATLAB MEX Bindings

Use this prompt when the iccDEV MATLAB gateway fails to build, load, or produce
correct profile or transform results.

1. Read `docs/matlab-bindings.md`,
   `.github/instructions/matlab-mex.instructions.md`, and
   `.github/skills/matlab-bindings-test/SKILL.md`.
2. Report the MATLAB release, `mexext`, selected C++ compiler, current branch,
   and dirty files before changing anything. Confirm the selected compiler is
   Microsoft Visual C++ 2022 before using the documented VS 2022 generator.
   Derive the repository root with `git rev-parse --show-toplevel` and discover
   MATLAB, Visual Studio, and vcpkg as documented in
   `docs/matlab-bindings.md`; use `repo\msvc` as the Windows build root and do
   not copy local absolute paths into fixes. Windows commands must be
   PowerShell. Use MATLAB `setenv` inside MATLAB; never present shell `export`
   as MATLAB or Windows syntax.
3. Inspect the selected CMake build's configuration, library configuration,
   `ICC_USE_ZLIB`, and vcpkg triplet. Confirm the minimum Release build includes
   `IccProfLib2-static`, `iccProfilePlot`, and `iccPawgReport`, then verify
   `$Build\bin\Release\iccPawgReport.exe` exists before running
   `run_local_qa()`.
4. Reproduce with the smallest exact command:

   ```powershell
   $Repo = (git rev-parse --show-toplevel).Trim()
   $Build = Join-Path $Repo 'msvc'
   cmake --build $Build --config Release `
     --target IccProfLib2-static iccProfilePlot iccPawgReport `
       iccPawgQ1QualityContractTest -- /m
   foreach ($RequiredTool in @(
     (Join-Path $Build 'bin\Release\iccProfilePlot.exe'),
     (Join-Path $Build 'bin\Release\iccPawgReport.exe')
   )) {
     if (-not (Test-Path $RequiredTool -PathType Leaf)) {
       throw "Required MATLAB QA tool was not built: $RequiredTool"
     }
   }
   $env:ICCDEV_BUILD_DIR = $Build
   $env:ICCDEV_REPO_ROOT = $Repo
   & $MatlabExe -batch "repo_root=getenv('ICCDEV_REPO_ROOT'); cd(repo_root); addpath(fullfile(repo_root, 'matlab')); build_mex()"
   ```

5. Classify the failure:

   - compiler configuration
   - Release/Debug mismatch
   - missing static dependency or runtime DLL
   - package/private path misuse
   - `test_plot()` started before `iccProfilePlot` was built
   - missing or stale `iccProfilePlot` executable
   - missing or stale `iccPawgReport` executable, missing structured Q1 metrics,
     or malformed Q1 JSON
   - PAWG Q1 sample-budget rejection or model/metric disagreement
   - invalid visualization JSON or MATLAB graph rendering
   - stale loaded MEX
   - missing generated profiles
   - CMM input shape or lifecycle error
   - issue #1811 fixture normalization or float-window mismatch
   - disagreement between the MATLAB model and native `CheckLuminance`
   - issue #1475 source-table parsing or legacy/registry D50 disagreement
   - disagreement between the MATLAB issue #1475 model and
     `iccdev.colorimetry-methods`
   - Docker daemon, image, mount, or output-contract error

6. For luminance normalization failures, separate the two validation layers:

   ```matlab
   test_luminance_normalization();
   run('matlab/examples/luminance_normalization.m');
   ```

   ```powershell
   $NativeBuildArgs = @(
     '--build', $Build
     '--config', 'Release'
     '--target', 'iccLuminanceNormalizationTest'
   )
   cmake @NativeBuildArgs
   $NativeTestArgs = @(
     '--test-dir', $Build
     '-C', 'Release'
     '-R', '^iccdev\.luminance-normalization$'
     '--output-on-failure'
     '--no-tests=error'
   )
   ctest @NativeTestArgs
   ```

   The MATLAB layer reads the XML fixtures and models single-precision
   arithmetic. The native layer is authoritative for
   `CIccInfo::CheckLuminance` status and message behavior.
7. For curveType gamma failures, the same split applies:

   ```matlab
   run_gamma_qa();
   run('matlab/examples/gamma_curve.m');
   ```

   ```powershell
   $NativeBuildArgs = @(
     '--build', $Build
     '--config', 'Release'
     '--target', 'iccCurveGammaU8Fixed8Test'
   )
   cmake @NativeBuildArgs
   $NativeTestArgs = @(
     '--test-dir', $Build
     '-C', 'Release'
     '-R', '^iccdev\.curve-gamma-u8fixed8$'
     '--output-on-failure'
     '--no-tests=error'
   )
   ctest @NativeTestArgs
   ```

   Confirm the issue #815 fixture decodes raw u8Fixed8 value 565 as gamma
   2.20703125. The MATLAB layer decodes the bytes independently; the native
   CTest is authoritative for what `Describe()` prints and what `ToJson()`
   emits.
8. For issue #1475 failures, run the independent source-table model and native
   reduction contract separately:

   ```matlab
   test_colorimetry_issue_1475();
   run(fullfile(repo_root, 'matlab', 'examples', ...
     'colorimetry_issue_1475.m'));
   ```

   ```powershell
   cmake --build $Build --config Release --target iccColorimetryMethodsTest
   ctest --test-dir $Build -C Release `
     -R '^iccdev\.colorimetry-methods$' `
     --output-on-failure --no-tests=error
   ```

   Preserve `IccProfLib/IccTagBasic.cpp` and
   `IccProfLib/IccColorimetry.cpp` in release artifacts because the MATLAB
   model parses those checked-in tables directly.
9. Fix the root cause and add the nearest MATLAB or native regression.
   PAWG Q1 changes must run both `test_pawg_q1` and
   `iccdev.pawg-q1-quality-contract`.
10. Before interactive MATLAB Desktop validation, derive `repo_root` from either
   the repository root or its `matlab` subdirectory, then add the package and
   tests using absolute paths. Do not assume `pwd` is the repository root.
11. Run `test_iccdev`, `test_pawg_q1`, `test_colorimetry_issue_1475`,
    `run_local_qa`, `run_gamma_qa`,
    `test_plot`, `run_docker_qa`, and all examples. If Docker Desktop is running
    but MATLAB reports that `docker` is not recognized, call
    `add_docker_path(docker_cli_directory)` with a user-selected directory
    containing the Docker CLI, verify `system('docker version')`, and rerun
    Docker plus local QA. Do not publish machine-specific installation paths;
    require zero skipped groups when Docker is available.
    Runnable smoke examples must use checked-in or generated `Testing/...`
    profiles, not `sRGB.icc`, `input.icc`, or `output.icc` placeholders.
12. Preserve unrelated generated files and report exact validation results.
13. Review staged and untracked files for credentials, licenses, tokens,
    personal data, and local MATLAB Project metadata before any push.
