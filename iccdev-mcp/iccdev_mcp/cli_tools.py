# iccdev-mcp -- CLI tool discovery and subprocess wrappers
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
Discover and invoke iccDEV CLI tools via subprocess.

Supports CLI tools:
- iccDumpProfile, iccToXml, iccFromXml, iccToJson, iccFromJson
- iccTiffDump, iccJpegDump, iccPngDump, iccHeifDump, iccFromCube
- iccApplyProfiles, iccApplyNamedCmm, iccApplyToLink
- iccV5DspObsToV4Dsp, iccSpecSepToTiff, iccApplySearch, iccRoundTrip
- iccPawgReport
"""

from __future__ import annotations

import os
import shutil
# Subprocess is required for fixed iccDEV CLI argv execution.
import subprocess  # nosec B404
import tempfile
from pathlib import Path
from typing import Optional

# Maximum output size (10 MB)
_MAX_OUTPUT = 10 * 1024 * 1024

# Default timeout (seconds)
_DEFAULT_TIMEOUT = 60
_CONFIGS = ("Release", "Debug", "RelWithDebInfo", "MinSizeRel")
_LIB_DIRS = ("IccProfLib", "IccXML", "IccJSON", "IccConnect")

# All known CLI tool binary names
TOOL_BINARIES = {
    "iccDumpProfile": "iccDumpProfile",
    "iccToXml": "iccToXml",
    "iccFromXml": "iccFromXml",
    "iccToJson": "iccToJson",
    "iccFromJson": "iccFromJson",
    "iccTiffDump": "iccTiffDump",
    "iccJpegDump": "iccJpegDump",
    "iccPngDump": "iccPngDump",
    "iccHeifDump": "iccHeifDump",
    "iccFromCube": "iccFromCube",
    "iccApplyProfiles": "iccApplyProfiles",
    "iccApplyNamedCmm": "iccApplyNamedCmm",
    "iccApplyToLink": "iccApplyToLink",
    "iccV5DspObsToV4Dsp": "iccV5DspObsToV4Dsp",
    "iccSpecSepToTiff": "iccSpecSepToTiff",
    "iccApplySearch": "iccApplySearch",
    "iccRoundTrip": "iccRoundTrip",
    "iccPawgReport": "iccPawgReport",
}


def _find_tools_dir() -> Optional[Path]:
    """Find the directory containing iccDEV CLI tool binaries."""
    # 1. Explicit environment variable
    env_dir = os.environ.get("ICCDEV_TOOLS_DIR")
    if env_dir:
        p = Path(env_dir)
        if p.is_dir():
            return p

    # 2. Relative to this package: iccDEV/Build/Tools/
    pkg_dir = Path(__file__).resolve().parent
    build_tools = pkg_dir.parent.parent / "Build" / "Tools"
    if build_tools.is_dir():
        return build_tools

    return None


def _find_tool(name: str) -> Optional[str]:
    """Find a specific CLI tool binary.

    Returns the full path or just the name if found on PATH.
    """
    tools_dir = _find_tools_dir()
    suffix = ".exe" if os.name == "nt" else ""
    exe_names = [name]
    if suffix and not name.endswith(suffix):
        exe_names.append(name + suffix)
    if tools_dir:
        # iccDEV build layout: Build/Tools/ToolName/toolName
        tool_subdir = tools_dir / name.replace("icc", "Icc", 1)
        candidates: list[Path] = []
        for exe_name in exe_names:
            candidates.append(tool_subdir / exe_name)
            candidates.extend(tool_subdir / config / exe_name for config in _CONFIGS)
            candidates.append(tools_dir / exe_name)
        for candidate in candidates:
            if candidate.is_file() and os.access(str(candidate), os.X_OK):
                return str(candidate)

    # 3. System PATH
    for exe_name in exe_names:
        found = shutil.which(exe_name)
        if found:
            return found

    return None


def discover_tools() -> dict:
    """Discover which CLI tools are available.

    Returns:
        Dict with 'available' (list of names), 'missing' (list of names),
        and 'tools_dir' (str or None).
    """
    tools_dir = _find_tools_dir()
    available = []
    missing = []

    for name in TOOL_BINARIES:
        if _find_tool(name):
            available.append(name)
        else:
            missing.append(name)

    return {
        "available": available,
        "missing": missing,
        "tools_dir": str(tools_dir) if tools_dir else None,
    }


def _run_tool(
    name: str,
    args: list[str],
    *,
    timeout: int = _DEFAULT_TIMEOUT,
    input_data: Optional[bytes] = None,
) -> dict:
    """Run a CLI tool and capture output.

    Args:
        name: Tool binary name (e.g., "iccDumpProfile").
        args: Command-line arguments.
        timeout: Timeout in seconds.
        input_data: Optional stdin data.

    Returns:
        Dict with 'stdout', 'stderr', 'returncode', and 'tool'.

    Raises:
        FileNotFoundError: If the tool binary is not found.
        subprocess.TimeoutExpired: If execution exceeds timeout.
    """
    tool_path = _find_tool(name)
    if not tool_path:
        raise FileNotFoundError(
            f"{name} not found. Install iccDEV CLI tools or set "
            f"ICCDEV_TOOLS_DIR to the directory containing them."
        )

    cmd = [tool_path] + args
    env = os.environ.copy()
    # Ensure shared libraries are findable
    tools_dir = _find_tools_dir()
    if tools_dir:
        lib_dirs = []
        for lib_name in _LIB_DIRS:
            lib_dir = tools_dir.parent / lib_name
            if lib_dir.is_dir():
                lib_dirs.append(str(lib_dir))
                lib_dirs.extend(
                    str(lib_dir / config)
                    for config in _CONFIGS
                    if (lib_dir / config).is_dir()
                )
        lib_dirs.extend(
            str(path)
            for path in (
                tools_dir.parent / "vcpkg_installed" / "x64-windows" / "bin",
                tools_dir.parent / "vcpkg_installed" / "x64-windows" / "debug" / "bin",
            )
            if path.is_dir()
        )
        existing_ld = env.get("LD_LIBRARY_PATH", "")
        env["LD_LIBRARY_PATH"] = os.pathsep.join(lib_dirs + [existing_ld])
        existing_path = env.get("PATH", "")
        env["PATH"] = os.pathsep.join(lib_dirs + [existing_path])

    try:
        # cmd is an argv list from a resolved binary path; shell is never enabled.
        result = subprocess.run(
            cmd,
            capture_output=True,
            timeout=timeout,
            input=input_data,
            env=env,
        )  # nosec B603
    except subprocess.TimeoutExpired:
        return {
            "stdout": "",
            "stderr": f"Tool timed out after {timeout}s",
            "returncode": -1,
            "tool": name,
            "timeout": True,
        }

    stdout = result.stdout
    stderr = result.stderr

    # Enforce output size limit
    if len(stdout) > _MAX_OUTPUT:
        stdout = stdout[:_MAX_OUTPUT]
    if len(stderr) > _MAX_OUTPUT:
        stderr = stderr[:_MAX_OUTPUT]

    return {
        "stdout": stdout.decode("utf-8", errors="replace"),
        "stderr": stderr.decode("utf-8", errors="replace"),
        "returncode": result.returncode,
        "tool": name,
    }


# -------------------------------------------------------------------------
# Individual tool wrappers
# -------------------------------------------------------------------------

def run_dump_profile(
    path: str,
    *,
    validate: bool = False,
    verbosity: int = 100,
    tag: str = "ALL",
) -> dict:
    """Run iccDumpProfile on a profile."""
    args: list[str] = []
    if validate:
        args.extend(["-v", str(verbosity)])
    args.extend([path, tag or "ALL"])
    return _run_tool("iccDumpProfile", args)


def run_to_xml(input_path: str, output_path: Optional[str] = None) -> dict:
    """Run iccToXml to convert ICC to XML."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".xml")
        os.close(fd)
        try:
            result = _run_tool("iccToXml", [input_path, output_path])
            if result["returncode"] == 0 and Path(output_path).exists():
                result["xml_content"] = Path(output_path).read_text(
                    errors="replace"
                )
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool("iccToXml", [input_path, output_path])


