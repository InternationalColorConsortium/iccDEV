# Container and MCP QA evidence, 2026-09-06 UTC

Scope: approved branch-only PAWG classification, scanner discovery, isolated
native validation, and real MCP/REST container checks. This is the cumulative
contract audit for `bde6cb128d112777ec4c1fe036e4ede02c9dedd1...HEAD`, not a
delta-only review. No C/C++ source, public ABI, dependency pins, image publication
selectors, or unrelated branch commits changed.

## Provenance

- Upstream base and remote feature head before implementation:
  `bde6cb128d112777ec4c1fe036e4ede02c9dedd1`.
- Branch: `ci-qa-pr-docker-testing`.
- Clean independent checkout: `/home/xss/qa/iccDEV-container-mcp-qa`.
  Fetch/push remote: `git@github.com:InternationalColorConsortium/iccDEV.git`.
- Protected reference remained at `2fd99a03fac50f3415ed1e0687a9dfafcb47d044`
  with all pre-existing tracked/untracked changes. The occupied worktree at
  `/home/xss/qa/iccDEV-ci-qa-pr-docker-testing` remained at its unrelated local
  commit `333948baeeef41122e4482e1f352b364f0e8a564`; it was not reset or reused.
- Research checkout revision at entry:
  `abd66db47dadc66bb0769d4ba3895b24e4a95e73`.
- Published baseline:
  `ghcr.io/internationalcolorconsortium/iccdev@sha256:e8fa045f175d1cc87044314700d3412afe299c235cf7ffacdb0b4a15c989a142`.
  OCI revision: `bde6cb128d112777ec4c1fe036e4ede02c9dedd1`.
- Final locally built image: `iccdev:container-mcp-qa-final`, resolved ID/digest
  `sha256:50b6394ca8c96fa311c54b56cc7f0d0cb4eea74119ffa7c13f49de866d00fbd0`.
  This is a local Docker image, not a GHCR publication.
  Local `GIT_COMMIT=unknown` intentionally exercises COPY of the unpushed tree
  instead of fetching/resetting it to the old remote commit. Its internal clean
  synthetic source revision is `f0cab19367a172ac735eba7a8fc4d3fce7397c7d`.
  All 20 implementation/documentation files were compared byte-for-byte with
  that image: zero mismatches. Only this evidence report was added afterward.
- Isolated library SHA256:
  `ae728be8dfb5846b1edc3d695f917d28dac5a49d12f328414bd3c4ac63c851f6`.

## Complete changed-surface contract matrix

All paths below are relative to the upstream root. Owners are the existing
iccDEV maintainers; no new third-party runtime dependency was introduced.

