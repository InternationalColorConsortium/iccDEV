# Working with ICC Profiles Using IccJSON

IccJSON provides `iccToJson` and `iccFromJson` for converting ICC and iccMAX
profiles to and from human-readable JSON. Use this guide for the workflow and
profile shape. Detailed tag examples live in [ICC JSON tag types](iccjson-tag-types.md).

## Tools

```bash
iccToJson src_profile.icc dest_profile.json [-indent=N]
iccFromJson src_profile.json dest_profile.icc [-noid]
```

- `-indent=N`: pretty-print with N spaces of indentation (default: 2)
- `-noid`: suppress writing the profile ID into the saved file

After creating a profile, validate it with:

```bash
iccDumpProfile dest_profile.icc
```

## Typical Workflow

```bash
iccToJson input.icc input.json
# edit input.json
iccFromJson input.json output.icc
iccDumpProfile output.icc
```

## JSON Structure

Every JSON profile is wrapped in a top-level `IccProfile` object:

```json
{
  "IccProfile": {
    "Header": { },
    "Tags": [ ]
  }
}
```

### Header

`Header` describes profile class, color spaces, creation time, rendering intent,
device attributes, illuminant, and optional iccMAX spectral ranges.

```json
"Header": {
  "ProfileVersion": "5.0",
  "ProfileDeviceClass": "mntr",
  "DataColourSpace": "RGB ",
  "PCS": "XYZ ",
  "CreationDateTime": "2024-01-15T10:30:00",
  "ProfileFileSignature": "acsp",
  "RenderingIntent": "Relative",
  "PCSIlluminant": [0.9642, 1.0, 0.8249],
  "ProfileCreator": "ICC "
}
```

Common signatures:

| Field | Common values |
|-------|---------------|
| `ProfileDeviceClass` | `scnr`, `mntr`, `prtr`, `link`, `spac`, `abst`, `nmcl`, `cenc` |
| `DataColourSpace` | `RGB `, `CMYK`, `XYZ `, `Lab ` |
| `PCS` | `XYZ `, `Lab `, spectral signatures for iccMAX |

### Tags

`Tags` is an ordered array. Each element is a single-key object whose key is the
ICC tag name and whose value contains tag metadata and `data`.

```json
"Tags": [
  {
    "redMatrixColumnTag": {
      "data": {
        "type": "XYZType",
        "XYZ": [0.4361, 0.2225, 0.0139]
      }
    }
  }
]
```

See [ICC JSON tag types](iccjson-tag-types.md) for text, curve, LUT, MPE,
structure, array, and signature examples.

## Minimal Matrix Display Example

```json
{
  "IccProfile": {
    "Header": {
      "ProfileVersion": "4.3",
      "ProfileDeviceClass": "mntr",
      "DataColourSpace": "RGB ",
      "PCS": "XYZ ",
      "ProfileFileSignature": "acsp",
      "RenderingIntent": "Relative",
      "PCSIlluminant": [0.9642, 1.0, 0.8249],
      "ProfileCreator": "ICC "
    },
    "Tags": [
      {
        "profileDescriptionTag": {
          "data": {
            "type": "multiLocalizedUnicodeType",
            "Localized": [
              {"Language": "en", "Country": "US", "Text": "Example RGB profile"}
            ]
          }
        }
      },
      {
        "redMatrixColumnTag": {
          "data": {"type": "XYZType", "XYZ": [0.4361, 0.2225, 0.0139]}
        }
      },
      {
        "greenMatrixColumnTag": {
          "data": {"type": "XYZType", "XYZ": [0.3851, 0.7169, 0.0971]}
        }
      },
      {
        "blueMatrixColumnTag": {
          "data": {"type": "XYZType", "XYZ": [0.1431, 0.0606, 0.7141]}
        }
      }
    ]
  }
}
```

## Tips

- Keep signature strings exactly four characters where the ICC format requires
  a four-byte signature.
- Preserve tag order when round-tripping profiles for comparison.
- If `iccFromJson` reports validation warnings, fix the JSON even when a binary
  profile was written.
- Standard observer values accept canonical names and legacy XML aliases; the
  regression gate is `.github/scripts/iccdev-stdobserver-regression-tests.sh`.
