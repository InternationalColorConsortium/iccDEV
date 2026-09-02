# iccdev-mcp -- MCP server for ICC color profile tools
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
FastMCP server exposing 26 tools for ICC color profile operations.

Phase 1 (Python-native): inspect_header, profile_summary, validate_profile,
    color_transform, roundtrip_delta, sig_to_str, enum_spaces
Phase 2 (subprocess): 17 CLI tool wrappers (see cli_tools.py)
"""

from __future__ import annotations

import argparse
import math
import os
import shlex
from pathlib import Path
from typing import Optional

from mcp.server.fastmcp import FastMCP
from mcp.server.fastmcp.server import Settings as FastMCPSettings

from iccdev_mcp import __version__, cli_tools
from iccdev_mcp.profiles import list_profiles, resolve_profile_path

# MCP 1.x's generic lifespan annotation needs resolution before pydantic-settings
# reads environment sources on Python 3.14.
FastMCPSettings.model_rebuild()

mcp = FastMCP(
    "iccdev-mcp",
    instructions=(
        "ICC color profile tools from the International Color Consortium's "
        "RefIccMAX (iccDEV) library. Provides profile inspection, color "
        "transforms, and format conversion for ICC.1 and ICC.2 profiles."
    ),
)
# FastMCP does not expose its low-level Server version parameter. Set the
# application version explicitly so MCP initialize does not report the SDK version.
mcp._mcp_server.version = __version__

_NATIVE_TOOL_NAMES = (
    "inspect_header",
    "profile_summary",
    "color_transform",
    "roundtrip_delta",
    "icc_sig_to_str",
    "enum_spaces",
)
_VALIDATION_TOOL_NAMES = ("validate_profile",)
_SERVICE_TOOL_NAMES = ("health_check", "list_available_profiles")


def _check_iccdev():
    """Verify iccdev is importable; raise clear error if not."""
    try:
        import iccdev  # noqa: F401
        return True
    except ImportError:
        return False


def _validate_pixels(pixels: list[list[float]]) -> None:
    """Validate Python-native pixel arrays before passing them to iccdev."""
    if not isinstance(pixels, list):
        raise ValueError("pixels must be a list of lists")
    if len(pixels) > 10000:
        raise ValueError("Too many pixels (max 10000)")
    if len(pixels) == 0:
        raise ValueError("At least one pixel required")
    for i, px in enumerate(pixels):
        if not isinstance(px, list):
            raise ValueError(f"Pixel {i} must be a list")
        if len(px) > 16:
            raise ValueError(f"Pixel {i} has too many channels (max 16)")
        if len(px) == 0:
            raise ValueError(f"Pixel {i} has no channels")
        for j, value in enumerate(px):
            if isinstance(value, bool) or not isinstance(value, (int, float)):
                raise ValueError(f"Pixel {i} channel {j} must be numeric")
            if not math.isfinite(float(value)):
                raise ValueError(f"Pixel {i} channel {j} must be finite")


def _printable_signature(signature: str | int) -> str:
    """Return an ASCII-safe display form for a 4-byte ICC signature."""
    if isinstance(signature, int):
        if signature < 0 or signature > 0xFFFFFFFF:
            raise ValueError("signature must fit in 32 bits")
        signature = signature.to_bytes(4, "big").decode("latin-1")
    return "".join(ch if 0x20 <= ord(ch) <= 0x7E else "." for ch in signature)


# -------------------------------------------------------------------------
# Tool 1: inspect_header
# -------------------------------------------------------------------------

@mcp.tool()
def inspect_header(path: str) -> dict:
    """Inspect an ICC profile's header fields.

    Returns 22 raw header fields plus 6 computed human-readable names
    (color_space_name, pcs_name, device_class_name, version_string,
    rendering_intent_name, platform_name).

    Args:
        path: ICC profile path (absolute, relative, or filename to search).
    """
    import iccdev

    resolved = resolve_profile_path(path)
    profile = iccdev.open_profile(str(resolved))
    try:
        h = profile.header
        result = h._asdict()
        # Convert bytes to hex string for JSON serialization
        if isinstance(result.get("profile_id"), bytes):
            result["profile_id"] = result["profile_id"].hex()
        # Add computed names
        result["color_space_name"] = h.color_space_name
        result["pcs_name"] = h.pcs_name
        result["device_class_name"] = h.device_class_name
        result["version_string"] = h.version_string
        result["rendering_intent_name"] = h.rendering_intent_name
        result["platform_name"] = h.platform_name
        result["file_path"] = str(resolved)
        return result
    finally:
        profile.close()


# -------------------------------------------------------------------------
# Tool 2: profile_summary
# -------------------------------------------------------------------------

@mcp.tool()
def profile_summary(path: str) -> dict:
    """Return concise Python-native ICC profile metadata.

    This is a compact alternative to ``inspect_header`` for clients that need
    classification and routing metadata without the full 22-field header dump.

    Args:
        path: ICC profile path (absolute, relative, or filename to search).
    """
    import iccdev

    resolved = resolve_profile_path(path)
    profile = iccdev.open_profile(str(resolved))
    try:
        h = profile.header
        return {
            "file_path": str(resolved),
            "filename": resolved.name,
            "file_size": resolved.stat().st_size,
            "profile_size": h.size,
            "version": h.version,
            "version_string": h.version_string,
            "is_v5": profile.is_v5,
            "device_class": h.device_class,
            "device_class_name": h.device_class_name,
            "color_space": h.color_space,
            "color_space_display": _printable_signature(h.color_space),
            "color_space_name": h.color_space_name,
            "pcs": h.pcs,
            "pcs_display": _printable_signature(h.pcs),
            "pcs_name": h.pcs_name,
            "rendering_intent": h.rendering_intent,
            "rendering_intent_name": h.rendering_intent_name,
            "platform_name": h.platform_name,
            "platform_display": _printable_signature(h.platform),
            "profile_id": h.profile_id.hex(),
        }
    finally:
        profile.close()


# -------------------------------------------------------------------------
# Tool 3: validate_profile
# -------------------------------------------------------------------------

@mcp.tool()
def validate_profile(path: str) -> dict:
    """Validate an ICC profile in memory through the native C validation API.

    This does not invoke an iccDEV command-line tool. A non-OK status is a
    validation result, not an MCP transport failure.

    Args:
        path: ICC profile path (absolute, relative, or filename to search).

    Returns:
        Dict with the resolved file path, numeric status, status name, and
        native validation report.
    """
    import iccdev

    resolved = resolve_profile_path(path)
    result = iccdev.validate_profile_file(str(resolved))
    return {
        "file_path": str(resolved),
        "status": int(result.status),
        "status_name": result.status.name,
        "report": result.report,
    }


# -------------------------------------------------------------------------
# Tool 4: color_transform
# -------------------------------------------------------------------------

@mcp.tool()
def color_transform(
    src_profile: str,
    dst_profile: str,
    pixels: list[list[float]],
    intent: str = "perceptual",
    interpolation: str = "tetrahedral",
) -> dict:
    """Apply a color transform between two ICC profiles.

    Transforms pixel values from the source profile's color space to the
    destination profile's color space using the ICC Color Management Module.

    Args:
        src_profile: Source ICC profile path.
        dst_profile: Destination ICC profile path.
        pixels: List of pixel values (each pixel is a list of floats,
                e.g. [[0.5, 0.3, 0.1]] for RGB). Maximum 10000 pixels.
        intent: Rendering intent -- "perceptual", "relative",
                "saturation", or "absolute".
        interpolation: Interpolation method -- "linear" or "tetrahedral".

    Returns:
        Dict with 'pixels' (transformed values), 'src_channels',
        'dst_channels', and 'pixel_count'.
    """
    _validate_pixels(pixels)

    import iccdev

    intent_map = {
        "perceptual": iccdev.RenderingIntent.Perceptual,
        "relative": iccdev.RenderingIntent.RelativeColorimetric,
        "saturation": iccdev.RenderingIntent.Saturation,
        "absolute": iccdev.RenderingIntent.AbsoluteColorimetric,
    }
    interp_map = {
        "linear": iccdev.Interpolation.Linear,
        "tetrahedral": iccdev.Interpolation.Tetrahedral,
    }

    ri = intent_map.get(intent.lower())
    if ri is None:
        raise ValueError(
            f"Unknown intent: {intent}. "
            f"Use: {', '.join(intent_map.keys())}"
        )
    interp = interp_map.get(interpolation.lower())
    if interp is None:
        raise ValueError(
            f"Unknown interpolation: {interpolation}. "
            f"Use: {', '.join(interp_map.keys())}"
        )

    src_path = resolve_profile_path(src_profile)
    dst_path = resolve_profile_path(dst_profile)

    cmm = iccdev.IccCmm()
    try:
        cmm.attach(str(src_path), intent=ri, interp=interp)
        cmm.attach(str(dst_path), intent=ri, interp=interp)
        cmm.begin()
        transformed = cmm.apply_multi(pixels)
        return {
            "pixels": transformed,
            "src_channels": cmm.src_channels,
            "dst_channels": cmm.dst_channels,
            "pixel_count": len(pixels),
        }
    finally:
        cmm.close()


# -------------------------------------------------------------------------
# Tool 5: roundtrip_delta
# -------------------------------------------------------------------------

@mcp.tool()
def roundtrip_delta(
    profile: str,
    pixels: list[list[float]],
    intent: str = "perceptual",
) -> dict:
    """Compute round-trip delta-E for an ICC profile.

    Transforms pixels device -> PCS -> device and measures the difference
    between original and round-tripped values (Euclidean distance).

    Args:
        profile: ICC profile path.
        pixels: Input pixel values in the profile's device color space.
        intent: Rendering intent for the transform.

    Returns:
        Dict with per-pixel 'deltas', 'roundtripped' values, and
        statistics (min, max, mean, median).
    """
    _validate_pixels(pixels)

    import iccdev

    intent_map = {
        "perceptual": iccdev.RenderingIntent.Perceptual,
        "relative": iccdev.RenderingIntent.RelativeColorimetric,
        "saturation": iccdev.RenderingIntent.Saturation,
        "absolute": iccdev.RenderingIntent.AbsoluteColorimetric,
    }
    ri = intent_map.get(intent.lower())
    if ri is None:
        raise ValueError(f"Unknown intent: {intent}")

    resolved = resolve_profile_path(profile)
    path_str = str(resolved)

    # Forward transform: device -> PCS
    fwd = iccdev.IccCmm()
    try:
        fwd.attach(path_str, intent=ri)
        fwd.begin()
        pcs_values = fwd.apply_multi(pixels)
    finally:
        fwd.close()

    # Reverse transform: PCS -> device
    rev = iccdev.IccCmm()
    try:
        rev.attach(path_str, intent=ri)
        rev.begin()
        roundtripped = rev.apply_multi(pcs_values)
    finally:
        rev.close()

    # Compute per-pixel Euclidean deltas
    deltas: list[float] = []
    for orig, rt in zip(pixels, roundtripped):
        d = math.sqrt(sum((a - b) ** 2 for a, b in zip(orig, rt)))
        deltas.append(round(d, 8))

    sorted_deltas = sorted(deltas)
    n = len(sorted_deltas)
    if n > 0:
        median = (
            sorted_deltas[n // 2]
            if n % 2 == 1
            else (sorted_deltas[n // 2 - 1] + sorted_deltas[n // 2]) / 2
        )
    else:
        median = 0.0

    return {
        "roundtripped": roundtripped,
        "deltas": deltas,
        "stats": {
            "min": min(deltas) if deltas else 0.0,
            "max": max(deltas) if deltas else 0.0,
            "mean": round(sum(deltas) / len(deltas), 8) if deltas else 0.0,
            "median": round(median, 8),
            "pixel_count": n,
        },
    }


# -------------------------------------------------------------------------
# Tool 6: sig_to_str
# -------------------------------------------------------------------------

@mcp.tool()
def icc_sig_to_str(signature: int) -> dict:
    """Convert a 4-byte ICC signature integer to a human-readable string.

    ICC profiles use 4-byte signatures to identify tags, color spaces,
    and other structures. This converts the integer to its string form.

    Args:
        signature: 4-byte ICC signature as integer (e.g., 1684370275
                   for 'desc').

    Returns:
        Dict with 'signature' (int), 'string' (4-char), and 'hex'.
    """
    import iccdev

    s = iccdev.sig_to_str(signature)
    return {
        "signature": signature,
        "string": s,
        "hex": f"0x{signature:08x}",
    }


# -------------------------------------------------------------------------
# Tool 7: enum_spaces
# -------------------------------------------------------------------------

@mcp.tool()
def enum_spaces() -> dict:
    """List all ICC color space identifiers.

    Returns the 32 ICC color space enum values with their names and
    integer values. Useful for understanding profile color space fields.

    Returns:
        Dict with 'color_spaces' list of {name, value} entries and 'count'.
    """
    import iccdev

    spaces = []
    for member in iccdev.ColorSpace:
        spaces.append({
            "name": member.name,
            "value": int(member),
        })

    return {
        "color_spaces": spaces,
        "count": len(spaces),
    }


# -------------------------------------------------------------------------
# Tool 8: list_available_profiles
# -------------------------------------------------------------------------

@mcp.tool()
def list_available_profiles(directory: Optional[str] = None) -> dict:
    """List available ICC test profiles.

    Searches the iccDEV Testing/ directory tree for .icc/.icm files.

    Args:
        directory: Optional subdirectory name to filter results.

    Returns:
        Dict with 'profiles' list and 'count'.
    """
    profiles = list_profiles(directory)
    return {
        "profiles": profiles,
        "count": len(profiles),
    }


# -------------------------------------------------------------------------
# Server health
# -------------------------------------------------------------------------

@mcp.tool()
def health_check() -> dict:
    """Check server status and available capabilities.

    Reports which backends are available (Python API, CLI tools)
    and how many profiles are accessible.

    Returns:
        Dict with dynamic native, CLI, and service tool inventories.
    """
    python_ok = _check_iccdev()
    validation_ok = False
    if python_ok:
        import iccdev
        validation_ok = getattr(
            iccdev,
            "native_validation_available",
            lambda: hasattr(iccdev, "validate_profile_file"),
        )()
    profiles = list_profiles()

    # Check CLI tools availability
    try:
        from iccdev_mcp.cli_tools import discover_tools
        cli_status = discover_tools()
    except ImportError:
        cli_status = {"available": [], "missing": [], "tools_dir": None}

    native_tools = list(_NATIVE_TOOL_NAMES) if python_ok else []
    if validation_ok:
        native_tools.extend(_VALIDATION_TOOL_NAMES)
    available_cli_tools = cli_status.get("available", [])

    return {
        "status": "ok",
        "version": __version__,
        "python_api": {
            "available": python_ok,
            "tools": len(native_tools),
            "available_tools": native_tools,
        },
        "validation_api": {
            "available": validation_ok,
        },
        "cli_tools": cli_status,
        "service_tools": list(_SERVICE_TOOL_NAMES),
        "total_tools": (
            len(native_tools)
            + len(available_cli_tools)
            + len(_SERVICE_TOOL_NAMES)
        ),
        "profile_count": len(profiles),
    }


# -------------------------------------------------------------------------
# Subprocess tools (Phase 2) -- 17 CLI tool wrappers
# -------------------------------------------------------------------------

@mcp.tool()
def dump_profile(
    path: str,
    validate: bool = False,
    verbosity: int = 100,
    tag: str = "ALL",
) -> dict:
    """Dump full ICC profile description (iccDumpProfile).

    Displays header, tag table, and all tag data for an ICC profile.

    Args:
        path: ICC profile path.
        validate: Include ``-v`` validation output.
        verbosity: Validation verbosity when ``validate`` is true.
        tag: Tag signature to dump, or ``ALL``.
    """
    resolved = resolve_profile_path(path)
    return cli_tools.run_dump_profile(
        str(resolved), validate=validate, verbosity=verbosity, tag=tag
    )


@mcp.tool()
def profile_to_xml(path: str) -> dict:
    """Convert ICC profile to XML (iccToXml).

    Serializes a binary ICC profile to human-readable XML format.

    Args:
        path: ICC profile path.

    Returns:
        Dict with 'xml_content', 'stdout', 'stderr', 'returncode'.
    """
    resolved = resolve_profile_path(path)
    return cli_tools.run_to_xml(str(resolved))


@mcp.tool()
def xml_to_profile(xml_path: str) -> dict:
    """Convert XML to ICC profile (iccFromXml).

    Reconstructs a binary ICC profile from XML specification.

    Args:
        xml_path: Path to ICC XML file.

    Returns:
        Dict with 'icc_base64' (base64-encoded ICC binary),
        'stdout', 'stderr', 'returncode'.
    """
    p = resolve_profile_path(xml_path)
    if not p.is_file():
        raise FileNotFoundError(f"XML file not found: {xml_path}")
    return cli_tools.run_from_xml(str(p))


@mcp.tool()
def profile_to_json(path: str) -> dict:
    """Convert ICC profile to JSON (iccToJson).

    Serializes a binary ICC profile to structured JSON format.

    Args:
        path: ICC profile path.

    Returns:
        Dict with 'json_content', 'stdout', 'stderr', 'returncode'.
    """
    resolved = resolve_profile_path(path)
    return cli_tools.run_to_json(str(resolved))


@mcp.tool()
def json_to_profile(json_path: str) -> dict:
    """Convert JSON to ICC profile (iccFromJson).

    Reconstructs a binary ICC profile from JSON specification.

    Args:
        json_path: Path to ICC JSON file.

    Returns:
        Dict with 'icc_base64' (base64-encoded ICC binary),
        'stdout', 'stderr', 'returncode'.
    """
    p = resolve_profile_path(json_path)
    if not p.is_file():
        raise FileNotFoundError(f"JSON file not found: {json_path}")
    return cli_tools.run_from_json(str(p))


@mcp.tool()
def tiff_dump(path: str) -> dict:
    """Dump TIFF image metadata and embedded ICC profile (iccTiffDump).

    Args:
        path: TIFF image path.
    """
    p = resolve_profile_path(path)
    return cli_tools.run_tiff_dump(str(p))


@mcp.tool()
def jpeg_dump(path: str) -> dict:
    """Dump JPEG image metadata and embedded ICC profile (iccJpegDump).

    Args:
        path: JPEG image path.
    """
    p = resolve_profile_path(path)
    return cli_tools.run_jpeg_dump(str(p))


@mcp.tool()
def png_dump(path: str) -> dict:
    """Dump PNG image metadata and embedded ICC profile (iccPngDump).

    Args:
        path: PNG image path.
    """
    p = resolve_profile_path(path)
    return cli_tools.run_png_dump(str(p))


@mcp.tool()
def from_cube(cube_path: str) -> dict:
    """Convert .cube LUT file to ICC profile (iccFromCube).

    Creates an ICC profile from a .cube 3D LUT file.

    Args:
        cube_path: Path to .cube LUT file.

    Returns:
        Dict with 'icc_base64', 'stdout', 'stderr', 'returncode'.
    """
    p = resolve_profile_path(cube_path)
    return cli_tools.run_from_cube(str(p))


@mcp.tool()
def apply_profiles(
    src_image: str = "",
    profiles: Optional[list[str]] = None,
    intents: Optional[list[int]] = None,
    encoding: int = 1,
    compress: int = 0,
    planar: int = 0,
    embed: int = 1,
    interpolation: int = 0,
    config_args: Optional[list[str]] = None,
) -> dict:
    """Apply ICC profile transform to a TIFF image (iccApplyProfiles).

    Transforms a TIFF image through one or more ICC profiles.

    Args:
        src_image: Source TIFF image path.
        profiles: List of ICC profile paths to apply (in order).
        intents: Per-profile rendering intents. Defaults to relative intent.
        encoding: TIFF encoding mode.
        compress: TIFF compression flag.
        planar: TIFF planar output flag.
        embed: Embed output ICC flag.
        interpolation: Interpolation (0=Linear, 1=Tetrahedral).
        config_args: Raw iccApplyProfiles CLI arguments,
                e.g. ``["-cfg", "x.json"]``.

    Returns:
        Dict with 'output_base64', 'stdout', 'stderr', 'returncode'.
    """
    if config_args:
        if not all(isinstance(arg, str) for arg in config_args):
            raise ValueError("config_args must contain only strings")
        return cli_tools.run_apply_profiles_args(config_args)
    if not src_image:
        raise ValueError("src_image is required when config_args is not provided")
    if not profiles:
        raise ValueError("profiles is required when config_args is not provided")
    if intents is not None and len(intents) != len(profiles):
        raise ValueError("intents length must match profiles length")

    import tempfile
    resolved_src = str(resolve_profile_path(src_image))
    resolved_profiles = [str(resolve_profile_path(p)) for p in profiles]
    fd, tmp_output = tempfile.mkstemp(suffix=".tif")
    os.close(fd)
    try:
        result = cli_tools.run_apply_profiles(
            resolved_src, tmp_output,
            encoding, compress, planar, embed, interpolation,
            resolved_profiles, intents
        )
        if result["returncode"] == 0 and Path(tmp_output).exists():
            import base64
            result["output_base64"] = base64.b64encode(
                Path(tmp_output).read_bytes()
            ).decode("ascii")
        return result
    finally:
        Path(tmp_output).unlink(missing_ok=True)


@mcp.tool()
def apply_named_cmm(config: str) -> dict:
    """Run Named Color CMM transform (iccApplyNamedCmm).

    Applies a named color CMM configuration for color matching.

    Args:
        config: JSON config file path or space-separated arguments.
    """
    args = shlex.split(config)
    return cli_tools.run_apply_named_cmm(args)


@mcp.tool()
def create_link(profiles: list[str]) -> dict:
    """Create a device link profile (iccApplyToLink).

    Combines multiple ICC profiles into a single device link profile.

    Args:
        profiles: List of ICC profile paths to link.

    Returns:
        Dict with 'icc_base64', 'stdout', 'stderr', 'returncode'.
    """
    import tempfile
    resolved = [str(resolve_profile_path(p)) for p in profiles]
    fd, tmp_output = tempfile.mkstemp(suffix=".icc")
    os.close(fd)
    try:
        result = cli_tools.run_apply_to_link(resolved, tmp_output)
        if result["returncode"] == 0 and Path(tmp_output).exists():
            import base64
            result["icc_base64"] = base64.b64encode(
                Path(tmp_output).read_bytes()
            ).decode("ascii")
        return result
    finally:
        Path(tmp_output).unlink(missing_ok=True)


@mcp.tool()
def v5_to_v4(display_profile: str, observer_profile: str) -> dict:
    """Convert v5 display+observer to v4 display profile.

    Uses iccV5DspObsToV4Dsp to combine a v5 display profile and
    observer profile into a traditional v4 display profile.

    Args:
        display_profile: v5 display ICC profile path.
        observer_profile: Observer ICC profile path.

    Returns:
        Dict with 'icc_base64', 'stdout', 'stderr', 'returncode'.
    """
    display = str(resolve_profile_path(display_profile))
    observer = str(resolve_profile_path(observer_profile))
    return cli_tools.run_v5_to_v4(display, observer)


@mcp.tool()
def spec_sep_to_tiff(
    reflectance_tiff: str,
    separation_profile: str,
    illuminant_profiles: Optional[list[str]] = None,
) -> dict:
    """Spectral separation to TIFF (iccSpecSepToTiff).

    Applies spectral separation using reflectance data and separation
    profile to produce a color-separated TIFF.

    Args:
        reflectance_tiff: Input reflectance TIFF path.
        separation_profile: Separation ICC profile path.
        illuminant_profiles: Optional list of illuminant profile paths.

    Returns:
        Dict with 'output_base64', 'stdout', 'stderr', 'returncode'.
    """
    import tempfile
    ref = str(resolve_profile_path(reflectance_tiff))
    sep = str(resolve_profile_path(separation_profile))
    illum = (
        [str(resolve_profile_path(p)) for p in illuminant_profiles]
        if illuminant_profiles
        else None
    )
    fd, tmp_output = tempfile.mkstemp(suffix=".tif")
    os.close(fd)
    try:
        result = cli_tools.run_spec_sep_to_tiff(ref, sep, tmp_output, illum)
        if result["returncode"] == 0 and Path(tmp_output).exists():
            import base64
            result["output_base64"] = base64.b64encode(
                Path(tmp_output).read_bytes()
            ).decode("ascii")
        return result
    finally:
        Path(tmp_output).unlink(missing_ok=True)


@mcp.tool()
def apply_search(config: str) -> dict:
    """Search-based profile application (iccApplySearch).

    Finds optimal profile matches and applies transforms.

    Args:
        config: JSON config file path or space-separated arguments.
    """
    args = shlex.split(config)
    return cli_tools.run_apply_search(args)


@mcp.tool()
def round_trip_test(path: str, intent: int = 1, use_mpe: int = 0) -> dict:
    """Run iccRoundTrip on an ICC profile.

    Tests round-trip transform fidelity by applying AToB
    then BToA transforms and measuring reconstruction error.

    Args:
        path: ICC profile path.
        intent: Rendering intent (0=Perceptual, 1=Relative, 2=Saturation,
                3=Absolute).
        use_mpe: Use MPE path when set to 1; use LUT path when 0.

    Returns:
        Dict with 'stdout', 'stderr', 'returncode'.
    """
    resolved = resolve_profile_path(path)
    return cli_tools.run_round_trip(str(resolved), intent, use_mpe)


@mcp.tool()
def pawg_report(path: str) -> dict:
    """Generate a PAWG profile assessment report (iccPawgReport).

    Runs the command-line PAWG security, conformance, and quality checklist for
    a single ICC profile and returns the tool output.

    Args:
        path: ICC profile path.
    """
    resolved = resolve_profile_path(path)
    return cli_tools.run_pawg_report(str(resolved))


# -------------------------------------------------------------------------
# Entry point
# -------------------------------------------------------------------------

def main():
    """CLI entry point for iccdev-mcp server."""
    parser = argparse.ArgumentParser(
        prog="iccdev-mcp",
        description="MCP server for ICC color profile tools (iccDEV)",
    )
    parser.add_argument(
        "--transport",
        choices=["stdio", "sse", "streamable-http"],
        default="stdio",
        help="MCP transport (default: stdio)",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=8080,
        help="Port for SSE/HTTP transport (default: 8080)",
    )
    args = parser.parse_args()

    # Set port on the server instance for network transports
    if args.transport != "stdio":
        mcp.settings.port = args.port

    mcp.run(transport=args.transport)


if __name__ == "__main__":
    main()
