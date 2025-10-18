# iccApplySearch

## Overview

`iccApplySearch` is a command-line tool that applies a sequence of profiles utilizing a search with the forward transform of the last profile.  apply the reverse application of two profiles.  When the first profile is a PCS encoding profile this provides a logical inverse of the forward transform of the last profile.  This is especally useful when the forward transfom of the last profile results in a spectral PCS without the availability of a reverse transform in the last profile and the first profile is a spectral PCS encoding profile.  Using a colorimetric PCS encoding intermediate profile with a weighted set of Profile Connection Condtions profiles allows for spectral color reproduction to be performed.  This tool supports JSON and legacy data inputs, and ICCv5 capabilities including debugging of calculator-based profiles.

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
iccApplySearch input.txt encoding[:precision[:digits]] interpolation {-ENV:tag value} profile1.icc intent1 {-ENV:tag value} mid_profile.icc mid_intent2} {-ENV:tag value}} profile2.icc intent2 init_intent2 {pcc1.pcc weight1 ... }
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
  - `+10000`: ICCv5 SubProfile

---

## Output Formats

Determined by config or filename:
- `output.txt`: legacy textual
- `output.json`: JSON color set
- `output.it8`: IT8 table

---

## Build Instructions

```sh
cd Build/
rm -rf CMakeCache.txt CMakeFiles/ Makefile IccProfLib/ IccXML/ Testing/ Tools/

cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local       -DCMAKE_BUILD_TYPE=Debug       -DCMAKE_CXX_FLAGS="-g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall"       -Wno-dev -DENABLE_TOOLS=YES Cmake/

make -j32
```

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
