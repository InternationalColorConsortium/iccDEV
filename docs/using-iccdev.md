# Using iccDEV

This guide is for people who want to **use** iccDEV, not build it. Every path
below starts from something prebuilt: a release bundle, a package manager, an
npm package, or the published container. Where no prebuilt option exists yet,
the guide says so and gives the shortest working alternative.

To build from source instead, see [Build](build.md).

## Pick Your Path

| I want to... | Use | Section |
|--------------|-----|---------|
| Run the command-line tools | Release bundle, Homebrew, Linux package, or Docker | <a href="#install-the-tools">Install the tools</a> |
| Learn what the tools do | Any of the above | <a href="#first-steps-with-the-tools">First steps with the tools</a> |
| Browse a profile in a graphical viewer | Release bundle or Homebrew | <a href="#icc-dump-profile-gui">iccDumpProfileGui</a> |
| Call iccDEV from Python | `pip` (compiles on install) or the tools through `subprocess` | <a href="#python">Python</a> |
| Call iccDEV from MATLAB | Windows release bundle (includes a built MEX file) | <a href="#matlab">MATLAB</a> |
| Call iccDEV from Node.js | `npm install iccdev` | <a href="#webassembly-and-nodejs">WebAssembly and Node.js</a> |
| Give an AI assistant ICC tools | Docker image or `pip` plus a tools bundle | <a href="#mcp-server">MCP server</a> |

All examples use `sRGB_v4_ICC_preference.icc`, which ships in the root of every
release bundle and under `Testing/` in the container and the repository.

<a name="install-the-tools"></a>

## Install the Tools

### Release bundle (Windows, Linux, macOS)

Each build of `master` publishes ready-to-run bundles on the
<a href="https://github.com/InternationalColorConsortium/iccDEV/releases/latest">latest release page</a>.
No compiler and no installer are needed: unzip and run.

| Platform | Download |
|----------|----------|
| Windows x64 | <a href="https://github.com/InternationalColorConsortium/iccDEV/releases/latest/download/iccdev-windows-msvc.zip">iccdev-windows-msvc.zip</a> |
| Linux x86_64 | <a href="https://github.com/InternationalColorConsortium/iccDEV/releases/latest/download/iccdev-linux-gcc.zip">iccdev-linux-gcc.zip</a> |
| macOS | <a href="https://github.com/InternationalColorConsortium/iccDEV/releases/latest/download/iccdev-macos-clang.zip">iccdev-macos-clang.zip</a> |

`SHA256SUMS` and `SHA512SUMS` are published beside the archives.

Every bundle unpacks to one `iccDEV-Testing` directory that holds the tools,
their runtime libraries, the sample profiles and data from `Testing/`, a copy
of `docs/`, and a helper script that puts the tools on `PATH`.

Windows Command Prompt:

```
cd iccDEV-Testing
path.bat
iccDumpProfile sRGB_v4_ICC_preference.icc
```

In PowerShell `path.bat` cannot change the calling shell, so either run the
tools as `.\iccDumpProfile.exe` or add the directory yourself:

```powershell
$env:PATH = "$PWD;$env:PATH"
```

Linux and macOS (the script must be sourced, not executed):

```bash
cd iccDEV-Testing
. ./path.sh
iccDumpProfile sRGB_v4_ICC_preference.icc
```

The Linux tools link against the system image, TIFF, PNG, JPEG, and XML
libraries of the Ubuntu release they were built on. If a tool reports a missing
shared library on your distribution, use the container or Homebrew instead.

### Homebrew (macOS and Linux)

```bash
brew install iccdev
iccDumpProfile /path/to/profile.icc
```

Homebrew installs the tagged release and puts the tools on `PATH`. It does not
install the sample profiles; download a release bundle if you want them.

### Debian and RPM packages (Linux x86_64)

The latest release page also carries a small runtime-only package
(`reficcmax-<version>-linux-x86_64-Release.deb`, `.rpm`, and `.zip`) with the
tools in `bin/` and the shared libraries in `lib/`. It contains no sample
profiles.