def run_from_xml(
    input_path: str, output_path: Optional[str] = None
) -> dict:
    """Run iccFromXml to convert XML to ICC."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".icc")
        os.close(fd)
        try:
            result = _run_tool("iccFromXml", [input_path, output_path])
            if result["returncode"] == 0 and Path(output_path).exists():
                import base64
                result["icc_base64"] = base64.b64encode(
                    Path(output_path).read_bytes()
                ).decode("ascii")
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool("iccFromXml", [input_path, output_path])


def run_to_json(input_path: str, output_path: Optional[str] = None) -> dict:
    """Run iccToJson to convert ICC to JSON."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".json")
        os.close(fd)
        try:
            result = _run_tool("iccToJson", [input_path, output_path])
            if result["returncode"] == 0 and Path(output_path).exists():
                result["json_content"] = Path(output_path).read_text(
                    errors="replace"
                )
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool("iccToJson", [input_path, output_path])


def run_from_json(
    input_path: str, output_path: Optional[str] = None
) -> dict:
    """Run iccFromJson to convert JSON to ICC."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".icc")
        os.close(fd)
        try:
            result = _run_tool("iccFromJson", [input_path, output_path])
            if result["returncode"] == 0 and Path(output_path).exists():
                import base64
                result["icc_base64"] = base64.b64encode(
                    Path(output_path).read_bytes()
                ).decode("ascii")
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool("iccFromJson", [input_path, output_path])


def run_tiff_dump(
    path: str, ifd: Optional[int] = None,
    list_directories: bool = False, evidence_json: bool = False,
) -> dict:
    """Inspect a main-chain TIFF IFD or inventory metadata without decoding."""
    if ifd is not None and (type(ifd) is not int or not 0 <= ifd <= 4095):
        raise ValueError("ifd must be an integer in 0..4095")
    if type(list_directories) is not bool or type(evidence_json) is not bool:
        raise ValueError("list_directories and evidence_json must be booleans")
    if list_directories and ifd is not None:
        raise ValueError("list_directories cannot be combined with ifd")
    args = []
    if ifd is not None:
        args.extend(["--ifd", str(ifd)])
    if list_directories:
        args.append("--list-directories")
    if evidence_json:
        args.append("--evidence-json")
    return _run_tool("iccTiffDump", [*args, path])


def run_jpeg_dump(path: str) -> dict:
    """Run iccJpegDump on a JPEG image."""
    return _run_tool("iccJpegDump", [path])


def run_png_dump(path: str) -> dict:
    """Run iccPngDump on a PNG image."""
    return _run_tool("iccPngDump", [path])


def run_heif_dump(path: str) -> dict:
    """Run iccHeifDump on a HEIF, HEIC, or AVIF image."""
    return _run_tool("iccHeifDump", [path])


def run_from_cube(
    input_path: str, output_path: Optional[str] = None
) -> dict:
    """Run iccFromCube to convert .cube LUT to ICC profile."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".icc")
        os.close(fd)
        try:
            result = _run_tool("iccFromCube", [input_path, output_path])
            if result["returncode"] == 0 and Path(output_path).exists():
                import base64
                result["icc_base64"] = base64.b64encode(
                    Path(output_path).read_bytes()
                ).decode("ascii")
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool("iccFromCube", [input_path, output_path])


