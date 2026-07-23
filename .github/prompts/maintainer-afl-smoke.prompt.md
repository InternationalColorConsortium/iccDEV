# Maintainer AFL++ Smoke Prompt

Use this prompt when adding or adjusting bounded AFL++ checks for iccDEV
command-line tools.

## Context

- Workflow: `.github/workflows/ci-afl-smoke.yml`
- Driver: `.github/scripts/iccdev-afl-smoke.sh`
- AFL++ source: `https://github.com/AFLplusplus/AFLplusplus/tree/dev`
- Seeds: `.github/ci/test-data/` and `.github/ci/afl-seeds/`
- Governance: `.github/instructions/workflow-governance.instructions.md`
- Docs: `docs/afl-fuzzing.md`

## Task

Update the AFL smoke target, seed set, or workflow inputs while keeping the job
manual, bounded, and governance-compliant.

## Required Validation

```bash
bash -n .github/scripts/iccdev-afl-smoke.sh
shellcheck .github/scripts/iccdev-afl-smoke.sh
actionlint .github/workflows/ci-afl-smoke.yml
yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
.github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 5000
```

Run `.github/scripts/preflight-safety-checks.sh --require-tools` before pushing
workflow changes.

## Handoff

Report the exact workflow ref, target list, duration, build type, exec timeout,
run URL, conclusion, and whether AFL reported crashes or hangs.