```bash
sudo apt install ./reficcmax-*-linux-x86_64-Release.deb
```

### Docker (any platform)

```bash
docker pull ghcr.io/internationalcolorconsortium/iccdev:latest
docker run --rm -it ghcr.io/internationalcolorconsortium/iccdev:latest
```

The tools are on `PATH` and the working directory is the repository checkout,
so the sample profiles are at `Testing/`. To work on your own files, mount a
directory:

```bash
docker run --rm -v "$PWD:/data" \
  ghcr.io/internationalcolorconsortium/iccdev:latest \
  iccDumpProfile /data/profile.icc
```

This is the project's unified image. It also carries the MCP server and the
maintainer toolchain, and its tools are sanitizer-instrumented builds, so it is
large and slower than a release bundle. Prefer a bundle for throughput work.

<a name="first-steps-with-the-tools"></a>

## First Steps with the Tools

Run any tool with no arguments to print its usage. The full index is the
[CLI tool reference](tools-cli-reference.md).

Look at a profile, then validate it:

```bash
iccDumpProfile sRGB_v4_ICC_preference.icc
iccDumpProfile -v sRGB_v4_ICC_preference.icc
```

The `-v` form ends with a `Validation Report` section such as
`Profile is valid for version 4.20`.

Convert a profile to editable text and back. JSON and XML are equivalent
routes; see the [IccJSON guide](iccjson.md) for the document structure.

```bash
iccToJson   sRGB_v4_ICC_preference.icc srgb.json
iccFromJson srgb.json srgb-from-json.icc

iccToXml    sRGB_v4_ICC_preference.icc srgb.xml
iccFromXml  srgb.xml srgb-from-xml.icc
```

Apply a profile to colour values in a text file. The arguments are the data
file, the output encoding (`0` = Lab/XYZ value), the interpolation
(`1` = tetrahedral), then one or more `profile intent` pairs
(`1` = relative colorimetric):

```bash
iccApplyNamedCmm ApplyDataFiles/rgbFloat.txt 0 1 sRGB_v4_ICC_preference.icc 1
```

The first lines of output are the Lab values with the source RGB after the
semicolon:

```
   55.2991   78.3915   61.3745	;    1.0000    0.0000    0.0000
   68.2954   44.6994   69.7283	;    1.0000    0.5000    0.0000
```

Apply a profile chain to a TIFF image. The arguments after the two file names
are output encoding (`0` = same as source), compression, planar layout, embed
the last profile (`1`), and interpolation:

```bash
iccApplyProfiles ApplyDataFiles/seed-tiff-none-rgb-8x8.tif out.tif 0 0 0 1 1 \
  sRGB_v4_ICC_preference.icc 1 sRGB_v4_ICC_preference.icc 1
iccTiffDump out.tif
```

Assess a profile:

```bash
iccRoundTrip   sRGB_v4_ICC_preference.icc
iccPawgReport  sRGB_v4_ICC_preference.icc
```

`iccRoundTrip` reports round-trip Delta E statistics. `iccPawgReport` prints the
ICC Profile Assessment Working Group checklist for security, conformance, and
quality; add `--json` for machine-readable output.

<a name="icc-dump-profile-gui"></a>

### Browse a profile in a window: iccDumpProfileGui

`iccDumpProfileGui` is the graphical counterpart of `iccDumpProfile`, and the
easiest way to explore a profile you have not seen before. It is in the
Windows, Linux, and macOS release bundles and in the Homebrew install. It is
not in the Debian/RPM runtime package, the npm package, or (usefully) the
container, which has no display.

Start it from the bundle directory, optionally naming a profile to open:

```bash
iccDumpProfileGui sRGB_v4_ICC_preference.icc
```

On Windows you can also double-click `iccDumpProfileGui.exe` and use
**File > Open Profile** (Ctrl+O). Each profile opens in its own child window,
so several can be compared side by side.

