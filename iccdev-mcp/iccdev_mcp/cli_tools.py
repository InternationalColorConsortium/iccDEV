# iccdev-mcp -- CLI tool discovery and subprocess wrappers
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
Discover and invoke iccDEV CLI tools via subprocess.

Supports 17 CLI tools:
- iccDumpProfile, iccToXml, iccFromXml, iccToJson, iccFromJson
- iccTiffDump, iccJpegDump, iccPngDump, iccFromCube
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


def run_tiff_dump(path: str) -> dict:
    """Run iccTiffDump on a TIFF image."""
    return _run_tool("iccTiffDump", [path])


def run_jpeg_dump(path: str) -> dict:
    """Run iccJpegDump on a JPEG image."""
    return _run_tool("iccJpegDump", [path])


def run_png_dump(path: str) -> dict:
    """Run iccPngDump on a PNG image."""
    return _run_tool("iccPngDump", [path])


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
    reflectance: str,
    separation: str,
    output: str,
    illuminants: Optional[list[str]] = None,
) -> dict:
    """Run iccSpecSepToTiff for spectral separation."""
    args = [reflectance, separation, output]
    if illuminants:
        args.extend(illuminants)
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
