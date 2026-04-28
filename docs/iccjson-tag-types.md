# ICC JSON Tag Types

This reference summarizes common `data.type` payloads used by `iccToJson` and
`iccFromJson`. See [IccJSON](iccjson.md) for the end-to-end workflow.

## Text and Metadata

```json
{
  "profileDescriptionTag": {
    "data": {
      "type": "multiLocalizedUnicodeType",
      "Localized": [
        {"Language": "en", "Country": "US", "Text": "Display profile"}
      ]
    }
  }
}
```

## XYZ Values

```json
{
  "mediaWhitePointTag": {
    "data": {
      "type": "XYZType",
      "XYZ": [0.9642, 1.0, 0.8249]
    }
  }
}
```

## Curves

```json
{
  "redTRCTag": {
    "data": {
      "type": "parametricCurveType",
      "FunctionType": 4,
      "Parameters": [2.4, 1.0, 0.055, 0.0, 0.04045, 0.0, 0.0]
    }
  }
}
```

Sampled curves use an explicit value array:

```json
{
  "grayTRCTag": {
    "data": {
      "type": "curveType",
      "Curve": [0.0, 0.1, 0.25, 0.5, 0.75, 1.0]
    }
  }
}
```

## LUT Tags

```json
{
  "AToB0Tag": {
    "data": {
      "type": "lutAToBType",
      "InputChannels": 3,
      "OutputChannels": 3,
      "AToB": {
        "CurvesA": [],
        "CLUT": {},
        "CurvesM": [],
        "Matrix": [],
        "CurvesB": []
      }
    }
  }
}
```

## Multi-Process Elements

```json
{
  "AToB0Tag": {
    "data": {
      "type": "multiProcessElementsType",
      "InputChannels": 3,
      "OutputChannels": 3,
      "Elements": [
        {
          "type": "CurveSetElement",
          "InputChannels": 3,
          "OutputChannels": 3,
          "Curves": []
        }
      ]
    }
  }
}
```

## Structures and Arrays

```json
{
  "profileSequenceDescTag": {
    "data": {
      "type": "profileSequenceDescType",
      "Profiles": []
    }
  }
}
```

Use the exact tag type emitted by `iccToJson` when editing complex structures.
For best fidelity, round-trip a known-good profile, make the smallest JSON edit,
then rebuild and validate with `iccFromJson` and `iccDumpProfile`.

## Signature Rules

- Four-byte ICC signatures are represented as strings.
- Preserve trailing spaces in signatures such as `RGB ` and `XYZ `.
- Empty signatures may be represented as an empty string when the field is unset.
