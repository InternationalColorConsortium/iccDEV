# iccDEV

## Quickstart

| Method | Command |
|--------|---------|
| **Homebrew** | `brew install iccdev` |
| **NPM** | `npm install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest` |

Prebuilt Windows, Linux, and macOS tool bundles are on the
[latest release page](https://github.com/InternationalColorConsortium/iccDEV/releases/latest).

## Using iccDEV

You do not need to build iccDEV to use it. The
[Using iccDEV guide](docs/using-iccdev.md) is a hands-on walkthrough that starts
from prebuilt downloads:

| I want to... | Start here |
|--------------|------------|
| Install and run the command-line tools | [Install the tools](docs/using-iccdev.md#install-the-tools), then [First steps](docs/using-iccdev.md#first-steps-with-the-tools) |
| Browse a profile in a graphical viewer | [iccDumpProfileGui](docs/using-iccdev.md#icc-dump-profile-gui) |
| Use iccDEV from my Python code | [Python](docs/using-iccdev.md#python) |
| Use iccDEV from MATLAB | [MATLAB](docs/using-iccdev.md#matlab) |
| Use iccDEV from Node.js (WebAssembly) | [WebAssembly and Node.js](docs/using-iccdev.md#webassembly-and-nodejs) |
| Connect an AI assistant through MCP | [MCP server](docs/using-iccdev.md#mcp-server) |

To build from source, see: [Build documentation](docs/build.md)

API docs:
[API reference](Tools/Winnt/IccIisIsapi/api.md) ·
[OpenAPI starter](Tools/Winnt/IccIisIsapi/iis-isapi.openapi.yaml) ·
[C validation API](docs/c-api-validation.md) ·
<a href="python/README.md">Python bindings</a> ·
<a href="matlab/README.md">MATLAB bindings</a> ·
[CLI tool reference](docs/tools-cli-reference.md) ·
[iccApply visual lanes](docs/iccapply/README.md)

## Introduction

The purpose of the International Color Consortium (ICC) is to promote
the use and adoption of open, vendor-neutral, cross-platform color management systems.
The International Color Consortium encourages vendors to support the ICC profile
format and the workflows required to use ICC profiles.

The iccDEV project (formerly known as DemoIccMAX) provides an
open source set of libraries and tools that allow for the interaction, manipulation,
and application of ICC based color management profiles based on the 
[ICC profile specification](http://www.color.org/icc_specs2.xalter) and the 
[iccMAX profile specification](http://www.color.org/iccmax.xalter).

All documentation is in the "docs" directory. If you're just getting started, 
here's how we recommend you read the Introduction for a list of features and 
libraries included in iccDEV.

## Contributing

Contributors are ICC members and other individual contributors who have volunteered to
maintain ICC software, documentation, or other technical artifacts. Our CONTRIBUTING
document explains our contribution processes and procedures, so please review it first:
[CONTRIBUTING](https://github.com/InternationalColorConsortium/iccDEV?tab=contributing-ov-file#contributing-to-international-color-consortium-software). Contributors are asked to sign a [Contributor License Agreement](https://github.com/InternationalColorConsortium/.github/blob/main/docs/CLA.md)

## License

iccDEV is licensed under the BSD 3-Clause “New” or “Revised” License

Membership in the ICC is encouraged when this software is used for commercial purposes.
For more information on The International Color Consortium,
please visit [www.color.org](http://www.color.org).
