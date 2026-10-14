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
