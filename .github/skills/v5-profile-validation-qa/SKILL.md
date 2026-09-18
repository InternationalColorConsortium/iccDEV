---
name: v5-profile-validation-qa
description: >
  Create and validate focused ICC v5/iccMAX XML profiles, including spectral
  range, high-channel, version-boundary, sanitizer, and source-coverage cases.
allowed-tools:
  - bash
  - read
  - grep
  - glob
  - shell(git:*)
---

# ICC v5 Profile Validation QA

Use this workflow for `Testing/ICS/V5Coverage`,
`iccdev.v5-profile-validation`, or related IccProfLib header validation work.

1. Read `../../../docs/v5-profile-validation-qa.md` and correlate the proposed
   expectation with ICC.2:2023 or ICC.1:2022 before editing.
2. Use XML source profiles as durable inputs. Write generated `.icc` files,
   logs, and coverage data only to a build or temporary directory.
3. Preserve a positive control adjacent to every negative boundary. Keep the
   36- and 81-channel controls when changing spectral channel logic.
4. Treat a successful `iccDumpProfile` result as implementation acceptance,
   not proof of complete ICC conformance. Full spectral examples also need the
   applicable viewing-condition and PCC tags.
5. Run:

   ```bash
   cmake --build build --target iccFromXml iccDumpProfile
   ctest --test-dir build -R '^iccdev\.v5-profile-validation$' \
     --output-on-failure --no-tests=error
   ```

6. Repeat in a sanitizer build. Use a separate coverage build and merge only
   the focused test's profile data when reporting source coverage.
7. Verify all changed files are ASCII, LF-terminated, and clean under
   `git diff --check`. Run the documentation gate when docs change.

Do not change the ICC.2 `(steps - 1)` wavelength interval formula merely to
accept a one-step range. Keep that case isolated as a specification-review
boundary until the ICC interpretation is resolved.
