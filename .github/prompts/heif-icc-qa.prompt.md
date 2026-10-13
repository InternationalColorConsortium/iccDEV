# HEIF ICC Carrier QA

Build and quality-assure Nokia `heifIccDump` against the libheif and
IccProfLib-backed release `iccHeifDump` on the tracked HEIF/AVIF
carrier corpus for issue #2647.

Use `.github/skills/heif-icc-qa/SKILL.md` and report:

- iccDEV and Nokia HEIF revisions;
- release and sanitizer build results;
- the valid, malformed, duplicate, conflicting, truncated, oversized, and
  overflow carrier outcomes;
- property-probe and CLI exit behavior;
- extracted ICC byte count and SHA-256;
- byte-for-byte A/B extraction comparison and IccProfLib parse statuses;
- bounded carrier and repeated-association rejection results;
- native `iccdev.heif-output-safety` results on Windows and Unix, including
  dangling symlinks, concurrent creation, and any symlink privilege skips;
- every upstream patch applied, the warning-free dedicated smoke result, and
  any test skipped.

Keep clones, builds, extracted profiles, and logs outside the repository. Add
or update the focused regression before fixing demonstrated behavior, and use
`--no-tests=error` for CTest discovery and execution.
Keep hosted Nokia dependency validation isolated in
`.github/workflows/ci-nokia-heif-icc-smoke.yml`.

When MATLAB integration is in scope, update `iccdev.heif_dump`, its
input-validation and bare-invocation tests, `matlab/examples/heif_dump.m`, and
the MATLAB build and staged-artifact requirements. Use Java `ProcessBuilder`,
not a shell command string, and keep the example interactive unless a checked-in
HEIF fixture is available.
