# Python packaging PR, merge, and production release

This guide applies to the `python/` package on the default `master` branch.
It separates PR validation, branch merge criteria, and production release steps.
Do not publish Python artifacts from a pull request branch.

## Published PyPI wheels and GitHub release assets

Both `ci-json-python.yml` and the manual `ci-latest-release.yml` call
`.github/workflows/_build-python-wheels.yml` to build CPython 3.9-3.13 wheels
directly from their checked-out commit. Seven matrix lanes produce 45 wheels
covering Linux x86_64/aarch64 (manylinux and musllinux), macOS
x86_64/arm64/universal2, and Windows AMD64/ARM64. The shared build driver uses
Python 3.13 and pins cibuildwheel 3.4.0, including when building older Python
wheels. Primary Python CI sets `upload-wheels: false`; the release caller sets
`upload-wheels: true`.
No PyPI account, publishing token, or PyPI upload is required for this route.

Wheel jobs compile the portable bundled library without a prebuilt CMake
tree and run installed-package unit tests, including core profile and CMM
operations. Native parity tests remain in `ci-json-python`. The universal2
arm64 slice is tested on its build host; its x86_64 slice is not executed
there, while the separate x86_64 lane tests native Intel wheels.

NumPy is optional. Windows ARM64 CPython 3.9/3.10 test the core without it
because NumPy publishes no compatible wheels for those targets; the existing
NumPy test uses `pytest.importorskip`. Other targets retain NumPy coverage.
The test dependency installer requires binary NumPy wheels to avoid an
unrelated source build inside wheel validation.

For the release caller, each lane uploads an explicit `iccdev-python-<lane>`
artifact with its wheels and cibuildwheel identifier inventory. The release
job downloads all seven artifacts by name and validates version,
ABI/platform coverage, native extension, type stubs, and complete canonical
BSD-3-Clause license contents before copying raw `.whl` assets into the release.
Both producer and release consumer pass `--lane <lane-id>` to the validator,
which independently requires that lane's full five- or ten-target inventory.
Missing, duplicate, or foreign identifiers cannot narrow the expected targets.
Inconsistent wheel metadata/tags, unsafe or duplicate archive members, and
missing, duplicate, or unexpected wheels stop publication. Wheels also require
complete RECORD file coverage, SHA-256, SHA-384, or SHA-512 file hashes,
and matching sizes. The release consumer validates the combined 45-wheel set
again before checksum generation and attestation. ZIP CRC validation does not
replace RECORD integrity checks.
Wheels participate in `SHA256SUMS`, `SHA512SUMS`, and artifact provenance alongside
the native packages. No PR artifacts or mutable build caches are reused.

Wheel jobs retain read-only repository permissions, clear Git credentials and
`GITHUB_TOKEN` before running package code, and do not receive publishing
secrets. Release write and attestation permissions belong to the release job;
`GH_TOKEN` is supplied only to its publish step. Keep the governed Bash and
PowerShell prologues in the reusable native jobs. The release summary
sanitizes dynamic values and reports actual publication and attestation
outcomes separately from locally staged assets.

Dispatch on a work branch builds and tests artifacts without publishing a
release; the existing release job branch allowlist stays in force. After
merge, a maintainer can manually run the workflow on `master` to publish a
new rolling Latest Build. Existing release assets are not modified.

Users on CPython 3.9-3.13 install the published wheel from PyPI:

```bash
python -m pip install iccdev
```

