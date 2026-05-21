# iccdev - Python bindings for RefIccMAX / iccDEV
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
Python bindings for the RefIccMAX (iccDEV) ICC color profile library.

Provides Pythonic access to ICC profile reading, color management,
and color space transformations via Cython wrappers around IccProfLib.

Quick start::

    import iccdev

    # Read a profile
    profile = iccdev.IccProfile("sRGB.icc")
    header = profile.header
    print(header.version_string)

    # Apply a color transform
    cmm = iccdev.IccCmm()
    cmm.attach("input.icc", intent=iccdev.Intent.Perceptual)
    cmm.attach("output.icc")
    cmm.begin()
    result = cmm.apply([0.5, 0.3, 0.1])
"""

__version__ = "0.1.0"

from iccdev._iccdev import (
    # Classes
    IccProfile,
    IccCmm,
    IccApply,
    IccProfileHeader,
    # Exceptions
    IccError,
    IccProfileError,
    IccCmmError,
    # Enums
    ColorSpace,
    ProfileClass,
    RenderingIntent,
    Interpolation,
    LutType,
    CmmStatus,
    # Convenience functions
    sig_to_str,
    open_profile,
    read_profile,
)
from iccdev.cli import (
    IccToolError,
    available_tools,
    dump_profile,
    find_tool,
    icc_from_json,
    icc_from_xml,
    icc_to_json,
    icc_to_xml,
    round_trip,
)

# Short alias for RenderingIntent
Intent = RenderingIntent

__all__ = [
    # Classes
    "IccProfile",
    "IccCmm",
    "IccApply",
    "IccProfileHeader",
    # Exceptions
    "IccError",
    "IccProfileError",
    "IccCmmError",
    # Enums
    "ColorSpace",
    "ProfileClass",
    "RenderingIntent",
    "Intent",
    "Interpolation",
    "LutType",
    "CmmStatus",
    # Functions
    "sig_to_str",
    "open_profile",
    "read_profile",
    # CLI-backed helpers
    "IccToolError",
    "available_tools",
    "find_tool",
    "icc_to_xml",
    "icc_from_xml",
    "icc_to_json",
    "icc_from_json",
    "dump_profile",
    "round_trip",
]
