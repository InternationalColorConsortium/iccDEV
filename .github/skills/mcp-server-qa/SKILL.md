---
name: mcp-server-qa
description: Validate iccdev-mcp source, native capability, Docker runtime, and release distributions after MCP server changes.
---

# MCP Server QA

Use this workflow for changes to `iccdev-mcp`, its Docker integration, or its
package/release contract. See `iccdev-mcp/README.md` for user setup and
`docs/python-packaging-release.md` for artifact intake.

1. Record the branch SHA and changed MCP surfaces. Keep the pure Python
   `iccdev-mcp` Python 3.10+ contract separate from the compiled `iccdev`
   CPython 3.11-3.14 wheel contract.
2. Install the declared test extras and run the package suite from the
   repository root:

   ```bash
   python -m pip install -e "./iccdev-mcp[dev,rest]"
   python -m pytest iccdev-mcp/tests -q
   python .github/scripts/iccdev-mcp-package-smoke.py --source-tree
   ```

   Record skips. If the change needs native tools, set `ICCDEV_TOOLS_DIR`,
   `ICCDEV_TIFF_FIXTURE`, and `ICCDEV_TESTING_DIR` to a real matching build and
   rerun the relevant tests; a skip is not native coverage.
3. For package or release changes, build the universal wheel and sdist,
   validate both:

   ```bash
   python -m build iccdev-mcp --outdir mcp-dist
   python .github/scripts/validate-mcp-release-package.py mcp-dist
   ```

   Install each distribution in its own virtual environment and run
   `.github/scripts/iccdev-mcp-package-smoke.py` without `--source-tree` from
   outside the source package. The release `mcp-package` job is the reference
   for the Windows/Linux installed-distribution checks.
4. For Docker changes, run `docker build --check -f Dockerfile .` and the real
   `.github/scripts/iccdev-container-smoke.py <image>` after building the
   relevant image. Assert required capabilities discovered in that image and
   exercise a tool call; do not hard-code a health-tool count.
5. Report source, installed wheel, installed sdist, native, and image results
   only for the surfaces actually tested. Include exact commands, SHA,
   pass/skip counts, and remaining platform boundaries. A branch push and
   local checks are a handoff for manually run CI, not hosted success.
