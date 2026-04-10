# Working with ICC Profiles Using IccJSON

IccJSON provides two command-line tools -- **IccToJson** and **IccFromJson** -- that let you
convert ICC profiles to and from a human-readable JSON representation. This makes it
straightforward to inspect, create, and edit ICC and iccMAX profiles using a text editor
or any JSON-capable tooling.

A JSON Schema for the format is provided at [`docs/icc-profile.schema.json`](icc-profile.schema.json).
Editors such as VS Code can use it to provide inline validation and auto-complete.

---

## Tools

### IccToJson

Converts a binary ICC profile to a JSON file.

```
IccToJson  src_profile.icc  dest_profile.json  [-indent=N]
```

- `-indent=N` -- pretty-print with N spaces of indentation (default: 2)

### IccFromJson

Converts a JSON file back to a binary ICC profile.

```
IccFromJson  src_profile.json  dest_profile.icc  [-noid]
```

- `-noid` -- suppress writing the profile ID (MD5 hash) into the saved file

If the profile fails validation the binary file is still saved, but the validation
report is printed so you can identify and fix issues before use.

### Validating a Profile

After creating a profile with IccFromJson, use **IccDumpProfile** to validate it:

```
IccDumpProfile  dest_profile.icc
```

The overall pass/fail result appears two lines below the `Validation Report` line:

```bash
grep --text -A 3 "^Validation Report" output.txt
```

---

## Typical Workflow

```
# Inspect an existing profile
IccToJson  input.icc  input.json

# Edit input.json in a text editor ...

# Rebuild the binary profile
IccFromJson  input.json  output.icc

# Validate
IccDumpProfile  output.icc
```

---

## JSON Structure

Every ICC profile JSON file is wrapped in a top-level `"IccProfile"` key:

```json
{
  "IccProfile": {
    "Header": { ... },
    "Tags":   [ ... ]
  }
}
```

### Header

The `Header` object describes the profile class, colour spaces, and metadata.
Signature fields that are zero (unset) are encoded as an empty string `""`. Optional non-signature fields (e.g. `ProfileSubClass`, `SpectralPCS`) are omitted when not set.

```json
"Header": {
  "ProfileVersion":     "5.0",
  "ProfileDeviceClass": "mntr",
  "DataColourSpace":    "RGB ",
  "PCS":                "XYZ ",
  "CreationDateTime":   "2024-01-15T10:30:00",
  "ProfileFileSignature": "acsp",
  "PrimaryPlatform":    "MSFT",
  "CMMFlags": {
    "EmbeddedInFile":          false,
    "UseWithEmbeddedDataOnly": false
  },
  "DeviceAttributes": {
    "ReflectiveOrTransparency": "reflective",
    "GlossyOrMatte":            "glossy",
    "MediaPolarity":            "positive",
    "MediaColour":              "colour"
  },
  "RenderingIntent":    "Relative",
  "PCSIlluminant": [0.9642, 1.0, 0.8249],
  "ProfileCreator":     "ICC "
}
```

Common `ProfileDeviceClass` values:

| Value  | Meaning           |
|--------|-------------------|
| `scnr` | Scanner           |
| `mntr` | Display / Monitor |
| `prtr` | Printer / Output  |
| `link` | DeviceLink        |
| `spac` | Color Space       |
| `abst` | Abstract          |
| `nmcl` | Named Color       |
| `cenc` | Color Encoding    |

Common `DataColourSpace` / `PCS` values: `RGB `, `CMYK`, `XYZ `, `Lab `.

For iccMAX spectral profiles, add:

```json
"SpectralPCS":   "rs0024",
"SpectralRange": { "start": 380.0, "end": 730.0, "steps": 36 }
```

For iccMAX bispectral profiles, add:

```json
"BiSpectralRange": { "start": 300.0, "end": 730.0, "steps": 44 }
```

### Tags Array

`Tags` is an ordered JSON array. Each element is a **single-key object** whose key is
the ICC tag name (e.g. `"redMatrixColumnTag"`) and whose value holds the tag data.

```json
"Tags": [
  {
    "redMatrixColumnTag": {
      "data": {
        "type": "XYZType",
        "XYZ":  [0.4361, 0.2225, 0.0139]
      }
    }
  }
]
```

