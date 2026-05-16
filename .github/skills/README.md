# iccDEV Skills

Skills are task-specific workflows for agents working in this repository. Keep
them short, operational, and linked to canonical docs instead of duplicating
long command references.

| Skill | Use when |
|-------|----------|
| `docs-maintenance` | Reducing documentation noise or reorganizing docs. |
| `pre-pr-security-cycle` | Running the maintainer pre-PR secure loop: code, build/test, SAST/CodeQL, sanitizer/DAST-style checks, fix, repeat, handoff. |
| `sanitizer-repro` | Reproducing ASAN/UBSAN findings or security advisories. |
| `json-config-regression` | Editing JSON/profile config parsing or tests. |
| `maintainer-ci-ctest` | Updating maintainer-owned CI, CTest, CPack, sanitizer, workflow, or release gates. |
| `maintainer-label-system` | Maintaining label taxonomy, path labeler rules, issue triage, PR status labels, and CodeQL label routing. |
| `regression-workflow-governance` | Adding regression gates or updating tool-test workflows. |
| `vcpkg-export-consumer-debug` | Fixing vcpkg, install/export, uninstall, or packaged consumer CI failures. |
| `version-bump` | Updating iccDEV release version references. |

Prompts remain better for one-off drafting. Skills are better for repeatable
multi-step repository workflows.
