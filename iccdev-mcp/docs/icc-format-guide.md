# ICC Binary Format Guide

Quick reference for the ICC binary profile format, focused on fields
and structures relevant to iccdev-mcp tool usage and security analysis.

> **Specification**: ICC.1-2022-05 (v4.4), ICC.2-2023 (v5/iccMAX)

---

## Profile Structure

An ICC profile is a binary file with three major sections:

```
+------------------+  Offset 0
|   Header         |  128 bytes, fixed layout
+------------------+  Offset 128
|   Tag Table      |  4 + (tag_count x 12) bytes
+------------------+
|   Tag Data       |  Variable length, referenced by tag table
+------------------+
```

All multi-byte fields are **big-endian** (network byte order).

---

## Header (128 bytes)

The `inspect_header` MCP tool parses all these fields:

```
Offset  Size  Field                     Spec Section
------  ----  ------------------------  ------------
0       4     Profile size (uint32)     7.2.2
4       4     CMM type signature        7.2.3
8       4     Version (BCD encoded)     7.2.4
12      4     Device class signature    7.2.5
16      4     Color space signature     7.2.6
20      4     PCS (XYZ or Lab)          7.2.7
24      12    Date/time (6 x uint16)    7.2.8
36      4     Magic 'acsp' (0x61637370) 7.2.9
40      4     Primary platform          7.2.10
44      4     Profile flags             7.2.11
48      4     Device manufacturer       7.2.12
52      4     Device model              7.2.13
56      8     Device attributes         7.2.14
64      4     Rendering intent          7.2.15
68      12    PCS illuminant (XYZ)      7.2.16
80      4     Profile creator           7.2.17
84      16    Profile ID (MD5)          7.2.18
100     28    Reserved (must be 0x00)   7.2.19
```

### Key Constants

| Field | Value | Hex |
|-------|-------|-----|
| Magic number | `'acsp'` | `0x61637370` |
| PCS illuminant (D50) | 0.9642, 1.0000, 0.8249 | s15Fixed16 |
| v4.4 version | 4.4.0.0 | `0x04400000` |
| v5 version | 5.1.0.0 | `0x05100000` |

### Device Class Signatures

| Signature | Hex | Class | Profile Purpose |
|-----------|-----|-------|-----------------|
| `scnr` | `0x73636E72` | Input | Scanner, camera |
| `mntr` | `0x6D6E7472` | Display | Monitor, projector |
| `prtr` | `0x70727472` | Output | Printer |
| `link` | `0x6C696E6B` | DeviceLink | Fixed transform chain |
| `spac` | `0x73706163` | ColorSpace | Abstract color space |
| `abst` | `0x61627374` | Abstract | Abstract transform |
| `nmcl` | `0x6E6D636C` | NamedColor | Named color collection |

### Color Space Signatures

The `enum_spaces` MCP tool returns all 32 identifiers. Common ones:

| Signature | Hex | Channels | Color Model |
|-----------|-----|----------|-------------|
| `RGB ` | `0x52474220` | 3 | RGB |
| `CMYK` | `0x434D594B` | 4 | CMYK |
| `Lab ` | `0x4C616220` | 3 | CIE L*a*b* |
| `XYZ ` | `0x58595A20` | 3 | CIE XYZ |
| `GRAY` | `0x47524159` | 1 | Grayscale |

### Rendering Intent Values

| Value | Intent | Description |
|-------|--------|-------------|
| 0 | Perceptual | Preserves visual relationships |
| 1 | Relative Colorimetric | Maps white point, clips gamut |
| 2 | Saturation | Maximizes saturation |
| 3 | Absolute Colorimetric | Exact colorimetric match |

---

## Tag Table

Immediately follows the header at offset 128:

```
Offset  Size  Field
------  ----  -----
128     4     Tag count (uint32)
132     12    Tag entry 0: signature(4) + offset(4) + size(4)
144     12    Tag entry 1: ...
...
```

Each tag entry points to data elsewhere in the file. Tags may share
data by pointing to the same offset (tag aliasing).

### Common Tag Signatures

| Signature | Purpose | Tool |
|-----------|---------|------|
| `desc` | Profile description | `dump_profile` |
| `rXYZ` | Red colorant XYZ | `inspect_header` |
| `gXYZ` | Green colorant XYZ | `inspect_header` |
| `bXYZ` | Blue colorant XYZ | `inspect_header` |
| `rTRC` | Red tone reproduction curve | `round_trip_test` |
| `gTRC` | Green TRC | `round_trip_test` |
| `bTRC` | Blue TRC | `round_trip_test` |
| `A2B0` | AToB perceptual transform | `round_trip_test` |
| `B2A0` | BToA perceptual transform | `round_trip_test` |
| `wtpt` | Media white point | `inspect_header` |
| `chad` | Chromatic adaptation | `inspect_header` |
| `targ` | Characterization target | `dump_profile` |
| `ncl2` | Named color 2 | `apply_named_cmm` |

---

## Version Encoding (BCD)

Version is 4 bytes in Binary Coded Decimal:

```
Byte 0: Major version (e.g., 0x04 = v4)
Byte 1: Minor.bugfix (e.g., 0x40 = v4.4.0)
Bytes 2-3: Reserved (0x00)
```

| Version | Hex | Notes |
|---------|-----|-------|
| 2.1.0 | `0x02100000` | Legacy ICC v2 |
| 2.4.0 | `0x02400000` | Common ICC v2 |
| 4.3.0 | `0x04300000` | ICC v4.3 |
| 4.4.0 | `0x04400000` | ICC.1-2022-05 (current) |
| 5.0.0 | `0x05000000` | ICC.2-2019 (iccMAX) |
| 5.1.0 | `0x05100000` | ICC.2-2023 (current iccMAX) |

---

## Multi-Process Elements (v5/iccMAX)

v5 profiles use Multi-Process Elements (MPE) for flexible transform
pipelines. The `mpet` tag type contains a sequence of processing elements:

| Element | Signature | Purpose |
|---------|-----------|---------|
| Curves | `cvst` | Parametric/sampled curves |
| Matrix | `matf` | Matrix multiplication |
| CLUT | `clut` | Color lookup table |
| Calculator | `calc` | Programmable operations |
| Spectral | various | Spectral transforms |
| Tone Map | `tmap` | HDR tone mapping |

The `calc` element is particularly complex -- it implements a stack-based
calculator with 100+ opcodes. This is the primary attack surface for
CWE-674 (infinite recursion) and CWE-681 (type confusion) vulnerabilities.

---

## Profile ID (MD5)

The Profile ID at offset 84 is an MD5 hash computed over the entire
profile with these fields zeroed during computation:

- Profile flags (offset 44, 4 bytes)
- Rendering intent (offset 64, 4 bytes)
- Profile ID field itself (offset 84, 16 bytes)

The `inspect_header` tool reports the stored Profile ID and whether it
matches a recomputed value.

---

## Image Container Formats

### TIFF

ICC profiles are embedded as TIFF tag 34675 (`ICCProfile`). The
`tiff_dump` and `apply_profiles` tools work with TIFF images.

Key TIFF fields reported by `tiff_dump`:
- ImageWidth, ImageLength, BitsPerSample, SamplesPerPixel
- PhotometricInterpretation (MinIsBlack=1, RGB=2, Separated=5)
- Compression (None=1, LZW=5, Deflate=8)

### JPEG

ICC profiles are stored in APP2 markers with the `ICC_PROFILE\0` identifier.
Profiles larger than 64KB are split across multiple APP2 segments with
sequence numbering. The `jpeg_dump` tool handles multi-segment reassembly.

### PNG

ICC profiles are stored in `iCCP` chunks as:
`profile_name + null + compression_method(0) + zlib_compressed_icc_data`

The `png_dump` tool handles zlib decompression.

---

## Data Type Encodings

| Type | Size | Description |
|------|------|-------------|
| `uint8` | 1 | Unsigned 8-bit |
| `uint16` | 2 | Unsigned 16-bit BE |
| `uint32` | 4 | Unsigned 32-bit BE |
| `uint64` | 8 | Unsigned 64-bit BE |
| `s15Fixed16` | 4 | Signed 15.16 fixed-point |
| `u16Fixed16` | 4 | Unsigned 16.16 fixed-point |
| `float16` | 2 | IEEE 754 half-precision |
| `float32` | 4 | IEEE 754 single-precision |
| `float64` | 8 | IEEE 754 double-precision |
| `dateTime` | 12 | 6 x uint16 (Y/M/D/h/m/s) |
| `XYZ` | 12 | 3 x s15Fixed16 |

### s15Fixed16 Encoding

Most ICC measurements use s15Fixed16: a 32-bit value where bits 31-16
are the signed integer part and bits 15-0 are the fractional part.

```
Value = raw_int32 / 65536.0
```

Example: D50 illuminant X = 0.9642 is stored as `0x0000F6D6` (63190 / 65536).

---

## Security-Relevant Structures

### Tag Offset/Size Validation

Every tag entry contains an offset and size. Security checks include:

- `offset + size <= profile_size` (bounds check, prevents OOB read)
- `offset >= 128 + 4 + tag_count * 12` (must not overlap header/table)
- No null bytes in path strings
- Tag data must not extend beyond file end

### Calculator Element Stack

The `calc` element's operand stack is the most complex attack surface.
File-controlled opcodes can trigger:

- Stack underflow/overflow (CWE-121, CWE-787)
- Division by zero (CWE-369)
- Infinite recursion via self-referencing sub-elements (CWE-674)
- NaN-to-integer casts (CWE-681)
- Unsigned integer overflow in size calculations (CWE-190)

### LUT Table Dimensions

CLUT dimensions control memory allocation. A 4-channel input with grid
size 256 allocates 256^4 entries -- 4GB with 1-byte precision. Attackers
manipulate grid dimensions to trigger OOM (CWE-400) or integer overflow
in allocation size calculations (CWE-190).