The `data` object always contains a `"type"` field identifying the ICC tag type,
followed by the type-specific fields at the same level.

**Shared tags** (where two tag signatures point to the same data) are written with a
`"sameAs"` field referencing the key of the first occurrence:

```json
{ "AToB1Tag": { "sameAs": "AToB0Tag" } }
```

**Private tags** use a generated key (`"PrivateTag_1"`, etc.) and carry a `"sig"` field
with the raw four-character signature:

```json
{ "PrivateTag_1": { "sig": "cust", "data": { "type": "PrivateType", ... } } }
```

---

## Common Tag Types

### Text Tags

#### `multiLocalizedUnicodeType` -- localised Unicode text

Used for `profileDescriptionTag` (desc), `copyrightTag`, and similar tags.

```json
{
  "profileDescriptionTag": {
    "data": {
      "type": "multiLocalizedUnicodeType",
      "localizedStrings": [
        { "language": "en", "country": "US", "text": "My sRGB Profile" }
      ]
    }
  }
}
```

#### `utf8TextType` -- simple UTF-8 string

```json
{
  "charTargetTag": {
    "data": {
      "type": "utf8TextType",
      "text": "My target name"
    }
  }
}
```

#### `textType` -- ASCII text (ICC v2/v4)

```json
{
  "copyrightTag": {
    "data": {
      "type": "textType",
      "text": "Copyright 2024 My Company"
    }
  }
}
```

### Numeric Tags

#### `XYZType` -- one or more XYZ tristimulus values

Each XYZ value is a JSON array `[X, Y, Z]`. The `"XYZ"` field is an array of such triplets.

```json
{
  "mediaWhitePointTag": {
    "data": {
      "type": "XYZType",
      "XYZ":  [0.9505, 1.0000, 1.0891]
    }
  }
}
```

#### `s15Fixed16ArrayType` / `u16Fixed16ArrayType` -- numeric arrays

```json
{
  "chromaticAdaptationTag": {
    "data": {
      "type":   "s15Fixed16ArrayType",
      "values": [ 1.0479, 0.0229, -0.0502,
                  0.0296, 0.9904, -0.0171,
                 -0.0092, 0.0151,  0.7519 ]
    }
  }
}
```

### Curve Tags

#### `curveType` -- tone-reproduction curve (TRC)

The `"curveType"` field selects one of three subtypes: `"identity"`, `"gamma"`, or `"table"`.

Identity (0-entry ICC no-op):
```json
{
  "redTRCTag": {
    "data": { "type": "curveType", "curveType": "identity" }
  }
}
```

Sampled identity (linear ramp with a specific number of entries — emitted instead of a full table when all entries match the ramp `i/(n-1)` within 16-bit quantisation tolerance):
```json
{
  "redTRCTag": {
    "data": { "type": "curveType", "curveType": "identity", "size": 256 }
  }
}
```

When parsing, a `"size"` of 2 or more reconstructs the linear ramp `m_Curve[i] = i/(size-1)`. When `"size"` is absent the 0-entry ICC identity is used.

Gamma:
```json
{
  "redTRCTag": {
    "data": { "type": "curveType", "curveType": "gamma", "gamma": 2.2 }
  }
}
```

Sampled table:
```json
{
  "redTRCTag": {
    "data": {
      "type":     "curveType",
      "curveType": "table",
      "table":    [0.0, 0.0031, 0.0145, 0.0331, ...]
    }
  }
}
```

#### `parametricCurveType` -- parametric TRC

```json
{
  "redTRCTag": {
    "data": {
      "type":         "parametricCurveType",
      "functionType": 3,
      "params":       [2.4, 0.9479, 0.0521, 0.0405, 0.0031]
    }
  }
}
```

`functionType` parameter counts:

| `functionType` | Formula                        | `params` count |
|----------------|--------------------------------|----------------|
| `0`            | `Y = X^gamma`                     | 1              |
| `1`            | `Y = (aX + b)^gamma`              | 3              |
| `2`            | `Y = (aX + b)^gamma + c`          | 4              |
| `3`            | sRGB-style (two-segment)       | 5              |
| `4`            | Full CIE 122-1996 (seven-param)| 7              |

#### `segmentedCurveType` -- piecewise segmented curve (iccMAX)

