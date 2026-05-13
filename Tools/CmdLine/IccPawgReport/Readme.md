# iccPawgReport

`iccPawgReport` generates an ICC Profile Assessment Working Group (PAWG) checklist report for an ICC profile.

## Usage

```sh
iccPawgReport profile.icc
```

The report prints 31 checklist items covering security, conformance, and quality. Results are summarized as `PASS`, `WARN`, `FAIL`, `GAP`, `N/A`, and `NOT RUN`.

## Notes

- Security checks include header validation, tag bounds, private-tag malware signatures, and NOP-sled detection.
- Conformance checks include required tags, tag encoding, profile class rules, and private-tag registry status.
- Quality checks include round-trip, curve, smoothness, and characterization metrics when the profile contains enough data.