| Changed surface | Producer -> consumer; defaults and overrides | Failure paths and boundary | Trigger/owner; local evidence |
| --- | --- | --- | --- |
| `Dockerfile`, `.github/ci/docker/build-validation-library.sh` | Current IccProfLib CMake target -> isolated Release shared library -> Python ctypes. CLI Debug sanitizer build stays separate. `BUILD_JOBS=32`; configurable positive job count. | Fresh temporary build, no inherited CFLAGS/CXXFLAGS/CPPFLAGS/LDFLAGS, LTO off, cleanup on exit; install only the shared library. Linux native architecture, no global ASAN preload. zlib/libstdc++/libm/libgcc/libc resolve from pinned image dependencies. | `ci-docker`, image-building `ci-docker-pr`; container/build maintainers. Two real Docker builds, native OK/malformed-input probes, `nm`, `ldd`, focused dlopen CTest. |
| `iccdev-mcp/iccdev/__init__.py` | Explicit `ICCDEV_VALIDATION_LIBRARY` -> ctypes; if unset, existing `ICCDEV_BUILD_DIR` discovery. Image sets `/opt/iccdev-validation/lib/libIccProfLib2.so`. | Explicit empty, missing, unloadable, missing-symbol, or sanitizer-only libraries never fall back. To select a custom build directory, unset the image library variable or override it explicitly. Optional pip-only health remains unavailable without an ABI. | MCP package tests and image smoke; Python/C ABI maintainers. Five added override tests plus real isolated-library success and four negative library cases. |
| `.github/scripts/icc-pawg-qa-classify.py`, `icc-tool-qa-scan-common.sh` in that directory | PAWG JSON verdicts/text checklist rows and consistent summary -> TSV issue counters/status/findings. Python 3 stdlib only. | Zero summaries and benign compression/detail text do not fail. FAIL/WARN/GAP/NOT RUN retained; incomplete/malformed/inconsistent output yields an error. Sanitizer/signal evidence and timeout precedence preserved; non-PAWG diagnostics unchanged. Exit 128+ is signal-like. | MCP package path-filter tests and registry child scanners; QA maintainers. Structured-state, malformed-report, actual timeout, sanitizer/signal and non-PAWG tests; live text+JSON both PASS. |
| `.github/scripts/icc-pawg-qa-scan.sh`, `icc-dumpprofile-qa-scan.sh`, `icc-roundtrip-qa-scan.sh` | `--tool` > per-tool environment override > `ICCDEV_TOOLS_DIR` > checkout `Build/Tools` > PATH. Directory search supports nested and flat layout. | Invalid/empty explicit per-tool paths fail closed, including directories/nonexecutables; directory hints can fall back. Help/list-variants do not need an installed tool. Existing timeout/fail-on controls remain. | MCP pytest trigger includes shared scanner paths; QA maintainers. 30 precedence cases across three tools, positive direct scans, no-network registry integration without overrides. |
| `.github/scripts/iccdev-container-smoke.py` | Image identity -> MCP initialize/initialized/tools-list/health/header/validate; then loopback ephemeral REST health/tools/header/validate/PAWG. Inventory comes from runtime and image TOOL_BINARIES. | Per-response and startup deadlines; stdin remains open until responses; wrong IDs, protocol errors, EOF and missing validation fail. Context-manager cleanup removes named containers; optional report directory preserves JSON identity/inventory and failure logs. No credentials/socket mounted. | `ci-docker`, newly built `ci-docker-pr` image, local maintainer command; MCP/container maintainers. Real baseline and new-image handshakes, native OK over both transports, baseline negative gate rejects unavailable validation, lifecycle unit tests. |
| `iccdev-mcp/tests/test_qa_scanners.py`, `test_docker_entrypoint.py`, `test_server.py` | Temporary executable stubs and public API -> existing pytest suite. No network corpus or fixed healthy-tool total. | Tests pin positive/negative paths, invalid output, native optional behavior, report cleanup, and transport deadlines. Test-only dependencies remain existing dev extras; no installs added to runtime image. | `ci-mcp-server` Python 3.12/3.14 jobs; MCP/QA maintainers. Host 200 passed/3 skipped; image 201 passed/2 skipped out of 203. |
| `.github/workflows/ci-docker.yml` | Tested image -> shared real transport gate; diagnostic directory -> explicitly named failure artifact. | Validation required; no permissive inventory-only option in CI. Artifact only after runtime step failure, missing files are errors, seven-day retention. Existing master/tag publish conditions and ordering unchanged; feature dispatch loads locally and cannot publish latest. | master/tag push or manual dispatch; CI/release maintainers. Full preflight and actual local image build/smoke before push. |
| `.github/workflows/ci-docker-pr.yml` | Newly built PR image -> same transport gate. | Read-only/test-only marked helper execution; no artifacts uploaded. Trusted-base-image-only path is unchanged and does not claim new-runtime coverage. | Reusable workflow only, no direct dispatch; CI maintainers. Shared helper exercised locally against final image; static policy/CodeQL pass. No PR created to invoke it. |
| `.github/workflows/ci-mcp-server.yml` | Shared scanner/build/smoke helper paths -> existing pytest and Dockerfile-check jobs. | Adds path-filter dependency edges; no package, permissions or workflow-dispatch input changes. | PR paths or manual dispatch; MCP CI maintainers. Full pytest and workflow static checks. |
| `docs/maintainer-qa-scans.md`, `docs/regression-container.md`, `iccdev-mcp/README.md` | Actual binary/library/transport behavior -> user commands and defaults. Unified container-only MCP config/example included. | Retired read variants removed from table; explicit overrides and optional ABI accurately documented; no unavailable second server recommended. | Documentation/Doxygen and maintainer use; docs/MCP maintainers. ASCII check, empty Doxygen warnings, commands exercised. |
| `.github/skills/regression-container-maintainer/SKILL.md`, `.github/prompts/regression-container-maintainer.prompt.md` | Shared local test/smoke commands -> maintainer workflow. | Runtime discovery, evidence directory and open-stdin contract; no cloud-first test/review loop. Existing AGENTS and other guidance already aligned and left untouched. | Maintainer tasks/preflight; agent-doc maintainers. Reviewed alongside implementation. |
| This report | Complete base-to-head contracts and local evidence -> handoff. | Historical measured counts, not release assertions; generated artifacts excluded from Git. Research-only setup correction kept outside upstream. | Branch handoff; maintainer evidence owner. ASCII and cumulative diff review. |

