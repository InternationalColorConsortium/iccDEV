# iccDEV CodeQL Queries

iccDEV includes custom CodeQL queries for ICC-specific C/C++ security patterns.
They complement GitHub's standard `cpp-security-and-quality` suite.

## Query Summary

| Query | CWE | Finds |
|-------|-----|-------|
| `signed-offset-overflow` | CWE-190 | Profile-derived offset used in pointer arithmetic |
| `unchecked-file-io` | CWE-252 | Ignored `fopen`, `fread`, or `fwrite` results |
| `unsafe-header-cast` | CWE-681 | Header field cast to narrower type |
| `snprintf-size-mismatch` | CWE-120 | Buffer smaller than formatted output |
| `unchecked-allocation` | CWE-252/476 | Allocation result used without null check |
| `unbounded-profile-loop` | CWE-400/834 | Loop bound from profile field without cap |
| `float-to-int-cast` | CWE-681 | Float-to-integer cast without range check |
| `division-by-zero-profile` | CWE-369 | Division by profile-derived value |
| `describe-without-validate` | CWE-476 | `Describe()` called without prior `Validate()` |
| `world-writable-output` | CWE-732 | File created with world-writable mode |
| `unsafe-tag-downcast` | CWE-843 | `FindTag()` result cast without type check |
| `tainted-format-string` | CWE-134 | Variable used as a `printf` format string |

## Running Locally

Prerequisites: GitHub CodeQL CLI or `gh codeql` extension, plus a working C/C++
toolchain.

```bash
.github/scripts/run-codeql-local.sh
.github/scripts/run-codeql-local.sh --custom-only
.github/scripts/run-codeql-local.sh --skip-build
```

Results are written to `codeql-results/`:

- `cpp-security-and-quality.sarif`
- `iccdev-security.sarif`
- `report.txt`

## Adding a Query

1. Create `.github/codeql-queries/my-query.ql` with `@kind problem` metadata.
2. Run `gh codeql pack install .github/codeql-queries`.
3. Test against a local database:
   `gh codeql database analyze <db> .github/codeql-queries/my-query.ql --format=sarif-latest --output=/tmp/test.sarif`
4. Verify results before committing.

Standard-suite findings such as CLI path input or intentional float comparisons
may be informational rather than vulnerabilities; triage them in context.
