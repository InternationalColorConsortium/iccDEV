---
applyTo: "iccdev-mcp/**"
---

# MCP Server Review

Review changed MCP lines against the package and runtime contracts below. Report
only reproducible defects with a concrete trigger and effect.

- Keep `iccdev-mcp` (pure Python, Python 3.10+) separate from the compiled
  `iccdev` bindings and their CPython 3.11-3.14 wheel matrix. The MCP wheel and
  sdist contain the `iccdev_mcp` namespace, not native binaries, validation
  libraries, or the `iccdev` extension. Optional native operations must fail
  clearly when their backend is absent.
- Check public MCP and REST changes together: argument and path validation,
  bounded subprocess execution and output, response shape, and the corresponding
  tests in `iccdev-mcp/tests/`. CLI wrappers must use fixed argument vectors,
  not shell commands. Preserve the REST-only route boundary in `/api/tools`.
- Derive availability from `health_check` and CLI discovery for the tested
  runtime. A static advertised MCP inventory or a fixed `total_tools` value does
  not prove that optional native or CLI backends work. For a required backend,
  assert its capability and run a real operation.
- For package changes, test source imports and isolated wheel and sdist
  installs. Validate the distribution with
  `.github/scripts/validate-mcp-release-package.py` and exercise installed
  parent/child imports, both console entry points, stdio calls, and REST health
  with `.github/scripts/iccdev-mcp-package-smoke.py`. An editable install does
  not establish installed-distribution behavior.
- For Docker integration changes, check the actual image capabilities with
  `.github/scripts/iccdev-container-smoke.py`. Keep source, image, and release
  artifact evidence distinct; local success does not establish hosted CI or
  PyPI publication.

Use `.github/skills/mcp-server-qa/SKILL.md` for the focused test and handoff
workflow and `docs/python-packaging-release.md` for release intake.
