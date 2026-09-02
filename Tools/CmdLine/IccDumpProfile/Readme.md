# IccDumpProfile

`iccDumpProfile` prints profile structure, tag contents, and validation output
for ICC and iccMAX profiles.

## Usage

Run without arguments to print the current command syntax and supported options:

```sh
iccDumpProfile
```

Validation messages start with `Warning!`, `Error!`, or `NonCompliant!`. The
overall validation status appears near the `Validation Report` section.

## QA target flag evidence

Builds configured with `-DICCDEV_ENABLE_QA_FLAGS=ON` also accept
`--qa-flags --evidence-json`. This mode is for maintainer QA and CI evidence,
not normal user-facing dump output. It emits one JSON object using schema
`iccdev-qa-evidence/v1`.

```sh
iccDumpProfile --qa-flags --evidence-json --diag -v 100 profile.icc
```

The evidence can include these QA flags:

| Flag | Meaning |
|------|---------|
| `ICCDEV_FLAG_LOAD` | The profile loaded successfully, with load mode, profile size, and profile ID. |
| `ICCDEV_FLAG_VALIDATE` | Validation was requested, with validation status and tag signature/offset/size evidence. |
| `ICCDEV_FLAG_TAG_PAYLOAD` | A private `qaFL` QA tag carried the expected marker and generated nonce when present. |
| `ICCDEV_FLAG_CONTROLLED_PATTERN` | The `qaFL` payload included controlled `0x41` bytes. |

The CTest `iccdev.qa-target-flags` generates deterministic true-positive,
negative-control, malformed marker-only, transform, embedded-profile extraction,
and sanitizer-log fixture evidence. CI writes a sanitized summary table and raw
evidence to the GitHub Actions job summary.

## See Also

- [CLI tool reference](../../../docs/tools-cli-reference.md)
- [Testing instructions](https://github.com/InternationalColorConsortium/iccDEV/blob/master/Testing/README.md)