PyPI currently provides binary wheels only. Users without a compatible wheel
must install from the repository as documented in the user guide; pip cannot
fall back to a PyPI source distribution. The
[Latest Build page](https://github.com/InternationalColorConsortium/iccDEV/releases/latest)
also provides exact-tag wheel URLs for reproducible installation. The wheel
contains the core library; CLI-backed helpers need the native tools separately.
Production PyPI publication uses the manual `ci-latest-release.yml` workflow
with explicit `pypi_packages` selection.

## Pull request validation

Python packaging PRs must prove both the extension build and the native iccDEV
tool parity path before merge. The parity path covers ProfileLib behavior plus
dump, XML, JSON, named-CMM config, display-profile round-trip, and PAWG report
CLI smoke checks. From a clean checkout:

```bash
cmake -S Build/Cmake -B Build -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON
cmake --build Build --parallel
python -m pip install -e "./python[dev]"
python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m "not parity"
ICCDEV_BUILD_DIR=$PWD/Build python -m pytest --rootdir . --import-mode=importlib python/tests -v --tb=short -m parity
cd python
python -m build
python -m twine check dist/*.whl dist/*.tar.gz
python -m cibuildwheel --print-build-identifiers --platform linux
python -m cibuildwheel --print-build-identifiers --platform macos
python -m cibuildwheel --print-build-identifiers --platform windows
```

On Windows, run from a Developer Command Prompt, or another shell where
`cl.exe` is already available. Use the built iccDEV tree through
`ICCDEV_BUILD_DIR`; do not rely on a stale extension or library from a previous
checkout. Release Python builds must link against release
`IccProfLib2-static.lib` plus its Release dependency libraries, including zlib
when `ICC_USE_ZLIB` is enabled; build iccDEV Release or point
`ICCDEV_BUILD_DIR` at the Release build root. When that zlib dependency is a
dynamic vcpkg library, the Windows wheel must bundle its matching runtime DLL
(`z.dll`, `zlib1.dll`, or `zlib.dll`) beside `_iccdev.pyd` so installed-package
tests do not rely on PATH state. Debug
CRT libraries are only valid with a debug Python interpreter or explicit
`ICCDEV_ALLOW_DEBUG_PYTHON_LIB=1`. An explicit `ICCDEV_BUILD_DIR` is
authoritative and must not be rescued by another Release library elsewhere in
the checkout.

The PR must also pass the `ci-json-python` workflow. Its native binding and
parity jobs cover Linux and Windows builds for Python 3.12 and 3.13 and macOS
builds for Python 3.9, 3.12, and 3.13. It retains isolated sdist installation
and `twine check`, and calls the same seven-lane installed-wheel matrix as the
release workflow, including Windows ARM64 Python 3.9/3.10. The shared wheel
matrix replaces the former per-platform cibuildwheel import smoke builds:
primary CI passes `run-wheel-smoke: false` to its native build jobs. Other
callers retain their existing bounded 2.x smoke driver through the default
`run-wheel-smoke: true`. Pytest exit code 5 (no tests collected) is a failure
in primary CI. Windows native parity uses CMake's `bin/Release` runtime
directory and requires all eight CLI tools before pytest, preventing a
missing tool path from silently skipping the native comparisons. The test
helper retains legacy `Tools/<target>/<config>` discovery for other builds.
The shared wheel workflow runs the wiring and collection
regressions in every lane. Run them locally from the repository root using
Python 3.13 with cibuildwheel 3.4.0, pytest, PyYAML, and packaging installed so
the wheel inventory test is collected:

```bash
python -m pytest --rootdir . --import-mode=importlib .github/tests/test_python_release_wheels.py .github/tests/test_python_test_collection.py -q
python .github/scripts/validate-python-release-wheels.py <wheel-dir> --identifiers <wheel-dir>/build-identifiers.txt --lane <lane-id>
```

The lane IDs are `linux-x86_64`, `linux-aarch64`, `macos-x86_64`, `macos-arm64`,
`macos-universal2`, `windows-amd64`, and `windows-arm64`. Omitting `--lane`
permits ad-hoc subset checks; release producers and consumers must specify it.
These tests cover workflow and artifact contracts and do not need native
CTest registration. CMake/CTest and CPack retain their native build, test,
and runtime-package responsibilities; setuptools and Cython produce wheels.
Release-wheel testing does not establish native sanitizer coverage. Run
separate instrumented native checks when fixing library behavior.

### Validate downloaded release assets

Pin the exact release tag and source commit before downloading. The published
raw wheel assets do not include the CI `build-identifiers.txt` files. Download
all wheels to one directory and validate the full repository inventory:

```bash
gh release download <release-tag> --repo InternationalColorConsortium/iccDEV --pattern '*.whl' --pattern SHA256SUMS --pattern SHA512SUMS --dir release-assets
python .github/scripts/validate-python-release-wheels.py release-assets --release
```

The `--release` mode requires all 45 targets across seven lanes, including both
Linux libc families. It rejects incomplete downloads. Use the lane mode above
for CI artifacts and subset checks. This validator ignores the separate
`iccdev_mcp-*` distribution in combined release downloads; validate the MCP
wheel and source distribution with the command below.
Verify each downloaded wheel against both
published checksum manifests and run `gh attestation verify <wheel-path> --repo
InternationalColorConsortium/iccDEV` before installing a compatible wheel into
an isolated environment. Checksum manifests may also list native packages that
were not selected for download; distinguish missing downloads from hash failures.
Record metadata inspection separately from execution: a Windows AMD64 host
cannot establish ARM64, macOS, or Linux runtime correctness. CLI-backed helpers
also require the separately downloaded native tool bundle. CMake/CTest and
CPack remain native package producers, not wheel producers.

### MCP wheel and source distribution

The manual rolling-release workflow also builds the pure Python `iccdev-mcp`
package for Python 3.10+. Its `mcp-package` job tests Windows and Linux with a
Python 3.13 driver, bounded Hatchling builds, source server/REST tests, and
isolated installations of both the universal wheel and source distribution.
The package smoke proves parent and child import paths, both console entry
points, real stdio discovery/health/profile calls, REST health, and clear errors
when compiled transform bindings are absent. It runs outside the source package
directory so a source import cannot mask an incomplete installed distribution.

Only the Linux lane uploads the explicitly named `iccdev-mcp-python` artifact;
the release waits for both lanes. Its consumer requires exactly one matching
`iccdev_mcp-*-py3-none-any.whl` and `iccdev_mcp-*.tar.gz`, validates metadata,
namespace, pure-Python wheel contents, console entries, complete license,
RECORD integrity, and safe source archive members, then publishes both raw
files with SHA256/SHA512 checksums and the existing provenance attestation.
The compiled `iccdev` wheels and MCP package retain separate intake validators:

```bash
python .github/scripts/validate-python-release-wheels.py release-assets --release
python .github/scripts/validate-mcp-release-package.py release-assets
```

The MCP wheel does not include native CLI tools, compiled transforms, or the
validation library. Install the matching tool bundle or compiled `iccdev`
binding wheel separately for those capabilities. Namespace fixes for #2798
and fail-closed transform behavior for #2797 must remain intact.
For an isolated client use `uvx iccdev-mcp`. The rolling-release workflow
publishes the exact `iccdev-mcp` version through PyPI trusted publishing and
does not use PyPI credentials or repository secrets.

### Trusted PyPI publication

The `iccdev` and `iccdev-mcp` trusted publishers use owner
`InternationalColorConsortium`, repository `iccDEV`, workflow
`ci-latest-release.yml`, and environment `release`. Configure that GitHub
environment with required maintainer approval. No API token or repository
secret is needed.

Dispatch `ci-latest-release.yml` from `master` and select `iccdev`,
`iccdev-mcp`, or `both` for `pypi_packages`.
Feature-branch dispatches cannot reach the release or publishing steps. The
release job requires all native and Python producers, validates the full
45-wheel `iccdev` inventory and the exact MCP wheel/sdist pair, generates
SHA-256 and SHA-512 manifests, and publishes GitHub attestations before staging
the 45 `iccdev` wheels and two MCP distributions in separate directories. Each
PyPI action uses the release job's `id-token: write` permission and protected
`release` environment; it receives no username, password, API token,
repository secret, or mutable cache.

After upload, the workflow waits for PyPI availability, installs the exact
published versions from the public index, runs the installed `iccdev`
non-parity suite, and exercises MCP parent/child imports, both console entry
points, stdio discovery and profile inspection, REST health, and optional
native-capability behavior. A GitHub release asset URL remains replaceable;
record the exact tag-specific URL, checksums, attestation identity, and source
commit rather than describing the URL as immutable.

Run pytest from the repository root with `--import-mode=importlib`. Installed
wheel validation must also set `ICCDEV_REQUIRE_INSTALLED_PACKAGE=1`; the test
guard fails if `iccdev` imports from the source checkout instead of the installed
package.

## Merge criteria

Merge only after:

1. `ci-json-python` is green for the PR head.
2. The package version is intentional and synchronized between
   `python/pyproject.toml` and `python/iccdev/__init__.py`.
3. User-visible API or install behavior changes are documented in
   `python/README.md`.
4. New Python behavior has tests in `python/tests/`; native-backed behavior has
   parity coverage when it depends on built iccDEV tools or generated profiles.
5. The PR does not include generated `dist/`, `wheelhouse/`, build tree, virtual
   environment, or vendored source artifacts unless the release manager
   explicitly requested them.

Keep the branch linear when stacking release candidates. If a release candidate
is rebuilt, regenerate artifacts from the final merge commit instead of reusing
PR artifacts.

## Production release

Production releases must be built from a clean, signed release commit selected
by maintainers, not from a PR branch or CI scratch artifact.

1. Verify the release commit and version.

   ```bash
   git fetch --tags origin
   git checkout <release-tag-or-commit>
   git verify-commit HEAD || git verify-tag <release-tag>
   python - <<'PY'
   import pathlib
   import re

   pyproject = pathlib.Path("python/pyproject.toml").read_text(encoding="utf-8")
   init = pathlib.Path("python/iccdev/__init__.py").read_text(encoding="utf-8")
   project_version = re.search(r'^version = "([^"]+)"$', pyproject, re.M).group(1)
   init_version = re.search(r'^__version__ = "([^"]+)"$', init, re.M).group(1)
   assert project_version == init_version, (project_version, init_version)
   print(project_version)
   PY
   ```

2. Build release artifacts from a clean source tree.

   ```bash
   cd python
   python -m pip install --upgrade "build>=1.2,<1.4" "twine>=5.0,<7" "cibuildwheel==3.4.0"
   python -m build
   python -m twine check dist/*.whl dist/*.tar.gz
   python -m cibuildwheel --output-dir wheelhouse
   ```

   Run the cibuildwheel driver with Python 3.13, as in the shared CI workflow;
   it can still build the configured Python 3.9-3.13 wheels.
   `python -m build` builds the wheel from the sdist, which vendors `IccProfLib`,
   so the wheel never links a single-arch `Build/` archive. The sdist also lets
   `pip install iccdev` build without a repository checkout. `vendor_iccproflib.py`
   remains available for manual inspection or cleanup, but a release build must
   not rely on a separate pre-vendoring step. Do not commit the generated
   `vendor/IccProfLib/`, `dist/`, or `wheelhouse/` directories as part of the
   release unless maintainers explicitly decide to archive them in git.

3. Smoke-test the exact artifacts that will be published.

   ```bash
   python -m venv /tmp/iccdev-release-smoke
   . /tmp/iccdev-release-smoke/bin/activate
   python -m pip install --no-index --find-links dist --find-links wheelhouse iccdev==<version>
   python - <<'PY'
   import pathlib
   import iccdev
   path = pathlib.Path(iccdev.__file__).resolve()
   print("iccdev", iccdev.__version__, path)
   assert "site-packages" in str(path), path
   PY
   deactivate
   ```

4. Publish with the manual `ci-latest-release.yml` workflow from `master`.
   Select the intended `pypi_packages` value and approve the protected
   `release` environment.
   The workflow uses OIDC trusted publishing; never add a PyPI token to PR
   workflows, forked branches, logs, or reusable workflows that run untrusted
   code.

5. Require the workflow's post-publication job to verify the public install.
   For an additional manual check in a fresh environment:

   ```bash
   python -m venv /tmp/iccdev-pypi-smoke
   . /tmp/iccdev-pypi-smoke/bin/activate
   python -m pip install "iccdev==<version>"
   python -c "import iccdev, pathlib; p=pathlib.Path(iccdev.__file__).resolve(); print(iccdev.__version__, p); assert 'site-packages' in str(p), p"
   deactivate
   ```

6. Update release notes with the Python version, supported wheel tags, source
   distribution status, and the commit or tag used to build the artifacts.
