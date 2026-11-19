# IccXML Library

IccXML converts ICC profiles and their tags between the binary profile format
and XML. It supports ICC.1 and ICC.2/iccMAX profiles through the `IccXML2`
library target and the `iccToXml` and `iccFromXml` command-line tools.

## Build and Link

IccXML is enabled by default and requires LibXml2. Build it with the main
iccDEV CMake project:

```sh
cmake -S Build/Cmake -B build -DENABLE_ICCXML=ON
cmake --build build --target IccXML2 iccToXml iccFromXml
```

Installed CMake consumers should link the exported target:

```cmake
find_package(RefIccMAX CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE RefIccMAX::IccXML2)
```

See [Build](build.md) for complete build and installation options.

## API and Tools

- \ref IccProfileXml.h provides `CIccProfileXml` for loading and serializing
  complete profiles.
- \ref IccTagXml.h and \ref IccMpeXml.h provide XML implementations for tags
  and multi-process elements.
- [iccToXml](../IccXML/CmdLine/IccToXml/Readme.md) converts a binary profile to
  XML.
- [iccFromXml](../IccXML/CmdLine/IccFromXml/Readme.md) converts XML to a binary
  profile and can validate the XML with a RELAX NG schema.

For round-trip and schema QA, see
[XML and JSON round-trip QA](xml-json-roundtrip-qa.md) and
[ICC.2 XML specification QA](xml-spec-qa.md).
