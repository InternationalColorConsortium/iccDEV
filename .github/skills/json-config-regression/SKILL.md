---
name: json-config-regression
description: >
  Validate iccDEV JSON/profile config parser changes with fail-closed
  regression gates and CLI exercises.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# JSON and Config Regression

Use this skill when editing `IccJSON/`, JSON serialization, or `-cfg` handling
for `iccApplyNamedCmm`, `iccApplyProfiles`, or `iccApplySearch`.

## Fail-Closed Rules

- Reject short fixed-size arrays instead of truncating.
- Reject non-numeric values for required numeric fields.
- Propagate failed nested `ParseJson()` or `fromJson()` calls.
- Reject bad struct members instead of skipping them.
- Reset stale state before parse and honor reset/fromJson flags.

## Validation

Run the focused gates from the repository root after building tools:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing .github/scripts/iccdev-json-parser-regression-tests.sh
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing .github/scripts/iccdev-json-cfg-tests.sh
```

If observer/profile behavior is involved, also run:

```bash
ICCDEV_TOOLS_DIR=$PWD/Build/Tools ICCDEV_TESTING_DIR=$PWD/Testing .github/scripts/iccdev-stdobserver-regression-tests.sh
```

## Review Checklist

- Verify exact CLI command names and config keys.
- Add malformed tests for rejected input.
- Confirm success paths still round-trip valid profiles/configs.
- Keep test output reproducible from the repository root.

## References

- `../../copilot-instructions.md`
- `../../prompts/bisect-regression.prompt.md`
- `../../../docs/iccjson.md`
- `../../../docs/iccjson-tag-types.md`
