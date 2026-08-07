# Maintainer Regression Container

The `iccdev-ci-regression` image is the maintainer environment for local
reproduction, pull request validation, issue investigation, sanitizer testing,
and CI-parity checks. It contains a clean iccDEV Git checkout, prebuilt
ASAN/UBSAN tools, reference profiles, compilers, debuggers, and QA utilities.

Package:
<https://github.com/InternationalColorConsortium/iccDEV/pkgs/container/iccdev-ci-regression>

## Choose an Image Tag

| Tag | Use |
|-----|-----|
| `latest` | Recommended current maintainer baseline for interactive PR and issue work. |
| `master` | Current maintainer baseline with the source branch stated explicitly. |
| `ci-qa-flags` | Changes being validated on the maintainer QA branch. |
| `ci-qa-pr-docker-testing` | Changes being validated for the container itself. |
| `sha-<40-character-commit>` | Immutable build tied to one source revision. |

Use `latest` to begin routine investigation, but record the resolved digest and
source revision in every report. An explicit protected Docker-testing branch
promotion can temporarily move `latest` ahead of `master`, so verify the
revision rather than assuming the two tags match. Use the immutable SHA tag when
another maintainer or CI job must reproduce the exact environment:

```bash
IMAGE=ghcr.io/internationalcolorconsortium/iccdev-ci-regression:latest
docker pull "$IMAGE"
docker image inspect "$IMAGE" \
  --format '{{index .RepoDigests 0}} revision={{index .Config.Labels "org.opencontainers.image.revision"}}'
```

## Basic Use

Start a disposable interactive shell:

```bash
docker run --rm -it "$IMAGE"
```

The source checkout is `/workspace/iccDEV`, the configured build is
`/workspace/build`, tools are on `PATH`, and generated reference profiles are
under `Testing/`.

Useful smoke commands:

```bash
git status --short --branch
iccDumpProfile -v Testing/sRGB_v4_ICC_preference.icc
iccRoundTrip Testing/sRGB_v4_ICC_preference.icc
iccdev-fuzz-env
ctest --test-dir /workspace/build -N --no-tests=error
```

The initial Git worktree must be clean. Stop if it is not; do not use
`git clean` to hide an image defect.

Container files disappear with `--rm`. Use a Docker-managed volume for evidence
so the container user can write without changing host-directory permissions:

```bash
EVIDENCE_VOLUME="iccdev-evidence-$(date +%s)"
docker volume create "$EVIDENCE_VOLUME"
docker run --rm --user 0 \
  -v "$EVIDENCE_VOLUME:/workspace/evidence" \
  "$IMAGE" chown iccdev-ci:iccdev-ci /workspace/evidence
docker run --rm -it \
  -v "$EVIDENCE_VOLUME:/workspace/evidence" \
  "$IMAGE"

mkdir -p evidence
COPY_CONTAINER="$(docker create \
  -v "$EVIDENCE_VOLUME:/workspace/evidence:ro" \
  "$IMAGE")"
docker cp "$COPY_CONTAINER:/workspace/evidence/." ./evidence/
docker rm "$COPY_CONTAINER"
docker volume rm "$EVIDENCE_VOLUME"
```

Do not mount the Docker socket, host SSH directory, or GitHub token into an
untrusted pull request checkout.

## Validate a Pull Request

GitHub pull request refs are public Git refs and do not require GitHub CLI
authentication:

```bash
PR=1752
git fetch --depth=1 origin "pull/${PR}/head:pr-${PR}"
git checkout --detach "pr-${PR}"
git log -1 --oneline
git status --short --branch
```

Rebuild after checkout. CMake automatically regenerates the existing configured
sanitizer build when required:

```bash
cmake --build /workspace/build --parallel "$(nproc)"
cmake --build /workspace/build --target build-test-binaries --parallel "$(nproc)"
```

Run the smallest focused test first, then its CTest wrapper when one exists:

```bash
ICCDEV_TOOLS_DIR=/workspace/build/Tools \
ICCDEV_TESTING_DIR=/workspace/iccDEV/Testing \
ICCDEV_TEST_OUTDIR=/workspace/evidence/focused \
  bash .github/scripts/<focused-regression>.sh

ctest --test-dir /workspace/build \
  -R '^<focused-ctest-name>$' \
  --output-on-failure --no-tests=error
```

