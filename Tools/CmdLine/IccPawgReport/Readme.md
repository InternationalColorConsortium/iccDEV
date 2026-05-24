# iccPawgReport

`iccPawgReport` provides an ICC Profile Assessment Working Group (PAWG) checklist report for an ICC profile. The report is aligned with the ICC ["Goals for profile assessment"](https://www.color.org/profiles/assessment/index.xalter) checklist. Tool intent is to help reduce security risks and check conformance with the ICC specification, and provide a common profile-quality reporting framework.

## Usage

```sh
iccPawgReport profile.icc
iccPawgReport --json profile.icc
iccPawgReport --read profile.icc
```

Options:

- `--json` out instead of text report
- `--read` eager `ReadIccProfile()` loading after validation parse failure

The report prints 31 checklist items:

- `S1` through `S13`: security checks
- `C1` through `C14`: conformance checks
- `Q1` through `Q4`: quality checks

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

Security checks cover the PAWG goals for channel counts, 128-byte header encoding, registered or zero header signatures, D50 illuminant, PCS rules, tag-table bounds and layout, EOF placement, iccMAX calculator-element cost, private-tag presence, private-tag malware scans, and private-tag NOP-sled detection.
- Note that malware-signature scans are not implemented

Conformance checks cover tag value encoding, `cprt`/`desc` text encoding, allowed tag types, required tags for profile class, unexpected additional tags, private-tag registration and documentation status, undocumented private-tag identification, profile-class and data-colour-space consistency, header conformance, profile-version/tag consistency, media white point encoding, reserved header bytes, and four-byte tag boundaries.

Quality checks report first and second round-trip CIEDE2000 differences, curve invertibility, transform smoothness metrics, and characterization-data CIEDE2000 differences when the profile contains enough supported data.

## Notes
- ICC PAWG checklist is a guide and not an exhaustive list
- Report does not indicate that a profile is suitable for use
- iccMAX profiles can have different conformance requirements
  - iccMAX-specific calculator-cost estimate where applicable
- Private-tag registration defined in source as ICC registry ranges
