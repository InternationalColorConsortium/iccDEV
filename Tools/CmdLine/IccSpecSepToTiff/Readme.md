# iccSpecSepToTiff

## Overview

`iccSpecSepToTiff` is a command-line utility that concatenates multiple monochrome spectral band TIFF images into a single multi-channel TIFF file. It supports optional compression and profile embedding, and is useful for assembling spectral image stacks into a standardized format.

---

## Features

- Merges multiple separated spectral TIFFs into one multichannel TIFF
- Optionally compresses the output TIFF
- Supports separation (planar) or interleaved channel format
- Preserves resolution and photometric interpretation
- Embeds ICC profile if provided

---

## Requirements

- Input TIFFs must:
  - Be grayscale (1 sample per pixel)
  - Have the same dimensions, bit depth, resolution, and photometric type
  - Be specified by a printf-style format string (e.g., `band_%03d.tif`)

- Output will:
  - Match input resolution and dimensions
  - Contain as many channels as there are spectral images
  - Use `MinIsBlack` photometric if not inverted

---

## Usage

```sh
Tools/IccSpecSepToTiff/iccSpecSepToTiff output.tiff compress_flag sep_flag input_fmt start_nm end_nm inc_nm [embedded_profile.icc]
```

### Parameters

- `output.tiff`: Name of the output TIFF file
- `compress_flag`: 1 to enable compression, 0 to disable
- `sep_flag`: 1 for separated planes, 0 for interleaved
- `input_fmt`: Format string for input filenames (e.g., `spec_%03d.tif`)
- `start_nm`: Starting wavelength (integer)
- `end_nm`: Ending wavelength (integer)
- `inc_nm`: Increment step (nm between bands)
- `embedded_profile.icc`: Optional ICC profile to embed

---

## Example

```sh
iccSpecSepToTiff merged.tiff 1 0 "bands/spec_%03d.tif" 400 700 10 profile.icc
```

Creates a compressed, interleaved TIFF from `spec_400.tif` to `spec_700.tif` in 10nm steps, embedding `profile.icc`.

---

## Changelog

- Initial implementation by Max Derhak (2013)
- Batch and ICC embedding enhancements by David Hoyt (2025)
