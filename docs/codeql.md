## CodeQL Security Analysis

iccDEV ships 12 custom CodeQL queries targeting ICC-specific security
patterns. These complement the standard `cpp-security-and-quality` suite
from GitHub.

### Custom Query Summary

| Query | CWE | What It Finds |
|-------|-----|---------------|
| `signed-offset-overflow` | CWE-190 | Profile-derived offset used in pointer arithmetic |
| `unchecked-file-io` | CWE-252 | fopen/fread/fwrite return values ignored |
| `unsafe-header-cast` | CWE-681 | Header field cast to narrower type |
| `snprintf-size-mismatch` | CWE-120 | snprintf buffer smaller than format output |
| `unchecked-allocation` | CWE-252/476 | new/malloc result used without null check |
| `unbounded-profile-loop` | CWE-400/834 | Loop bound from profile field without cap |
| `float-to-int-cast` | CWE-681 | Float-to-integer cast without range check |
| `division-by-zero-profile` | CWE-369 | Division by profile-derived value |
| `describe-without-validate` | CWE-476 | Describe() called without prior Validate() |
| `world-writable-output` | CWE-732 | File created with 0666/world-writable mode |
| `unsafe-tag-downcast` | CWE-843 | FindTag() result cast without type check |
| `tainted-format-string` | CWE-134 | Variable used as printf format string |

Queries live in `.github/codeql-queries/` with a qlpack
(`iccdev/security-queries`) and suite file
(`iccdev-security-suite.qls`).

### Running Locally

Prerequisites: `gh codeql` extension installed, clang/clang++ available.

```bash
# Full analysis (standard + custom queries, builds from source)
.github/scripts/run-codeql-local.sh

# Custom queries only (faster)
.github/scripts/run-codeql-local.sh --custom-only

# Skip rebuild (reuse existing database)
.github/scripts/run-codeql-local.sh --skip-build

# Specify output directory
.github/scripts/run-codeql-local.sh --db-dir /tmp/my-codeql-db
```

Results are written to `codeql-results/`:
- `cpp-security-and-quality.sarif` -- standard suite findings
- `iccdev-security.sarif` -- custom query findings
- `report.txt` -- human-readable summary

### Reading the Report

The report generator (`.github/scripts/codeql-report.sh`) parses SARIF
files and produces:

1. **Severity Summary** -- error/warning/note counts
2. **Findings by Rule** -- sorted by count
3. **Top 20 Files** -- highest finding density
4. **Component Breakdown** -- IccProfLib, CLI Tools, IccLibXML, etc.
5. **CWE Summary** -- mapped weakness counts
6. **Error-Level Details** -- full location for any error-severity findings

### CI Workflow

The `ci-codeql-security.yml` workflow runs on:
- Push to `master`
- Pull requests touching source files
- Weekly schedule (Sunday 03:00 UTC)
- Manual dispatch

Results are uploaded as a build artifact (30-day retention). SARIF is
NOT uploaded to the GitHub Security tab.

### Adding a New Query

1. Create `.github/codeql-queries/my-query.ql` with `@kind problem`
   metadata
2. Run `gh codeql pack install .github/codeql-queries` to resolve deps
3. Test: `gh codeql database analyze <db> .github/codeql-queries/my-query.ql --format=sarif-latest --output=/tmp/test.sarif`
4. Verify results, then commit

### Known Informational Findings

Some standard-suite findings are expected and not bugs:

- `cpp/path-injection` -- CLI tools inherently accept user file paths
- `cpp/equality-on-floats` -- intentional ICC s15Fixed16 comparisons
- `cpp/commented-out-code` -- debug/reference code blocks
- `cpp/poorly-documented-function` -- style, not security
