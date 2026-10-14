<!--
SPDX-FileCopyrightText: 2026 International Color Consortium
SPDX-License-Identifier: BSD-3-Clause
-->

# ICC XML and JSON round-trip quality assurance

`.github/scripts/iccdev-xml-json-roundtrip-qa.sh` measures ICC to XML to ICC
and ICC to JSON to ICC behavior for one profile or a directory of profiles. It
is a Bash 3-compatible launcher for
`.github/scripts/iccdev_xml_json_roundtrip_qa.py` (Python 3.9+, standard library
only). Windows can invoke the Python script directly. These are source-checkout
developer tools, not installed command-line tools or CPack runtime dependencies.
They are intended for bounded investigations and external image-carrier corpora,
not for committing generated data.

Run it with a native tools build and an unused evidence directory:

```bash
ICCDEV_TOOLS_DIR="$PWD/Build/Tools" \
  .github/scripts/iccdev-xml-json-roundtrip-qa.sh \
  --jobs 8 \
  "$PWD/Testing/png/icc-profiles" \
  /tmp/iccdev-png-roundtrip-qa
```

Every conversion and validation has a timeout (`--timeout`, default 60 seconds).
`--jobs` accepts 1 through 32, and
`--max-profiles` bounds a trial without changing input order. Use
`--compare-baseline` with a prior completed output directory to measure changes
in metrics, diagnostics, and per-profile results including input SHA-256.
Worker count is excluded because it is not a profile-quality change.
Only completed schema-1 Python-runner baselines are accepted; the earlier
Bash-only evidence format is not a baseline. Differences are informational,
while unreadable, incomplete, or malformed baselines are setup errors.

`ICCDEV_TOOLS_DIR` accepts the native `Tools/<Tool>/<executable>` layout or a
flat installed bin directory. Exact executable overrides are `ICC_TO_XML`,
`ICC_FROM_XML`, `ICC_TO_JSON`, `ICC_FROM_JSON`, and `ICC_DUMP_PROFILE`; an
invalid explicit override fails rather than falling back. `ICCDEV_PYTHON`
selects the launcher's interpreter. CTest supplies exact target paths, including
the configuration directory for multi-config generators.

## Evidence and classification

The runner preserves forward XML and JSON, returned ICC blobs, complete
per-stage logs, `results.tsv`, `diagnostics.tsv`, `summary.txt`, and
`developer-summary.txt`, and `run.json`. The latter records completion,
configuration, platform, and tool paths and hashes. Each launched stage has a
`.log`, `.log.exit`, and `.log.command.json` sidecar with exact replay arguments
and native status. Extensions are retained in artifact names to keep same-stem
`.icc` and `.ICC` inputs separate. TSV fields escape tabs, newlines, and non-ASCII
characters; raw native logs remain untouched.

The summary distinguishes ordinary conversion rejections from timeouts, actual
signal termination, loader failures, and sanitizer findings, including findings
with exit zero. Python's negative POSIX subprocess return codes identify signals;
ordinary positive 255 (native `main` returning -1) is not called a crash.
Windows exception-status exits are abnormal. A nonzero conversion producing a
nonempty output is `warning-output`, not proof of successful validation.

Source and returned profiles are independently validated with
`iccDumpProfile -v`. The result records byte identity, size deltas, XML/JSON
diagnostic parity, and source-diagnostic fidelity:

| Fidelity | Meaning |
|----------|---------|
| `same` | Returned validation diagnostics match the source. |
| `masked` | Returned data lost source diagnostics. |
| `added` | Returned data introduced diagnostics absent from the source. |
| `changed` | Both source and returned data have different diagnostics. |
| `n/a` | Output is unavailable or source/returned validation failed. |

Known diagnostic signatures are labeled separately. Any other validation
diagnostic remains `untriaged` and makes the runner exit 3 when no profile failed.
Missing/empty output, failed validation, timeout, signal, loader, or sanitizer
evidence makes it exit 1. Setup/evidence errors exit 2; partial evidence keeps
`run.json` incomplete and cannot be used as a baseline. Exit 0 means measured
without operational failures or untriaged diagnostics, not byte identity,
conformance, or an absence of diagnostic masking.

CTest runs one tracked, valid display profile as
`iccdev.xml-json-roundtrip-qa`. It asserts byte identity, clean validation,
diagnostic fidelity, and a matching serial/parallel baseline. A separate
`iccdev.xml-json-roundtrip-qa-contract` injects rejection, timeout, signal,
sanitizer, missing-output, collision, and baseline faults without native tools.
Both are registered before the Windows/no-Bash CMake returns when Python 3.9+
is found; the native control additionally requires all five tool targets.
Each CTest run uses a distinct directory below the build's CTest output root,
so it never overwrites prior evidence. The runner also atomically reserves its
requested evidence directory and rejects any existing directory or symlink.
Keep output outside the input tree: recursively scanning a tree containing prior
QA outputs would include those returned profiles as new inputs.
Run a representative staged image corpus explicitly before reporting a tool
defect, and retain the summary, TSV files, relevant logs, exact tool paths, and
the native reproduction command.
