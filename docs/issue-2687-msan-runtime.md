# Issue 2687 MemorySanitizer Runtime Report

Issue: https://github.com/InternationalColorConsortium/iccDEV/issues/2687

Source CI run:
https://github.com/InternationalColorConsortium/iccDEV/actions/runs/35946281244

## Resolution

Issue 2687 is a false positive against `CIccProfile::GetTag()`. The reported
value came from the link fields of a `std::list<IccTagEntry>` node, not from an
`IccTagEntry` field. `CIccProfile::ReadBasic()` initializes the complete entry
before `push_back()`.

The ClusterFuzzLite memory build compiled iccDEV with MemorySanitizer while its
C++ standard library remained uninstrumented. Writes performed inside that
runtime did not update MSan shadow state, so a later instrumented iterator read
reported initialized list linkage as poisoned. The CI build log stated that it
was building without MSan-instrumented libraries.

No `CIccProfile` source change is warranted. The build boundary is corrected
instead.

## Regression Artifact

`.github/ci/regression/issue-2687-profile-list-node.icc.base64` is the exact
144-byte report input. Its decoded SHA-256 is:

```text
bb9c4ad53f9269920947bac185a634da2971a94b17da6cff117dd7ad45bcfe85
```

The ClusterFuzzLite memory adapter verifies that digest and replays the decoded
profile once through `icc_profilevisualize_fuzzer` and
`icc_writerserialize_fuzzer` before the targets are accepted.

## Runtime Contract

| Producer | Consumer | Build/runtime behavior | Platform/toolchain boundary | CI trigger | Dependency owner | Local evidence |
| --- | --- | --- | --- | --- | --- | --- |
| `iccdev-build-msan-libcxx.sh` | ClusterFuzzLite adapter | Build pinned MSan libc++ and libc++abi without the unused XML dependency | Linux x86-64, OSS-Fuzz Clang 22 | `memory` matrix entry | LLVM at pinned commit | Runtime libraries export MSan references |
| ClusterFuzzLite adapter | Shared CFL builder | Add instrumented headers and explicit final link libraries only for `memory` | OSS-Fuzz builder environment | Manual or `ci-qa-clusterfuzz` push | iccDEV CFL adapter | Configuration contract checks the flag handoff |
| Shared CFL builder | Both in-process fuzzers | Link the explicit runtime after iccDEV and fuzzer objects | Linux ELF loader with `$ORIGIN` | `memory` matrix entry | iccDEV CFL builder | `ldd` resolves bundled libc++ and libc++abi with no libstdc++ |
| #2687 base64 fixture | Both in-process fuzzers | Decode the exact input, verify its SHA-256, and execute one replay | MSan with origin tracking | `memory` matrix entry | iccDEV regression fixtures | Both one-shot replays exit zero with no report |
| ClusterFuzzLite workflow | Memory adapter | Validate a 2-45 minute total manual budget, pass it directly to the runner, and retain a 2-minute total budget for push runs | GitHub Ubuntu 24.04 runner and pinned CFL containers | Manual or `ci-qa-clusterfuzz` push | iccDEV workflow maintainers | The matrix job passes the requested total budget and targeted workflow lint |

XML, JSON, tools, and zlib remain disabled in the official ClusterFuzzLite
lane. The broader repository MSan QA continues to build instrumented libxml2 as
well as libc++ and libc++abi.