Segments cover the real line with `"-infinity"` / `"+infinity"` at the outermost boundaries.
Segment types are `"FormulaSegment"` or `"SampledSegment"`.

```json
{
  "redTRCTag": {
    "data": {
      "type": "segmentedCurveType",
      "segments": [
        {
          "start": "-infinity",
          "end":   0.04045,
          "type":  "FormulaSegment",
          "functionType": 0,
          "parameters":   [1.0, 0.07739938, 0.0125, 0.0]
        },
        {
          "start": 0.04045,
          "end":   "+infinity",
          "type":  "FormulaSegment",
          "functionType": 0,
          "parameters":   [2.4, 0.94786562, 0.05213438, 0.0031308]
        }
      ]
    }
  }
}
```

Segment types:

| `type`            | Required fields               |
|-------------------|-------------------------------|
| `FormulaSegment`  | `functionType`, `parameters`  |
| `SampledSegment`  | `samples`                     |

### Colorant Tags

#### `colorantTableType` — colorant names and PCS coordinates

Because `ToJson`/`ParseJson` have no access to the profile header, the encoding of the `"pcs"` arrays is declared explicitly via a `"pcsEncoding"` field:

| `pcsEncoding` | `pcs` array contents |
|---------------|----------------------|
| `"Lab"` (default) | L\* (0–100), a\*, b\* (−128–127) — human-readable CIE Lab |
| `"XYZ"` | X, Y, Z floats (0–2 range) |
| `"16bit"` | Raw ICC U16 integers (0–65535) |

`ToJson` always writes `"pcsEncoding": "Lab"`. `ParseJson` defaults to `"Lab"` when the field is absent (backward compatible with files written before this field was added).

```json
{
  "colorantTableTag": {
    "data": {
      "type": "colorantTableType",
      "pcsEncoding": "Lab",
      "colorantTable": [
        { "name": "Cyan",    "pcs": [55.11, -37.40,  -5.18] },
        { "name": "Magenta", "pcs": [48.24,  74.12, -49.52] },
        { "name": "Yellow",  "pcs": [89.02,  -5.68,  93.14] },
        { "name": "Black",   "pcs": [ 0.00,   0.00,   0.00] }
      ]
    }
  }
}
```

### Signature Tags

#### `signatureType`

```json
{
  "technologyTag": {
    "data": {
      "type":      "signatureType",
      "signature": "CRT "
    }
  }
}
```

### LUT Tags

#### `lutAtoBType` / `lutBtoAType` -- multi-stage LUT

MBB (Multidimensional Black Box) LUT curve arrays (`bCurves`, `mCurves`, `aCurves`) use curve objects with
`"type"` set to `"Curve"`, `"ParametricCurve"`, or `"SegmentedCurve"`.

Both CLUT data and sampled curve tables (`"curveType": "table"`) use a `"precision"` field
to indicate integer encoding, matching the behaviour of iccXML:

| `"precision"` | Data value range | Storage |
|---------------|-----------------|---------|
| `1`           | 0 - 255         | 8-bit   |
| `2`           | 0 - 65535       | 16-bit  |
| *(absent)*    | floating-point  | float   |

Named colour device coordinates (`"deviceCoords"`) are always 16-bit integers (0-65535).

```json
{
  "AToB0Tag": {
    "data": {
      "type":           "lutAtoBType",
      "inputChannels":  3,
      "outputChannels": 3,
      "bCurves": [
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" }
      ],
      "matrix": {
        "e": [ 0.4361, 0.3851, 0.1431,
               0.2225, 0.7169, 0.0606,
               0.0139, 0.0971, 0.7141 ]
      },
      "mCurves": [
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" }
      ],
      "clut": {
        "gridPoints": [9, 9, 9],
        "precision": 2,
        "data": [0, 0, 0,  128, 64, 32,  ...]
      },
      "aCurves": [
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" },
        { "type": "Curve", "curveType": "identity" }
      ]
    }
  }
}
```

MBB curve type values:

| `type`           | Subtype fields                              |
|------------------|---------------------------------------------|
| `Curve`          | `curveType` (`"identity"`, `"gamma"`, `"table"`), `size` (identity with entries), `gamma`, `table` |
| `ParametricCurve`| `functionType`, `params`                    |
| `SegmentedCurve` | `segments` array                            |

---

