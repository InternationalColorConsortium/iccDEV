# iccDEV CodeQL Queries

iccDEV includes custom CodeQL queries for ICC-specific C/C++ security patterns.
They complement a high-signal stock suite: GitHub's default C++ code-scanning
selection plus `cpp/uncontrolled-allocation-size`,
`cpp/inconsistent-null-check`, and `cpp/world-writable-file-creation`. The
`iccdev-mcp/` subdirectory contains Python queries for the MCP REST/API package.

## Query Summary

| Query | CWE | Finds |
|-------|-----|-------|
| `argv-unchecked-index` | CWE-125, CWE-787 | CmdLine tool indexes argv without bound check |
| `describe-without-validate` | CWE-476 | `Describe()` called without prior `Validate()` check |
| `signed-offset-overflow` | CWE-190 | Profile-derived offset used in pointer arithmetic |
| `division-by-zero-profile` | CWE-369 | Rule ID `iccdev/division-by-zero-profile-v2`: unguarded reintroduction of the profile/CAM denominator defects fixed in #1406, #1410, #1679, #1782, or #1950 |
| `float-to-int-cast` | CWE-681 | Rule ID `iccdev/float-to-int-cast-v2`: missing finite/range validation at the #1396/#2230 conversion boundaries and the interpolation family behind dismissed alerts #1086-#1088 |
| `signed-char-byte-conversion` | CWE-704 | Signed char data assigned to unsigned byte without explicit cast |
| `unchecked-file-io` | CWE-252 | Ignored `fopen`, `fread`, or `fwrite` results |
| `unsafe-header-cast` | CWE-681 | Header field cast to narrower type |
| `snprintf-size-mismatch` | CWE-120 | Buffer smaller than formatted output |
| `unchecked-allocation` | CWE-252, CWE-476 | Allocation result dereferenced in the assigning function without a null check |
| `unbounded-profile-loop` | CWE-400, CWE-834 | Loop bound from profile field without a recognized setup/layout cap |
| `unbounded-clut-index` | CWE-125 | CLUT `GetData()` buffer indexed without a length bound (#1548) |
| `world-writable-output` | CWE-732 | Research-only duplicate; CI uses stock `cpp/world-writable-file-creation` for this signal |
| `unsafe-tag-downcast` | CWE-843 | `FindTag()` result cast without type check |
| `tainted-format-string` | CWE-134 | Variable used as a `printf` format string |
| `tell-subtraction-underflow` | CWE-191, CWE-681 | Tell-derived offset subtraction narrowed without bounds check |
| `factory-bare-new` | CWE-252, CWE-755 | Factory allocation without `std::nothrow` and without catch-all protection at every known call site |
| `fixedpoint-type-confusion` | CWE-681, CWE-197 | Fixed-point conversion function called with wrong type |
| `ignored-save-profile-result` | CWE-252 | `SaveIccProfile()` result ignored before reporting output success |
| `json-raw-normalized-value` | CWE-682 | JSON serialization of raw normalized ICC value |
| `json-recursive-parse` | CWE-674, CWE-400 | Recursive JSON structure without depth limit |
| `json-size-overflow` | CWE-680, CWE-190 | JSON array size cast to narrow integer without bounds check |
| `json-size-to-narrow-setsize` | CWE-680, CWE-190 | JSON array count converted to `icUInt32Number`, then implicitly narrowed by `SetSize()` |
| `localized-text-unsafe-serialization` | CWE-116, CWE-20 | Localized ICC text emitted without XML/JSON hex fallback |
| `multiplication-overflow-alloc` | CWE-190, CWE-680, CWE-787 | Multiplication overflow before allocation |
| `fixed-buffer-loop-bound` | CWE-121, CWE-787 | Fixed aggregate position table indexed by a member-count loop that can exceed the array (#2608) |
| `fixed-buffer-transform-output` | CWE-121, CWE-787 | Fixed local buffer forwarded through transform wrappers without a recognized literal capacity guard; forwarded capacity parameters require manual review (#2868, #2886) |
| `counted-buffer-off-by-one` | CWE-125, CWE-787 | `index > count` guard permits a one-past-end counted-buffer access |
| `new-array-delete-mismatch` | CWE-762 | `new[]` allocation released with scalar `delete` |
| `null-after-read` | CWE-476 | Pointer dereferenced after `Read()` / `ParseXml()` without null check |
| `recursive-parse-no-depth-limit` | CWE-674 | Recursive parse/read/apply call lacks depth guard |
| `rule-of-three-violation` | CWE-415, CWE-416 | Destructor frees member without copy constructor / assignment operator |
| `signed-char-byte-conversion` | CWE-704 | UTF-8 byte parser assigns signed `char` data to unsigned bytes without an explicit cast |
| `tell-subtraction-underflow` | CWE-191, CWE-681 | Writer offset/size field computed from unchecked `Tell()` subtraction and narrowed to `icUInt32Number` |
| `u8fixed8-reconstruction-error` | CWE-682 | Incorrect u8Fixed8Number reconstruction from normalized float |
| `unterminated-ucs2-settext` | CWE-125, CWE-170 | Unterminated UCS-2 buffer passed to `SetText()` |
| `unterminated-suffix-comparison` | CWE-125, CWE-191 | Unchecked suffix length subtracted from color-name length before `strncmp()` |
| `self-alias-owning-setter` | CWE-415, CWE-416 | Owning setter deletes its member before storing an unguarded pointer parameter (#2631) |

## iccdev-mcp Python Queries

Run the MCP-specific suite against a Python CodeQL database rooted at
`iccdev-mcp/`:

```bash
gh codeql pack ci .github/codeql-queries
gh codeql database create /tmp/codeql-db-iccdev-mcp \
  --language=python --source-root=iccdev-mcp --overwrite
gh codeql database analyze /tmp/codeql-db-iccdev-mcp \
  --format=sarif-latest --output=/tmp/iccdev-mcp-security.sarif \
  --threads=0 .github/codeql-queries/iccdev-mcp/iccdev-mcp-security-suite.qls
```

## Running Locally

Prerequisites: GitHub CodeQL CLI or `gh codeql` extension, plus a working C/C++
toolchain.

```bash
.github/scripts/run-codeql-local.sh
.github/scripts/run-codeql-local.sh --custom-only
.github/scripts/run-codeql-local.sh --skip-build
```

The `iccdev-jsonlib-security.sarif` output covers the IccJSON library and
IccConnect JSON configuration code.

Results are written to `codeql-results/`:

- `cpp-security-and-quality.sarif`
- `iccdev-security.sarif`
- `iccdev-jsonlib-security.sarif`
- `iccdev-mcp-security.sarif` when the MCP Python suite is run separately
- `report.txt`
- `fingerprints.tsv` (category, rule, path, line, and SARIF primary-location fingerprint)

## Adding a Query

1. Create `.github/codeql-queries/my-query.ql` with `@kind problem` metadata.
2. Run `gh codeql pack ci .github/codeql-queries`.
3. Test against a local database:
   `gh codeql database analyze <db> .github/codeql-queries/my-query.ql --format=sarif-latest --output=/tmp/test.sarif`
4. Verify results before committing.

Run the checked-in query tests after changing query logic:

```bash
codeql pack ci .github/codeql-queries
codeql pack ci .github/codeql-queries/test \
  --additional-packs=.github/codeql-queries
codeql test run .github/codeql-queries/test \
  --additional-packs=.github/codeql-queries
```

`unbounded-profile-loop` treats `Begin()` as an invariant-establishing setup
method, alongside `Init()`, `Read()`, and the other setup methods recognized by
the query. Alerts #999, #1000, and #1647 were false positives after
`CIccCLUT::Begin()` bounded `m_nNodes` and its CMM callers propagated failure.
The rule also excludes the exact internal/derived fields behind alerts #2360
and #881; neither is an untrusted profile loop bound. Do not generalize those
exclusions to other job or count fields.
Keep the negative query-test case: genuinely unguarded profile-count loops must
still be reported.
`Calc*Layout()` is also recognized when it bounds the same field used by the
loop; the TIFF tile-layout mutant pins that narrow contract.

`unchecked-allocation` reports only dereference-capable uses in the function
that receives the allocation result. Returning or comparing a pointer is not a
dereference, and a later member access in a different method must not be
attributed to the assigning call. `factory-bare-new` suppresses a factory only
when it has at least one resolved call and every resolved call is inside a
`try` with a catch-all handler. `unsafe-header-cast` recognizes an `INT_MAX`
fallback only when the ternary condition compares the same header field.

`iccdev-security-suite.qls` owns the non-JSON C/C++ queries. JSON-derived
queries, including `json-size-to-narrow-setsize`, belong only to
`iccdev-jsonlib-suite.qls` so the core and JSON reports remain disjoint.

Standard-suite findings such as CLI path input or intentional float comparisons
may be informational rather than vulnerabilities; triage them in context.

`division-by-zero-profile-v2` and `float-to-int-cast-v2` are regression-fingerprint
queries, not broad arithmetic linters. Their retired predecessors matched 65
and 64 locations respectively on the 2026-11-03 branch database, including
known guarded code. The replacements match exact historical defect families
and use value/control-flow evidence. Keep every positive historical mutant and
guarded negative control in the query tests; do not restore field-name regexes
or fixed source-line windows. The retired
`signature-serialized-without-zero-guard` experiment remains excluded: it
reported zero locations while missing known unguarded serializers, so a clean
result did not establish coverage.

The hosted dismissed-alert audit found 112 alerts under the retired DBZ rule
and 72 under the retired float-cast rule. Do not bulk reopen them. The three
float-cast alerts #1086-#1088 were the exception: UBSan proved NaN reached all
three interpolation casts, and the v2 query now fingerprints that family.
The evidence and maintainer reproduction are in
[`docs/codeql-dismissed-alert-audit.md`](../../docs/codeql-dismissed-alert-audit.md).
