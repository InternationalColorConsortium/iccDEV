# iccDEV

## Quickstart

| Method | Command |
|--------|---------|
| **Homebrew** | `brew install iccdev` |
| **NPM** | `npm install iccdev` |
| **Docker Pull** | `docker pull ghcr.io/internationalcolorconsortium/iccdev:latest` |
| **Docker Run** | `docker run -it ghcr.io/internationalcolorconsortium/iccdev:latest` |

To build from source, see: [Build documentation](docs/build.md)

API docs:
[API reference](Tools/Winnt/IccIisIsapi/api.md) -
[OpenAPI starter](Tools/Winnt/IccIisIsapi/iis-isapi.openapi.yaml)

## IccJSON -- ICC Profile JSON Serialization

IccJSON provides native JSON serialization for ICC and iccMAX profiles.
Two command-line tools convert between binary ICC and human-readable JSON:

| Tool | Description |
|------|-------------|
| **iccToJson** | Binary ICC profile to JSON |
| **iccFromJson** | JSON to binary ICC profile |

- [Full guide and JSON format reference](docs/iccjson.md)
- [JSON Schema for ICC profiles](docs/icc-profile.schema.json)
- [Standalone C++ example (hello-iccdev)](examples/hello-iccdev/)
- [Doxygen API docs](Doxyfile) (generate with `doxygen Doxyfile`)

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

iccDEV is licensed under the BSD 3-Clause "New" or "Revised" License

Membership in the ICC is encouraged when this software is used for commercial purposes.
For more information on The International Color Consortium,
please visit [www.color.org](http://www.color.org).
