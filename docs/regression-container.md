# Unified iccDEV Container

`ghcr.io/internationalcolorconsortium/iccdev` is the one supported image for
CLI tools, MCP, maintainer reproduction, sanitizer testing, AFL/CFL smoke work,
and CI parity. It contains the unpatched source checkout at
`/workspace/iccDEV`, its configured build at `/workspace/build`, the generated
reference profiles, the MCP runtime, and the maintainer compiler and QA tools.
Clang 22 is the default compiler; the packaged AFL++ LLVM plugin uses the
included Clang 21 pair for compatible instrumentation.

## Tags

| Tag | Purpose |
|-----|---------|
| `latest` | Current image from `master`; convenient but mutable. |
| `sha-<40-character-commit>` | Immutable CI and investigation reference. |
| `v<release>` | Immutable released image. |
| Existing legacy tags | Retained temporarily for continuity; unsupported for new use. |

Use `latest` only to start interactive work. Record the resolved digest and
source revision in a report; use the SHA tag whenever another person or CI job
must reproduce the result.

Existing short-SHA, branch, and image-variant tags remain available only to
avoid breaking current users during the consolidation transition. Do not create,
recommend, or depend on new legacy tags. Re-evaluate their retention and
removal through a separately announced tag-management change.

```bash
IMAGE=ghcr.io/internationalcolorconsortium/iccdev:latest
docker pull "$IMAGE"
docker image inspect "$IMAGE" \
  --format '{{index .RepoDigests 0}} revision={{index .Config.Labels "org.opencontainers.image.revision"}}'
docker run --rm -it "$IMAGE"
```

The default command is an interactive shell. Tools are on `PATH`; useful
starting checks are:

```bash
git status --short --branch
iccDumpProfile -v Testing/sRGB_v4_ICC_preference.icc
iccRoundTrip Testing/sRGB_v4_ICC_preference.icc
iccdev-fuzz-env
ctest --test-dir /workspace/build -N --no-tests=error
```

## MCP

Start MCP stdio or the REST API explicitly; a single image deliberately has one
shell default instead of per-tag entrypoint behavior.

```bash
docker run --rm -i "$IMAGE" iccdev-mcp-entrypoint mcp
docker run --rm -p 127.0.0.1:8080:8080 "$IMAGE" iccdev-mcp-entrypoint rest
curl -fsS http://127.0.0.1:8080/api/health
```

## Local Review

Mount reviewed source read-only, copy it to container-local scratch space, then
build and test there. Do not mount the Docker socket, SSH files, or tokens into
untrusted code.

```bash
WORKTREE=/path/to/iccDEV
docker run --rm -v "$WORKTREE:/src:ro" "$IMAGE" bash -lc '
  set -euo pipefail
  work="$(mktemp -d)"
  cp -a --no-preserve=ownership /src/. "$work/iccDEV"
  cmake -S "$work/iccDEV/Build/Cmake" -B "$work/build" \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ -DENABLE_ASAN=ON -DENABLE_UBSAN=ON \
    -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON \
    -DENABLE_TOOLS=ON -DENABLE_TESTS=ON
  cmake --build "$work/build" --target all build-test-binaries --parallel "$(nproc)"
  ctest --test-dir "$work/build" --output-on-failure --no-tests=error \
    --label-exclude "^(slow|calculator)$"
'
```

Use the smallest focused project test before the broad CTest envelope. Preserve
evidence outside the disposable container. Exit `1` through `127` is a graceful
failure; exit `128` or higher is signal termination. Attribute sanitizer
findings by stack-frame source path, not input filename.

## Valgrind and Helgrind

The unified image includes Valgrind. Build a separate non-sanitized Debug tree
before using Memcheck or Helgrind; do not place either tool around the image's
ASAN/UBSAN build. Issue #2380 provides a bounded manual workflow at
`.github/workflows/ci-issue-1948-segmented-curve-repro.yml`. Its default
scenario demonstrates the PR #2378 `GetNewApplyCmm()` race before and after the
fix. It is a proof-of-concept workflow, not a hosted fuzzing service.

