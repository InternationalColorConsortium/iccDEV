# HEIF ICC Carrier QA

Build and quality-assure `iccHeifDump` against the pinned Nokia HEIF reader and
the tracked HEIF/AVIF carrier corpus for issue #2647.

Use `.github/skills/heif-icc-qa/SKILL.md` and report:

- iccDEV and Nokia HEIF revisions;
- release and sanitizer build results;
- the valid, malformed, duplicate, conflicting, truncated, oversized, and
  overflow carrier outcomes;
- property-probe and CLI exit behavior;
- extracted ICC byte count and SHA-256;
- every upstream patch applied, the warning-free dedicated smoke result, and
  any test skipped.

Keep clones, builds, extracted profiles, and logs outside the repository. Add
or update the focused regression before fixing demonstrated behavior, and use
`--no-tests=error` for CTest discovery and execution.
Keep hosted Nokia dependency validation isolated in
`.github/workflows/ci-nokia-heif-icc-smoke.yml`.
