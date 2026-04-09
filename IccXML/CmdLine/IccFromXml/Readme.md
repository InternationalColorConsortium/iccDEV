# IccFromXml

## Overview

`IccFromXml` is a command-line tool that converts ICC profile XML representations into binary ICC profiles. It supports validation against RELAX NG schemas and preserves profile metadata including profile IDs. This tool is essential for ICC profile developers working with XML interchange formats.

---

## Features

- Converts `.xml` to binary ICC profile files
- Validates XML using RELAX NG schema (optional)
- Supports `-noid` to skip profile ID writing
- Reports validation warnings and errors
- Auto-locates `SampleIccRELAX.rng` schema if not provided
- Compatible with ICC.1 and ICC.2 profile structures

---

## Usage

```sh
IccFromXml input.xml output.icc {-noid -v[=schema.rng]}
```

### Parameters

- `input.xml`: The ICC profile XML file
- `output.icc`: The output ICC binary profile
- `-noid`: Prevents writing a profile ID (optional)
- `-v[=schema.rng]`: Enables validation using optional RELAX NG schema

---

## Behavior

- If `-v` is used without a schema file, the tool looks for `SampleIccRELAX.rng` in the current directory.
- If validation fails but parsing succeeds, the profile is still saved and marked as invalid.
- Profile ID is handled based on content unless `-noid` is used:
  - `icNeverWriteID`, `icAlwaysWriteID`, or `icVersionBasedID` depending on header state

---

## Output

- Console status messages including:
  - Parsing success or failure
  - Validation report (if applicable)
  - Save status

---

## Example

```sh
IccFromXml DisplayProfile.xml DisplayProfile.icc -v=SampleIccRELAX.rng
```

---

## Changelog

- Initial version by Max Derhak
- Validation enhancements and ID control by David Hoyt (2025)
