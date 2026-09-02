---
applyTo: "matlab/**"
---

# MATLAB MEX Code Review

Use these checks when reviewing changes under `matlab/`. Report only
changed-line defects with a concrete trigger and impact.

## MEX Boundary

- Verify each changed `icc_mex` action validates argument count, MATLAB type, scalar
  shape, enum range, and native handle type before converting or dereferencing.
- Check that native handles are freed on every MATLAB error path and that child
  apply handles cannot use a closed parent CMM or freed native state.
- Treat a missing or wrong `iccdev:*` error identifier for a documented
  validation failure as a compatibility defect when tests or callers rely on it.

## MATLAB and Octave Wrappers

- Check public required-argument entry points for an actionable bare-invocation
  error and a matching `test_usage_guidance` fixture entry.
- Require `inputParser` validators to reject nonscalar paths, malformed option
  pairs, and unsupported values before calling the MEX gateway or a tool.
- Preserve `uint8` binary profile data and UTF-8 JSON text without implicit
  character-set conversion or row/column shape changes.
- Ensure temporary files, file handles, and Java streams are closed on success
  and failure.

## Native Tool Integration

- Require Java `ProcessBuilder` argument lists, never shell command strings,
  for wrapper-launched tools.
- Verify an explicit tool path is a regular file and that discovered tools use
  the selected build root or a safe `PATH` entry.
- Require nonzero tool status to surface native diagnostics and prevent output
  from being returned as success.

## Review Evidence

- Pair a changed wrapper with focused success and failure coverage in
  `matlab/tests/`; use checked-in `Testing/...` inputs rather than placeholders.
- For MEX enum or lifecycle changes, verify the boundary value and the first
  rejected value through the gateway.
- When a wrapper adds a native tool dependency, confirm the Release target,
  CI workflow, staged release bundle, MATLAB README, and
  `docs/matlab-bindings.md` name that tool.

See `matlab-mex.instructions.md` for implementation conventions and
`.github/skills/code-review/SKILL.md` for changed-line finding standards.
