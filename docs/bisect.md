# Bisecting Regressions in iccDEV

Use this workflow to identify, fix, and lock down regressions in profile
generation, validation, conversion, or tool behavior.

## Prerequisites

- A local iccDEV checkout
- A known bad revision and a known good revision or release tag
- A copy-pasteable command that returns success for good commits and failure for
  bad commits
- A clean build tree; remove `Build/CMakeCache.txt` and `Build/CMakeFiles/` when
  changing compiler or sanitizer flags

Build instructions are in [build.md](build.md). Maintainer sanitizer details are
in `.github/instructions/build-system.instructions.md`.

## Reproduce

Run the smallest command that demonstrates the failure. For profile-suite
failures, generate profiles first and then run the specific failing command
before running the full suite.

```bash
Testing/CreateAllProfiles.sh
Testing/RunTests.sh
```

Classify the failure:

| Type | Example |
|------|---------|
| Validation regression | known-good profile now reports invalid |
| Parse failure | XML/JSON input is rejected unexpectedly |
| ASAN finding | heap-buffer-overflow, use-after-free, SEGV |
| UBSAN finding | signed overflow, enum out-of-range |
| Crash | exit code >= 128 |

## Bisect

```bash
git bisect start
git bisect bad HEAD
git bisect good <known_good>
git bisect run bash -c '<build command> && <test command>'
```

For directory-sensitive tests, use a wrapper script that `cd`s into the test
directory before running the tool.

Useful known-good anchors:

```bash
git tag --sort=-creatordate | head -10
git log --oneline -1 <release-tag>
```

## Analyze

```bash
git show <commit_sha> --stat
git show <commit_sha> -- <file>
git log -1 --format="%H %s" <commit_sha>
```

Common patterns:

| Pattern | Example |
|---------|---------|
| Rename missed at use sites | variable renamed but checks still use old meaning |
| Case-sensitive filename | passes on Windows, fails on Linux |
| Validation became too strict | legal profiles rejected |
| Validation branches collapsed | PCS/device distinctions lost |
| Size semantics mixed | buffer size compared with file size |
| Reader/writer string mismatch | one path writes a name another cannot parse |
| XML parser limit | large inputs need explicit parser option |

## Fix

- Make the smallest change that fixes the regression.
- Match nearby style.
- Avoid unrelated cleanup.
- Add a short comment only when the invariant is non-obvious.

## Verify

Run the specific failing command, then the relevant broader suite.

```bash
Testing/CreateAllProfiles.sh
Testing/RunTests.sh
```

For sanitizer bugs, confirm the rebuilt tool is instrumented and the output is
clean:

```bash
nm Build/Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
```

## Add Regression Coverage

Add the nearest durable guard:

- profile or data file under `Testing/`
- PoC entry under `.github/ci/regression/`
- script gate under `.github/scripts/`
- workflow assertion when the behavior is CI-specific

Use `.github/ci/regression/README.md` as the canonical PoC catalog.

## File Upstream

Security and sanitizer issue format is documented in
`.github/prompts/SECURITY_ISSUE_FORMAT.md`. Include the exact commit, build,
PoC replay, bad output, patch, and clean output when available.
