# AGENTS.md -- iccDEV

This file is a short navigation aid for automated coding agents and maintainers.
Detailed rules live in `.github/copilot-instructions.md` and
`.github/instructions/*.instructions.md`.

`AGENTS.md` is the canonical agent-instruction file. GitHub Copilot also
recognizes `CLAUDE.md` (Anthropic Claude) and `GEMINI.md` (Google Gemini) at the
repository root; both are thin mirrors that route back here so every agent
shares one source of truth. Update rules here, not in the mirrors.

## Ground Rules

- Check `git --no-pager status --short --branch` before edits.
- Prefer focused changes with exact repros and regression coverage.
- Use 2-space C++ indentation, K&R braces, `m_` members, and return-value errors.
- Exit 1-127 is graceful failure. Exit 128+ is signal termination.
- Use sanitizer builds for bug hunting; see `.github/instructions/build-system.instructions.md`.
- Add the nearest regression test for behavior fixes.

## Navigation

| Need | File |
|------|------|
| Build, test, style, CI | `.github/copilot-instructions.md` |
| AVX2 CLUT diagnostics and optimization handoff | `docs/avx2-clut-diagnostics.md` |
| Pull request preparation and handoff | `docs/pre-pr-security-cycle.md` |
| Pre-PR security skill | `.github/skills/pre-pr-security-cycle/SKILL.md` |
| Pre-PR security prompt | `.github/prompts/pre-pr-security-cycle.prompt.md` |
| Stacked PR and fast-lane workflow | `.github/skills/stacked-pr-fast-lane/SKILL.md` |
| Focused pull request review | `.github/skills/code-review/SKILL.md` |
| Code-review bug hunting | `.github/prompts/code-review-hunting.prompt.md` |
| Regression bisect workflow | `.github/prompts/bisect-regression.prompt.md` |
| Maintainer regression container | `docs/regression-container.md` |
| Regression container prompt | `.github/prompts/regression-container-maintainer.prompt.md` |
| Maintainer CTest selection and CI budget | `.github/skills/maintainer-ci-ctest/SKILL.md` |
| New CLI tool onboarding | `.github/prompts/add-new-tool.prompt.md` |
| Contributor onboarding | `.github/prompts/contributor-onboarding.prompt.md` |
| Apply-path throughput benchmark | `Tools/CmdLine/IccBenchApply/Readme.md` |
| Security repro | `.github/prompts/reproduce-security-issue.prompt.md` |
| iccSpecSepToTiff QA | `.github/skills/specsep-qa/SKILL.md` |
| IIS ISAPI endpoint QA | `.github/skills/iis-isapi-qa/SKILL.md` |
| Issue filing format | `.github/prompts/file-security-issue.prompt.md` |
| Library hardening | `.github/instructions/icc-library-code.instructions.md` |
| Workflow hardening | `.github/instructions/workflow-governance.instructions.md` |
| Maintainer label system | `docs/label-system.md` |
| Label triage prompt | `.github/prompts/maintainer-label-triage.prompt.md` |
| Testing details | `.github/instructions/testing.instructions.md` |
| Python bindings | `.github/instructions/python-bindings.instructions.md` |
| MATLAB MEX bindings | `.github/instructions/matlab-mex.instructions.md` |
| MATLAB build and QA | `docs/matlab-bindings.md` |
| MATLAB binding tests | `.github/skills/matlab-bindings-test/SKILL.md` |
| MATLAB binding debug | `.github/prompts/debug-matlab-bindings.prompt.md` |
| Python binding tests | `.github/skills/python-bindings-test/SKILL.md` |
| WASM build tests | `.github/skills/wasm-build-test/SKILL.md` |
| MCP subprocess debug | `.github/prompts/debug-mcp-subprocess.prompt.md` |
| Python/Cython debug | `.github/prompts/debug-python-bindings.prompt.md` |
| Documentation maintenance | `docs/documentation-maintenance.md` |

## CFL Harness Scope

Keep the six command-line CFL targets as filesystem-backed CLI wrappers. The
in-process targets are the intentional exceptions: consume only public headers,
compile the engine sources as separate translation units, and never include a
CLI implementation or call `processLuts()`.

| target | consumes | covers |
| --- | --- | --- |
| `icc_profilevisualize_fuzzer` | `IccVizModel.hpp` | the data-first model: `Enumerate`, `RenderGraph`, `RenderRaster` |
| `icc_writerserialize_fuzzer` | `MiniPDF.hpp`, `MiniSVG.hpp`, `MiniTIFF.hpp` | the serialization layer the model feeds (#2116) |

Keep them separate rather than folding serialization into the model target, so
a crash stays attributable to one layer.

An in-process target that needs a CLUT raster also needs a corpus that still
contains one. `max_seed_bytes` and libFuzzer's `max_len` both removed the only
CLUT-bearing seed at their old defaults (#2120), which left such a target
reporting runs, coverage and a clean exit while never executing the layer it
exists to cover. Both defaults now admit it, and dropping a seed committed
under `.github/ci/test-data` is a hard error rather than a silent prune. Raise
`max_len` alongside the cap when adding a target that consumes large structured
inputs -- they are independent gates. Validate with

```bash
cfl/build.sh --targets profilevisualize,writerserialize --seconds 30
```

## WASM Scope

WASM now ships as a staged Node/npm-style module package. Do not expect or
restore legacy `wasm/*.html`, `wasm/*.css`, or `wasm/*.js` browser UI assets.
Use `.github/skills/wasm-build-test/SKILL.md` for the current module smoke and
profile parity checks.

## AVX2 CLUT Diagnostics

Use `vs2022-clangcl-x64-avx2-diagnostics` for a Windows source-level AVX2
debugging session. It enables `ICC_AVX2_CLUT_DEBUG`, which traces runtime
dispatch and kernel inputs through `IccSignatureUtils.h`. Do not use the
diagnostic preset for throughput comparisons; use the matching
`vs2022-clangcl-x64-avx2-qa-flags` build for measurements. The focused
regression is `iccdev.clut-eight-output-regression`.
Use `.github/scripts/iccdev-windows-clut-avx2-benchmark.ps1` for interleaved
Release comparisons across 8-16 outputs. Windows AVX2 is a ClangCL path;
native MSVC intentionally retains SSE2 after measured AVX2 regressions.
Runtime AVX2 dispatch is limited to 15 outputs. Require
`output_vector_match=True` for every benchmark row.
