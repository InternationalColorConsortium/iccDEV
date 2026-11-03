# CodeQL Security Analysis

iccDEV ships custom CodeQL queries for ICC-specific security patterns. These
queries are maintainer-local training and triage tools because recent custom
query rounds produced both useful findings and noisy false positives. The
canonical query reference lives with the queries:

- [`.github/codeql-queries/README.md`](https://github.com/InternationalColorConsortium/iccDEV/blob/master/.github/codeql-queries/README.md)

The targeted JSON query suite covers both `IccJSON` and `IccConnect` JSON
configuration code. Keep `.github/codeql-config.yml` and the local runner help
text in sync when that scope changes.
`iccdev-security-suite.qls` excludes JSON-derived queries, including
`json-size-to-narrow-setsize.ql`; the JSON suite is their sole reporting
surface.

Run local analysis with:

```bash
.github/scripts/run-codeql-local.sh
```

The local runner prefers a `codeql` executable on `PATH` and falls back to the
GitHub CLI `gh codeql` extension. It fails closed when that CLI does not match
the Dockerfile bundle pin; `--allow-version-mismatch` is reserved for explicit
research A/B runs. Evaluator defaults are four threads and 8192 MiB to avoid
the unbounded-thread out-of-memory failure seen with `--threads=0`; override
them with `--codeql-threads`, `--codeql-ram`, or the corresponding
`ICCDEV_CODEQL_THREADS` / `ICCDEV_CODEQL_RAM_MB` environment variables. The
unified container includes the pinned CodeQL bundle, so it can analyze the
caller's current checkout without installing the extension. Resolve the
selected image to a digest, mount the checkout read-only, copy it to container
scratch, and copy the results back to the caller:

```bash
IMAGE_TAG=ghcr.io/internationalcolorconsortium/iccdev:latest
WORKTREE="$PWD"
RESULTS="$WORKTREE/codeql-results"
docker pull "$IMAGE_TAG"
IMAGE="$(docker image inspect "$IMAGE_TAG" --format '{{index .RepoDigests 0}}')"
IMAGE_REVISION="$(docker image inspect "$IMAGE_TAG" --format '{{index .Config.Labels "org.opencontainers.image.revision"}}')"
printf 'digest=%s revision=%s\n' "$IMAGE" "$IMAGE_REVISION"
mkdir -p "$RESULTS"
docker run --rm --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -v "$WORKTREE:/src:ro" \
  -v "$RESULTS:/results" \
  "$IMAGE" bash -lc '
    set -euo pipefail
    work="$(mktemp -d)"
    cp -a --no-preserve=ownership /src/. "$work/iccDEV"
    cd "$work/iccDEV"
    .github/scripts/run-codeql-local.sh --custom-only
    cp -a codeql-results/. /results/
  '
```

The bootstrap paths in `ci-codeql-security.yml`, `ci-preflight-safety.yml`,
`ci-codeql-query-tests.yml`, and the Dockerfile are pinned to CodeQL bundle
2.27.3 and verify the official Linux release-asset SHA-256 before extraction.
Update the version and digest together in all four locations, then run:

```bash
python3 .github/scripts/check-codeql-bundle-pin-parity.py
```

The preflight gate runs this parity check for every invocation, so a partial
bump fails before the separate checksum checks can succeed independently.

`ci-codeql-query-tests.yml` runs the checked-in query unit tests under
`.github/codeql-queries/test` on any PR touching `.github/codeql-queries/**`.
It builds no database. Unlike the other two it never falls back to a `codeql`
already on the runner's PATH, because `.expected` files assert extractor
line/column locations exactly and an unpinned CLI can flip them.
Each query fixture must include a reported vulnerable case and an unreported
guarded case, so a passing query test proves both detection and its intended
scope.

The GitHub Actions CodeQL workflow uploads three SARIF categories:
`iccdev-standard-cpp-security-and-quality`, `iccdev-custom-security` and
`iccdev-jsonlib-security`. Custom-query alerts still need maintainer triage
before they are cited in issues or PRs.
Both hosted CI and the local runner set those categories in SARIF and generate
`codeql-results/fingerprints.tsv`. `report.txt` records each raw category's
count, CodeQL version, and SHA-256 over its sorted fingerprint rows. Compare
those raw values rather than the Security UI's open-alert count, which omits
dismissed findings. A mode-selective local run reports only suites executed in
that invocation, and `--skip-build` forces query reevaluation so an older BQRS
cannot masquerade as the current query result.
The local runner uses the same `iccdev-cpp-security-and-quality.qls` wrapper as
hosted CI. The historical filename and SARIF category are retained for alert
continuity, but the wrapper now contains the default code-scanning selection
plus three explicitly reviewed stock rules. Invoking GitHub's raw
security-and-quality suite directly reintroduces 79 reviewed
`cpp/path-injection` alerts and 152 other style/documentation findings, so it
is not a comparable baseline.
Hosted CI installs `pkg-config` and `libheif-dev`, matching the unified
Dockerfile and the reference local database. The previous hosted run omitted
`iccHeifDump.cpp`; that dependency mismatch accounted for its two-finding
standard-suite delta and one-finding custom-suite delta.

`division-by-zero-profile-v2` now fingerprints the five historical profile/CAM
families fixed in #1406, #1410, #1679, #1782, and #1950. It requires a
denominator guard to control the division, so the #1837 early-return case is a
negative regression instead of an alert. `float-to-int-cast-v2` similarly
covers the named #1396 and #2230 conversion boundaries and requires finiteness
plus two-sided range validation on the converted value. It also covers the
three interpolation casts behind dismissed alerts #1086-#1088: `[0,1]`
relational clamps do not constrain NaN. These are intentionally high-precision
regression queries: broad denominator-name and five-line proximity heuristics
are the recorded noise baselines, not templates to reuse. See the
[dismissed-alert audit](codeql-dismissed-alert-audit.md) for the sampling,
three-alert reversal recommendation, and executable proof.
For `unbounded-profile-loop`, trace the full profile-to-tool path before
synthesizing a PoC. A guard in `Begin()` may establish an Apply-path invariant;
alerts #999, #1000, and #1647 are the reference case. If every tool path
propagates the setup failure, add a focused query regression with both a
suppressed guarded case and a reported unguarded control. Do not use a direct
library call that violates the public Begin-before-Apply contract as evidence
of profile reachability.
Similarly, do not classify internal scheduler state or an allocation-sized
derived count as profile-controlled solely because its member name contains
`count`. Alerts #2360 and #881 are the narrow reference exclusions; retain the
type-and-field scope when maintaining them.
The XML narrowing query is scoped to high-signal channel and spectral-step
attributes; broader enum, reserved, storage-type, and explicit-cast cases are
left for separate local experiments.

`fixed-buffer-loop-bound` covers the #2608 stack-buffer-overflow shape: a
fixed-size aggregate position table indexed by a loop whose member-count bound
has a wider range than the array. The standard `cpp/static-buffer-overflow` query is
not a substitute for this rule: its fixed-buffer helper is character-array
specific, and its classic-loop model requires a literal bound. Keep the query
test's 16-entry/16-iteration negative control so broad integer type ranges do
not turn a safe fixed loop into an alert.

`fixed-buffer-transform-output` covers the shared #2868 and #2886
cross-function variant: a fixed local buffer is forwarded through one or more
helpers to `CIccCmm::Apply()` without a recognized `GetDestSamples()` capacity
guard. The nested query fixture mirrors the vulnerable
`CalcFactors()`/`calcBlackPoint()`/`calcSrcBlackPoint()` path to the
three-element black-point buffer. Keep its literal and parameterized guarded
buffers, mismatched capacities, different transform, non-transform, and
dynamically sized controls so the rule stays scoped to unsafe fixed-output
wrappers. Only a direct `GetDestSamples() > capacity` check for the applied CMM
counts as a guard, and a capacity parameter must receive the fixed buffer's
actual size as a literal argument directly at the guarded wrapper call.
Capacity parameters forwarded through additional helpers are not resolved;
their medium-precision alerts require manual verification, not removal of a
valid guard. The forwarded-capacity fixture pins this conservative limitation.
Arithmetic expressions involving the destination count are also outside the
recognized capacity contract.

`counted-buffer-off-by-one` covers the nearby counted-buffer family. Review the allocation count, loop count, and
serialization count as one contract. In particular, `index > count` does not
protect `buffer[index]` when `index == count`; valid indices end at
`count - 1`. A query for that boundary family should require the same index,
count field, and buffer access in one function. The query deliberately excludes
arithmetic bounds such as `index > count - 1`; its fixture keeps that negative
control because a repository-wide textual `>` rule is too noisy.

`self-alias-owning-setter` covers the public owning-setter pattern in
InternationalColorConsortium/iccDEV#2631: deleting a pointer member and then
storing an unguarded pointer parameter allows `SetX(GetX())` to retain a
dangling pointer. The rule requires both operations in the same member function
and accepts an earlier equality-and-return guard, avoiding broad ownership or
copy-semantics findings.

Numerical accuracy findings need separate, semantics-aware queries. Prefer
narrow rules for a named conversion family or formula, with an executable
oracle that pins finite-domain, boundary, and round-trip error behavior. Do
not combine NaN/Inf guards, fixed-point reconstruction, float-to-int range
checks, and ICC encoding tolerances into one generic numeric query. The
retired broad DBZ/float experiments remain the A/B noise baseline; the restored
queries are historical regression fingerprints. The retired zero-signature
query remains research-only until it can detect the known unguarded serializer
sites as well as suppress guarded ones.

## Stock-query A/B reference

The 2026-11-03 CodeQL 2.27.4 reference database records these decisions:

| Candidate | Result | CI decision |
|-----------|--------|-------------|
| Raw `cpp-security-and-quality` | 328 alerts, including 79 reviewed `cpp/path-injection` alerts | Do not upload the raw suite |
| Previous repository-filtered wrapper | 249 alerts: 17 error, 61 warning, 171 note | Replace its quality/style tail while retaining its filename and SARIF category for alert continuity |
| Default `cpp-code-scanning` | Zero findings on the fixed-source database | Use as the high-precision stock base |
| Default plus three reviewed stock rules | 18 findings: 15 allocation-size, two inconsistent-null-check, one world-writable output | Upload this targeted wrapper; 231 fewer findings than the previous wrapper |
| `cpp-security-extended` | 95 findings: 79 path-injection, 15 allocation-size, one world-writable output | Do not upload; it adds no useful family beyond the targeted wrapper |
| `cpp-security-experimental` | 196 findings: 79 path-injection, 69 expired-lifetime, 31 allocation-multiplication, 15 allocation-size, one signed pointer-arithmetic conversion, one world-writable output | Manual research only; sampled lifetime reports mis-model live containers/iterators, while sampled multiplication reports include existing 64-bit guards and copy paths |
| Experimental `cpp/divide-by-zero-using-return-value` | No SARIF after a 10-minute ceiling, about 4.3 GiB RSS, and sustained two-core use | Manual/periodic research only |
| `cpp/uncontrolled-allocation-size` | 15 existing standard-suite alerts | Retain for manual tag-size/allocation-contract triage; do not duplicate with a broad custom rule |
| `cpp/inconsistent-null-check` | Two existing standard-suite alerts; reviewed default-string and transform-invariant cases were not reachable defects | Retain as review prompts, not blocking custom findings |

The wrapper remains named `iccdev-cpp-security-and-quality.qls`, and its SARIF
category remains `iccdev-standard-cpp-security-and-quality`, to avoid needless
alert-identity churn. Its contents are now the default code-scanning selection
plus the three explicit rules above.

The refined custom suite reduced the same-database noise baseline from 20
reviewed alerts to four actionable findings: three NaN interpolation casts and
one unchecked calculator apply allocation. UBSan/ASan reproduced all four
before their fixes; the final fixed-source database reports zero custom
findings. The full A/B and fingerprint digest are recorded in the
[dismissed-alert audit](codeql-dismissed-alert-audit.md).

The closed SBO/HBO issue fingerprints remain covered by narrow custom rules:
`unbounded-clut-index` (#1548), `fixed-buffer-loop-bound` (#2608), and
`fixed-buffer-transform-output` (#2868/#2886). Add another rule only when a
closed or newly reproduced bug supplies a positive mutant and a correctly
bounded negative control.

Workflow and Python-script governance uses the preflight CodeQL gates instead
of the C/C++ database runner:

```bash
PREFLIGHT_BASE_REF=origin/master .github/scripts/preflight-safety-checks.sh --require-tools
```