## Multi-Process Element (MPE) Tags

iccMAX profiles use `multiProcessElementType` tags containing an ordered pipeline
of processing elements. Each element is a **flat object** with a `"type"` discriminator
and `"inputChannels"` / `"outputChannels"` counts.

```json
{
  "AToB1Tag": {
    "data": {
      "type":           "multiProcessElementType",
      "inputChannels":  3,
      "outputChannels": 3,
      "elements": [
        {
          "type":           "CurveSetElement",
          "inputChannels":  3,
          "outputChannels": 3,
          "curves": [ ... ]
        },
        {
          "type":           "MatrixElement",
          "inputChannels":  3,
          "outputChannels": 3,
          "matrix": [0.4361, 0.3851, 0.1431,
                     0.2225, 0.7169, 0.0606,
                     0.0139, 0.0971, 0.7141],
          "constants": [0.0, 0.0, 0.0]
        }
      ]
    }
  }
}
```

### MPE Element Types

| `type`                       | Purpose                                         |
|------------------------------|-------------------------------------------------|
| `CurveSetElement`            | Per-channel independent curves                  |
| `MatrixElement`              | Linear matrix with optional additive constants  |
| `CLutElement`                | N-dimensional colour lookup table               |
| `ExtCLutElement`             | CLUT with explicit storage type                 |
| `BAcsElement` / `EAcsElement`| Auxiliary colour space conversion bookends      |
| `CalculatorElement`          | Programmable calculator (iccMAX)                |
| `XYZToJabElement`            | CIECAM02 forward conversion                     |
| `JabToXYZElement`            | CIECAM02 inverse conversion                     |
| `TintArrayElement`           | Spectral tinting via a tag array                |
| `EmissionMatrixElement`      | Spectral-to-colorimetric matrix                 |
| `EmissionCLutElement`        | Spectral CLUT                                   |
| `EmissionObserverElement`    | Observer spectral weighting                     |

### CurveSetElement Curves

Each entry in `"curves"` is a curve object with a `"type"` field:

| `type`                  | Fields                                                 |
|-------------------------|--------------------------------------------------------|
| `SegmentedCurve`        | `segments` array (same format as `segmentedCurveType`) |
| `SingleSampledCurve`    | `firstEntry`, `lastEntry`, `samples`, `extensionType`  |
| `SampledCalculatorCurve`| `firstEntry`, `lastEntry`, `desiredSize`, `calculator` |
| `DuplicateCurve`        | `index` -- source channel index (integer)               |

```json
"curves": [
  {
    "type": "SegmentedCurve",
    "segments": [
      {
        "start": "-infinity", "end": "+infinity",
        "type": "FormulaSegment",
        "functionType": 0,
        "parameters": [1.0, 0.5, 0.0, 0.0]
      }
    ]
  },
  { "type": "DuplicateCurve", "index": 0 },
  { "type": "DuplicateCurve", "index": 0 }
]
```

---

## Complete Profile Examples

### Simple Matrix/TRC Display Profile

```json
{
  "IccProfile": {
    "Header": {
      "ProfileVersion":     "4.4",
      "ProfileDeviceClass": "mntr",
      "DataColourSpace":    "RGB ",
      "PCS":                "XYZ ",
      "CreationDateTime":   "2024-01-15T00:00:00",
      "ProfileFileSignature": "acsp",
      "PrimaryPlatform":    "MSFT",
      "CMMFlags": { "EmbeddedInFile": false, "UseWithEmbeddedDataOnly": false },
      "DeviceAttributes": {
        "ReflectiveOrTransparency": "reflective",
        "GlossyOrMatte": "glossy",
        "MediaPolarity": "positive",
        "MediaColour": "colour"
      },
      "RenderingIntent": "Relative",
      "PCSIlluminant": [0.9642, 1.0, 0.8249]
    },
    "Tags": [
      {
        "profileDescriptionTag": {
          "data": {
            "type": "multiLocalizedUnicodeType",
            "localizedStrings": [
              { "language": "en", "country": "US", "text": "Simple sRGB Display Profile" }
            ]
          }
        }
      },
      {
        "copyrightTag": {
          "data": { "type": "textType", "text": "Public Domain" }
        }
      },
      {
        "mediaWhitePointTag": {
          "data": { "type": "XYZType", "XYZ": [0.9505, 1.0000, 1.0891] }
        }
      },
      {
        "redMatrixColumnTag": {
          "data": { "type": "XYZType", "XYZ": [0.4361, 0.2225, 0.0139] }
        }
      },
      {
        "greenMatrixColumnTag": {
          "data": { "type": "XYZType", "XYZ": [0.3851, 0.7169, 0.0971] }
        }
      },
      {
        "blueMatrixColumnTag": {
          "data": { "type": "XYZType", "XYZ": [0.1431, 0.0606, 0.7141] }
        }
      },
      {
        "redTRCTag": {
          "data": {
            "type": "parametricCurveType",
            "functionType": 3,
            "params": [2.4, 0.9479, 0.0521, 0.0405, 0.0031]
          }
        }
      },
      { "greenTRCTag": { "sameAs": "redTRCTag" } },
      { "blueTRCTag":  { "sameAs": "redTRCTag" } }
    ]
  }
}
```

