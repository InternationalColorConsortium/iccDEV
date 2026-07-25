# Maintainer QA scans

This page documents headless command-line QA scans for maintainers who need
broad profile coverage without turning every warning into a CTest regression.

The scan scripts live under `.github/scripts/` and are designed for local shells,
scheduled CI, workflow dispatch jobs, and post-build tool-test jobs. They do not
replace focused CTest regressions. Use CTest for fixed bugs with stable
invariants; use these scans for broad corpus sweeps, fault testing, and external
profile compatibility reporting.

## Scripts

| Script | Tool | Default variants |
|--------|------|------------------|
| `.github/scripts/icc-pawg-qa-scan.sh` | `iccPawgReport` | text, JSON, eager-read, JSON eager-read |
| `.github/scripts/icc-dumpprofile-qa-scan.sh` | `iccDumpProfile` | basic, validate, verbosity, tag, `ALL`, read, diagnostic read |
| `.github/scripts/icc-roundtrip-qa-scan.sh` | `iccRoundTrip` | intents 0-3 with LUT and MPE modes |
| `.github/scripts/iccdev-registry-profile-qa.sh` | registry runner | PAWG text, dump validate-all, roundtrip intent 1 |

Each tool scanner writes:

| File | Purpose |
|------|---------|
| `logs/<variant>-<file>.log` | complete stdout/stderr for one tool run |
| `results.tsv` | one row per file and variant |
| `findings.txt` | matched diagnostic lines plus synthetic timeout/signal rows |
| `summary.md` | sanitized Markdown summary suitable for CI artifacts or review |

Use `--log-tail-lines N` to bound archived per-run logs after classification.
Counts, status, and `findings.txt` are computed from the complete original
output before truncation. Use `--log-tail-lines 0` when preserving complete raw
logs is more important than artifact size.

Statuses are:

| Status | Meaning |
|--------|---------|
| `PASS` | exit 0 and no matched diagnostic indicators |
| `QA-ISSUE` | exit 0 plus warning/error/PAWG issue indicators |
| `FAIL` | non-zero exit without sanitizer or signal evidence |
| `CRASH` | sanitizer finding, signal text, or signal-like exit code |
| `TIMEOUT` | `timeout(1)` exit 124 |

By default, scanners exit non-zero only for `CRASH,TIMEOUT`. Use
`--fail-on none` for inventory-only reporting, or `--fail-on CRASH,TIMEOUT,FAIL`
for stricter gates. The registry workflow intentionally uses `--fail-on CRASH`
because its source list includes malformed profiles and full-matrix variants
that exercise expected non-pass validation paths.

## Local examples

Build the tools first, then run scans from the repository root:

```bash
cmake -S Build/Cmake -B build -DENABLE_TOOLS=ON -DENABLE_TESTS=ON
cmake --build build --parallel "$(nproc)"

.github/scripts/icc-pawg-qa-scan.sh --timeout 10 --progress Testing
.github/scripts/icc-dumpprofile-qa-scan.sh --timeout 10 --variant validate-all Testing
.github/scripts/icc-roundtrip-qa-scan.sh --timeout 10 --variant intent-1 Testing
```

For build trees that place tools outside `Build/Tools`, pass explicit tool paths:

```bash
.github/scripts/icc-pawg-qa-scan.sh \
  --tool "$PWD/build/Tools/IccPawgReport/iccPawgReport" \
  --timeout 10 \
  Testing
```

Use `--list-variants` to see available option matrices:

```bash
.github/scripts/icc-pawg-qa-scan.sh --list-variants
.github/scripts/icc-dumpprofile-qa-scan.sh --list-variants
.github/scripts/icc-roundtrip-qa-scan.sh --list-variants
```

## ICC profile registry scans

The registry source list is maintained in:

```text
.github/ci/profile-registry/profile-source-list.txt
```

Run the default CI-sized sweep:

```bash
.github/scripts/iccdev-registry-profile-qa.sh \
  --timeout 20 \
  --log-tail-lines 2000 \
  --out-dir "$PWD/out/registry-qa"
```

This downloads the listed ICC registry and malformed-security profiles, records
`download-manifest.tsv`, scans `.icc` files, and writes a top-level
`summary.md`. The APTEC characterization `.txt` sidecar is downloaded and hashed
for provenance, but the ICC command-line tools scan only `.icc` files.