| In the window | What it does |
|---------------|--------------|
| **Profile Header** panel | Shows the same header fields as `iccDumpProfile`: version, class, colour spaces, rendering intent, illuminant, spectral and MCS ranges, profile ID. |
| **Profile Tags** list | One row per tag with its signature, type, offset, size, and padding. A tag that cannot be read is marked `***Invalid Tag!***`. |
| Double-click a tag | Opens a **View Tag** window with the full decoded contents of that tag. Double-clicking an embedded-profile tag opens the embedded profile as a profile window of its own. |
| **Validate Profile** button | Runs the same validation as `iccDumpProfile -v` and shows the status and report. |
| **Round Trip Report** button | Shows the `iccRoundTrip` statistics. It appears only for profile classes that can be round-tripped. |

The viewer is read-only. To change a profile, convert it with `iccToJson` or
`iccToXml`, edit the text, and convert it back.

Where to go next:

| Topic | Reference |
|-------|-----------|
| Every tool, option tables, rendering-intent codes | [CLI tool reference](tools-cli-reference.md) |
| JSON-configured apply (`-cfg`) | [IccConnect library](icc-connect.md) |
| Editing profiles as JSON | [IccJSON guide](iccjson.md), [ICC JSON tag reference](iccjson-tag-types.md) |
| Scripts that build the full sample-profile set | `README.md` in the bundle root |

<a name="python"></a>

## Python

The Python package is named `iccdev`. It wraps IccProfLib in-process, so
profile inspection, validation, and colour transforms need no command-line
tools.

### Install

**There is no prebuilt wheel yet.** `iccdev` is not published on PyPI, so
`pip install iccdev` fails. Until it is, `pip` builds the package from the
repository. The build is automatic and compiles its own copy of IccProfLib, but
it needs a C++17 compiler on the machine (Visual Studio Build Tools on Windows,
Xcode Command Line Tools on macOS, `g++` or `clang++` on Linux) and takes a few
minutes. Python 3.9 or newer is required.

```bash
python -m venv .venv
```

Activate it (`.venv\Scripts\activate` on Windows,
`. .venv/bin/activate` elsewhere), then:

```bash
pip install "iccdev[numpy] @ git+https://github.com/InternationalColorConsortium/iccDEV.git#subdirectory=python"
```

Check the install:

```bash
python -c "import iccdev; print(iccdev.__version__)"
```

### Use it in your own code

```python
import numpy as np
import iccdev

path = "sRGB_v4_ICC_preference.icc"

# Inspect the header
with iccdev.IccProfile(path) as profile:
    hdr = profile.header
    print(hdr.version_string, hdr.device_class_name,
          hdr.color_space_name, hdr.pcs_name)

# Validate in-process
result = iccdev.validate_profile_file(path)
print(result.status.name, result.report)

# Transform device values to the profile connection space
with iccdev.IccCmm() as cmm:
    cmm.attach(path, intent=iccdev.Intent.RelativeColorimetric)
    cmm.begin()
    print(cmm.src_space.name, "->", cmm.dst_space.name)   # RGB -> Lab

    print(cmm.apply([1.0, 0.0, 0.0]))                     # one pixel

    pixels = np.array([[1, 0, 0], [0, 1, 0], [0, 0, 1]], dtype=np.float32)
    print(cmm.apply_ndarray(pixels))                      # N x channels
```

Attach two profiles (for example a camera profile then a printer profile) to
convert device to device. Catch `iccdev.IccProfileError` and
`iccdev.IccCmmError` when the profiles come from an untrusted source.

**Values are ICC-encoded, in the range 0 to 1.** A Lab result is not
`L*a*b*` directly. The red pixel above returns
`[0.5530, 0.8094, 0.7426]`; convert it with:

```python
L = v[0] * 100.0
a = v[1] * 255.0 - 128.0
b = v[2] * 255.0 - 128.0        # -> 55.30, 78.39, 61.37
```

### XML, JSON, dump, and round-trip helpers

`icc_to_xml`, `icc_from_xml`, `icc_to_json`, `icc_from_json`, `dump_profile`,
and `round_trip` run the matching command-line tool. They need the tools from
<a href="#install-the-tools">Install the tools</a> on `PATH`, or `ICCDEV_TOOLS_DIR` set to
the directory that holds them:

