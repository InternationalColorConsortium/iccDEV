# ICC Apply Tool Quality Assurance

This directory contains local quality-assurance drivers and generated command
corpora for the main `iccApply*` command-line tools:

- `iccApplyNamedCmm`
- `iccApplyProfiles`
- `iccApplySearch`
- `iccApplyToLink`

The scripts are intended for maintainer QA on a local checkout. They exercise
documented command-line argument shapes, optional config export/replay paths,
environment-variable arguments, profile connection condition arguments, and
sanitizer-visible failures.

## Prerequisites

Build the command-line tools and generate the hybrid test profiles first:

```sh
cd Build
cmake Cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build . --parallel
cd ../Testing/hybrid
./BuildAndTest.sh
```

Run all commands below from `Testing/hybrid`. The scripts will use build-tree
tool paths when they are available.

## Quick smoke

```sh
../../.github/ci/quality-assurance/scripts/icc_apply_qa_suite.sh --mutations 12
```

The suite runs a small smoke across all four apply tools and scans logs for
sanitizer signatures. It exits nonzero if any tool command fails, if generated
config replay fails, or if sanitizer output is detected.

## Per-tool drivers

```sh
../../.github/ci/quality-assurance/scripts/iccApplyNamedCmm_ci_path_exercise.sh --mutations 200
../../.github/ci/quality-assurance/scripts/iccApplyProfiles_ci_path_exercise.sh --mutations 200 --no-replay-cfg
../../.github/ci/quality-assurance/scripts/icc_ci_tool_path_exercise.sh --tool search --mutations 200 --no-replay-cfg
../../.github/ci/quality-assurance/scripts/tolink-script-random-001.sh --tests 200
```

Use `--generate FILE` to regenerate the command corpora under `commands/`.
Use `--count` to show each generator's mutation space without requiring a built
test tree.

## Command corpora

The files under `commands/` are generated replay corpora. They intentionally
contain raw commands only so they can be executed with standard shell tooling:

```sh
while IFS= read -r cmd; do
  ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 bash -lc "$cmd"
done < ../../.github/ci/quality-assurance/commands/iccApplyToLink_QA_200.txt
```

Generated corpora are ASCII text and should remain line-ending clean for Linux
and Windows checkout compatibility.