Use the broader gate only after the focused test passes:

```bash
ctest --test-dir /workspace/build \
  --output-on-failure --no-tests=error
```

## Prove a Local PR Worktree

When reviewing a PR or issue fix from a local worktree, prefer the maintainer
regression image before handoff. It provides the current strict compiler stack,
including GCC 15.2 and the Clang sanitizer toolchain used by the PR Docker
verification lane. Mount the local tree read-only so the container proves the
exact files under review without changing host permissions. Copy that mount to
container-local scratch space before configuring so CTest fixtures can generate
profiles under `Testing/`:

```bash
IMAGE=ghcr.io/internationalcolorconsortium/iccdev-ci-regression:latest
WORKTREE=/path/to/iccDEV
docker pull "$IMAGE"
docker run --rm -v "$WORKTREE:/workspace/review:ro" "$IMAGE" bash -lc '
  set -euo pipefail
  scratch="$(mktemp -d)"
  source_dir="$scratch/iccDEV"
  build_dir="$scratch/build"
  mkdir -p "$source_dir"
  cp -a /workspace/review/. "$source_dir/"
  cd "$source_dir"
  target=iccApplyProfiles
  focused_ctest=iccdev.applyprofiles-spectral-pcs-regression
  SAN_FLAGS="-fsanitize=address,undefined,integer,bounds,null,float-divide-by-zero,alignment,vla-bound"
  CC=clang CXX=clang++ cmake -S "$source_dir/Build/Cmake" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="$SAN_FLAGS -fno-omit-frame-pointer -g -O0" \
    -DCMAKE_CXX_FLAGS="$SAN_FLAGS -fno-omit-frame-pointer -g -O0 -std=c++17" \
    -DENABLE_ASAN=ON \
    -DENABLE_UBSAN=ON \
    -DENABLE_INTEGER_SANITIZER=ON \
    -DENABLE_FLOAT_SANITIZER=ON \
    -DUBSAN_IGNORELIST="$source_dir/.github/ci/ubsan-ignorelist.txt" \
    -DENABLE_TOOLS=ON \
    -DENABLE_TESTS=ON
  cmake --build "$build_dir" --target "$target" --parallel "$(nproc)"
  cmake --build "$build_dir" --target iccFromXml --parallel "$(nproc)"
  cmake --build "$build_dir" --target build-test-binaries --parallel "$(nproc)"
  ctest --test-dir "$build_dir" -R "^${focused_ctest}$" \
    --output-on-failure --no-tests=error
'
```

The Docker PR verification job pulls the published `latest` maintainer image
instead of rebuilding `Dockerfile.ci-regression` for each PR. It mounts the PR
tree read-only, copies it to container-local scratch space for writable CTest
fixtures, then builds the configured tool and test target set with strict Clang
sanitizers. Its routine PR CTest envelope excludes the separately labelled
`slow` and `calculator` suites:

```bash
docker run --rm -v "$WORKTREE:/src:ro" "$IMAGE" bash -lc '
  set -euo pipefail
  work="$(mktemp -d)"
  source_dir="$work/iccDEV"
  mkdir -p "$source_dir"
  cp -a /src/. "$source_dir/"
  SAN_FLAGS="-fsanitize=address,undefined,integer,bounds,null,float-divide-by-zero,alignment,vla-bound"
  CC=clang CXX=clang++ cmake -S "$source_dir/Build/Cmake" -B "$work/build" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="$SAN_FLAGS -fno-omit-frame-pointer -g -O0" \
    -DCMAKE_CXX_FLAGS="$SAN_FLAGS -fno-omit-frame-pointer -g -O0 -std=c++17" \
    -DENABLE_ASAN=ON \
    -DENABLE_UBSAN=ON \
    -DENABLE_INTEGER_SANITIZER=ON \
    -DENABLE_FLOAT_SANITIZER=ON \
    -DUBSAN_IGNORELIST="$source_dir/.github/ci/ubsan-ignorelist.txt" \
    -DENABLE_TOOLS=ON \
    -DENABLE_TESTS=ON
  cmake --build "$work/build" --target all build-test-binaries --parallel "$(nproc)" \
    2>&1 | tee "$work/build.log"
  warning_count="$(grep -cE "warning:" "$work/build.log" || true)"
  test "$warning_count" -eq 0
  ASAN_OPTIONS="halt_on_error=0,detect_leaks=0" \
  UBSAN_OPTIONS="halt_on_error=0,print_stacktrace=1" \
  LLVM_PROFILE_FILE="/dev/null" \
  ctest --test-dir "$work/build" --output-on-failure --no-tests=error \
    --label-exclude "^(slow|calculator)$"
'
```

