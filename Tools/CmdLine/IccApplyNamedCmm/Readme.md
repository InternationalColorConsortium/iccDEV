# iccApplyNamedCmm

## Overview

`iccApplyNamedCmm` is a command-line tool that applies a chain of ICC profiles to textual or structured color data, producing transformed results in various formats. It supports named color transformations, JSON and legacy data inputs, and ICCv5 capabilities including debugging of calculator-based profiles.

---

## Features

- Supports color data in:
  - Legacy (plain text)
  - JSON structured format
  - Embedded ICC configurations
- Applies ICC profiles using ICCMAX-style CMM
- Supports named-to-named and pixel-to-named transformations
- Built-in support for:
  - Luminance PCS matching
  - Profile Connection Conditions (PCC)
  - ICC environment variable hints
- Outputs transformed color data in:
  - IT8
  - JSON
  - Text

---

## Usage

### Config-Based Mode

```sh
iccApplyNamedCmm -cfg config.json
```

- `config.json` must include:
  - `dataFiles`
  - `profileSequence`
  - (optionally) `colorData`

### Legacy CLI Mode

```sh
iccApplyNamedCmm input.txt encoding[:precision[:digits]] interpolation {{-ENV:tag value} profile.icc intent {-PCC pcc.icc}}
```

---

## Arguments

- **`encoding` values**:
  - `0` = Lab/XYZ Value
  - `1` = Percent
  - `2` = Unit Float
  - `3` = Raw Float
  - `4` = 8-bit
  - `5` = 16-bit
  - `6` = 16-bit ICCv2 style

- **Interpolation**:
  - `0` = Linear
  - `1` = Tetrahedral

- **Intent** (plus modifiers):
  - `0–3`: Perceptual, Relative, Saturation, Absolute
  - `+10`: Disable D2Bx/B2Dx
  - `+20`: Preview
  - `+30`: Gamut
  - `+40`: With BPC
  - `+100`: Luminance-based PCS match
  - `+1000`: ICCv5 SubProfile
  - `+100000`: Use HToS tag

---

## Output Formats

Determined by config or filename:
- `output.txt`: legacy textual
- `output.json`: JSON color set
- `output.it8`: IT8 table

---

## Example

```sh
iccApplyNamedCmm -cfg config_named.json
```

```sh
iccApplyNamedCmm colors.txt 0:4:7 1 profile.icc 1
```

---

## Changelog

- Original implementation by Max Derhak (2003)
- JSON support and calculator debugger added by Max Derhak (2024)
- Structural cleanup and usage clarity by David Hoyt (2025)
