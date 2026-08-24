# MATLAB Bindings Build and Test

Build and validate the iccDEV MATLAB MEX gateway, object-oriented wrappers,
profiles, examples, and native handle lifecycle behavior.

## Steps

1. Read `docs/matlab-bindings.md` and
   `.github/instructions/matlab-mex.instructions.md`.
   On Windows, use PowerShell and the `repo\msvc` build root. Use MATLAB
   `setenv` for current-process environment changes; shell `export` belongs
   only in explicitly Unix-shell commands.
2. Check `git status` and preserve existing generated profiles and local test
   artifacts.
3. Confirm MATLAB has an installed C++ compiler:

   ```powershell
   $Repo = (git rev-parse --show-toplevel).Trim()
   $Build = Join-Path $Repo 'msvc'
   # Discover $MatlabExe and the vcpkg toolchain as documented in
   # docs/matlab-bindings.md; do not embed local absolute paths.
   & $MatlabExe -batch "cfg=mex.getCompilerConfigurations('C++','Selected'); assert(~isempty(cfg)); assert(contains(cfg.Name,'Microsoft Visual C++ 2022')); disp(cfg.Name)"
   ```

4. Build the Release static library:

   ```powershell
   $BuildArgs = @(
     '--build', $Build
     '--config', 'Release'
     '--target', 'IccProfLib2-static', 'iccProfilePlot', 'iccPawgReport',
     'iccPawgQ1QualityContractTest'
     '--', '/m'
   )
   cmake @BuildArgs
   $PlotTool = Join-Path $Build 'bin\Release\iccProfilePlot.exe'
   $PawgTool = Join-Path $Build 'bin\Release\iccPawgReport.exe'
   foreach ($RequiredTool in @($PlotTool, $PawgTool)) {
     if (-not (Test-Path $RequiredTool -PathType Leaf)) {
       throw "Required MATLAB QA tool was not built: $RequiredTool"
     }
   }
   ```

   When the request covers the complete Windows deliverable, build
   `ALL_BUILD` rather than stopping at the minimum MEX and plotting targets.
   Keep `iccPawgReport` in the minimum target list because `run_local_qa()`
   invokes `test_pawg_q1()`.
5. Build the MEX gateway:

   ```powershell
   $env:ICCDEV_BUILD_DIR = $Build
   $env:ICCDEV_REPO_ROOT = $Repo
   & $MatlabExe -batch "repo_root=getenv('ICCDEV_REPO_ROOT'); cd(repo_root); addpath(fullfile(repo_root, 'matlab')); build_mex()"
   ```

6. Generate or copy canonical `Testing/Display/*.icc` profiles. On Windows,
   prepend `$Build\bin\Release` to `PATH`, enter `Testing`, and invoke
   `.\CreateAllProfiles.bat`; do not rely on a bare batch-file name from another
   working directory. The complete corpus contains 213 profiles. Exclude logs,
   crash artifacts, and unrelated untracked files.
7. For a complete Windows build, build `build-test-binaries` and run CTest with
   `-C Release`. The batch-backed fixture uses a disposable `Testing` copy and
   must pass whether or not ignored generated profiles already exist in the
   source checkout.