```python
import os, iccdev

os.environ["ICCDEV_TOOLS_DIR"] = r"C:\tools\iccDEV-Testing"
print(sorted(iccdev.available_tools()))

json_text = iccdev.icc_to_json("sRGB_v4_ICC_preference.icc")
profile_bytes = iccdev.icc_from_json(json_text)
```

### No compiler available

Install a release bundle and drive the tools with `subprocess`. This needs
nothing but the Python standard library:

```python
import json, subprocess, tempfile, pathlib

def profile_as_json(icc_path):
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / "profile.json"
        subprocess.run(["iccToJson", icc_path, str(out)], check=True,
                       capture_output=True)
        return json.loads(out.read_text(encoding="utf-8"))

profile = profile_as_json("sRGB_v4_ICC_preference.icc")
print(profile["IccProfile"]["Header"])
```

The complete API is in
<a href="https://github.com/InternationalColorConsortium/iccDEV/blob/master/python/README.md">python/README.md</a>.

<a name="matlab"></a>

## MATLAB

### Install (Windows, prebuilt)

The Windows release bundle includes the MATLAB package **and a compiled MEX
gateway**, so no compiler is needed. Download and unzip
`iccdev-windows-msvc.zip` as described in
<a href="#install-the-tools">Install the tools</a>, then in MATLAB:

```matlab
bundle = 'C:\tools\iccDEV-Testing';          % where you unzipped it
addpath(fullfile(bundle, 'matlab'));
setenv('PATH', [bundle pathsep getenv('PATH')]);
```

`addpath` makes the `iccdev` package visible. The `setenv` line lets the
functions that call command-line tools (`iccdev.to_json`, `iccdev.from_json`,
`iccdev.plot`, `iccdev.qa.audit_pawg_q1`) find them. Add both lines to
`startup.m` to make them permanent.

The MEX file is built with the MATLAB release named on the release page
(R2026a at the time of writing). If your MATLAB refuses to load
`icc_mex.mexw64`, or you are on Linux, macOS, or GNU Octave, no prebuilt MEX is
available and the gateway has to be built once; follow
[MATLAB bindings and QA](matlab-bindings.md).

### Use it in your own code

```matlab
profile_path = fullfile(bundle, 'sRGB_v4_ICC_preference.icc');

% Inspect the header
p = iccdev.IccProfile(profile_path);
hdr = p.header();
fprintf('%s %s\n', hdr.versionString, ...
  iccdev.sig_to_str(uint32(hdr.colorSpace)));
p.close();

% Transform device values to the profile connection space
cmm = iccdev.IccCmm();
cmm.attach(profile_path, 'intent', iccdev.RenderingIntent.RelativeColorimetric);
cmm.begin();
lab_encoded = cmm.apply([1 0 0]);              % one pixel, 1-by-3
many = cmm.apply(rand(1000, 3, 'single'));     % N-by-channels, one row each
cmm.close();

% Edit a profile as JSON, plot it, compare colours
json_text = iccdev.to_json(profile_path);
profile_bytes = iccdev.from_json(json_text);   % uint8 column vector
plots = iccdev.plot(profile_path);
delta_e = iccdev.qa.delta_e_2000([50 2.6772 -79.7751], [50 0 -82.7485]);
```

As in Python, transform results are ICC-encoded in the range 0 to 1; a Lab
result converts with `L = v(1)*100`, `a = v(2)*255 - 128`,
`b = v(3)*255 - 128`.

Always `close()` profile, CMM, and apply handles. Calling a constructor or
function without its arguments prints a working example. Runnable scripts are
in `matlab/examples/` inside the bundle, and the full API is in
<a href="https://github.com/InternationalColorConsortium/iccDEV/blob/master/matlab/README.md">matlab/README.md</a>.

<a name="webassembly-and-nodejs"></a>

## WebAssembly and Node.js

### Install

```bash
npm install iccdev
```

The package is prebuilt; no Emscripten toolchain is needed. It requires
Node.js 16 or newer and ships the sample profiles under
`node_modules/iccdev/Testing/`.

