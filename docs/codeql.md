# CodeQL Security Analysis

iccDEV ships custom CodeQL queries for ICC-specific security patterns. These
queries are maintainer-curated training and triage tools because recent custom
query rounds produced both useful findings and noisy false positives. The
canonical query reference lives with the queries:

- [`.github/codeql-queries/README.md`](https://github.com/InternationalColorConsortium/iccDEV/blob/master/.github/codeql-queries/README.md)

The targeted JSON query suite covers both `IccJSON` and `IccConnect` JSON
configuration code. Keep `.github/codeql-config.yml` and the local runner help
text in sync when that scope changes.
`iccdev-security-suite.qls` excludes JSON-specific queries, including
`json-size-to-narrow-setsize.ql`; the JSON suite is their sole reporting
surface. The cross-format `localized-text-unsafe-serialization.ql` query runs
only in the core suite.

Run local analysis with:

```bash
.github/scripts/run-codeql-local.sh
```

The runner reports only SARIF selected by the current invocation and fails if
an expected result is missing or invalid.

The local runner prefers a `codeql` executable on `PATH` and falls back to the
GitHub CLI `gh codeql` extension. The unified container includes the pinned
CodeQL bundle, so it can analyze the caller's current checkout without
installing the extension. Resolve the selected image to a digest, mount the
checkout read-only, copy it to container scratch, and copy the results back to
the caller:

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
2.27.5. All three workflows verify the official Linux release-asset SHA-256
before extraction.
Update the version and digest together in all four locations, then run:

```bash
python3 .github/scripts/check-codeql-bundle-pin-parity.py
```

The preflight gate runs this parity check for every invocation, so a partial
bump fails before the separate checksum checks can succeed independently.

`ci-codeql-query-tests.yml` runs the checked-in query unit tests under
`.github/codeql-queries/test` on same-repository PRs and `master` pushes
changing the queries or this workflow, or by manual dispatch.
It does not build the product database or upload SARIF; `codeql test run`
extracts each fixture into a temporary database. The pinned bundle keeps the
extractor line/column locations asserted by `.expected` files stable.
Each query fixture must include a reported vulnerable case and an unreported
guarded case, so a passing query test proves both detection and its intended
scope.

`ci-codeql-security.yml` builds the product C/C++ database and uploads three
SARIF categories:
`iccdev-standard-cpp-security-and-quality`, `iccdev-custom-security` and
`iccdev-jsonlib-security`. The standard category excludes
`cpp/path-injection`; the two custom categories run disjoint query suites.
Custom-query alerts still need maintainer triage before they are cited in
issues or PRs. Labeled PR scans use trusted-base queries; the read-only query
tests exercise proposed query changes.

The workflow also publishes `iccdev-codeql-review-report`, including the complete
text report and the QA detector ownership matrix. The report expands all
`iccdev/*` custom-query results and findings tagged `security`, even when SARIF
classifies them as warnings. This avoids hiding actionable locations behind an
error-only report summary. The matrix at `.github/ci/qa-detection-matrix.json`
is the canonical map from recent QA issues to their primary detector and local
evidence, tracked by
[#3094](https://github.com/InternationalColorConsortium/iccDEV/issues/3094).
Validate or render it with:

```bash
python3 .github/scripts/qa-detection-matrix.py \
  --input .github/ci/qa-detection-matrix.json \
  --repo-root . --markdown /tmp/qa-detection-matrix.md
```

CodeQL is not expected to report every runtime defect. A missing CodeQL alert
is actionable only when a query models the defect's source-to-sink or structural
pattern. Sanitizer, parser-regression, format round-trip, numerical-oracle, and
CLI-contract tests remain the primary detectors for behavior that static
analysis does not model. Coverage confirms that those detectors exercised code;
it does not establish that they asserted the required behavior.

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

`recursive-parse-no-depth-limit` covers recursive `Read`, `Parse`,
`ParseImport`, `Apply`, and composite `Validate` dispatch. Its query fixture
keeps vulnerable and depth-guarded controls for both direct recursion and
nested composite validation. Treat the latter as a structural signal for the
#3086 family, not proof that every reported composite is profile reachable.

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
checks, and ICC encoding tolerances into one generic numeric query: the removed
division-by-zero and broad float-range experiments are the false-positive
baseline for why those contracts must remain separate.

Workflow and Python-script governance uses the preflight CodeQL gates instead
of the C/C++ database runner:

```bash
PREFLIGHT_BASE_REF=origin/master .github/scripts/preflight-safety-checks.sh --require-tools
```
