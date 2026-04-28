# IccFromXml

`iccFromXml` converts an ICC profile XML file to a binary ICC profile. It can
optionally validate input with a RELAX NG schema before saving.

## Usage

```sh
iccFromXml input.xml output.icc {-noid -v[=schema.rng]}
```

- `input.xml`: The ICC profile XML file
- `output.icc`: The output binary ICC profile
- `-noid`: Prevents writing a profile ID (optional)
- `-v[=schema.rng]`: Enables RELAX NG validation, using the supplied schema or
  `SampleIccRELAX.rng` from the current directory

## Examples

```sh
iccFromXml DisplayProfile.xml DisplayProfile.icc -v=SampleIccRELAX.rng
iccFromXml edited-profile.xml edited-profile.icc -noid
```

## Notes

- If validation fails but parsing succeeds, the profile may still be saved and
  reported as invalid.
- Use `iccToXml` for the reverse conversion.