### How the modules work

The package is **the command-line tools compiled to WebAssembly**, not a
JavaScript object API. It exports 17 factories, one per tool (`IccDumpProfile`,
`IccToJson`, `IccFromJson`, `IccToXml`, `IccFromXml`, `IccApplyNamedCmm`,
`IccApplyProfiles`, `IccRoundTrip`, `IccPawgReport`, and so on). Each call:

1. creates a module instance,
2. writes the input files into that instance's in-memory file system,
3. runs the tool with `callMain([...arguments])`, and
4. reads the output files back out.

Arguments are exactly those of the native tool, so the
[CLI tool reference](tools-cli-reference.md) applies unchanged.

### Use it in your own code

```js
const fs = require('fs');
const { IccToJson, IccDumpProfile } = require('iccdev');

async function profileToJson(iccPath) {
  const mod = await IccToJson({
    noInitialRun: true,      // do not run main() on load
    noExitRuntime: true,     // keep the file system alive after main()
    print: () => {},         // discard the tool's stdout
    printErr: () => {},
  });
  mod.FS.writeFile('in.icc', new Uint8Array(fs.readFileSync(iccPath)));
  mod.callMain(['in.icc', 'out.json']);
  return JSON.parse(mod.FS.readFile('out.json', { encoding: 'utf8' }));
}

async function dumpProfile(iccPath) {
  const lines = [];
  const mod = await IccDumpProfile({
    noInitialRun: true,
    noExitRuntime: true,
    print: (line) => lines.push(line),   // capture stdout
    printErr: (line) => lines.push(line),
  });
  mod.FS.writeFile('in.icc', new Uint8Array(fs.readFileSync(iccPath)));
  mod.callMain(['in.icc']);
  return lines.join('\n');
}

(async () => {
  const sample = require.resolve('iccdev/Testing/sRGB_v4_ICC_preference.icc');
  const profile = await profileToJson(sample);
  console.log(Object.keys(profile.IccProfile));
  console.log(await dumpProfile(sample));
})();
```

Practical notes:

- Create a fresh module instance for each run. An instance keeps the state of
  the tool's previous `main()` call.
- Files exist only inside the instance. Nothing is read from or written to disk
  unless your code does it with Node's `fs`.
- `node node_modules/iccdev/test_all.js` exercises every module and is a good
  source of further calling patterns.
- The package targets Node.js with CommonJS `require`. Browser use is not
  packaged or tested.

<a name="mcp-server"></a>

## MCP Server

The iccDEV MCP server lets an AI assistant (Claude, GitHub Copilot, Cursor, and
other Model Context Protocol clients) inspect, validate, convert, and apply ICC
profiles.

### Option A: Docker (nothing else to install)

The published container already holds the server and every tool it calls.
Add this to the client's MCP configuration (for Claude Desktop,
`claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "iccdev": {
      "command": "docker",
      "args": ["run", "--rm", "-i",
               "-v", "/path/to/your/profiles:/profiles:ro",
               "-e", "ICCDEV_PROFILE_DIRS=/profiles",
               "ghcr.io/internationalcolorconsortium/iccdev:latest",
               "iccdev-mcp-entrypoint", "mcp"]
    }
  }
}
```

Keep `-i` and do not add `-t`. The `-v` and `-e` lines expose a directory of
your own profiles to the server read-only; drop them to use only the bundled
sample profiles. The server sees container paths, so refer to your files as
`/profiles/name.icc`.

For Claude Code:

```bash
claude mcp add iccdev -- docker run --rm -i ghcr.io/internationalcolorconsortium/iccdev:latest iccdev-mcp-entrypoint mcp
```

### Option B: pip plus a tools bundle (no Docker)

`iccdev-mcp` is pure Python (3.10 or newer) and installs without a compiler,
but it is **not on PyPI yet**, so install it from the repository. Give it its
own virtual environment: it ships a module that is also named `iccdev`, and it
must not share an environment with the Python bindings above.

