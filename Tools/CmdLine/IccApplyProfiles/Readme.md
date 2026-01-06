# iccApplyProfiles

## Overview

`iccApplyProfiles` is a command-line tool that applies a sequence of ICC profiles to a TIFF image. It supports TIFF read/write with compression, planar or interleaved data, optional ICC embedding, and various interpolation/rendering intent modes.

It is particularly well-suited for automated pipelines where precise color transformations must be applied across TIFF files using complex ICC profiles, including those with profile connection conditions.

---

## Features

- Applies multiple ICC profiles to a TIFF image
- Supports input/output bit depths: 8-bit, 16-bit, float (32-bit)
- Maintains embedded profile or overrides with custom profile
- Handles profile connection conditions (PCC)
- Includes support for BPC (black point compensation)
- Offers both linear and tetrahedral interpolation
- Full CLI and JSON configuration support
- Supports Luminance PCS adjustments and environmental variable overrides

---

## Usage

### Command-Line Mode

```sh
iccApplyProfiles input.tif output.tif encoding compression planar embed_icc interpolation {{-ENV:sig value} profile.icc intent {-PCC pcc.icc}}
```

### Config File Mode

```sh
iccApplyProfiles -cfg config.json
```

---

## Arguments (CLI Mode)

- `encoding`:
  - `0` = Same as source
  - `1` = 8-bit
  - `2` = 16-bit
  - `4` = Float (32-bit)

- `compression`:
  - `0` = None
  - `1` = LZW

- `planar`:
  - `0` = Interleaved
  - `1` = Separated

- `embed_icc`:
  - `0` = Do not embed
  - `1` = Embed last ICC profile

- `interpolation`:
  - `0` = Linear
  - `1` = Tetrahedral

- `intent`:
  - Standard (0–3), Preview (20–23), Gamut (30+), No D2Bx (10+), BPC (40+)
  - `+100` adds luminance-based PCS adjustment
  - `+1000` enables ICCv5 sub-profile selection

---

## JSON Config Support

The tool also accepts a JSON configuration file that must define:

- `"imageFiles"` (for input/output settings)
- `"profileSequence"` (for color transformations)

---

## Example

```sh
iccApplyProfiles input.tif output.tif 2 1 0 1 1 source.icc 1 printer.icc 0
```

---

## Changelog

- Original version by Max Derhak (2003)
- Enhanced for iccMAX and JSON configs in 2014–2016
- CLI+JSON dual mode & extended BPC / luminance logic by David Hoyt (2025)
