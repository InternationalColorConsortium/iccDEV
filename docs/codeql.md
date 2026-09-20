# CodeQL Security Analysis

iccDEV ships custom CodeQL queries for ICC-specific security patterns. These
queries are maintainer-local training and triage tools because recent custom
query rounds produced both useful findings and noisy false positives. The
canonical query reference lives with the queries:

- [`.github/codeql-queries/README.md`](https://github.com/InternationalColorConsortium/iccDEV/blob/master/.github/codeql-queries/README.md)

The targeted JSON query suite covers both `IccJSON` and `IccConnect` JSON
configuration code. Keep `.github/codeql-config.yml` and the local runner help
text in sync when that scope changes.

Run local analysis with:

```bash
.github/scripts/run-codeql-local.sh
```

The bootstrap path in `ci-codeql-security.yml`, `ci-preflight-safety.yml` and
`ci-codeql-query-tests.yml` is pinned to CodeQL bundle 2.27.0 and verifies the
official Linux release-asset SHA-256 before extraction. Update the version and
digest together in all three workflows -- a bump applied to only some of them
fails the others at their next `sha256sum -c`.

`ci-codeql-query-tests.yml` runs the checked-in query unit tests under
`.github/codeql-queries/test` on any PR touching `.github/codeql-queries/**`.
It builds no database. Unlike the other two it never falls back to a `codeql`
already on the runner's PATH, because `.expected` files assert extractor
line/column locations exactly and an unpinned CLI can flip them.

The GitHub Actions CodeQL workflow uploads only the standard
`cpp-security-and-quality` SARIF. Run custom iccDEV query suites locally and
attach report excerpts to issues or PRs only after maintainer triage.
This includes the division-by-zero profile query, which has produced useful
fixes but also guard-sensitive false positives.
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

`counted-buffer-off-by-one` covers the nearby counted-buffer family. Review the allocation count, loop count, and
serialization count as one contract. In particular, `index > count` does not
protect `buffer[index]` when `index == count`; valid indices end at
`count - 1`. A query for that boundary family should require the same index,
count field, and buffer access in one function. The query deliberately excludes
arithmetic bounds such as `index > count - 1`; its fixture keeps that negative
control because a repository-wide textual `>` rule is too noisy.

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