## Validation commands and results

Run from `/home/xss/qa/iccDEV-container-mcp-qa` unless noted:

```bash
docker pull ghcr.io/internationalcolorconsortium/iccdev:latest
docker build --check -f Dockerfile .
docker build --progress=plain --build-arg BUILD_JOBS=32 -t iccdev:container-mcp-qa-final -f Dockerfile .
python .github/scripts/iccdev-container-smoke.py iccdev:container-mcp-qa-final --report-dir out/final-container-smoke
python -m pytest iccdev-mcp/tests -q -rs
PREFLIGHT_BASE_REF=bde6cb128d112777ec4c1fe036e4ede02c9dedd1 .github/scripts/preflight-safety-checks.sh --require-tools
shellcheck .github/ci/docker/build-validation-library.sh .github/scripts/icc-*-qa-scan*.sh
doxygen .github/ci/doxygen/Doxyfile
test ! -s docs/generated/doxygen-warnings.log
git diff --check
```

Container pytest reused the existing host test environment read-only, not a new
runtime dependency installation:

```bash
docker run --rm -v "$PWD:/src:ro" -v /home/xss/work/copilot/tools/fuzz-reachability/.venv:/test-venv:ro -e PYTHONPATH=/src/iccdev-mcp:/test-venv/lib/python3.14/site-packages iccdev:container-mcp-qa-final python -m pytest /src/iccdev-mcp/tests -q -rs -p no:cacheprovider
docker run --rm iccdev:container-mcp-qa-final ctest --test-dir /workspace/build -R "^iccdev\.c-validation-dlopen$" --output-on-failure --no-tests=error
```

- Full Dockerfile built the sanitizer CLI/test binaries and isolated normal ABI,
  generated standard profiles, and passed its native validation assertion.
  Final build log contained no compiler/CMake warnings or sanitizer findings.
- Focused C ABI CTest: 1/1 passed; no full CTest/coverage run requested or used.
- Host pytest: 200 passed, 3 skipped (two native ABI cases unavailable on host,
  plus absent generated hybrid profiles). Container pytest: 201 passed, 2
  skipped (hybrid fixture absent, Node not installed). Both native cases passed
  inside the image; Node dashboard suite passed on host. No claimed hybrid
  accept-path coverage or architecture coverage beyond Linux amd64.
- PAWG: sRGB text and JSON each exit 0/PASS, zero issue counters. Dump validate
  and roundtrip intent-1: each PASS. Smallest registry integration used one
  checked-in sRGB profile with `--skip-download --max-profiles 1 --timeout 30`:
  all three child scanners PASS, SpecSep EXPECTED_REJECT (3 vs 8 channels),
  zero crashes/timeouts. No repeated external corpus download.
- ABI: real sRGB status OK; malformed bytes CRITICAL_ERROR; empty/missing,
  sanitizer-only and missing-symbol explicit libraries unavailable; valid
  explicit library wins an invalid build-directory hint. `ldd` resolved native
  dependencies; `nm -D` showed no ASAN/UBSAN imports in the isolated library,
  while the CLI library retained `__asan_option_detect_stack_use_after_return`.
- Final authoritative preflight: 0 failures, 0 skips. Includes YAML, actionlint,
  yamllint, ShellCheck, cache/prologue/canary policy, zizmor, hadolint, Trivy
  config, CodeQL Actions/Python, query resolution, Doxygen and permissions.
  CodeQL Actions: 40 existing reviewed exceptions, zero unreviewed findings;
  Python: zero findings. Zizmor: 4 ignored/29 suppressed, unchanged configuration.
  Existing advisory fork-gate token exposure, CodeQL Python dynload-path
  warnings and offline zizmor limitations were not silently treated as new fixes.
  Dependency installation in the first full image build emitted harmless
  missing-manpage update-alternatives warnings, not compiler warnings.
