# Pre-PR Security Cycle

Use this prompt to plan or audit the minimum secure loop before opening,
updating, or merging an iccDEV PR.

## Inputs

- Branch:
- Issue or PR:
- Changed surface:
- Security-sensitive paths:
- Maintainer-owned infrastructure touched:
- Expected merge signal:

## Loop Checklist

1. Scope the smallest complete change.
2. Build and run nearest deterministic tests.
3. Run SAST:
   - workflow changes: governance audit, YAML parse, `actionlint`, expression
     injection scan;
   - C/C++ or CMake changes: CodeQL local script or hosted CodeQL workflow;
   - parser/profile/tool changes: code-review hunting prompt.
4. Run dynamic checks:
   - CTest, sanitizer, CLI smoke, Docker runtime, WASM parity, release assets,
     or vcpkg consumer smoke as appropriate.
5. Fix confirmed findings and repeat only affected checks.
6. Prepare a golfed handoff with commands, run IDs, sentinels, and known skips.

## Handoff Format

- Branch and commit:
- Files or surfaces changed:
- Local validation:
- Hosted validation:
- SAST/CodeQL status:
- Dynamic/sanitizer status:
- Known skips, suppressions, or deferred follow-ups:
- Merge-ready decision:

## References

- `../../docs/pre-pr-security-cycle.md`
- `../skills/pre-pr-security-cycle/SKILL.md`
- `audit-workflow-governance.prompt.md`
- `build-and-test.prompt.md`
- `code-review-hunting.prompt.md`
