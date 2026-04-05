## Quickstart installation: 

| Method | Command |
|--------|---------|
| **NPM** | `npm install iccdev` |
| **Homebrew** | `brew install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **NixOS Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev-nixos:latest` |
| **NixOS Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev-nixos:latest` |

## Docker Quick Verification

```bash
# Validate the tools are present
docker run --rm ghcr.io/internationalcolorconsortium/iccdev:latest \
  sh -c "ls /workspace/iccDEV/Build/Tools/*/icc*"

# Round-trip a profile
docker run --rm ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccRoundTrip Testing/sRGB_v4_ICC_preference.icc

# Dump profile header
docker run --rm ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccDumpProfile -v Testing/sRGB_v4_ICC_preference.icc
```

Run each tool without arguments for usage help.

## Build Instructions

Build instructions are available in [Build](build.md).