For a local PR #2378 comparison, check out the issue #2380 branch as `TOOLING`
and the PR head as `TARGET`. These setup commands are each independently
copyable one-liners; replace the three `/path/to` locations first:

```bash
git clone --branch ci-qa-issue-2380-valgrind https://github.com/InternationalColorConsortium/iccDEV.git /path/to/iccDEV-issue-2380
git clone https://github.com/InternationalColorConsortium/iccDEV.git /path/to/iccDEV-pr-2378
git -C /path/to/iccDEV-pr-2378 fetch origin pull/2378/head
git -C /path/to/iccDEV-pr-2378 switch --detach FETCH_HEAD
mkdir /path/to/new/iccdev-valgrind-evidence
docker pull ghcr.io/internationalcolorconsortium/iccdev:latest
```

Then run the single Docker invocation below. It prints the complete first
Helgrind or Memcheck record for each before/after state and also preserves every
run under `EVIDENCE`:

```bash
IMAGE=ghcr.io/internationalcolorconsortium/iccdev:latest
TOOLING=/path/to/iccDEV-issue-2380
TARGET=/path/to/iccDEV-pr-2378
EVIDENCE=/path/to/new/iccdev-valgrind-evidence
mkdir -p "$EVIDENCE"
docker pull "$IMAGE"
docker image inspect "$IMAGE" \
  --format '{{index .RepoDigests 0}} revision={{index .Config.Labels "org.opencontainers.image.revision"}}'
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$TOOLING:/tooling:ro" \
  -v "$TARGET:/target:ro" \
  -v "$EVIDENCE:/evidence" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    work="$(mktemp -d)"
    cp -a --no-preserve=ownership /target/. "$work/after"
    if [ -f "$work/after/.git" ]; then unlink "$work/after/.git"; fi
    cp -a "$work/after" "$work/before"
    patch -d "$work/before" -p1 < \
      /tooling/.github/ci/regression/pr-2378-helgrind-before.patch
    qa=/tooling/.github/scripts/iccdev-valgrind-qa.sh
    "$qa" --source-dir "$work/before" --build-dir "$work/build-before" \
      --tool helgrind --expect finding --runs 3 \
      --label before \
      --out-dir /evidence/before-helgrind
    "$qa" --source-dir "$work/after" --build-dir "$work/build-after" \
      --tool helgrind --expect clean --runs 3 \
      --label after \
      --out-dir /evidence/after-helgrind
    "$qa" --source-dir "$work/before" --build-dir "$work/build-before" \
      --tool memcheck --expect clean --runs 1 \
      --label before \
      --out-dir /evidence/before-memcheck
    "$qa" --source-dir "$work/after" --build-dir "$work/build-after" \
      --tool memcheck --expect clean --runs 1 \
      --label after \
      --out-dir /evidence/after-memcheck
  '
```

The focused one-line form for any already prepared non-sanitized source tree is:

```bash
.github/scripts/iccdev-valgrind-qa.sh --source-dir "$PWD" --build-dir /tmp/iccdev-valgrind --tool helgrind --expect clean --runs 3 --label selected
```

For actual local mutation fuzzing, build an uninstrumented tool and a
libFuzzer-only CLI harness, then place Memcheck around each tool child. Do not
use `.github/ci/cfl/build.sh` for this combination because that normal CFL path
adds ASAN/UBSAN. The run count is intentionally operator-controlled; the hosted
issue workflow does not run this campaign. The following container command
fuzzes `iccDumpProfile` for 300 iterations and leaves the evolving corpus,
Valgrind log, and findings in the mounted evidence directory:

