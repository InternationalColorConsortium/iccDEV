# Unified iccDEV Container

`ghcr.io/internationalcolorconsortium/iccdev` is the one supported image for
CLI tools, MCP, maintainer reproduction, sanitizer testing, AFL/CFL smoke work,
and CI parity. It contains the unpatched source checkout at
`/workspace/iccDEV`, its configured build at `/workspace/build`, the generated
reference profiles, the MCP runtime, and the maintainer compiler and QA tools.

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
Do not publish branch, run, image-variant, or legacy-package tags. The workflow
always creates an SBOM artifact and provenance attestation for the canonical
digest. Its SBOM attestation is emitted only when the SBOM is at most 16 MiB;
larger SBOMs remain available as artifacts and the workflow reports the skipped
attestation.

For detailed regression gate policy, use
`docs/regression-workflow-governance.md`. For MCP developer setup, use
`iccdev-mcp/docs/build-and-test.md`.