Use a focused or full CTest explicitly when the change affects a suite excluded
from the routine Docker PR envelope. Record the resolved image digest, mounted
worktree commit, command, exit status, warning count, and sanitizer result in
the PR or issue handoff.

The prebuilt image uses the maintainer Clang sanitizer configuration. For exact
`ci-iccdev-tool-tests.yml` GCC strict parity, configure a separate build:

```bash
cmake -S Build/Cmake -B /workspace/gcc-review-build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++ \
  -DENABLE_SANITIZERS=ON \
  -DSANITIZER_RECOVER=ON \
  -DENABLE_TOOLS=ON \
  -DENABLE_TESTS=ON \
  -DENABLE_WXWIDGETS=OFF \
  -DENABLE_SHARED_LIBS=ON \
  -DENABLE_STATIC_LIBS=ON \
  -DICC_USE_ZLIB=ON \
  -DICCDEV_ENABLE_QA_FLAGS=ON \
  -DICCDEV_ENABLE_STRICT_WARNINGS=ON \
  -Wno-dev
cmake --build /workspace/gcc-review-build --parallel "$(nproc)"
```

## Investigate an Issue

Read issue metadata on the host when possible:

```bash
gh issue view <issue-number> \
  --repo InternationalColorConsortium/iccDEV \
  --comments
```

Inside the container:

1. Record the image digest and starting source revision.
2. Identify the affected tool, existing regression script, and smallest saved
   input.
3. Reproduce with project tools from `/workspace/build/Tools`; do not create a
   custom C++ reproducer.
4. If a fix commit is known, test the baseline and fixed revisions separately.
5. Save command output, exit status, and sanitizer diagnostics under the mounted
   evidence directory.

To test a specific revision:

```bash
REVISION=<commit>
git fetch --depth=1 origin "$REVISION"
git checkout --detach "$REVISION"
cmake --build /workspace/build --parallel "$(nproc)"
```

Exit status is authoritative:

| Exit status | Classification |
|-------------|----------------|
| `0` | Success. |
| `1` through `127` | Graceful finding, validation failure, or tool error. |
| `128` or greater | Signal termination; investigate as a possible crash. |

Attribute ASAN/UBSAN findings by stack-frame source paths, not by the input
filename. Treat sanitizer diagnostics as failures unless a documented
suppression applies.

## AFL/CFL Onboarding Checks

The regression image includes packaged AFL++ tools plus their matching compiler
runtime for maintainer inspection and lightweight local checks, and includes
Clang/libFuzzer support for CFL harness builds. The `ci-afl-smoke` workflow
still rebuilds AFL++ dev wrappers against LLVM 22 before instrumentation so the
CI wrapper and compiler versions stay aligned.

Inspect the fuzzing environment:

```bash
iccdev-fuzz-env
```

Validate that the AFL/CFL local patch stacks match the checked-out source and
workflow applicator semantics:

```bash
.github/scripts/check-fuzz-patches.sh
```

Run a short patched AFL smoke from the image checkout:

```bash
.github/scripts/iccdev-afl-smoke.sh \
  --patches \
  --seconds 10 \
  --targets dump \
  --exec-timeout-ms 30000
```

Run the current core CFL smoke:

```bash
cfl/build.sh \
  --patches \
  --targets dump,toxml,fromxml,tojson,fromjson,roundtrip \
  --seconds 30
```

Use these checks after changing `Dockerfile.ci-regression`, AFL/CFL scripts,
patch stacks, seeds, or maintainer workflow inputs. Do not commit generated
`build-afl-smoke`, `.afl-smoke`, `cfl/bin`, `.cfl-smoke`, crash artifacts, or
coverage/profiler outputs.

## Trigger Maintainer CI

