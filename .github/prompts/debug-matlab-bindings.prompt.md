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
   - Docker daemon, image, mount, or output-contract error

6. Fix the root cause and add the nearest MATLAB regression.
7. Run `test_iccdev`, `run_local_qa`, `run_docker_qa`, and all examples.
8. Preserve unrelated generated files and report exact validation results.
9. Review staged and untracked files for credentials, licenses, tokens,
   personal data, and local MATLAB Project metadata before any push.
