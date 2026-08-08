# Debug MATLAB MEX Bindings

Use this prompt when the iccDEV MATLAB gateway fails to build, load, or produce
correct profile or transform results.

1. Read `docs/matlab-bindings.md`,
   `.github/instructions/matlab-mex.instructions.md`, and
   `.github/skills/matlab-bindings-test/SKILL.md`.
2. Report the MATLAB release, `mexext`, installed C++ compiler, current branch,
   and dirty files before changing anything.
3. Inspect the selected CMake build's configuration, library configuration,
   `ICC_USE_ZLIB`, and vcpkg triplet.
4. Reproduce with the smallest exact command:

   ```matlab
   addpath('matlab');
   build_mex('BuildDir', fullfile(pwd, 'msvc'));
   ```

5. Classify the failure:

   - compiler configuration
   - Release/Debug mismatch
   - missing static dependency or runtime DLL
   - package/private path misuse
   - stale loaded MEX
   - missing generated profiles
   - CMM input shape or lifecycle error
   - issue #1811 fixture normalization or float-window mismatch
   - disagreement between the MATLAB model and native `CheckLuminance`
   - Docker daemon, image, mount, or output-contract error

6. For luminance normalization failures, separate the two validation layers:

   ```matlab
   test_luminance_normalization();
   run('matlab/examples/luminance_normalization.m');
   ```

   ```powershell
   cmake --build msvc --config Release `
     --target iccLuminanceNormalizationTest
   ctest --test-dir msvc -C Release `
     -R '^iccdev\.luminance-normalization$' `
     --output-on-failure --no-tests=error
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
   cmake --build msvc --config Release `
     --target iccCurveGammaU8Fixed8Test
   ctest --test-dir msvc -C Release `
     -R '^iccdev\.curve-gamma-u8fixed8$' `
     --output-on-failure --no-tests=error
   ```

   Confirm the issue #815 fixture decodes raw u8Fixed8 value 565 as gamma
   2.20703125. The MATLAB layer decodes the bytes independently; the native
   CTest is authoritative for what `Describe()` prints and what `ToJson()`
   emits.
8. Fix the root cause and add the nearest MATLAB or native regression.
9. Run `test_iccdev`, `run_local_qa`, `run_gamma_qa`, `run_docker_qa`, and all
   examples.
10. Preserve unrelated generated files and report exact validation results.
11. Review staged and untracked files for credentials, licenses, tokens,
    personal data, and local MATLAB Project metadata before any push.
