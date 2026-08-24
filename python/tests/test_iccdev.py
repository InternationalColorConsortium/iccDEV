"""
test_iccdev.py - Unit tests for iccdev Python bindings.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.

These tests verify the Python API layer. Many tests require actual ICC
profile files and a built IccProfLib2 library to work end-to-end.
Tests that require profiles are marked with @pytest.mark.skipif.
"""

import os
from pathlib import Path
import pytest


# Try importing the module - skip all tests if not built
try:
    from iccdev import (
        IccProfile, IccCmm,
        IccError, IccProfileError, IccCmmError,
        IccProfileHeader,
        ColorSpace, ProfileClass, RenderingIntent, Intent,
        Interpolation, LutType, CmmStatus,
        ValidationResult, ValidationStatus,
        sig_to_str, open_profile, read_profile, validate_profile, validate_profile_file,
    )
    HAS_ICCDEV = True
except ImportError:
    HAS_ICCDEV = False

pytestmark = pytest.mark.skipif(
    not HAS_ICCDEV,
    reason="iccdev extension not built"
)

# Path to test profiles (relative to repo root)
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TESTING_DIR = os.path.join(REPO_ROOT, "Testing")


def find_test_profile(*paths):
    """Find a test profile file, return path or None."""
    for p in paths:
        full = os.path.join(TESTING_DIR, p)
        if os.path.isfile(full):
            return full
    return None


# ---------------------------------------------------------------------------
# Enum tests (always work, no profiles needed)
# ---------------------------------------------------------------------------

class TestEnums:
    def test_color_space_values(self):
        assert ColorSpace.RGB == 0x52474220
        assert ColorSpace.CMYK == 0x434D594B
        assert ColorSpace.Lab == 0x4C616220
        assert ColorSpace.XYZ == 0x58595A20

    def test_color_space_from_sig(self):
        cs = ColorSpace.from_sig(0x52474220)
        assert cs == ColorSpace.RGB

    def test_rendering_intent_values(self):
        assert RenderingIntent.Perceptual == 0
        assert RenderingIntent.RelativeColorimetric == 1
        assert RenderingIntent.Saturation == 2
        assert RenderingIntent.AbsoluteColorimetric == 3

    def test_intent_alias(self):
        assert Intent is RenderingIntent

    def test_interpolation_values(self):
        assert Interpolation.Linear == 0
        assert Interpolation.Tetrahedral == 1

    def test_cmm_status_values(self):
        assert CmmStatus.Ok == 0
        assert CmmStatus.Bad == -1
        assert CmmStatus.CantOpenProfile == 1

    def test_lut_type_values(self):
        assert LutType.Color == 0
        assert LutType.NamedColor == 1

    def test_profile_class_values(self):
        assert ProfileClass.Input == 0x73636E72
        assert ProfileClass.Display == 0x6D6E7472
        assert ProfileClass.Output == 0x70727472


# ---------------------------------------------------------------------------
# Utility function tests
# ---------------------------------------------------------------------------

class TestUtilities:
    def test_sig_to_str_rgb(self):
        assert sig_to_str(0x52474220) == "RGB"

    def test_sig_to_str_cmyk(self):
        assert sig_to_str(0x434D594B) == "CMYK"

    def test_sig_to_str_lab(self):
        assert sig_to_str(0x4C616220) == "Lab"

    def test_sig_to_str_zero(self):
        result = sig_to_str(0)
        assert isinstance(result, str)

    def test_cli_helpers_import_without_tool_discovery(self):
        import iccdev.cli as cli

        assert callable(cli.find_tool)
        assert callable(cli.icc_to_xml)


class TestValidation:
    def test_status_values(self):
        assert ValidationStatus.OK == 0
        assert ValidationStatus.CRITICAL_ERROR == 3
        assert ValidationStatus.INVALID_ARGUMENT == 4

    def test_rejects_empty_input(self):
        result = validate_profile(b"")
        assert isinstance(result, ValidationResult)
        assert result.status is ValidationStatus.INVALID_ARGUMENT
        assert result.report

    def test_rejects_malformed_input(self):
        result = validate_profile(b"\x00\x01\x02\x03")
        assert result.status is ValidationStatus.CRITICAL_ERROR
        assert result.report

    def test_validates_profile_bytes_and_file(self):
        path = find_test_profile("sRGB_v4_ICC_preference.icc")
        if path is None:
            pytest.skip("No tracked sRGB ICC profile found")

        with open(path, "rb") as profile_file:
            bytes_result = validate_profile(profile_file.read())
        file_result = validate_profile_file(path)

        assert bytes_result.status in (ValidationStatus.OK, ValidationStatus.WARNING)
        assert file_result == bytes_result


# ---------------------------------------------------------------------------
# Exception tests
# ---------------------------------------------------------------------------