### Minimal Encoding-Class Profile

Encoding profiles declare a colour space encoding without transformation data.

```json
{
  "IccProfile": {
    "Header": {
      "ProfileVersion":     "5.0",
      "ProfileDeviceClass": "cenc",
      "DataColourSpace":    "RGB ",
      "PCS":                "XYZ ",
      "CreationDateTime":   "2024-01-15T00:00:00",
      "ProfileFileSignature": "acsp",
      "RenderingIntent":    "Perceptual",
      "PCSIlluminant": [0.9642, 1.0, 0.8249]
    },
    "Tags": [
      {
        "referenceNameTag": {
          "data": { "type": "utf8TextType", "text": "sRGB" }
        }
      }
    ]
  }
}
```

### iccMAX MPE Profile (RGB -> XYZ)

```json
{
  "IccProfile": {
    "Header": {
      "ProfileVersion":     "5.0",
      "ProfileDeviceClass": "spac",
      "DataColourSpace":    "RGB ",
      "PCS":                "XYZ ",
      "CreationDateTime":   "2024-01-15T00:00:00",
      "ProfileFileSignature": "acsp",
      "PrimaryPlatform":    "MSFT",
      "CMMFlags": { "EmbeddedInFile": false, "UseWithEmbeddedDataOnly": false },
      "DeviceAttributes": {
        "ReflectiveOrTransparency": "reflective",
        "GlossyOrMatte": "glossy",
        "MediaPolarity": "positive",
        "MediaColour": "colour"
      },
      "RenderingIntent": "Relative",
      "PCSIlluminant": [0.9642, 1.0, 0.8249]
    },
    "Tags": [
      {
        "profileDescriptionTag": {
          "data": {
            "type": "multiLocalizedUnicodeType",
            "localizedStrings": [
              { "language": "en", "country": "US", "text": "Example iccMAX Profile" }
            ]
          }
        }
      },
      {
        "AToB1Tag": {
          "data": {
            "type":           "multiProcessElementType",
            "inputChannels":  3,
            "outputChannels": 3,
            "elements": [
              {
                "type":           "CurveSetElement",
                "inputChannels":  3,
                "outputChannels": 3,
                "curves": [
                  {
                    "type": "SegmentedCurve",
                    "segments": [
                      {
                        "start": "-infinity", "end": 0.04045,
                        "type": "FormulaSegment",
                        "functionType": 0,
                        "parameters": [1.0, 0.07739938, 0.0125, 0.0]
                      },
                      {
                        "start": 0.04045, "end": "+infinity",
                        "type": "FormulaSegment",
                        "functionType": 0,
                        "parameters": [2.4, 0.94786562, 0.05213438, 0.0031308]
                      }
                    ]
                  },
                  { "type": "DuplicateCurve", "index": 0 },
                  { "type": "DuplicateCurve", "index": 0 }
                ]
              },
              {
                "type":           "MatrixElement",
                "inputChannels":  3,
                "outputChannels": 3,
                "matrix": [
                  0.4361, 0.3851, 0.1431,
                  0.2225, 0.7169, 0.0606,
                  0.0139, 0.0971, 0.7141
                ],
                "constants": [0.0, 0.0, 0.0]
              }
            ]
          }
        }
      }
    ]
  }
}
```

---

## Tag Structures (`tagStructType`)

