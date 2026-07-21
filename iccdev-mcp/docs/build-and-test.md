# Build and Test Guide

Building iccDEV from source, running the test suite, and configuring
iccdev-mcp for development.

---

## Prerequisites

### Ubuntu / Debian

```bash
sudo apt install -y clang-18 clang++-18 libclang-rt-18-dev \
  cmake make build-essential \
  libpng-dev libjpeg-dev libtiff-dev \
  libxml2-dev nlohmann-json3-dev libssl-dev
```

### macOS

```bash
brew install libpng nlohmann-json libxml2 libtiff jpeg openssl cmake
```

### Python (for iccdev-mcp)

```bash
python3 -m pip install --upgrade pip
# Python 3.10+ required
```

---

## Build iccDEV

### Standard Build (Release)

```bash
git clone https://github.com/InternationalColorConsortium/iccDEV.git
cd iccDEV/Build
cmake -S Cmake -B . -DCMAKE_BUILD_TYPE=Release -DENABLE_TOOLS=ON
make -j$(nproc)
```

### Debug + ASAN + UBSAN (Recommended for Security Research)

```bash
cd iccDEV/Build
cmake -S Cmake -B . \
  -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON \
  -DCMAKE_C_FLAGS="-g3 -O0 -fno-omit-frame-pointer -fsanitize=address,undefined -fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_CXX_FLAGS="-g3 -O0 -fno-omit-frame-pointer -fsanitize=address,undefined -fprofile-instr-generate -fcoverage-mapping" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined -fprofile-instr-generate" \
  -DCMAKE_SHARED_LINKER_FLAGS="-fsanitize=address,undefined -fprofile-instr-generate" \
  -Wno-dev
make -j$(nproc)
```

### Verify Build

```bash
# Check binaries exist
ls Build/Tools/*/icc*

# Check ASAN instrumentation
nm Build/Tools/IccDumpProfile/iccDumpProfile | grep -c __asan
# Must be > 0 for ASAN builds

# Check shared libraries
ls Build/IccProfLib/libIccProfLib2.so Build/IccXML/libIccXML2.so
```

---

## Environment Setup

### Add Tools to PATH

```bash
cd iccDEV
for d in Build/Tools/*/; do
  [ -d "$d" ] && export PATH="$(realpath "$d"):$PATH"
done
export LD_LIBRARY_PATH="$(pwd)/Build/IccProfLib:$(pwd)/Build/IccXML"
```

### Verify Tools

```bash
which iccDumpProfile iccToXml iccFromXml iccRoundTrip
iccDumpProfile 2>&1 | head -1   # Should print usage
```

---

## Run iccDEV Test Suite

The iccDEV Testing/ directory contains profile creation scripts and
validation tests.

### Full Test Sequence

```bash
cd iccDEV/Testing

# Create all test profiles from XML specifications
sh CreateAllProfiles.sh

# Run validation tests
sh RunTests.sh

# HDR profiles
cd HDR && sh mkprofiles.sh && cd ..

# Hybrid profiles
cd hybrid && sh BuildAndTest.sh && cd ..

# Calculator test (invalid profile rejection)
cd CalcTest && sh checkInvalidProfiles.sh && cd ..

# Multi-channel spectral
cd mcs && sh updateprev.sh && sh updateprevWithBkgd.sh && cd ..
```

### Sanitizer Settings for Testing

```bash
# Suppress leak detection (false positives in test harness)
export ASAN_OPTIONS=detect_leaks=0,halt_on_error=0

# Suppress profraw files when not collecting coverage
export LLVM_PROFILE_FILE=/dev/null
```

### Quick Smoke Test

```bash
# Test a single profile through the pipeline
iccDumpProfile -v Testing/sRGB_D65_MAT.icc ALL
iccToXml Testing/sRGB_D65_MAT.icc /tmp/test.xml
iccFromXml /tmp/test.xml /tmp/test_rt.icc
iccRoundTrip Testing/sRGB_D65_MAT.icc 1
```

---

## Install iccdev-mcp

Use the regression Docker image below when you want MCP tools without building
iccDEV locally. Source installs are for development and require a local
`ICCDEV_TOOLS_DIR` for the 17 CLI-backed tools.

### From Source (Development)

```bash
cd iccdev-mcp
python3 -m venv .venv
source .venv/bin/activate
pip install -e ".[dev,rest]"
```

### Configure iccDEV Tools Path

```bash
export ICCDEV_TOOLS_DIR=/path/to/iccDEV/Build/Tools
export LD_LIBRARY_PATH=/path/to/iccDEV/Build/IccProfLib:/path/to/iccDEV/Build/IccXML
```

### Configure Profile Directories

```bash
# Point to iccDEV Testing/ directory for built-in profiles
export ICCDEV_TESTING_DIR=/path/to/iccDEV/Testing

# Additional profile search directories (colon-separated)
export ICCDEV_PROFILE_DIRS=/path/to/custom/profiles
```

---

## Run iccdev-mcp

### Docker Runtime

The regression Docker image includes the Python MCP package, REST dependencies,
iccDEV CLI tools, runtime libraries, maintainer utilities, and `Testing/`
profiles. Use it when you want the MCP server without a local CMake tool build.