class TestExceptions:
    def test_icc_error_hierarchy(self):
        assert issubclass(IccProfileError, IccError)
        assert issubclass(IccCmmError, IccError)

    def test_icc_error_message(self):
        err = IccError("test error", 5)
        assert str(err) == "test error"
        assert err.status == 5
        assert err.status_code == 5  # backward compat alias

    def test_profile_not_found(self):
        with pytest.raises(IccProfileError):
            IccProfile("/nonexistent/path/profile.icc")

    def test_profile_path_rejects_null_byte(self):
        with pytest.raises(ValueError, match="null bytes"):
            IccProfile("bad\x00profile.icc")

    def test_cmm_begin_without_profiles(self):
        """Begin without any profiles may raise or succeed with identity."""
        cmm = IccCmm()
        try:
            cmm.begin()
        except IccCmmError:
            pass  # expected on most implementations
        cmm.close()


# ---------------------------------------------------------------------------
# IccProfile tests (require test profiles)
# ---------------------------------------------------------------------------

class TestIccProfile:
    @pytest.fixture
    def srgb_profile_path(self):
        """Find an sRGB test profile."""
        path = find_test_profile(
            "Display/sRGB_v4_ICC_preference.icc",
            "Display/sRGB2014.icc",
        )
        if path is None:
            # Search for any .icc file
            for root, dirs, files in os.walk(TESTING_DIR):
                for f in files:
                    if f.endswith(".icc"):
                        return os.path.join(root, f)
        if path is None:
            pytest.skip("No test ICC profiles found")
        return path

    def test_open_profile(self, srgb_profile_path):
        profile = IccProfile(srgb_profile_path)
        assert profile.is_valid
        profile.close()
        assert not profile.is_valid

    def test_open_profile_pathlib(self, srgb_profile_path):
        with IccProfile(Path(srgb_profile_path)) as profile:
            assert profile.is_valid

    def test_context_manager(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            assert p.is_valid
        assert not p.is_valid

    def test_bool(self, srgb_profile_path):
        p = IccProfile(srgb_profile_path)
        assert bool(p)
        p.close()
        assert not bool(p)

    def test_header_type(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            header = p.header
            assert isinstance(header, IccProfileHeader)

    def test_header(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            header = p.header
            assert header.size > 0
            assert header.magic == 0x61637370  # 'acsp'

    def test_header_named_tuple(self, srgb_profile_path):
        """IccProfileHeader should be comparable."""
        with IccProfile(srgb_profile_path) as p:
            h1 = p.header
            h2 = p.header
            assert h1 == h2
            assert h1.size == h2.size
            assert h1.version == h2.version

    def test_header_version_string(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            ver = p.header.version_string
            assert isinstance(ver, str)
            parts = ver.split(".")
            assert len(parts) == 3

    def test_header_color_space_name(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            name = p.header.color_space_name
            assert isinstance(name, str)

    def test_color_space_property(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            cs = p.color_space
            assert isinstance(cs, (ColorSpace, int))

    def test_version_string(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            ver = p.version_string
            assert isinstance(ver, str)
            parts = ver.split(".")
            assert len(parts) == 3

    def test_repr(self, srgb_profile_path):
        with IccProfile(srgb_profile_path) as p:
            r = repr(p)
            assert "IccProfile" in r

    def test_read_vs_open(self, srgb_profile_path):
        """Both lazy=True and lazy=False should work."""
        with IccProfile(srgb_profile_path, lazy=True) as p1:
            h1 = p1.header
        with IccProfile(srgb_profile_path, lazy=False) as p2:
            h2 = p2.header
        assert h1.size == h2.size

    def test_open_profile_function(self, srgb_profile_path):
        p = open_profile(srgb_profile_path)
        assert p.is_valid
        p.close()

    def test_read_profile_function(self, srgb_profile_path):
        p = read_profile(srgb_profile_path)
        assert p.is_valid
        p.close()


# ---------------------------------------------------------------------------
# IccCmm tests
# ---------------------------------------------------------------------------

class TestIccCmm:
    def test_create_cmm(self):
        cmm = IccCmm()
        assert not cmm.is_ready
        cmm.close()

    def test_context_manager(self):
        with IccCmm() as cmm:
            assert not cmm.is_ready

    def test_bool(self):
        cmm = IccCmm()
        assert bool(cmm)
        cmm.close()
        assert not bool(cmm)

    def test_repr_before_begin(self):
        cmm = IccCmm()
        assert "not initialized" in repr(cmm)

    def test_apply_without_begin(self):
        cmm = IccCmm()
        with pytest.raises(IccCmmError, match="not yet initialized"):
            cmm.apply([0.5, 0.3, 0.1])

    def test_channels_without_begin(self):
        cmm = IccCmm()
        with pytest.raises(IccCmmError, match="not yet initialized"):
            _ = cmm.src_channels

    def test_attach_nonexistent_profile(self):
        cmm = IccCmm()
        with pytest.raises(IccCmmError):
            cmm.attach("/nonexistent/profile.icc")
