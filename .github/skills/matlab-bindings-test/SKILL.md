# MATLAB Bindings Build and Test

Build and validate the iccDEV MATLAB MEX gateway, object-oriented wrappers,
profiles, examples, and native handle lifecycle behavior.

## Steps

1. Read `docs/matlab-bindings.md` and
   `.github/instructions/matlab-mex.instructions.md`.
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
     '--target', 'IccProfLib2-static'
     '--', '/m'
   )
   cmake @BuildArgs
   ```

5. Build the MEX gateway:

   ```powershell
   $env:ICCDEV_BUILD_DIR = $Build
   $env:ICCDEV_REPO_ROOT = $Repo
   & $MatlabExe -batch "repo_root=getenv('ICCDEV_REPO_ROOT'); cd(repo_root); addpath(fullfile(repo_root, 'matlab')); build_mex()"
   ```

6. Generate or copy canonical `Testing/Display/*.icc` profiles. Exclude logs,
   crash artifacts, and unrelated untracked files.
7. When validating issue #1811 or luminance diagnostics, run both layers:

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
8. Run:

   ```matlab
   addpath('matlab/tests');
   test_iccdev();
   run_local_qa();
   run_gamma_qa();
   run_docker_qa();
   run('matlab/examples/read_profile.m');
   run('matlab/examples/color_transform.m');
   run('matlab/examples/gamma_curve.m');
   run('matlab/examples/docker_interop.m');
   ```

9. Confirm `icc_mex` loads from the package private directory and any required
   runtime DLL is staged beside it.
10. For release packaging changes, rerun the suite and examples from the staged
    bundle root, not the source checkout. Preserve `Testing/`,
    `.github/ci/regression/gamma-2.20703125.icc`, and the two C++ table sources
    used by the colorimetry check at their repository-relative paths. Require
    zero skipped test groups before publishing unless Docker interoperability
    is the sole skip because the pinned image is unavailable.

## Failure Rules

- Do not add `+iccdev/private` to MATLAB's path.
- Do not publish local checkout, MATLAB, Visual Studio, or vcpkg absolute paths;
  derive them with the canonical PowerShell workflow.
- Do not link a Release MEX against Debug CRT libraries.
- If `ICC_USE_ZLIB=ON`, require the matching zlib import library and runtime.
- Windows MATLAB must use Windows MSVC artifacts; WSL2 artifacts are for
  Linux/Octave validation only.
- Docker interoperability must use the official image, read-only profile
  mounts, no container network, and an immutable digest in hosted CI.
- Keep MATLAB Project metadata local and ignored.
- Close native handles before `clear mex`.

## Completion Evidence

- Release static library path.
- MEX output path and dependency paths.
- Exact MATLAB test result.
- Issue #1811 fixture normalization error and warning-window values, when in scope.
- Native `iccdev.luminance-normalization` CTest result, when in scope.
- Gamma QA result for the `rTRC`, `gTRC`, and `bTRC` issue #815 fixture.
- Native `iccdev.curve-gamma-u8fixed8` CTest result, when in scope.
- Example completion.
- Docker image ID or digest and interoperability result.
- Any skipped profile-dependent checks and why.
- For release work, staged-bundle paths and the staged test result, including
  confirmation that any sole Docker skip was caused by an unavailable pinned
  image.
