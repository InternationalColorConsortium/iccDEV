# iccApplyToLink

## Overview

`iccApplyToLink` is a versatile command-line tool that builds either an ICC DeviceLink profile or a `.cube` LUT file by applying a sequence of ICC profiles. It supports fine-grained control over rendering intents, LUT size, interpolation, and Profile Connection Conditions (PCC).

This tool is especially useful for creating link profiles for production workflows or transforming color spaces for VFX, printing, or display calibration.

---

## Features

- Supports output as ICC v4/v5 DeviceLink or `.cube` 3D LUT format
- Accepts a sequence of profiles and rendering intents
- Handles Profile Connection Conditions (`-PCC`)
- Applies CMM Environment variables (`-ENV:sig value`)
- Supports LUT precision and custom input ranges
- Choice of interpolation methods (linear/tetrahedral)

---

## Usage

```sh
iccApplyToLink output_file link_type lut_size option title min_input max_input use_src_xform interp profile_seq...
```

### Example

```sh
iccApplyToLink myLink.icc 0 33 1 "My DisplayLink" 0.0 1.0 1 1 sRGB.icc 1 printer.icc 1
```

---

## Parameters

- `output_file`: Path to save the ICC or `.cube` output
- `link_type`:
  - `0` = ICC DeviceLink
  - `1` = `.cube` LUT
- `lut_size`: Grid size per dimension (e.g., 33)
- `option`:
  - For ICC: `0` = v4, `1` = v5
  - For cube: precision (e.g., `4`)
- `title`: Title embedded in output
- `min_input` / `max_input`: Input value range, e.g., `0.0` to `1.0`
- `use_src_xform`: `1` = use source xform from first profile, else destination
- `interp`: `0` = linear, `1` = tetrahedral
- `profile_seq`: Sequence of profile files and intents, optionally with:
  - `-ENV:TAG value` to set environment sigs
  - `-PCC path.icc` to provide connection conditions

---

## Rendering Intents

Supports all ICC intents plus combinations:

| Code | Intent |
|------|--------|
| 0–3  | Perceptual / Relative / Saturation / Absolute |
| 10+  | Skip D2Bx/B2Dx |
| 20+  | Preview Intents |
| 40+  | BPC-enabled |
| 50+  | BDRF variations |
| 100+ | Luminance adjusted |
| 1000+| V5 sub-profile override |

---

## Changelog

- Initial implementation by Max Derhak (Mar 2023)
- Multi-output support and argument extensions by David Hoyt (2025)
