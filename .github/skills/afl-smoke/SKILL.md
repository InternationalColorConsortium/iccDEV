---
name: afl-smoke
description: Run or update the iccDEV AFL++ manual smoke workflow, seeds, and maintainer documentation.
---

# AFL++ Smoke Workflow

Use this skill when changing `.github/workflows/ci-afl-smoke.yml`,
`.github/scripts/iccdev-afl-smoke.sh`, or AFL smoke seed documentation.

## Local Checks

1. Validate shell syntax and style:

   ```bash
   bash -n .github/scripts/iccdev-afl-smoke.sh
   shellcheck .github/scripts/iccdev-afl-smoke.sh
   ```

2. Validate workflow syntax and governance:

   ```bash
   actionlint .github/workflows/ci-afl-smoke.yml
   yamllint -d '{extends: default, rules: {line-length: disable, document-start: disable, truthy: disable}}' .github/workflows/ci-afl-smoke.yml
   .github/scripts/preflight-safety-checks.sh --require-tools
   ```

3. Run a short local AFL smoke before pushing when AFL++ is installed:

   ```bash
   .github/scripts/iccdev-afl-smoke.sh --seconds 10 --targets dump --exec-timeout-ms 1000
   ```

## Update Rules

- Keep workflow triggers manual or reusable unless maintainers explicitly widen
  them.
- Keep the target allow-list inside `.github/scripts/iccdev-afl-smoke.sh`.
- Keep numeric workflow options validated by the script, not only by the
  Actions input UI.
- Pass workflow inputs through `env:` before shell use.
- Sanitize all summary output.
- Track only tiny deterministic seeds. Do not commit AFL queues, crashes,
  hangs, coverage reports, profiler data, or generated build trees.
