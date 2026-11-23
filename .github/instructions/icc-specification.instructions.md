---
applyTo: "IccProfLib/icProfileHeader.h"
---

# ICC Profile Specification Quick Reference

## ICC.1 Header Structure (128 bytes)

| Offset | Size | Field | Reference |
|--------|------|-------|-----------|
| 0-3 | 4 | Profile size (bytes, big-endian) | ICC.1 clause 7.2.2 |
| 4-7 | 4 | Preferred CMM Type | ICC.1 clause 7.2.3 |
| 8-11 | 4 | Version (BCD: major.minor.bugfix.0) | ICC.1 clause 7.2.4 |
| 12-15 | 4 | Profile/Device class | ICC.1 clause 7.2.5 |
| 16-19 | 4 | Color space of data | ICC.1 clause 7.2.6 |
| 20-23 | 4 | PCS (Profile Connection Space) | ICC.1 clause 7.2.7 |
| 24-35 | 12 | Date/time created | ICC.1 clause 7.2.8 |
| 36-39 | 4 | Magic: `'acsp'` (0x61637370) | ICC.1 clause 7.2.9 |
| 40-43 | 4 | Primary platform | ICC.1 clause 7.2.10 |
| 44-47 | 4 | Profile flags | ICC.1 clause 7.2.11 |
| 48-51 | 4 | Device manufacturer | ICC.1 clause 7.2.12 |
| 52-55 | 4 | Device model | ICC.1 clause 7.2.13 |
| 56-63 | 8 | Device attributes | ICC.1 clause 7.2.14 |
| 64-67 | 4 | Rendering intent | ICC.1 clause 7.2.15 |
| 68-79 | 12 | PCS illuminant (D50: X=0.9642 Y=1.0 Z=0.8249) | ICC.1 clause 7.2.16 |
| 80-83 | 4 | Profile creator | ICC.1 clause 7.2.17 |
| 84-99 | 16 | Profile ID (MD5) | ICC.1 clause 7.2.18 |
| 100-127 | 28 | Reserved (must be zero) | ICC.1 clause 7.2.19 |

## ICC.2 Extended Header Fields (bytes 100-127)

ICC.2:2023 Table 12 extends the same 128-byte header. Do not apply the ICC.1
reserved-byte rule to an ICC.2 profile.

| Offset | Size | Field | Reference |
|--------|------|-------|-----------|
| 100-103 | 4 | Spectral PCS signature | ICC.2 clause 7.2.21 |
| 104-109 | 6 | Spectral PCS wavelength range | ICC.2 clause 7.2.22 |
| 110-115 | 6 | Bi-spectral PCS wavelength range | ICC.2 clause 7.2.23 |
| 116-119 | 4 | MCS signature | ICC.2 clause 7.2.24 |
| 120-123 | 4 | Profile/device subclass | ICC.2 clause 7.2.25 |
| 124-127 | 4 | Reserved (must be zero) | ICC.2 clause 7.2.26 |

ICC.2 clause 7.2.3 defines a separate restricted header layout for
ColorEncodingSpace (`cenc`) profiles; consult that table rather than inferring
requirements from either summary above.

## ICC.1 Profile Classes

| Signature | Name | Normative class requirements |
|-----------|------|------------------------------|
| `scnr` | Input (Scanner) | ICC.1 clause 8.3 |
| `mntr` | Display (Monitor) | ICC.1 clause 8.4 |
| `prtr` | Output (Printer) | ICC.1 clause 8.5 |
| `link` | DeviceLink | ICC.1 clause 8.6 |
| `spac` | ColorSpace | ICC.1 clause 8.7 |
| `abst` | Abstract | ICC.1 clause 8.8 |
| `nmcl` | NamedColor | ICC.1 clause 8.9 |

ICC.1 clause 8.2 defines common requirements and exceptions. The allowed and
required transform tags depend on the class, color spaces, and processing
model, so use the applicable class table rather than a one-tag shorthand.

## Version Encoding (BCD at bytes 8-11)

| Version | Hex | Examples |
|---------|-----|---------|
| v2.1.0.0 | `02100000` | Legacy sRGB, AdobeRGB |
| v2.4.0.0 | `02400000` | Common v2 profiles |
| v4.3.0.0 | `04300000` | ICC.1-2010 |
| v4.4.0.0 | `04400000` | ICC.1-2022-05 (current) |
| v5.0.0.0 | `05000000` | iccMAX profile version required by ICC.2:2023 clause 7.2.6 |

