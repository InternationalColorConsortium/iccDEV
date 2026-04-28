# IccToXml

`iccToXml` converts a binary ICC profile to an XML representation for
inspection, editing, validation, or round-trip testing with `iccFromXml`.

## Usage

```sh
iccToXml source.icc output.xml
```

- `source.icc`: The input ICC profile (ICC.1 or ICC.2)
- `output.xml`: Destination XML file

## Examples

```sh
iccToXml DisplayProfile.icc DisplayProfile.xml
iccToXml profile.icc profile.xml
```

## Notes

- Output contains profile header data, the tag table, tag contents, and
  multi-process element structure when present.
- Use `iccFromXml` for the reverse conversion.