def run_apply_profiles(
    src_image: str,
    dst_image: str,
    encoding: int,
    compress: int,
    planar: int,
    embed: int,
    interpolation: int,
    profiles: list[str],
    intents: Optional[list[int]] = None,
) -> dict:
    """Run iccApplyProfiles to transform a TIFF image."""
    args = [
        src_image,
        dst_image,
        str(encoding),
        str(compress),
        str(planar),
        str(embed),
        str(interpolation),
    ]
    if intents is None:
        intents = [1] * len(profiles)
    for profile, intent in zip(profiles, intents):
        args.extend([profile, str(intent)])
    return _run_tool("iccApplyProfiles", args)


def run_apply_profiles_args(config_args: list[str]) -> dict:
    """Run iccApplyProfiles with caller-supplied CLI arguments."""
    return _run_tool("iccApplyProfiles", config_args)


def run_apply_named_cmm(config_args: list[str]) -> dict:
    """Run iccApplyNamedCmm with the given arguments."""
    return _run_tool("iccApplyNamedCmm", config_args)


def run_apply_to_link(
    profiles: list[str], output_path: str
) -> dict:
    """Run iccApplyToLink to create a device link profile."""
    args = profiles + [output_path]
    return _run_tool("iccApplyToLink", args)


def run_v5_to_v4(
    display: str, observer: str, output_path: Optional[str] = None
) -> dict:
    """Run iccV5DspObsToV4Dsp to convert v5 display profile to v4."""
    if output_path is None:
        fd, output_path = tempfile.mkstemp(suffix=".icc")
        os.close(fd)
        try:
            result = _run_tool(
                "iccV5DspObsToV4Dsp", [display, observer, output_path]
            )
            if result["returncode"] == 0 and Path(output_path).exists():
                import base64
                result["icc_base64"] = base64.b64encode(
                    Path(output_path).read_bytes()
                ).decode("ascii")
            return result
        finally:
            Path(output_path).unlink(missing_ok=True)
    else:
        return _run_tool(
            "iccV5DspObsToV4Dsp", [display, observer, output_path]
        )


