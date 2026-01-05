# 🔄 iccV5DspObsToV4Dsp

**iccV5DspObsToV4Dsp** converts ICC v5 Display and Observer profiles into ICC v4 Display profiles.
This enables broader compatibility with systems and applications that only support ICC v4.

---

## 🔧 Features

- ✅ **ICC v5 → v4 Conversion**
  - Converts observer-specific tags to legacy v4-compatible structures
- ✅ **Display Profile Simplification**
  - Translates spectral viewing conditions into ICC v4 colorimetric approximations
- ✅ **CLI Integration**
  - Usable in batch pipelines and profile pre-processing
- ✅ **Metadata Preservation**
  - Retains device class, rendering intent, and profile description

---

## 🚀 Usage

```bash
iccV5DspObsToV4Dsp inputV5Disp.icc inputV5Obs.icc output.icc
```

Converts the `inputV5Disp.icc` and `inputV5Obs.icc` (v5 observer-based) profiles into a legacy v4 format written to `output.icc`.

---

## 📘 Use Cases

- Ensures ICC v5 profiles can be interpreted by legacy apps
- Enables testing fallback behavior in color pipelines
- Supports roundtrip simulations for v4-only workflows

---

## 🔒 Notes

- Only applicable to Display class profiles with observer extensions
- Conversion may lose fidelity of original observer data

---

© ICC Consortium — MIT License
