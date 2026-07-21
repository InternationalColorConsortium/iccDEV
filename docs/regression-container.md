# Maintainer Regression Container

The `iccdev-ci-regression` image is the maintainer environment for local
reproduction, pull request validation, issue investigation, sanitizer testing,
and CI-parity checks. It contains a clean iccDEV Git checkout, prebuilt
ASAN/UBSAN tools, reference profiles, compilers, debuggers, and QA utilities.

Package:
<https://github.com/InternationalColorConsortium/iccDEV/pkgs/container/iccdev-ci-regression>

## Choose an Image Tag

The regression image does not publish a `latest` tag.

| Tag | Use |
|-----|-----|
| `master` | Current maintainer baseline. |
| `ci-qa-flags` | Changes being validated on the maintainer QA branch. |
| `ci-qa-pr-docker-testing` | Changes being validated for the container itself. |
| `sha-<40-character-commit>` | Immutable build tied to one source revision. |

Record the pulled digest in reports:

```bash
IMAGE=ghcr.io/internationalcolorconsortium/iccdev-ci-regression:master
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
ctest --test-dir /workspace/build -N --no-tests=error
```

The initial Git worktree must be clean. Stop if it is not; do not use
`git clean` to hide an image defect.

Container files disappear with `--rm`. Mount a host directory for evidence:

```bash
mkdir -p evidence
docker run --rm -it \
  -v "$PWD/evidence:/workspace/evidence" \
  "$IMAGE"
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

## Trigger Maintainer CI

`ci-pr-action` runs automatically for pull requests targeting `master`. It does
not have a branch push trigger. For a same-repository branch without an open
pull request, dispatch it explicitly:

```bash
gh workflow run ci-pr-action.yml \
  --repo InternationalColorConsortium/iccDEV \
  --ref <branch> \
  -f ci_scope=full
```

The default long cycle runs the Unix GCC/Clang Release and Debug matrix, the
regression-container GCC 15.2 strict Release LTO build, GCC 15.2 ASAN+UBSAN tool
tests, Windows, and Docker verification. Pull request events and Web UI
dispatches also default to `full`. Use `auto` only when path-scoped selection is
intentional.

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
recent registered test by default. Windows and Docker are opt-in fast-lane
additions through the Web UI, CLI inputs, or maintainer labels.

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
Publish `ci-qa-pr-docker-testing` by dispatching `ci-docker` on that branch:

```bash
gh workflow run ci-docker.yml \
  --repo InternationalColorConsortium/iccDEV \
  --ref ci-qa-pr-docker-testing
```

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