8. When validating issue #1811 or luminance diagnostics, run both layers:

   ```matlab
   addpath('matlab');
   addpath('matlab/tests');
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

   The MATLAB test confirms fixture arithmetic independently. The CTest must
   call the real `CIccInfo::CheckLuminance`; do not treat the MATLAB model as
   proof of native message or status behavior.
9. For PAWG Check Q1 work, run `test_pawg_q1`. The MATLAB implementation must
   independently calculate PCS decoding, both CIEDE2000 passes, aggregation,
   and verdicts over the shared IccProfLib CMM transforms. It must agree with
   the structured, unrounded Q1 sample count, metrics, model, and verdict from
   `iccPawgReport --json`. Build `iccPawgReport` and
   `iccPawgQ1QualityContractTest`, run
   `iccdev.pawg-q1-quality-contract`, and use the checked-in
   `Testing/sRGB_v4_ICC_preference.icc` fixture so hosted QA does not depend on
   generated profiles. Both implementations must reject grids above the shared
   two-million-sample budget before allocation or iteration.
10. For issue #1475 or TN-06 tristimulus work, run both validation layers.
    Require the same-data 10 nm weighting control to reproduce the built-in
    5 nm D50/1931 perfect-diffuser white. Keep direct 10 nm decimation as a
    separate control, and do not generalize a perfect-diffuser result to
    non-flat spectra.

    ```matlab
    test_colorimetry_issue_1475();
    run(fullfile(repo_root, 'matlab', 'examples', ...
      'colorimetry_issue_1475.m'));
    ```

    ```powershell
    $Repo = (git rev-parse --show-toplevel).Trim()
    $Build = Join-Path $Repo 'msvc'
    cmake --build $Build --config Release --target iccColorimetryMethodsTest -- /m
    ctest --test-dir $Build -C Release `
      -R '^iccdev\.colorimetry-methods$' `
      --output-on-failure --no-tests=error
    ```

    The MATLAB layer reads the checked-in source tables independently. The
    native CTest is authoritative for the compiled reduction methods.
    Never present shell `export` as MATLAB or Windows syntax. MATLAB uses
    `setenv`, and PowerShell uses `$env:NAME = value`.
11. Run:

   ```matlab
   repo_root = pwd;
   if exist(fullfile(repo_root, 'Build', 'Cmake'), 'dir') ~= 7
     repo_root = fileparts(repo_root);
   end
   assert(exist(fullfile(repo_root, 'Build', 'Cmake'), 'dir') == 7);
   cd(repo_root);
   addpath(fullfile(repo_root, 'matlab'));
   addpath(fullfile(repo_root, 'matlab', 'tests'));
   test_usage_guidance();
   test_iccdev();
   run_local_qa();
   test_add_docker_path();
   run_gamma_qa();
   test_plot();
   run_docker_qa();
   run('matlab/examples/read_profile.m');
   run('matlab/examples/color_transform.m');
   run('matlab/examples/gamma_curve.m');
   run('matlab/examples/colorimetry_issue_1475.m');
   run('matlab/examples/docker_interop.m');
   ```

   For local workflow/security iteration, use
   `.github/scripts/preflight-safety-checks.sh --fast-lane=matlab`. Do not run
   broad CTest suites unless the MATLAB change also modifies native CTest
   behavior; build only the required MATLAB/native targets and run the focused
   MATLAB QA.

12. If Docker is installed but `run_local_qa` skips interoperability because
    MATLAB did not inherit `docker.exe`, add the current-process path and rerun:

    ```matlab
    docker_cli_directory = uigetdir('', ...
      'Select the directory containing the Docker CLI');
    assert(~isequal(docker_cli_directory, 0), ...
      'Docker CLI directory selection was cancelled.');
    add_docker_path(docker_cli_directory);
    assert(system('docker version') == 0);
    run_docker_qa();
    run_local_qa();
    ```

    Select the directory containing the Docker CLI; do not publish a
    machine-specific installation path. Require zero failed and zero skipped
    groups when Docker Desktop is available.
13. Confirm `icc_mex` loads from the package private directory and any required
   runtime DLL is staged beside it.
14. For release packaging changes, rerun the suite and examples from the staged
    bundle root, not the source checkout. Preserve `Testing/`,
    `.github/ci/regression/gamma-2.20703125.icc`, and the two C++ table sources
    used by the colorimetry check at their repository-relative paths. Require
    zero skipped test groups before publishing unless Docker interoperability
    is the sole skip because the pinned image is unavailable. Require the
    staged bundle to contain `iccPawgReport.exe`,
    `matlab/+iccdev/+qa/audit_pawg_q1.m`, `bounded_grid.m`,
    `delta_e_2000.m`, `pcs_to_lab.m`, `matlab/tests/test_pawg_q1.m`, and
    `matlab/tests/test_usage_guidance.m`,
    `matlab/tests/fixtures/default_usage_examples.txt`, and
    `Testing/sRGB_v4_ICC_preference.icc`. Add the staged bundle root to the
    MATLAB process `PATH` before `run_local_qa()`.

## Failure Rules

- Do not add `+iccdev/private` to MATLAB's path.
- Do not publish local checkout, MATLAB, Visual Studio, or vcpkg absolute paths;
  derive them with the canonical PowerShell workflow.
- Do not use `sRGB.icc`, `input.icc`, or `output.icc` in runnable smoke tests;
  use an existing `Testing/...` profile.
- Do not link a Release MEX against Debug CRT libraries.
- If `ICC_USE_ZLIB=ON`, require the matching zlib import library and runtime.
- Windows MATLAB must use Windows MSVC artifacts; WSL2 artifacts are for
  Linux/Octave validation only.
- Docker interoperability must use the official image, read-only profile
  mounts, no container network, and an immutable digest in hosted CI.
- Keep MATLAB Project metadata local and ignored.
- Close native handles before `clear mex`.
- Public entry points that require arguments must return an actionable
  `iccdev:*Required` error with a working example when invoked bare.

## Completion Evidence

- Release static library path.
- Complete Windows target result when `ALL_BUILD` is in scope.
- MEX output path and dependency paths.
- Generated profile count and full Windows CTest result.
- Exact MATLAB test result.
- PAWG Q1 MATLAB/native metric and verdict agreement.
- PAWG tool and MATLAB Q1 support-file presence in staged release artifacts.
- Issue #1811 fixture normalization error and warning-window values, when in scope.
- Native `iccdev.luminance-normalization` CTest result, when in scope.
- Issue #1475 MATLAB result and native `iccdev.colorimetry-methods` CTest result.
- Gamma QA result for the `rTRC`, `gTRC`, and `bTRC` issue #815 fixture.
- Native `iccdev.curve-gamma-u8fixed8` CTest result, when in scope.
- Example completion.
- Docker image ID or digest and interoperability result.
- Any skipped profile-dependent checks and why.
- For release work, staged-bundle paths and the staged test result, including
  confirmation that any sole Docker skip was caused by an unavailable pinned
  image.