```bash
docker run --rm \
  --user "$(id -u):$(id -g)" \
  -v "$TARGET:/target:ro" \
  -v "$EVIDENCE:/evidence" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    work="$(mktemp -d)"
    cp -a --no-preserve=ownership /target/. "$work/iccDEV"
    if [ -f "$work/iccDEV/.git" ]; then unlink "$work/iccDEV/.git"; fi
    cmake -S "$work/iccDEV/Build/Cmake" -B "$work/build" \
      -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON -DENABLE_TESTS=OFF \
      -DENABLE_SANITIZERS=OFF -DENABLE_ASAN=OFF -DENABLE_UBSAN=OFF \
      -DENABLE_TSAN=OFF -DENABLE_LTO=OFF -DENABLE_WXWIDGETS=OFF
    cmake --build "$work/build" --target iccDumpProfile --parallel "$(nproc)"
    clang++ -std=c++17 -g -O1 -fsanitize=fuzzer \
      -DICCDEV_CFL_TARGET=\"dump\" \
      "$work/iccDEV/.github/ci/cfl/icc_cli_fuzzer.cpp" \
      -o "$work/icc_dump_valgrind_fuzzer"
    mkdir -p "$work/wrappers/IccDumpProfile" \
      /evidence/valgrind-corpus /evidence/valgrind-findings
    cp "$work/iccDEV/Testing/sRGB_v4_ICC_preference.icc" \
      /evidence/valgrind-corpus/seed.icc
    printf "%s\n" \
      "#!/bin/bash" \
      "set +e" \
      "valgrind --quiet --tool=memcheck --leak-check=full --track-origins=yes --error-exitcode=99 \"\$ICCDEV_VALGRIND_REAL_TOOL\" \"\$@\"" \
      "status=\$?" \
      "if [ \"\$status\" -eq 99 ]; then kill -ABRT \"\$\$\"; fi" \
      "exit \"\$status\"" \
      > "$work/wrappers/IccDumpProfile/iccDumpProfile"
    chmod +x "$work/wrappers/IccDumpProfile/iccDumpProfile"
    set +e
    ICCDEV_CFL_TOOL_DIR="$work/wrappers" \
    ICCDEV_VALGRIND_REAL_TOOL="$work/build/Tools/IccDumpProfile/iccDumpProfile" \
      "$work/icc_dump_valgrind_fuzzer" \
      /evidence/valgrind-corpus \
      -runs=300 -max_len=262144 \
      -artifact_prefix=/evidence/valgrind-findings/ \
      > /evidence/libfuzzer-valgrind.log 2>&1
    status=$?
    set -e
    tail -80 /evidence/libfuzzer-valgrind.log
    exit "$status"
  '
```

The wrapper converts Valgrind exit 99 into a child signal so the existing CFL
harness saves the triggering input. Replay each saved input once outside the
mutation loop. Use Helgrind instead of Memcheck only for a target that exercises
concurrent callers; the threaded CMM regression above is the canonical example.
Long campaigns, corpus minimization, and crash triage remain local maintainer
operations.

## Building and Publishing

Build the one Dockerfile locally before publishing:

```bash
docker build --no-cache -t iccdev:local .
docker run --rm iccdev:local bash -lc '
  set -euo pipefail
  iccDumpProfile -v Testing/sRGB_v4_ICC_preference.icc >/dev/null
  iccdev-mcp-entrypoint --help >/dev/null
  ctest --test-dir /workspace/build -N --no-tests=error >/dev/null
'
```

`ci-docker` publishes only the canonical package: `master` adds `latest` and
the immutable SHA tag; a `v*` ref adds its release tag and immutable SHA tag.
Do not publish branch, run, image-variant, or legacy-package tags. Publishing
runs create BuildKit SBOM and provenance attestations for the canonical digest.
The separate GitHub SBOM attestation is emitted only when the SBOM is at most
16 MiB; larger SBOMs remain available as artifacts and the workflow reports
that GitHub attestation as skipped.

For detailed regression gate policy, use
`docs/regression-workflow-governance.md`. For MCP developer setup, use
`iccdev-mcp/docs/build-and-test.md`.
