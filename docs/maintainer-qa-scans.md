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
| `.github/scripts/iccdev-registry-profile-qa.sh` | registry runner | PAWG text, dump validate-all, roundtrip intent 1, SpecSep optional-profile sweep |

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

## Validation report levels

`iccDumpProfile -v` prints one line per finding, prefixed by the level the
validator assigned, and a single verdict line derived from the highest level
reached:

| Prefix | Verdict contribution | Meaning |
|--------|----------------------|---------|
| `Information - ` | none | A defined, conformant state worth naming in the report. Does not affect the verdict. |
| `Warning! - ` | `Profile has warning(s)` | Conforms, but with something a reader may want to act on. |
| `NonCompliant! - ` | `Profile violates ICC specification` | Does not conform; may still be usable. Note that `iccDumpProfile` still exits 0 here — parse the text, not `$?`, when this verdict matters. |
| `Error! - ` | `Profile has Critical Error(s)` | Does not conform and is not usable. |

Because a `QA-ISSUE` status keys off warning indicators, a diagnostic emitted at
the wrong level inflates the cohort a scan reports and buries the findings that
need attention. When a scan surfaces a large uniform cohort, check whether the
level is right before treating the profiles as suspect.

### Empty multi-process-element tags (#1809)

A `multiProcessElementType` tag holding zero processing elements is reported at
information level when its input and output channel counts are equal, and as a
critical error when they are not. The split follows what the library does with
the tag rather than a separate validation rule:

- Equal counts are the identity transform. `CIccTagMultiProcessElement::Read()`
  accepts a zero element count, `Write()` emits one, `Begin()` returns true
  without building an apply list, and `Apply()` copies the source pixel to the
  destination unchanged.
- Unequal counts have nothing to bridge the change, so `Begin()` refuses the
  shape and the tag cannot be applied at all.

Profiles are therefore not expected to omit an optional intent tag merely because
its chain is empty. Presence with an identity chain and absence are different
statements — the `PCC`, `ICS` and `mcs` fixtures under `Testing/` declare
`DToB3`/`BToD3` and the `AToB`/`BToA`/`AToM0` intent tags this way on purpose, to
advertise the channel counts of a pass-through connection.

Reporting the equal-channel case at information level moved 74 of the 210
profiles in a freshly generated corpus from `Profile has warning(s)` to
`Profile is valid`, with the critical-error count unchanged. Note that corpus
counts only reproduce from a clean checkout plus
`Testing/CreateAllProfiles.sh` — a working tree that has accumulated profiles
from earlier runs will report higher numbers.

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
`summary.md`. The SpecSep lane exercises the optional profile argument against
fixed checked-in inputs. It runs at the sweep's default eight channels, so a
profile is embedded only when its sample count is eight; anything else is a
clean rejection with no partial TIFF, and the sweep says so when it accepted
nothing. This runner does not expose the sweep's `--channels`, so to aim the
sample count at a particular corpus run
`.github/scripts/iccdev-specsep-profile-sweep.sh` directly. The APTEC characterization `.txt` sidecar is downloaded and
hashed for provenance, but the ICC command-line tools scan only `.icc` files.

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
file by every PAWG, DumpProfile, and RoundTrip variant and can be much slower
than the default CI smoke matrix. The SpecSep lane always runs one fixed-input
case per selected profile.

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

## Sanitizer Suppression Maintenance

Maintainers should keep sanitizer suppression changes small and evidence-backed.
There are two separate suppression surfaces:

| File | Purpose |
|------|---------|
| `Testing/silence.txt` | Runtime UBSAN suppressions for recoverable local or CI runs. |
| `.github/ci/ubsan-ignorelist.txt` | Compile-time sanitizer ignorelist for known-benign instrumentation sites. |

Use `Testing/silence.txt` when the sanitizer runtime loads suppressions and the
run is allowed to recover. Use `.github/ci/ubsan-ignorelist.txt` when a fatal
Clang IntegerSanitizer build aborts before runtime suppression can help, or when
the instrumentation site must be omitted during compilation. GCC does not use
the compile-time ignorelist path; keep a normal GCC build in the test matrix to
confirm Clang-only suppression flags are not applied to GCC.

Safe suppression candidates:

- Intentional unsigned wrapping arithmetic with a cited algorithm or invariant.
- Standard-library implementation paths, such as `*/include/c++/*/bits/...`,
  after stack triage shows no actionable project-owned frame.

Unsafe suppression candidates:

- Broad project paths such as `*/IccProfLib/*`, `*/IccConnect/*`, `*/Tools/*`,
  `*/cfl/*`, or workflow helper scripts.
- A sanitizer report whose first actionable frame is in iccDEV code.
- A timeout, signal, or graceful failure without sanitizer evidence.

For additions:

1. Save the exact unsuppressed replay command and sanitizer summary.
2. Add the narrowest pattern possible.
3. Rebuild when changing `.github/ci/ubsan-ignorelist.txt`.
4. Replay the same input and confirm the sanitizer report is gone.
5. Run normal GCC and Clang builds to verify suppression config does not break
   unsanitized compiler paths.
6. Run the sanitizer helper smoke tests:

   ```bash
   bash .github/tests/test_sanitization.sh
   ```

7. For CI parity, run GCC normal and Clang IntegerSanitizer builds in the
   regression Docker image:

   ```bash
   docker run --rm -v "$PWD":/work -w /work \
     ghcr.io/internationalcolorconsortium/iccdev:latest \
     bash -lc '
       CC=gcc CXX=g++ cmake -S Build/Cmake -B /tmp/iccdev-gcc-normal \
         -DENABLE_TOOLS=ON -DENABLE_TESTS=OFF -DENABLE_IMAGE_TOOLS=OFF \
         -DENABLE_CMM_TOOLS=ON -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=ON &&
       cmake --build /tmp/iccdev-gcc-normal --target iccApplyNamedCmm -j2 &&
       CC=clang CXX=clang++ cmake -S Build/Cmake -B /tmp/iccdev-intsan \
         -DENABLE_TOOLS=ON -DENABLE_TESTS=OFF -DENABLE_IMAGE_TOOLS=OFF \
         -DENABLE_CMM_TOOLS=ON -DENABLE_ICCXML=OFF -DENABLE_ICCJSON=ON \
         -DENABLE_INTEGER_SANITIZER=ON \
         -DUBSAN_IGNORELIST=.github/ci/ubsan-ignorelist.txt &&
       cmake --build /tmp/iccdev-intsan --target iccApplyNamedCmm -j2
     '
   ```

For removals:

1. Delete one pattern at a time.
2. Rebuild if it was an ignorelist entry.
3. Replay a representative input.
4. Keep the removal only when the old noise does not return, or when it returns
   as a project-owned report that is fixed separately.

Record the issue number, before/after commands, exit codes, and whether the
result depended on runtime suppression or compile-time ignorelist rebuild.

The developer report artifact preserves downloaded `profiles/` payloads when
registry QA is enabled. Keep `download-manifest.tsv` with the payloads so
reviewers can verify source URLs, byte counts, and SHA-256 values.

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
