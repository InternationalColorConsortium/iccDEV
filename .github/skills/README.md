# iccDEV Skills

Skills are task-specific workflows for agents working in this repository. Keep
them short, operational, and linked to canonical docs instead of duplicating
long command references.

| Skill | Use when |
|-------|----------|
| `afl-smoke` | Running or updating the manual AFL++ smoke workflow, seeds, and maintainer documentation. |
| `docs-maintenance` | Reducing documentation noise or reorganizing docs. |
| `pre-pr-security-cycle` | Running the maintainer pre-PR secure loop: code, build/test, SAST/CodeQL, sanitizer/DAST-style checks, fix, repeat, handoff. |
| `code-review` | Reviewing pull requests with changed-line evidence and without duplicate or speculative findings. |
| `stacked-pr-fast-lane` | Managing related short-window PR stacks and guarded maintainer fast-lane validation. |
| `sanitizer-repro` | Reproducing ASAN/UBSAN findings or security advisories. |
| `specsep-qa` | Running or diagnosing the repository-owned `iccSpecSepToTiff` QA suites and fixtures. |
| `json-config-regression` | Editing JSON/profile config parsing or tests. |
| `iis-isapi-qa` | Building, deploying, and validating the Windows IIS ISAPI HTTP and browser assessment surface. |
| `maintainer-ci-ctest` | Updating maintainer-owned CI, CTest, CPack, sanitizer, workflow, or release gates. |
| `maintainer-label-system` | Maintaining label taxonomy, path labeler rules, issue triage, PR status labels, and CodeQL label routing. |
| `matlab-bindings-test` | Building and validating the MATLAB MEX gateway, profiles, examples, dependencies, and lifecycle tests. |
| `python-bindings-test` | Building and validating the Python/Cython bindings and package-facing tests. |
| `regression-container-maintainer` | Using the published maintainer container for smoke tests, PR validation, issue reproduction, sanitizer review, and CI handoff. |
| `regression-workflow-governance` | Adding regression gates or updating tool-test workflows. |
| `vcpkg-export-consumer-debug` | Fixing vcpkg, install/export, uninstall, or packaged consumer CI failures. |
| `version-bump` | Updating iccDEV release version references. |
| `wasm-build-test` | Building and validating the staged WASM Node/npm-style module package. |
| `avx2-clut-diagnostics` | Debugging AVX2 CLUT dispatch, collecting trace evidence, and preparing a performance handoff. |

Prompts remain better for one-off drafting. Skills are better for repeatable
multi-step repository workflows.
