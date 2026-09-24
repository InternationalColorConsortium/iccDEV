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
- Build only `icc_profilevisualize_fuzzer` and
  `icc_writerserialize_fuzzer` in ClusterFuzzLite. They are the in-process
  targets whose coverage is visible to libFuzzer.
- Keep the CLI-fidelity wrappers in the local CFL smoke lane; they launch child
  tools and do not provide useful parent-process coverage feedback.
- Consume `CC`, `CXX`, `CFLAGS`, `CXXFLAGS`, and `LIB_FUZZING_ENGINE` from the
  OSS-Fuzz build environment. Do not combine hard-coded ASan flags with MSan.
- Require matching Clang 21 or Clang 22 compilers. The pinned OSS-Fuzz builder
  supplies Clang 22; do not allow an older fallback.
- Keep XML, JSON, tools, and zlib disabled in this lane so MSan does not mix the
  in-process target with uninstrumented system dependencies.
- Package the tracked `.github/ci/test-data/*.icc` files as seed corpora and
  keep both `.options` files aligned with the explicit local CFL limits.
- Keep the workflow limited to `workflow_dispatch` and pushes to
  `ci-qa-clusterfuzz` unless a maintainer explicitly broadens the trigger.
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
python3 infra/helper.py run_fuzzer --external \
  --engine libfuzzer --sanitizer SANITIZER \
  /path/to/iccDEV icc_profilevisualize_fuzzer -- -max_total_time=30
python3 infra/helper.py run_fuzzer --external \
  --engine libfuzzer --sanitizer SANITIZER \
  /path/to/iccDEV icc_writerserialize_fuzzer -- -max_total_time=30
```

Also run:

```bash
bash -n .clusterfuzzlite/build.sh .github/ci/cfl/build.sh \
  .github/scripts/iccdev-clusterfuzzlite-config-tests.sh
.github/scripts/iccdev-clusterfuzzlite-config-tests.sh
ctest --test-dir Build -R '^iccdev\.clusterfuzzlite-configuration$' \
  --output-on-failure --no-tests=error
actionlint -no-color .github/workflows/ci-clusterfuzzlite.yml
.github/scripts/preflight-safety-checks.sh --require-tools
```

Report each build, instrumentation check, and bounded run separately. An MSan
failure is not equivalent to an ASan or UBSan failure and must not be hidden by
fallback flags or an allowed-broken-target percentage.
