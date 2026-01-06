# iccApplySearch

## Overview

`iccApplySearch` is a command-line tool that applies a sequence of profiles, utilizing a search with the forward transform of the last profile. When the first profile is a PCS encoding profile this provides a logical inverse of the forward transform of the last profile. This is especially useful when the forward transform of the last profile results in a spectral PCS without the availability of a reverse transform in the last profile and the first profile is a spectral PCS encoding profile. Using a colorimetric PCS encoding intermediate profile with a weighted set of Profile Connection Conditions profiles allows for spectral color reproduction to be performed. This tool supports JSON and legacy data inputs, and ICCv5 capabilities including debugging of calculator-based profiles.

---

## Features

- Supports color data in:
  - Legacy (plain text)
  - JSON structured format
  - Embedded ICC configurations
- Applies ICC profiles using ICCMAX-enabled CMM
- Outputs transformed color data in:
  - IT8
  - JSON
  - Text

---

## Usage

### Config-Based Mode

```sh
iccApplySearch -cfg config.json
```

- `config.json` must include:
  - `dataFiles`
  - `reverseProfileSequence`
  - (optionally) `colorData`

### Legacy CLI Mode

```sh
iccApplySearch {-debugcalc} data_file_path encoding[:precision[:digits]] interpolation {-ENV:tag value} profile1_path intent1 {{-ENV:tag value} middle_profile_path mid_intent} {-ENV:tag value} profile2_path intent2 -INIT init_intent2 {pcc_path1 weight1 ...}
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
  - `+40`: With BPC
  - `+90 + Intent - Colorimetric Only`
  - `100 + Intent - Spectral Only`
  - `+10000 - Use V5 sub-profile if present`

---

## Output Formats

Determined by config or filename:
- `output.txt`: legacy textual
- `output.json`: JSON color set
- `output.it8`: IT8 table

---

## Example

```sh
iccApplySearch -cfg config_named.json
```

```sh
iccApplySearch colors.txt 0:4:7 1 3 spec400-10-700.icc 3 profile.icc 1003 lab.icc 3 d50.icc d95.icc illA.icc
```

---

## Changelog

- Original implementation by Max Derhak (2025)
