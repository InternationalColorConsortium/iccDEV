# ICC v5 Profile Validation QA

Use this prompt to add or review focused ICC v5/iccMAX XML validation cases.

## Inputs

- Profile class and subclass:
- Device data signature and channel count:
- Spectral PCS signature and channel count:
- Spectral and bi-spectral ranges:
- Expected valid or invalid result:
- ICC.2 or ICC.1 clause:

## Procedure

1. Read `docs/v5-profile-validation-qa.md` and the applicable ICC
   specification clauses before editing a fixture.
2. Keep XML source fixtures under `Testing/ICS/V5Coverage`; never track the
   generated profile, logs, coverage data, or crash artifacts.
3. Make every signature count explicit. Remember that the four hexadecimal
   digits in `ncXXXX` and `rsXXXX` encode the channel count.
4. Pair every invalid profile with a nearby accepted control so a validator
   that rejects everything cannot pass.
5. Distinguish focused IccProfLib acceptance from full profile conformance.
   Include spectral viewing conditions and PCC transforms in any fixture
   presented as a complete ICC.2 example.
6. Run the focused CTest in a normal build and a separate sanitizer build.
   Use a separate coverage build when line or branch measurements are needed.
7. Report the generated profile count, expected rejection count, sanitizer
   result, exact command, and temporary evidence directory.

## Required Result

- Both 36- and 81-channel accepted controls remain covered.
- Spectral signature/range mismatch is rejected.
- ICC.2 `ncXXXX` data space is rejected in an ICC.1 v4 profile.
- The one-step interpretation is reported as an open specification edge, not
  silently described as either an ICC.2 minimum or a supported profile.