def run_spec_sep_to_tiff(
    input_prefix: str,
    output: str,
    start: int,
    end: int,
    increment: int,
    compress: int = 0,
    planar: int = 0,
    profile: Optional[str] = None,
    tile_width: int = 0,
    tile_height: int = 0,
    bigtiff: bool = False,
) -> dict:
    """Combine numbered single-channel TIFFs using the native CLI contract."""
    from .profiles import resolve_profile_path

    for name, value in (("start", start), ("end", end), ("increment", increment)):
        if type(value) is not int or not -(2**31) <= value < 2**31:
            raise ValueError(f"{name} must be a signed 32-bit integer")
    span = end - start
    if not increment or span * increment < 0 or span % increment:
        raise ValueError("increment must be nonzero and land exactly on end")
    count = span // increment + 1
    if not 1 <= count <= 65535:
        raise ValueError("channel count must be in 1..65535")
    if type(compress) is not int or compress not in (0, 1):
        raise ValueError("compress must be 0 or 1")
    if type(planar) is not int or planar not in (0, 1):
        raise ValueError("planar must be 0 or 1")
    for name, value in (("tile_width", tile_width), ("tile_height", tile_height)):
        if type(value) is not int or not 0 <= value < 2**31 or value % 16:
            raise ValueError(f"{name} must be zero or a positive int32 multiple of 16")
    if bool(tile_width) != bool(tile_height):
        raise ValueError("tile_width and tile_height must both be zero or positive")
    if type(bigtiff) is not bool:
        raise ValueError("bigtiff must be a boolean")
    if not isinstance(input_prefix, str) or not input_prefix:
        raise ValueError("input_prefix must be a nonempty literal filename prefix")
    if profile is not None and not isinstance(profile, str):
        raise ValueError("profile must be a filename string or null")

    # A prefix is not a file. Resolve every numbered input under the existing
    # allowed-directory policy, then retain one literal prefix for native argv.
    first = resolve_profile_path(input_prefix + str(start))
    prefix = str(first)[:-len(str(start))]
    for channel in range(start, end + (1 if increment > 0 else -1), increment):
        resolved = resolve_profile_path(input_prefix + str(channel))
        if resolved != Path(prefix + str(channel)).resolve():
            raise ValueError("All numbered inputs must share one resolved prefix")
    args = []
    if tile_width:
        args.extend(["--tiles", str(tile_width), str(tile_height)])
    if bigtiff:
        args.append("--bigtiff")
    args.extend([output, str(compress), str(planar), prefix,
                 str(start), str(end), str(increment)])
    if profile:
        args.append(str(resolve_profile_path(profile)))
    return _run_tool("iccSpecSepToTiff", args)


def run_apply_search(config_args: list[str]) -> dict:
    """Run iccApplySearch with the given arguments."""
    return _run_tool("iccApplySearch", config_args)


def run_round_trip(path: str, intent: int = 1, use_mpe: int = 0) -> dict:
    """Run iccRoundTrip on a profile."""
    return _run_tool("iccRoundTrip", [path, str(intent), str(use_mpe)])


def run_pawg_report(path: str) -> dict:
    """Run iccPawgReport on a profile."""
    return _run_tool("iccPawgReport", [path])
