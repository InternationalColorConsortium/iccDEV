# IccFromXml

`iccFromXml` converts an ICC profile XML file to a binary ICC profile. It can
optionally validate input with a RELAX NG schema before saving.

## Usage

```sh
iccFromXml input.xml output.icc {-noid -v[=schema.rng]}
iccFromXml -h | --help
```

- `input.xml`: The ICC profile XML file
- `output.icc`: The output binary ICC profile
- `-noid`: Prevents writing a profile ID (optional). The saved profile's ID field
  is zeroed, including when the input XML carries a `<ProfileID>` of its own
- `-v[=schema.rng]`: Enables RELAX NG validation, using the supplied schema or
  `SampleIccRELAX.rng` taken from the current directory, falling back to the
  directory holding the executable. The schema actually used is reported on stdout
- `-h`, `--help`: Prints this usage screen on stdout and exits successfully

## Exit status

- `0`: the conversion completed, or a help screen was requested
- non-zero: the invocation was malformed, the input could not be parsed, or the
  profile could not be saved

Both operands are required. An incomplete invocation, an unrecognised option, and a
`-v` whose schema cannot be found or read are all errors: usage and diagnostics go to
stderr and the exit status is non-zero. `-v` never silently skips validation.

## Examples

```sh
iccFromXml DisplayProfile.xml DisplayProfile.icc -v=SampleIccRELAX.rng
iccFromXml edited-profile.xml edited-profile.icc -noid
```

## Notes

- If validation fails but parsing succeeds, the profile may still be saved and
  reported as invalid.
- Use `iccToXml` for the reverse conversion.
