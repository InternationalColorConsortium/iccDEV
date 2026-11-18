# ICC XML and JSON Round-Trip QA

Use `.github/skills/xml-json-roundtrip-qa/SKILL.md` to assess a bounded ICC
corpus through XML and deterministic JSON. Read `developer-summary.txt`,
`summary.txt`, `diagnostics.tsv`, and `results.tsv` before examining logs.

Report byte identity, size deltas, XML/JSON diagnostic parity, and
source-diagnostic fidelity separately from operational failures. Classify every
nonzero exit as ordinary rejection, timeout, signal exit, or sanitizer finding;
only signal and sanitizer evidence are crash candidates. Require an exact
native-tool reproduction for every untriaged diagnostic or fidelity mask.
Require Python 3.9+, preserve `run.json` and per-stage `.command.json` replay
metadata, and treat invalid validation as `n/a`, never `same` fidelity.
Positive native 255 is not a POSIX signal. Compare completed schema-1
baselines including per-profile hashes, not only totals. Do
not commit generated artifacts or external corpus files.

When the task concerns authored parser symmetry rather than an ICC corpus, use
XML as the reference and hand-translate the JSON. Do not create tracked JSON by
passing XML through an ICC blob. Parse both documents independently, compare
their validation and tag/type/channel inventories, and require the same exact
semantic result on shared inputs. Treat returned XML/JSON and binary identity as
secondary evidence because calculator serialization flattens authored source.
Keep format-level pairs under `Testing/Format/`, extend an existing aggregate
when its oracle fits, and compare coverage with the same compiler, tests,
matched objects, and denominator. The format-pair aggregate is
`.github/scripts/iccdev-json-parser-regression-tests.sh`, registered as
`iccdev.json-parser-regressions` in CTest and also run directly in
`ci-json-roundtrip`.
For numeric aliases, select values that distinguish signedness and width and
verify that the factory object carries the required XML or JSON extension.
