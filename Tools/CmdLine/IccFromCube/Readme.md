# iccFromCube

## Overview

`iccFromCube` is a command-line utility that converts a 3D LUT file in `.cube` format (common in video and color grading applications) into an ICC.2 DeviceLink profile. This tool enables integration of creative grading LUTs into ICC color management workflows.

---

## Features

- Parses 3D LUTs from `.cube` files
- Creates ICC.2 v5 DeviceLink profiles
- Automatically handles custom input domains using linear mapping curves
- Embeds LUT data as a multi-process element (MPE)
- Preserves title and comments as profile metadata

---

## Supported Format

- Only **3D LUTs** are supported
- Cube file must contain `LUT_3D_SIZE` directive
- Optional directives:
  - `DOMAIN_MIN`, `DOMAIN_MAX`
  - `LUT_3D_INPUT_RANGE`
  - `TITLE`, comments (`#`)

---

## Usage

```sh
Tools/IccFromCube/iccFromCube input.cube output.icc
```

---

## Output

- Profile Class: `DeviceLink`
- Version: `ICC.2 v5`
- Color Space: `RGB` (input and PCS)
- Tags:
  - `AToB0`: MPE with optional input curves and CLUT
  - `desc`: Title from cube
  - `cprt`: Comment lines from cube

---

## Example

```sh
iccFromCube creativeGrade.cube creativeGrade.icc
```

---

## Changelog

- Initial implementation by Max Derhak (Mar 2023)
- File parsing and ICCv5 MPE improvements by David Hoyt (2025)