iccMAX struct tags contain named member tags. Each member in `"memberTags"` is a
single-key object whose key is the element name and whose value is a flat tag object
with a `"type"` field (same `type` values as top-level tag data).

```json
{
  "colorEncodingParamsTag": {
    "data": {
      "type":          "tagStructType",
      "structureType": "colorEncodingParamsStruct",
      "memberTags": [
        {
          "blueColorantTag": {
            "type": "XYZType",
            "XYZ":  [0.1431, 0.0606, 0.7141]
          }
        }
      ]
    }
  }
}
```

---

## Tag Arrays (`tagArrayType`)

iccMAX array tags contain an ordered list of typed tag objects in `"arrayTags"`.
Each element is a flat object with a `"type"` field (same `type` values as
top-level tag data).

```json
{
  "spectralViewingConditionsTag": {
    "data": {
      "type":      "tagArrayType",
      "arrayType": "namedColorArray",
      "arrayTags": [
        {
          "type": "utf8TextType",
          "text": "D50"
        }
      ]
    }
  }
}
```

---

## Signatures

Signature fields (4-character ICC codes) appear in several tag types and the profile
header. Four formats are accepted:

| Format        | Example      | Meaning                                    |
|---------------|--------------|--------------------------------------------|
| Empty string  | ""           | Zero / unset signature                     |
| 4 characters  | "RGB "       | Direct four-character code (space-padded)  |
| 6 characters  | "rs0024"     | Two-char prefix + 4 hex digits             |
| 8 hex digits  | "73663031"   | Full signature as hex                      |

---

## Tips and Common Mistakes

- **Always include the `"IccProfile"` wrapper** -- the top-level key is `"IccProfile"`,
  not `"Header"` or `"Tags"` directly.
- **XYZ values are arrays, not objects** -- write `[0.95, 1.0, 1.09]` not
  `{ "X": 0.95, "Y": 1.0, "Z": 1.09 }`.
- **`"type"` is always required** on `data` objects, MPE elements, curve-set curves,
  MBB curves, and struct member tags. Missing `"type"` is the most common parse error.
- **`curveType` tag needs the subtype** -- a curveType tag requires both `"type": "curveType"`
  (the ICC tag type) and `"curveType": "gamma"` (the subtype selector).
- **MBB curve types are `"Curve"`, `"ParametricCurve"`, `"SegmentedCurve"`** -- not
  `"curveType"` / `"parametricCurveType"` / `"segmentedCurveType"`.
- **Segment types are PascalCase** -- `"FormulaSegment"` and `"SampledSegment"`,
  not `"formulaSegment"` / `"sampledSegment"`.
- **`parametricCurveType` uses `"params"`** -- not `"Parameters"` or `"parameters"`.
- **Segment boundaries must be contiguous** -- segments must form a non-overlapping
  cover of the real line. Use `"-infinity"` / `"+infinity"` for the outermost ends.
- **`sameAs` must come after the original** -- the referenced tag key must appear
  earlier in the `Tags` array.
- **Validation before use** -- always run `IccDumpProfile` after building a profile
  with `IccFromJson` to catch structural issues before the profile is deployed.
- **Round-trip inspection** -- convert an existing reference profile with `IccToJson`
  first to see the expected JSON structure for a given profile class.

---

## Integration examples

### Standalone C++ example

The [`examples/hello-iccdev/`](../examples/hello-iccdev/) directory contains a
minimal standalone project that links IccProfLib2, IccXML2, and IccJSON2. When
built with `USE_ICCJSON=1` it demonstrates JSON round-tripping via the
`CIccProfileJson::ToJson()` and `ParseJson()` APIs, factory registration, and
library version reporting. See
[`examples/hello-iccdev/README.md`](../examples/hello-iccdev/README.md) for
build instructions.

### IIS ISAPI server-side integration

The Windows IIS ISAPI sample at
[`Tools/Winnt/IccIisIsapi/`](../Tools/Winnt/IccIisIsapi/) conditionally invokes
`iccToJson` and `iccFromJson` as part of its tool pipeline when IccJSON is built.
Uploaded ICC profiles are converted to JSON for inspection, and XML-originated
profiles are also exported to JSON. See
[`api.md`](../Tools/Winnt/IccIisIsapi/api.md) for the HTTP endpoint reference.
