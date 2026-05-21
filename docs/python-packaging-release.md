# Python packaging PR, merge, and production release

This guide applies to the `python/` package on the `pip-install-iccdev` branch.
It separates PR validation, branch merge criteria, and production release steps.
Do not publish Python artifacts from a pull request branch.

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
python -m build --sdist --wheel
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
dynamic vcpkg library, the Windows wheel must bundle the matching `zlib1.dll`
beside `_iccdev.pyd` so installed-package tests do not rely on PATH state. Debug
CRT libraries are only valid with a debug Python interpreter or explicit
`ICCDEV_ALLOW_DEBUG_PYTHON_LIB=1`. An explicit `ICCDEV_BUILD_DIR` is
authoritative and must not be rescued by another Release library elsewhere in
the checkout.

The PR must also pass the `ci-json-python` workflow. That workflow validates
Linux, macOS, and Windows builds for Python 3.9, 3.12, and 3.13, builds source
and wheel artifacts, runs `twine check`, prints cibuildwheel identifiers for
all target platforms, and runs a native wheel smoke build on each hosted
platform.

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
   python -m pip install --upgrade "build>=1.2,<1.4" "twine>=5.0,<7" "cibuildwheel>=2.23,<3"
   python -m build --sdist --wheel
   python -m twine check dist/*.whl dist/*.tar.gz
   python -m cibuildwheel --output-dir wheelhouse
   ```

   The `sdist` build command vendors `IccProfLib` automatically so `pip install
   iccdev` can build without a repository checkout. `vendor_iccproflib.py`
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

4. Publish only from a maintainer-controlled release environment using the
   repository's approved PyPI publishing method. Prefer PyPI trusted publishing
   or a scoped release token. Never expose a PyPI token to PR workflows, forked
   branches, logs, or reusable workflows that run untrusted code.

5. After PyPI publication, verify the public install in a fresh environment.

   ```bash
   python -m venv /tmp/iccdev-pypi-smoke
   . /tmp/iccdev-pypi-smoke/bin/activate
   python -m pip install "iccdev==<version>"
   python -c "import iccdev, pathlib; p=pathlib.Path(iccdev.__file__).resolve(); print(iccdev.__version__, p); assert 'site-packages' in str(p), p"
   deactivate
   ```

6. Update release notes with the Python version, supported wheel tags, source
   distribution status, and the commit or tag used to build the artifacts.
