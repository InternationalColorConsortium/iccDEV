# ProfileVisualize PDF Reliability Developer Handoff

## Scope

`ci-qa-profilevisualize-pdf-reliability` is a standalone branch rooted at
`master`. It must not be added to the `ci-qa-iccconnect-threading` and
`ci-qa-perf-analysis` stack.

The change covers the legacy `Tools/CmdLine/IccProfileVisualize` PDF writer:

- `MiniPDF.hpp` owns `PDFObject` instances with `std::unique_ptr`.
- `AddNewObject()` uses `new (std::nothrow)` and retains ownership if vector
  growth fails.
- `CloseFile()` returns success only after PDF assembly and
  `icWriteAndClose()` finish successfully.
- `outputDataToPDF()` propagates finalization failure to the CLI.
- `iccProfileVisualize` returns a soft nonzero status when a PDF cannot be
  finalized.

The sibling `iccProfileVisualizePlot` implementation is deliberately out of
scope because it uses a separate, diverged writer.

## Regression Artifact

`.github/scripts/iccdev-profile-visualize-tests.sh` is the durable artifact.
It verifies ordinary LUT TIFF/PDF output, malformed-profile handling, and a
blocked `<basename>_luts.pdf` directory. The blocked-output case must emit a
PDF writing error, preserve the directory, return a nonzero non-signal status,
and remain sanitizer-clean.

## Developer Validation

```bash
cmake -S Build/Cmake -B build-profilevisualize \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON \
  -DENABLE_ASAN=ON -DENABLE_UBSAN=ON \
  -DENABLE_INTEGER_SANITIZER=ON -DENABLE_FLOAT_SANITIZER=ON
cmake --build build-profilevisualize --target iccProfileVisualize --parallel 32
ICCDEV_TOOLS_DIR="$PWD/build-profilevisualize/Tools" \
ICCDEV_TESTING_DIR="$PWD/Testing" \
.github/scripts/iccdev-profile-visualize-tests.sh
```

Run the `iccdev/factory-bare-new` CodeQL query against a current C++ database
after modifying allocation or error propagation. Do not suppress a result
without proving ownership and the caller's failure path.
