# IccToXml

## Overview

`IccToXml` is a command-line utility that converts binary ICC profiles to their XML representations. This is useful for inspection, validation, and editing of profile contents in a human-readable format, or for round-trip transformations in conjunction with `IccFromXml`.

---

## Features

- Converts `.icc` binary profiles into `.xml` representation
- Preserves all tag and header data using ICC XML schema
- Supports profiles using multi-process elements (MPEs)
- Based on ICC ProfLib and ICC LibXML libraries
- Outputs well-formed XML suitable for re-import or validation

---

## Usage

```sh
IccToXml source.icc output.xml
```

---

## Parameters

- `source.icc`: The input ICC profile (any valid ICC.1 or ICC.2 binary file)
- `output.xml`: Destination XML file to save the profile representation

---

## Output

- A full XML representation of the ICC profile including:
  - Header metadata
  - Tag table with all defined tags
  - Multi-process elements and nested structure
- Can be re-imported using `IccFromXml`

---

## Example

```sh
IccToXml DisplayProfile.icc DisplayProfile.xml
```

---

## Changelog

- Original version by Max Derhak
- XML serialization reliability improvements by David Hoyt (2025)