- During local validation the artifact policy caught permissive missing-file
  handling; changed to error and final preflight passed. An initial registry
  invocation under host UID could not traverse the image-owned `/workspace`
  (0750); rerunning as the intended image user with the host evidence group
  passed. No image permission broadening was needed. All smoke containers removed.

## Exact runtime capability inventory

Final MCP discovery (26 names, measured rather than asserted as a constant):

```text
apply_named_cmm apply_profiles apply_search color_transform create_link
dump_profile enum_spaces from_cube health_check icc_sig_to_str inspect_header
jpeg_dump json_to_profile list_available_profiles pawg_report png_dump
profile_summary profile_to_json profile_to_xml round_trip_test roundtrip_delta
spec_sep_to_tiff tiff_dump v5_to_v4 validate_profile xml_to_profile
```

Native: `inspect_header`, `profile_summary`, `color_transform`, `roundtrip_delta`,
`icc_sig_to_str`, `enum_spaces`, `validate_profile`. Services: `health_check`,
`list_available_profiles`. CLI inventory:

```text
iccDumpProfile iccToXml iccFromXml iccToJson iccFromJson iccTiffDump iccJpegDump
iccPngDump iccFromCube iccApplyProfiles iccApplyNamedCmm iccApplyToLink
iccV5DspObsToV4Dsp iccSpecSepToTiff iccApplySearch iccRoundTrip iccPawgReport
```

All are available in the final image, with 225 discovered profiles. Baseline
advertised the same MCP tool names but had only 25 available capabilities:
`validation_api.available=false`. Final MCP and REST both report validation
available and actually return native OK for sRGB.

REST inventory (26 MCP-backed routes): `inspect_header`, `profile_summary`,
`validate_profile`, `color_transform`, `roundtrip_delta`, `sig_to_str`,
`enum_spaces`, `list_profiles`, `health_check`, `dump_profile`, `pawg_report`,
`to_xml`, `from_xml`, `to_json`, `from_json`, `round_trip_test`, `tiff_dump`,
`jpeg_dump`, `png_dump`, `from_cube`, `apply_profiles`, `apply_named_cmm`,
`create_link`, `v5_to_v4`, `spec_sep`, `apply_search`. REST-only utilities:
`tools_inventory`, `list_files`, `upload_file`. Exact methods/paths and capability
objects are in the local JSON evidence, not duplicated as test constants.

## Evidence and dispatch handoff

- Local runtime artifacts: `out/final-container-smoke/{image,mcp,rest}.json`.
- Negative baseline: `out/baseline-negative/` includes the expected failure log.
- No-network registry: `out/registry-confirmation/` includes TSV, findings,
  summaries and SpecSep rejection evidence.
- Tool-managed complete build log:
  `/tmp/1788654936000-copilot-tool-output-2387905-5c42a8ca-9988-4b7e-8d5c-4436e982daf2.txt`.
- Tool-managed final preflight log:
  `/tmp/1788655017811-copilot-tool-output-2387905-38aae98e-060a-46aa-853e-6997f645d6c0.txt`.
- Generated docs, binaries, pytest caches and logs are not committed.

Inspected dispatch semantics before publication: `ci-docker.yml` and
`ci-mcp-server.yml` accept no dispatch inputs; `ci-preflight-safety.yml` is
directly dispatchable; `ci-pr-action.yml` supports `ci_scope=governance` without
an open PR and reuses risk analysis at the exact target SHA. Its `fast-lane`
mode needs an open same-repository PR and is not selected. `ci-docker-pr.yml`
is reusable only. Do not create a PR, request review, or dispatch duplicate
risk analysis. Check existing runs after normal SSH push, then dispatch each
selected workflow once with `--ref ci-qa-pr-docker-testing`. The caller handoff
records actual commit/push/run IDs and statuses; this pre-push report does not
claim future hosted success.

Research-only correction: `docs/MCP_SERVER_SETUP.md` in `xsscx/research` now
documents the already registered unified `iccdev` MCP server and removes the
unavailable second-server recommendation. Existing `.mcp.json` was not changed;
only its non-secret `iccdev` entry was inspected. No ambiguous CLI/client
registration was invented. No research file is part of this upstream diff.