`ci-pr-action` runs automatically for pull requests targeting `master` or
`ci-qa-flags`. It does not have a branch push trigger. For a same-repository
branch without an open pull request, dispatch it explicitly:

```bash
gh workflow run ci-pr-action.yml \
  --repo InternationalColorConsortium/iccDEV \
  --ref <branch> \
  -f ci_scope=full
```

The full long cycle runs the Unix GCC/Clang Release and Debug matrix, the
regression-container GCC 15.2 strict Release LTO build, GCC 15.2 ASAN+UBSAN tool
tests, Windows, and Docker verification. Pull request events and Web UI
dispatches default to `auto`: source, build, test, and container changes select
the full matrix, while workflow-only changes use the preflight and
workflow-security gates. Dispatch `full` explicitly when a long-cycle matrix is
needed.

For the fastest same-repository PR lane, provide the open PR number:

```bash
gh workflow run ci-pr-action.yml \
  --repo InternationalColorConsortium/iccDEV \
  --ref <branch> \
  -f ci_scope=fast-lane \
  -f pr_number=<pr-number>
```

Fast lane uses the regression container for exact GCC 15.2, runs strict Release
LTO plus the GCC 15.2 ASAN+UBSAN Release tool lane, and limits CTest to the most
recent registered test by default. Windows is opt-in through the Web UI or CLI
`include_windows` input. Docker is opt-in through the Web UI or CLI
`include_docker` input.

Watch the run:

```bash
gh run list \
  --repo InternationalColorConsortium/iccDEV \
  --workflow ci-pr-action.yml \
  --branch <branch> \
  --limit 5
gh run watch <run-id> \
  --repo InternationalColorConsortium/iccDEV \
  --exit-status
```

Pushes to `master` or `ci-qa-flags` that touch the container surface trigger
`ci-docker` and publish the corresponding regression branch and SHA tags.
After all image smoke tests and regression CTest checks succeed, a successful
`master` publication promotes the verified immutable regression digest to
`latest`.

Publish `ci-qa-pr-docker-testing` by dispatching `ci-docker` on that branch. The
protected testing branch can promote the verified immutable digest to `latest`
only through the explicit opt-in input, which prevents ordinary testing runs
and failed image checks from replacing the maintainer baseline:

```bash
gh workflow run ci-docker.yml \
  --repo InternationalColorConsortium/iccDEV \
  --ref ci-qa-pr-docker-testing \
  -f publish-regression-latest=true
```

After publication, pull `latest` and confirm that its digest and
`org.opencontainers.image.revision` label match the workflow output before using
it for PR or issue work.

Before publishing a Docker change, compare every maintained container file
across `ci-qa-pr-docker-testing` and `ci-qa-flags`:

```bash
git fetch origin \
  ci-qa-pr-docker-testing:refs/remotes/origin/ci-qa-pr-docker-testing \
  ci-qa-flags:refs/remotes/origin/ci-qa-flags
git diff --name-status \
  origin/ci-qa-pr-docker-testing..origin/ci-qa-flags -- \
  'Dockerfile*' '**/Dockerfile*'
```

Carry applicable fixes for `Dockerfile`, `Dockerfile.nixos`, `Dockerfile.mcp`,
and `Dockerfile.ci-regression` to both branches. A successful testing-branch
image does not replace the required `ci-qa-flags` update and hosted validation.

Repository rules intentionally differ by branch:

- `master` requires the stable PR aggregate, matrix initialization, both risk
  audits, and WASM parity.
- `ci-qa-flags` requires the stable PR aggregate, matrix initialization, and
  both risk audits.
- `ci-qa-pr-docker-testing` permits direct maintainer iteration but requires
  signed commits and linear fast-forward history, and blocks force pushes and
  deletion. Its hosted `ci-pr-action` and `ci-docker` runs are dispatched after
  the push rather than configured as pre-push required contexts.

## Maintainer Report

Report:

- Image tag, digest, and source revision.
- PR, issue, branch, and tested commit.
- Exact focused and CTest commands.
- Pass, fail, skip, warning, and sanitizer totals.
- Exit status classification for reproductions.
- Evidence directory or log paths.
- GitHub workflow run URLs and final conclusions.
- Any container defect, workaround, or missing dependency.

Container implementation and publishing changes remain governed by
[Regression workflow governance](regression-workflow-governance.md).