ICC.2:2023 is a revised specification publication, but clause 7.2.6 continues
to encode conforming profile headers as version 5.0.0.0. Do not substitute a
document-edition label into the four profile-version bytes.

For ICC.1, bytes 10-11 are reserved and zero. For ICC.2, clause 7.2.6 uses
bytes 10 and 11 for the profile-subclass major and minor versions when the
profile/device subclass field is nonzero; they remain zero when there is no
subclass.

## Rendering Intents (bytes 64-67, upper 16 bits = 0)

| Value | Intent | Use Case |
|-------|--------|----------|
| 0 | Perceptual | Photos, general images |
| 1 | Media-relative colorimetric | Proofing |
| 2 | Saturation | Business graphics |
| 3 | ICC-absolute colorimetric | Spot colors |

## Color Space Signatures

| Signature | Channels | Description |
|-----------|----------|-------------|
| `RGB ` | 3 | RGB color space |
| `CMYK` | 4 | CMYK color space |
| `GRAY` | 1 | Grayscale |
| `Lab ` | 3 | CIELAB (PCS) |
| `XYZ ` | 3 | CIEXYZ (PCS) |
| `HLS ` | 3 | HLS |
| `HSV ` | 3 | HSV |
| `Luv ` | 3 | CIELUV |
| `YCbr` | 3 | YCbCr |
| `nCLR` | n | n-channel (n=2..15) |

## Tag Table (starts at byte 128)

```
Byte 128-131: Tag count (uint32 big-endian)
Byte 132+:    Tag entries, each 12 bytes:
  [0-3] Tag signature (4 bytes)
  [4-7] Offset from profile start (uint32 BE)
  [8-11] Tag data size (uint32 BE)
```

Constraints:
- No duplicate tag signatures
- All offsets must be 4-byte aligned
- offset + size <= profile size
- Tags MAY share offsets (same data), but then sizes MUST match
- No partial overlaps between tag data regions

## Key Tag Types

| Signature | Type | Description |
|-----------|------|-------------|
| `desc` | multiLocalizedUnicode | Profile description |
| `cprt` | multiLocalizedUnicode | Copyright text |
| `wtpt` | XYZ | Media white point |
| `chad` | s15Fixed16Array | Chromatic adaptation matrix |
| `A2B0` | lutAToB / lut8 / lut16 | AToB transform (perceptual) |
| `B2A0` | lutBToA / lut8 / lut16 | BToA transform (perceptual) |
| `rTRC` | curveType / parametricCurve | Red TRC |
| `gTRC` | curveType / parametricCurve | Green TRC |
| `bTRC` | curveType / parametricCurve | Blue TRC |
| `rXYZ` | XYZ | Red matrix column |
| `gXYZ` | XYZ | Green matrix column |
| `bXYZ` | XYZ | Blue matrix column |
| `gamt` | lutBToA | Gamut check |
| `ncl2` | namedColor2 | Named color data |

## s15Fixed16Number Encoding

Signed 15.16 fixed-point: `value = raw_int32 / 65536.0`

Range: -32768.0 to +32767.99998 (approximately)

PCS D50 illuminant (canonical values):
- X = 0.9642 -> `0x0000F6D6`
- Y = 1.0000 -> `0x00010000`
- Z = 0.8249 -> `0x0000D32D`

## Profile ID Calculation (MD5)

MD5 hash of the entire profile with these fields zeroed before hashing:
- Bytes 44-47 (profile flags)
- Bytes 64-67 (rendering intent)
- Bytes 84-99 (profile ID field itself)

## Specification Documents

| Document | Scope |
|----------|-------|
| [ICC.1:2022](https://www.color.org/specification/ICC.1-2022-05.pdf) | v2/v4 profile structure - primary reference |
| [ICC.2:2023](https://www.color.org/specification/ICC.2-2023.pdf) | iccMAX profiles, spectral PCS, calculator elements - primary reference |
| [Research specification archive](https://github.com/xsscx/research/tree/main/docs/iccDEV/specifications) | Supporting cross-reference for ICC specifications, errata, and technical notes; not a substitute for the normative documents |
| TIFF 6.0 clause 8 | TIFFTAG_ICCPROFILE (tag 34675) for embedded ICC |
| PNG 1.2 clause 4.2.2.3 | iCCP chunk for embedded ICC |
| JPEG JFIF | APP2 ICC_PROFILE marker (multi-segment reassembly) |
