# iccDumpProfile

## Overview

`iccDumpProfile` is a command-line tool for inspecting and validating ICC profile files. It provides detailed output of the profile header, tag table, and individual tag contents, with options for verbose output and structural validation against ICC specifications.

This tool is particularly useful for developers, color scientists, and QA engineers who need to ensure ICC profiles conform to standards and contain expected metadata.

---

## Features

- Displays comprehensive ICC header and tag table information
- Validates tag layout, structure, and alignment (compliance with ICC.1/ICC.2)
- Detects overlapping or redundant tag entries
- Offers detailed output for each tag, including localized descriptions
- Supports verbose tag content dumps
- Flags non-compliance issues and prints warnings or critical errors

---

## Output Details

### Profile Header
- Profile ID, creation date, version, and profile class
- Color spaces, rendering intent, platform, and device info
- PCS and spectral data ranges

### Tag Table
- Tag type, offset, size, and padding
- Warning for duplicate and misaligned tag entries

### Validation Report (if `-v` is used)
- Status levels: OK, Warning, NonCompliant, CriticalError
- Reports on misaligned tags, overlapping data, non-4-byte padded files

---

## Build Instructions

### Clean previous build artifacts
```sh
cd Build/
rm -rf CMakeCache.txt CMakeFiles/ Makefile IccProfLib/ IccXML/ Testing/ Tools/
```

### Configure build with debugging and AddressSanitizer
```sh
cd Build/
cmake -DCMAKE_INSTALL_PREFIX=$HOME/.local       -DCMAKE_BUILD_TYPE=Debug       -DCMAKE_CXX_FLAGS="-g -fsanitize=address,undefined -fno-omit-frame-pointer -Wall"       -Wno-dev -DENABLE_TOOLS=YES Cmake/
```

### Compile the tool
```sh
make -j32
```

---

## Usage Examples

### Dump header and tag info of a profile
```sh
Tools/IccDumpProfile/iccDumpProfile profile.icc
```

### Dump all tags with max verbosity
```sh
Tools/IccDumpProfile/iccDumpProfile -v 100 profile.icc ALL
```

### Validate and dump a specific tag
```sh
Tools/IccDumpProfile/iccDumpProfile -v 50 profile.icc desc
```

---

## Validation Options

- `-v` enables validation mode
- Optional verbosity level (1-100); default is 100
- Tag name can be `ALL` or a specific tag (e.g., `desc`, `wtpt`)

---

## Changelog

- Initial implementation by Max Derhak (2003)
- Validation improvements by Peter Wyatt (2021)
- Additional diagnostics added by David Hoyt (2025)
