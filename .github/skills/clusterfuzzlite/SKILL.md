---
name: clusterfuzzlite
description: Build, test, or update the iccDEV ClusterFuzzLite libFuzzer integration across ASan, UBSan, and MSan.
---

# ClusterFuzzLite Integration

Use this skill for `.clusterfuzzlite/**`,
`.github/workflows/ci-clusterfuzzlite.yml`, or the ClusterFuzzLite mode in
`.github/ci/cfl/build.sh`.

## Contract

- Treat libFuzzer as the single fuzzing engine and `address`, `undefined`, and
  `memory` as three separate sanitizer builds.
- Build all three in-process groups in ClusterFuzzLite: `core`
  (`profileparse`, `cmmapply`, `profilevisualize`, `writerserialize`),
  `formats` (`xmlparse`, `jsonparse`, `connectconfig`), and `assessment`
  (`pawgreport`).
- Forward the GitHub matrix group and patch mode through `CFL_EXTRA_*`; the
  official action builds a fresh `GITHUB_SHA` clone, so pre-step checkout file
  mutations do not reach the builder container.
- Keep the CLI-fidelity wrappers in the local CFL smoke lane; they launch child
  tools and do not provide useful parent-process coverage feedback.
- Consume `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, and `LIB_FUZZING_ENGINE` from the
  OSS-Fuzz build environment. Do not combine hard-coded ASan flags with MSan.
- Require matching Clang 21 or Clang 22 compilers. The pinned OSS-Fuzz builder
  supplies Clang 22; do not allow an older fallback.
- Keep tools and zlib disabled. Enable XML/JSON only for targets that consume
  those libraries. The XML memory build must use the pinned instrumented
  libxml2 produced by the repository bootstrap.
- For `memory`, build the pinned MSan libc++ and libc++abi before compiling the
  fuzzers. Reject libstdc++ or a libc++ outside that runtime in `ldd`; for
  `xmlparse`, also reject libxml2 outside the bundled runtime. Replay the pinned
  #2687 artifact through every emitted target. Do not classify a dependency
  report from an uninstrumented runtime as iccDEV.
- Package tracked ICC, XML, and JSON fixtures only for the matching target
  family. Keep the shared profile/text options and profile/XML/JSON
  dictionaries aligned with explicit local CFL limits.
- Keep the workflow limited to `workflow_dispatch` and pushes to
  `ci-qa-clusterfuzz` unless a maintainer explicitly broadens the trigger.
- Keep the manual fuzz duration selectable as whole minutes from 2 through 45,
  validate it before the sanitizer matrix, pass it as the budget for each
  target group, and retain a 2-minute per-group budget for push runs.
- Keep corpus pruning runnable after a fuzz finding. Keep coverage an explicit
  manual option that emits a ClusterFuzzLite artifact without broadening
  repository permissions.
- Keep one temporary CFL patch per open MSan issue. Attempt patches in order,
  report drift or already-integrated fixes, and continue the default workflow;
  reserve `--strict` for local patch-stack maintenance. Remove only the patch
  for an issue whose normal source fix has landed.
- Pin every action to a full commit SHA and the builder image to a digest.

## Local Validation

From an OSS-Fuzz checkout, run for each sanitizer in `address`, `undefined`,
and `memory`:

```bash
python3 infra/helper.py build_image --external --pull /path/to/iccDEV
python3 infra/helper.py build_fuzzers --external --clean \
  --engine libfuzzer --sanitizer SANITIZER /path/to/iccDEV
python3 infra/helper.py check_build --external \
  --engine libfuzzer --sanitizer SANITIZER /path/to/iccDEV
for target in profileparse cmmapply profilevisualize writerserialize \
  xmlparse jsonparse connectconfig pawgreport; do
  python3 infra/helper.py run_fuzzer --external \
    --engine libfuzzer --sanitizer SANITIZER \
    /path/to/iccDEV "icc_${target}_fuzzer" -- -max_total_time=30
done
```

Also run:

```bash
bash -n .clusterfuzzlite/build.sh .github/ci/cfl/build.sh \
  .github/scripts/iccdev-clusterfuzzlite-config-tests.sh \
  .github/scripts/iccdev-clusterfuzzlite-target-tests.sh
.github/scripts/iccdev-clusterfuzzlite-config-tests.sh
.github/scripts/iccdev-fuzz-patch-check-tests.sh
.github/scripts/check-fuzz-patches.sh
ctest --test-dir Build -R '^iccdev\.clusterfuzzlite-(configuration|targets)$' \
  --output-on-failure --no-tests=error
actionlint -no-color .github/workflows/ci-clusterfuzzlite.yml
.github/scripts/preflight-safety-checks.sh --require-tools
```

Report each build, instrumentation check, and bounded run separately. An MSan
failure is not equivalent to an ASan or UBSan failure and must not be hidden by
fallback flags or an allowed-broken-target percentage. Record the resolved C++
runtime for every MSan fuzzer, the XML target's resolved libxml2, and the #2687
one-shot replay result.
