# iccPawgReport

`iccPawgReport` provides an ICC Profile Assessment Working Group (PAWG) checklist report for an ICC profile. The report is aligned with the ICC ["Goals for profile assessment"](https://www.color.org/profiles/assessment/index.xalter) checklist. Tool intent is to help reduce security risks and check conformance with the ICC specification, and provide a common profile-quality reporting framework.

## Usage

From PowerShell at the repository root with the documented `repo\msvc` build:

```powershell
$Profile = Join-Path $PWD 'Testing\sRGB_v4_ICC_preference.icc'
& '.\msvc\bin\Release\iccPawgReport.exe' $Profile
& '.\msvc\bin\Release\iccPawgReport.exe' --json $Profile
& '.\msvc\bin\Release\iccPawgReport.exe' --qa-flags --evidence-json $Profile
```

From a Unix shell with the tool on `PATH`:

```bash
iccPawgReport Testing/sRGB_v4_ICC_preference.icc
iccPawgReport --json Testing/sRGB_v4_ICC_preference.icc
iccPawgReport --qa-flags --evidence-json Testing/sRGB_v4_ICC_preference.icc
```

Options:

- `--json` outputs JSON instead of the text report
- `--qa-flags --evidence-json` emits schema-versioned load and validation
  evidence when built with `ICCDEV_ENABLE_QA_FLAGS=ON`

A profile that IccProfLib refuses to parse is still assessed: the raw-byte checks run from the
file contents directly, and the checks that need a parsed profile are reported as `NOT RUN`.
(A `--read` option previously claimed to load such a profile via `ReadIccProfile()`. It could
never do so and was retired in #1977.)

The report prints 32 checklist items:

- `S1` through `S14`: security checks
- `C1` through `C14`: conformance checks
- `Q1` through `Q4`: quality checks

A profile of the HDR Profile sub-class of ICC.1 clause 8.10 gets a further eight, `H1` through
`H8`. That section is **absent** — not `NOT RUN` — for every other profile, so a non-HDR report
is byte-for-byte what it was before the section existed and still totals 32 items.

**Indicators:** `PASS`, `WARN`, `FAIL`, `GAP`, `N/A`, and `NOT RUN`.

| Status | Meaning |
|--------|---------|
| `PASS` | The local check completed and no issue was detected. |
| `WARN` | The profile should be reviewed, but the condition is not treated as a hard failure. |
| `FAIL` | The check found a security, structure, or quality failure. |
| `GAP` | The checklist item is relevant, but the local tool could not fully evaluate it. |
| `N/A` | The checklist item does not apply to this profile or profile class. |
| `NOT RUN` | The profile could not be loaded enough to run that check. |

## Checklist coverage

Security checks cover the PAWG goals for channel counts, 128-byte header encoding, registered or zero header signatures, D50 illuminant, PCS rules, tag-table bounds and layout, EOF placement, iccMAX calculator-element cost, private-tag presence, private-tag malware scans, private-tag NOP-sled detection, and DEFLATE-compressed tag (`zut8`/`zxml`/compressed `data`) measurement.
- Note that malware-signature scans are not implemented

Conformance checks cover tag value encoding, `cprt`/`desc` text encoding, allowed tag types, required tags for profile class, unexpected additional tags, private-tag registration and documentation status, undocumented private-tag identification, profile-class and data-colour-space consistency, header conformance, profile-version/tag consistency, media white point encoding, reserved header bytes, and four-byte tag boundaries.

Quality checks report first and second round-trip CIEDE2000 differences, curve invertibility, transform smoothness metrics, and characterization-data CIEDE2000 differences when the profile contains enough supported data.

The JSON Q1 item includes structured, unrounded sample count, model, first-pass
and second-pass average and maximum CIEDE2000 metrics, and the final verdict.
MATLAB QA compares these fields through `iccdev.qa.audit_pawg_q1`; build the
`iccPawgReport` and `iccPawgQ1QualityContractTest` targets for that workflow.

HDR checks (`H1`–`H8`, ICC.1 clause 8.10) report the HDR Profile classification (conforming or
merely intended), the 4.5.0.0 version requirement, presence of the `cicpTag`, whether its
`TransferCharacteristics` is one of the three an HDR Profile may use, how the source primaries
resolved (ITU-T H.273 table or the profile's own matrix columns, per clause 9.2.17), which of
clause 8.10.3's ranked tone-mapping descriptors is present, and — the two provenance items —
where the content HDR reference white came from and which rule of clause 8.10.5 produced the
display headroom. The last two read entries out of the `metadataTag` whose names and encodings
are reconstructed pending publication of the ICC dictType Metadata Registry, and the detail text
says so rather than presenting a derived number as settled.

The section is PAWG's own selection, not a rendering of the IccProfLib validation report: the
two overlap but their intent differs, and `H5`, `H7` and `H8` are not spec violations at all, so
they could never appear in a validation log.

## Notes
- ICC PAWG checklist is a guide and not an exhaustive list
- Report does not indicate that a profile is suitable for use
- iccMAX profiles can have different conformance requirements
  - iccMAX-specific calculator-cost estimate where applicable
- Private-tag registration defined in source as ICC registry ranges