Use a bounded smoke test during workflow bring-up:

```bash
.github/scripts/iccdev-registry-profile-qa.sh \
  --timeout 10 \
  --max-profiles 3 \
  --out-dir "$PWD/out/registry-qa-smoke"
```

Use a previously downloaded directory for no-network reruns:

```bash
.github/scripts/iccdev-registry-profile-qa.sh \
  --skip-download \
  --profile-dir "$PWD/out/registry-qa/profiles" \
  --out-dir "$PWD/out/registry-qa-rerun"
```

Use `--full-matrix` only for scheduled or manual jobs. It multiplies each ICC
file by every tool variant and can be much slower than the default CI smoke
matrix.

## CI wiring

`.github/workflows/ci-iccdev-tool-tests.yml` exposes the registry profile scan as
an opt-in maintainer/tool-test gate. It reuses the ASAN+UBSAN tool build from
the normal `ci-regression-checks` job and writes registry QA outputs under:

```text
build/Testing/ctest-output/registry-profile-qa/
```

Workflow inputs:

| Input | Default | Purpose |
|-------|---------|---------|
| `run_registry_qa` | `false` | Enable the registry profile QA scan. |
| `registry_qa_timeout` | `20` | Per-tool timeout in seconds. |
| `registry_qa_max_profiles` | `3` | Maximum ICC registry profiles for CI smoke; `0` means all. |
| `registry_qa_full_matrix` | `false` | Run every PAWG, DumpProfile, and RoundTrip variant. |
| `registry_qa_log_tail_lines` | `2000` | Final per-run log lines retained in the artifact after classification; `0` keeps full logs. |

Failure policy in the workflow is `CRASH`. `QA-ISSUE`, `FAIL`, and `TIMEOUT`
rows remain reporting signal unless maintainers promote a specific finding to a
focused regression.

Large external profiles can produce hundreds of thousands of validation lines.
The workflow keeps bounded log excerpts by default so artifacts stay reviewable;
use `results.tsv`, `findings.txt`, and `summary.md` for authoritative status,
and rerun with `registry_qa_log_tail_lines=0` only when complete raw output is
needed for diagnosis.

The developer report artifact omits downloaded `profiles/` payloads to avoid
duplicating large external registry inputs. Use `download-manifest.tsv` for the
source URLs, byte counts, and SHA-256 values, then rerun the registry script to
recreate the profile directory when raw inputs are needed.

Suggested use:

| Workflow area | Suggested input set | Failure policy |
|---------------|---------------------|----------------|
| Tool workflow smoke | `run_registry_qa=true`, `registry_qa_max_profiles=3` | `CRASH` |
| Nightly/scheduled tools | `run_registry_qa=true`, `registry_qa_max_profiles=0`, `registry_qa_full_matrix=true` | `CRASH` |
| ci-regression post-build | single tool scanner with `--variant` | `CRASH,TIMEOUT,FAIL` for fixed bug repros |
| Manual maintainer dispatch | registry runner with custom `--url-list` | maintainer-selected |

Keep workflow steps hardened:

```bash
set -euo pipefail
git config --global credential.helper ""
unset GITHUB_TOKEN || true
```

Recommended workflow shape:

1. Build `ENABLE_TOOLS=ON`.
2. Run `.github/scripts/iccdev-registry-profile-qa.sh`.
3. Upload the output directory as an artifact.
4. Append or attach `summary.md` for reviewer convenience.
5. Keep downloaded profiles out of git; rerun from the URL list when needed.

Do not add caches for downloaded profiles. The registry sweep is an external
compatibility check, and the checked-in URL list plus manifest provide the
reproducibility record.

## When to promote a finding

Promote a scan finding to a focused regression when it is one of:

1. Sanitizer finding or signal crash.
2. Repeatable timeout that exceeds the selected CI budget.
3. Graceful failure that contradicts a supported command contract.
4. PAWG or validation output that reveals an incorrect invariant.

Minimize the profile or generate it in a script, add a focused CTest or
`.github/scripts/iccdev-*.sh` regression, then reference it from
`.github/ci/regression/README.md`.