```bash
python -m venv mcp-venv
mcp-venv/bin/pip install "git+https://github.com/InternationalColorConsortium/iccDEV.git#subdirectory=iccdev-mcp"
```

(On Windows the commands are under `mcp-venv\Scripts\`.) Then point the server
at an unzipped release bundle:

```json
{
  "mcpServers": {
    "iccdev": {
      "command": "C:\\path\\to\\mcp-venv\\Scripts\\iccdev-mcp.exe",
      "env": {
        "ICCDEV_TOOLS_DIR": "C:\\tools\\iccDEV-Testing",
        "ICCDEV_TESTING_DIR": "C:\\tools\\iccDEV-Testing",
        "ICCDEV_VALIDATION_LIBRARY": "C:\\tools\\iccDEV-Testing\\IccProfLib2.dll"
      }
    }
  }
}
```

`ICCDEV_TOOLS_DIR` enables the tool-backed operations, `ICCDEV_TESTING_DIR`
is where `list_available_profiles` looks, and `ICCDEV_VALIDATION_LIBRARY`
enables `validate_profile` (use `libIccProfLib2.so` or `.dylib` from the Linux
or macOS bundle).

### Check that it works

Ask the assistant to "run the iccdev health check". The `health_check` tool
reports which tools are available and which are missing in that environment,
which is the fastest way to find a wrong path. Then try "summarize
`sRGB_v4_ICC_preference.icc`" or "validate this profile and explain the
report".

### What to ask for

| Goal | Tools the assistant will use |
|------|------------------------------|
| Describe or classify a profile | `profile_summary`, `inspect_header`, `dump_profile` |
| Check conformance and quality | `validate_profile`, `pawg_report`, `round_trip_test` |
| Read or edit a profile as text | `profile_to_json`, `json_to_profile`, `profile_to_xml`, `xml_to_profile` |
| Convert colours or images | `apply_named_cmm`, `apply_profiles`, `create_link` |
| Inspect an image's embedded profile | `tiff_dump`, `jpeg_dump`, `png_dump` |

**For real colour conversion use `apply_named_cmm` or `apply_profiles`.** The
`color_transform` and `roundtrip_delta` tools currently run a placeholder that
passes values through unchanged; they do not apply the profiles.

The server can also run as a local REST service with a browser dashboard
(`iccdev-mcp-entrypoint rest` in the container). Transports, the full tool
list, and environment variables are in
<a href="https://github.com/InternationalColorConsortium/iccDEV/blob/master/iccdev-mcp/README.md">iccdev-mcp/README.md</a>.

## Current Limitations

These are the places where a prebuilt, install-and-go path does not exist yet.

| Area | Limitation | Workaround |
|------|------------|------------|
| Python | `iccdev` is not on PyPI; no prebuilt wheels (<a href="https://github.com/InternationalColorConsortium/iccDEV/issues/2799">#2799</a>) | `pip` install from the repository (compiles), or use the tools through `subprocess` |
| MCP | `iccdev-mcp` is not on PyPI (<a href="https://github.com/InternationalColorConsortium/iccDEV/issues/2800">#2800</a>) | `pip` install from the repository, or use Docker |
| MCP | `color_transform` and `roundtrip_delta` do not apply profiles (<a href="https://github.com/InternationalColorConsortium/iccDEV/issues/2797">#2797</a>) | Use `apply_named_cmm`, `apply_profiles`, `round_trip_test` |
| MCP and Python | Both packages install a module named `iccdev` (<a href="https://github.com/InternationalColorConsortium/iccDEV/issues/2798">#2798</a>) | Use separate virtual environments |
| MATLAB | Prebuilt MEX is Windows-only and tied to one MATLAB release (<a href="https://github.com/InternationalColorConsortium/iccDEV/issues/2801">#2801</a>) | Build the MEX gateway once on other platforms |
| WebAssembly | Node.js only; tool-style interface, no browser package | Wrap `callMain` as shown above |
| Docker | One large image with sanitizer-instrumented tools | Use a release bundle for speed |
| Releases | Binaries are attached to the rolling latest build, not to version tags | Record the commit from the release notes if you need to pin a build |
