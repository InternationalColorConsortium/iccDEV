# iccDEV CodeQL Queries

iccDEV includes custom CodeQL queries for ICC-specific C/C++ security patterns.
They complement GitHub's standard `cpp-security-and-quality` suite.

## Query Summary

| Query | CWE | Finds |
|-------|-----|-------|
| `argv-unchecked-index` | CWE-125, CWE-787 | CmdLine tool indexes argv without bound check |
| `describe-without-validate` | CWE-476 | `Describe()` called without prior `Validate()` check |
| `signed-offset-overflow` | CWE-190 | Profile-derived offset used in pointer arithmetic |
| `unchecked-file-io` | CWE-252 | Ignored `fopen`, `fread`, or `fwrite` results |
| `unsafe-header-cast` | CWE-681 | Header field cast to narrower type |
| `snprintf-size-mismatch` | CWE-120 | Buffer smaller than formatted output |
| `unchecked-allocation` | CWE-252, CWE-476 | Allocation result used without null check |
| `unbounded-profile-loop` | CWE-400, CWE-834 | Loop bound from profile field without cap |
| `float-to-int-cast` | CWE-681 | Float-to-integer cast without range check |
| `division-by-zero-profile` | CWE-369 | Division by profile-derived value |
| `world-writable-output` | CWE-732 | File created with world-writable mode |
| `unsafe-tag-downcast` | CWE-843 | `FindTag()` result cast without type check |
| `tainted-format-string` | CWE-134 | Variable used as a `printf` format string |
| `factory-bare-new` | CWE-252, CWE-755 | Factory allocation without `std::nothrow` |
| `fixedpoint-type-confusion` | CWE-681, CWE-197 | Fixed-point conversion function called with wrong type |
| `json-raw-normalized-value` | CWE-682 | JSON serialization of raw normalized ICC value |
| `json-recursive-parse` | CWE-674, CWE-400 | Recursive JSON structure without depth limit |
| `json-size-overflow` | CWE-680, CWE-190 | JSON array size cast to narrow integer without bounds check |
| `json-unchecked-type-access` | CWE-704, CWE-20 | JSON `.get<T>()` without type guard |
| `multiplication-overflow-alloc` | CWE-190, CWE-680, CWE-787 | Multiplication overflow before allocation |
| `new-array-delete-mismatch` | CWE-762 | `new[]` allocation released with scalar `delete` |
| `null-after-read` | CWE-476 | Pointer dereferenced after `Read()` / `ParseXml()` without null check |
| `recursive-parse-no-depth-limit` | CWE-674 | Recursive parse/read/apply call lacks depth guard |
| `rule-of-three-violation` | CWE-415, CWE-416 | Destructor frees member without copy constructor / assignment operator |
| `u8fixed8-reconstruction-error` | CWE-682 | Incorrect u8Fixed8Number reconstruction from normalized float |
| `unterminated-ucs2-settext` | CWE-125, CWE-170 | Unterminated UCS-2 buffer passed to `SetText()` |

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
- `iccdev-jsonlib-security.sarif`
- `report.txt`

## Adding a Query

1. Create `.github/codeql-queries/my-query.ql` with `@kind problem` metadata.
2. Run `gh codeql pack install .github/codeql-queries`.
3. Test against a local database:
   `gh codeql database analyze <db> .github/codeql-queries/my-query.ql --format=sarif-latest --output=/tmp/test.sarif`
4. Verify results before committing.

Standard-suite findings such as CLI path input or intentional float comparisons
may be informational rather than vulnerabilities; triage them in context.
