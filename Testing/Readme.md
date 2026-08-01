XML files that can be used to create iccMAX profiles can be found in
the following folders:

## [Calc](Calc)
This folder contains profiles that demonstrate modeling using the
Calculator MultiProcessElement.  The srgbCalcTest profile exercises
all specified calculator operations.

## [Display](Display)
This folder contains profiles that demonstrate spectral modeling of
display profiles allowing for late binding of the observer using
MultiProcessElements that are transformed at startup to colorimetry
for the desired observer.

## [Encoding](Encoding)
This folder contains 3 channel encoding class profiles.  Both "name
only" profiles as well as fully specified profiles are present.

## [Named](Named)
This folder contains named color profiles showcasing features such as
tints, spectral reflectance, and fluorescence, including sparse notation.

## [PCC](PCC)
This folder contains various profiles that can be used to define Profile
Connection Conditions (PCC).  All profiles are abstract profiles that
perform no operation to PCS values.  However, all profiles contain fully
defined PCC tags that provide information that can be used to define
rendering for various observers and illuminants. Profiles that use
both absolute colorimetry and Material Adjusted colorimetry
are present.

## [SpecRef](SpecRef)
This folder contains profiles that convert data to/from/between
a spectral reflectance PCS.  The argbRef (AdobeRGB) and srgbRef (sRGB)
convert RGB values to/from spectral reflectance.  RefDecC, RefDecH, and
RefIncW are abstract spectral reflectance profiles that modify "chroma",
"hue", and "lightness" of spectral reflectance values in a spectral
reflectance PCS.  
The argbRef, srgbRef, RefDecC, RefDecH, RefIncW profiles all estimate
and/or manipulate spectral reflectance using Wpt based spectral estimation
(see chapter 7 of http://scholarworks.rit.edu/theses/8789/ ).  
Additionally, examples of 6 channel abridged spectral encoding are provided.

## Creating Profiles

`CreateAllProfiles.bat` and `CreateAllProfiles.sh` use `iccFromXml` to create
ICC profiles from XML files in these folders. Add the built tools to your local
PATH before running the scripts.

For [Release Downloads](https://github.com/InternationalColorConsortium/iccDEV/releases)
run the included PATH Scripts:

- Windows run `path.bat` then `CreateAllProfiles.bat`
- Unix run `./path.sh` then `./CreateAllProfiles.sh`

## CI Round-Trip Classification

The Linux CI sweep separates clean reconstruction from expected negative
fixtures during `iccFromXml` XML-to-ICC round-trip testing.

- Clean profiles must reconstruct and save without validation errors.
- Known invalid fixtures are listed in `expected-invalid-fromxml.tsv`.
- New parse failures, sanitizer findings, or unclassified validation diagnostics
  fail CI until a maintainer fixes the behavior or classifies the fixture.

Use this split when interpreting CI output: the generated corpus includes both
spec-valid examples and negative regression profiles that intentionally exercise
ICC/iccMAX validation failures.

## QA Profile Manifest

`expected-invalid-fromxml.tsv` above covers the XML-to-ICC direction.
`qa-profile-manifest.tsv` covers the other half: the validation verdict each
profile in the corpus is expected to produce when `iccDumpProfile -v` reads it.

Every profile is assigned to one of three suites, and the suite follows from the
verdict rather than from a hand-adjudicated list:

| suite | expected status | rule |
|---|---|---|
| `positive` | `valid` | must validate clean; any new diagnostic is a regression |
| `compatibility` | `warning` | a known diagnostic is baselined; escalation beyond it fails |
| `negative` | `critical` | rejection is the expected result; acceptance is a failure |

A sanitizer finding or fatal signal fails every suite, negatives included: a
malformed fixture is meant to be *rejected*, not to crash the validator.

`sha256` is populated only for the profiles git tracks. The generated corpus
cannot carry a stable digest because nearly every source XML sets
`<CreationDateTime>now</CreationDateTime>`, which resolves through `localtime_r`
at conversion time and feeds the recomputed profile ID — two conversions a second
apart differ, and two machines in different time zones differ as well. For those
rows `expected_status` and `expected_exit` carry the contract instead.

The manifest also records `expected_status` and `expected_exit` as independent
fields rather than deriving one from the other, because `iccDumpProfile` maps
`noncompliant` to exit 0: a harness reading `$?` and a harness parsing the report
do not see the same verdict.

The CTest `iccdev.qa-profile-manifest` enforces the manifest. To re-baseline it
deliberately, after a change that is meant to move verdicts:

```sh
.github/scripts/iccdev-qa-profile-manifest.sh generate
```

Review the resulting diff — it is the list of profiles whose validation outcome
changed.
