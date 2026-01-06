# iccRoundTrip

## Overview

`iccRoundTrip` is a command-line utility that performs **round-trip colorimetric evaluation** of ICC color profiles. It computes ?E (Delta E) statistics through two transformation loops and evaluates **Perceptual Reference Medium Gamut (PRMG)** compatibility when applicable.

This tool is part of the ICC profile diagnostic suite and is particularly useful for developers validating color fidelity through ICC transformation pipelines.

---

## Features

- Performs Lab ? Profile ? Lab round-trip comparisons
- Evaluates profile using a perceptual gamut compatibility test (PRMG)
- Reports min, mean, and max Delta E values
- Highlights worst-case Lab values
- Offers control over rendering intent and MPE evaluation path

---

## Output Details

### Round Trip Statistics
- **Round Trip 1**: Lab (device) ? Profile ? Lab
- **Round Trip 2**: Round Trip 1 output reprocessed via profile

For each trip:
- Min, Mean, Max ?E values
- Corresponding max L*, a*, b* values

### PRMG Evaluation (if supported by profile)
- % of values below ?E thresholds: 1.0, 2.0, 3.0, 5.0, 10.0
- Total pixel count evaluated


---

## Usage Examples

### Basic Round Trip Evaluation
```sh
Tools/IccRoundTrip/iccRoundTrip input.icc
```

### Specify rendering intent (e.g., Absolute Colorimetric)
```sh
Tools/IccRoundTrip/iccRoundTrip input.icc 3
```

### Enable MPE (Multi-Processing Elements) mode
```sh
Tools/IccRoundTrip/iccRoundTrip input.icc 1 1
```

---

## Interpretation Notes

- **Rendering Intent** options:
  - 0 = Perceptual
  - 1 = Relative Colorimetric
  - 2 = Saturation
  - 3 = Absolute Colorimetric

- **MPE Toggle**:
  - 0 = Disable MPE (default)
  - 1 = Enable MPE path

---

## Changelog

- Original implementation by Max Derhak (2007)
- Minor modifications by David Hoyt (2025) for improved PRMG evaluation logging and batch compatibility