The Docker workflow publishes branch and immutable tags from `master` and
`ci-qa-flags`, and from the protected `ci-qa-pr-docker-testing` branch through a
manual dispatch. It promotes the verified regression digest to `latest` only
after all required image checks succeed. Other feature branches build and smoke
test locally on the runner without pushing or attesting images.

```bash
# MCP stdio mode
docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev-ci-regression:latest \
  iccdev-mcp-entrypoint mcp

# REST API mode
docker run --rm -p 127.0.0.1:8080:8080 \
  ghcr.io/internationalcolorconsortium/iccdev-ci-regression:latest \
  iccdev-mcp-entrypoint rest

# Health check
curl -fsS http://127.0.0.1:8080/api/health
```

Build the image locally from a checkout:

```bash
docker build -t iccdev-ci-regression:mcp-local -f Dockerfile.ci-regression .
docker run --rm -p 127.0.0.1:8080:8080 \
  iccdev-ci-regression:mcp-local iccdev-mcp-entrypoint rest
curl -fsS http://127.0.0.1:8080/api/health
curl -fsS http://127.0.0.1:8080/api/tools
```

The container entrypoint accepts `mcp`/`stdio`, `rest`/`api`, `sse`, and
`streamable-http`/`http` modes.

### MCP Mode (stdio)

```bash
iccdev-mcp
# Or:
python -m iccdev_mcp.server
```

### SSE Transport (for remote access)

```bash
iccdev-mcp --transport sse --port 8080
```

### REST API

```bash
python -m iccdev_mcp.rest_api --port 8080
```

---

## Run Tests

### iccdev-mcp Tests

```bash
cd iccdev-mcp
pytest tests/ -v
```

### CodeQL Security Analysis

```bash
cd iccdev-mcp

# Install CodeQL pack dependencies
gh codeql pack install ../.github/codeql-queries/iccdev-mcp/

# Create database
gh codeql database create /tmp/codeql-db-iccdev-mcp \
  --language=python --source-root=. --overwrite

# Run analysis (standard + custom queries)
gh codeql database analyze /tmp/codeql-db-iccdev-mcp \
  --format=sarif-latest --output=/tmp/codeql-results.sarif --threads=0 \
  codeql/python-queries:codeql-suites/python-security-and-quality.qls \
  ../.github/codeql-queries/iccdev-mcp/iccdev-mcp-security-suite.qls
```

---

## Coverage Instrumentation

iccDEV uses clang source-based coverage (NOT gcov):

```bash
# Generate coverage data
LLVM_PROFILE_FILE=output_%m_%p.profraw iccDumpProfile profile.icc ALL

# Merge profraw files
llvm-profdata-18 merge -sparse *.profraw -o merged.profdata

# Generate report
llvm-cov-18 report Build/Tools/IccDumpProfile/iccDumpProfile \
  -instr-profile=merged.profdata

# Generate HTML report
llvm-cov-18 show Build/Tools/IccDumpProfile/iccDumpProfile \
  -instr-profile=merged.profdata -format=html -output-dir=coverage-html
```

---

## Batch Testing

### Test All Profiles with a Single Tool

```bash
find Testing/ -name "*.icc" | while read f; do
  echo "=== $f ==="
  iccDumpProfile "$f" ALL 2>&1 | tail -1
done
```

### Round-Trip All Profiles

```bash
find Testing/ -name "*.icc" | while read f; do
  echo "=== $f ==="
  iccRoundTrip "$f" 1 2>&1 | tail -3
done
```

### XML Round-Trip Validation

```bash
for f in Testing/*.icc; do
  base=$(basename "$f" .icc)
  iccToXml "$f" "/tmp/${base}.xml" 2>/dev/null
  if [ $? -eq 0 ]; then
    iccFromXml "/tmp/${base}.xml" "/tmp/${base}_rt.icc" 2>/dev/null
    if [ $? -eq 0 ]; then
      echo "[OK] $base"
    else
      echo "[FAIL] $base (FromXml failed)"
    fi
  else
    echo "[FAIL] $base (ToXml failed)"
  fi
done
```

### xmllint Validation

```bash
# Validate generated XML files
find /tmp -name "*.xml" -newer /tmp/test_start | while read f; do
  if xmllint --noout "$f" 2>/dev/null; then
    echo "[OK] $f"
  else
    echo "[FAIL] $f"
  fi
done
```

---

## Troubleshooting

### "Tool not found" errors

Verify `ICCDEV_TOOLS_DIR` points to the correct directory:

```bash
ls "$ICCDEV_TOOLS_DIR"/*/icc*
```

Each tool is in its own subdirectory under `Build/Tools/`.

### ASAN errors on profile loading

Expected for malicious/fuzzed profiles. Set `halt_on_error=0` to
continue past ASAN findings:

```bash
export ASAN_OPTIONS=detect_leaks=0,halt_on_error=0
```

### Missing shared libraries

```bash
export LD_LIBRARY_PATH=/path/to/iccDEV/Build/IccProfLib:/path/to/iccDEV/Build/IccXML
```

### Python import errors

Ensure iccdev-mcp is installed in your active virtual environment:

```bash
pip install -e ".[dev,rest]"
python -c "from iccdev_mcp import server; print('OK')"
```
