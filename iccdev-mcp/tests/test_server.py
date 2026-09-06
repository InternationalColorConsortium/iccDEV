# iccdev-mcp -- MCP server tests
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Tests for iccdev-mcp server tools."""

from __future__ import annotations

import math
import os
import pytest
import sys
from pathlib import Path

# Add package to path for testing without install
sys.path.insert(0, str(Path(__file__).parent.parent))

from iccdev_mcp.profiles import (
    _find_testing_dir,
    list_files,
    list_profiles,
    resolve_profile_path,
)
from iccdev_mcp.cli_tools import _find_tool, discover_tools, TOOL_BINARIES


# ---------------------------------------------------------------------------
# Profile resolution tests
# ---------------------------------------------------------------------------

class TestValidationLibraryOverrides:
    @pytest.mark.parametrize("value", ["", "/missing/libIccProfLib2.so"])
    def test_invalid_explicit_override_fails_closed(self, monkeypatch, tmp_path, value):
        import iccdev
        library = tmp_path / "IccProfLib" / "libIccProfLib2.so"
        library.parent.mkdir()
        library.write_bytes(b"not a library")
        monkeypatch.setenv("ICCDEV_BUILD_DIR", str(tmp_path))
        monkeypatch.setenv("ICCDEV_VALIDATION_LIBRARY", value)
        with pytest.raises(RuntimeError, match="ICCDEV_VALIDATION_LIBRARY"):
            iccdev._validation_library_path()
        assert not iccdev.native_validation_available()
        with pytest.raises(RuntimeError):
            iccdev.validate_profile(b"invalid profile")

    def test_explicit_library_precedes_build_dir(self, monkeypatch, tmp_path):
        import iccdev
        library = tmp_path / "explicit.so"
        library.write_bytes(b"not a library")
        monkeypatch.setenv("ICCDEV_VALIDATION_LIBRARY", str(library))
        monkeypatch.setenv("ICCDEV_BUILD_DIR", "/missing/build")
        assert iccdev._validation_library_path() == library
        assert not iccdev.native_validation_available()
        with pytest.raises(RuntimeError, match="Unable to load"):
            iccdev.validate_profile(b"profile")

    def test_build_dir_when_explicit_library_unset(self, monkeypatch, tmp_path):
        import iccdev
        library = tmp_path / "IccProfLib" / "libIccProfLib2.so"
        library.parent.mkdir()
        library.write_bytes(b"not a library")
        monkeypatch.delenv("ICCDEV_VALIDATION_LIBRARY", raising=False)
        monkeypatch.setenv("ICCDEV_BUILD_DIR", str(tmp_path))
        assert iccdev._validation_library_path() == library

    def test_optional_without_native_configuration(self, monkeypatch):
        import iccdev
        monkeypatch.delenv("ICCDEV_VALIDATION_LIBRARY", raising=False)
        monkeypatch.delenv("ICCDEV_BUILD_DIR", raising=False)
        assert not iccdev.native_validation_available()
        from iccdev_mcp.server import health_check
        health = health_check()
        assert not health["validation_api"]["available"]
        assert "validate_profile" not in health["python_api"]["available_tools"]


class TestProfileResolution:
    """Tests for profile path resolution."""

    def test_null_byte_rejected(self):
        with pytest.raises(ValueError, match="null bytes"):
            resolve_profile_path("test\x00.icc")

    def test_traversal_rejected(self):
        with pytest.raises(ValueError, match="traversal"):
            resolve_profile_path("../../etc/passwd")

    def test_nonexistent_file_raises(self):
        with pytest.raises(FileNotFoundError):
            resolve_profile_path("nonexistent_profile_12345.icc")

    def test_absolute_path_existing(self, tmp_path):
        profile = tmp_path / "test.icc"
        profile.write_bytes(b"\x00" * 128)
        result = resolve_profile_path(str(profile))
        assert result == profile.resolve()

    def test_absolute_path_missing(self):
        with pytest.raises(FileNotFoundError):
            resolve_profile_path("/tmp/no_such_profile_xyz.icc")

    def test_relative_symlink_outside_allowed_base_rejected(self, monkeypatch, tmp_path):
        outside = Path("/etc/hosts")
        if not outside.exists():
            pytest.skip("/etc/hosts not available")
        link = tmp_path / "outside.icc"
        try:
            link.symlink_to(outside)
        except OSError:
            pytest.skip("symlinks not supported")

        monkeypatch.chdir(tmp_path)

        with pytest.raises(ValueError, match="outside allowed"):
            resolve_profile_path("outside.icc")

    def test_testing_dir_discovery(self):
        """Testing/ directory should be findable from package location."""
        testing = _find_testing_dir()
        # May or may not exist depending on install context
        if testing is not None:
            assert testing.is_dir()
            assert testing.name == "Testing"


class TestListProfiles:
    """Tests for profile listing."""

    def test_list_profiles_returns_list(self):
        result = list_profiles()
        assert isinstance(result, list)

    def test_list_profiles_structure(self):
        result = list_profiles()
        for item in result:
            assert "name" in item
            assert "path" in item
            assert "size" in item

    def test_list_profiles_nonexistent_dir(self):
        result = list_profiles("nonexistent_subdir_xyz")
        assert result == []

    def test_list_profiles_skips_symlink_outside_allowed_base(self, monkeypatch, tmp_path):
        outside = Path("/etc/hosts")
        if not outside.exists():
            pytest.skip("/etc/hosts not available")
        testing = tmp_path / "Testing"
        testing.mkdir()
        link = testing / "outside.icc"
        try:
            link.symlink_to(outside)
        except OSError:
            pytest.skip("symlinks not supported")

        monkeypatch.setenv("ICCDEV_TESTING_DIR", str(testing))

        assert list_profiles() == []

    def test_list_profiles_skips_symlink_inside_cwd_outside_testing(self, monkeypatch, tmp_path):
        testing = tmp_path / "Testing"
        testing.mkdir()
        private = tmp_path / "private"
        private.mkdir()
        secret = private / "secret.icc"
        secret.write_bytes(b"\x00" * 128)
        link = testing / "linked.icc"
        try:
            link.symlink_to(secret)
        except OSError:
            pytest.skip("symlinks not supported")

        monkeypatch.chdir(tmp_path)
        monkeypatch.setenv("ICCDEV_TESTING_DIR", str(testing))

        assert list_profiles() == []
        assert list_files(None, ("*.icc",)) == []


# ---------------------------------------------------------------------------
# CLI tool discovery tests
# ---------------------------------------------------------------------------

class TestCLIToolDiscovery:
    """Tests for CLI tool binary discovery."""

    def test_discover_returns_structure(self):
        result = discover_tools()
        assert "available" in result
        assert "missing" in result
        assert "tools_dir" in result
        assert isinstance(result["available"], list)
        assert isinstance(result["missing"], list)

    def test_all_tools_accounted(self):
        result = discover_tools()
        total = len(result["available"]) + len(result["missing"])
        assert total == len(TOOL_BINARIES)

    def test_pawg_report_is_tracked(self):
        assert "iccPawgReport" in TOOL_BINARIES

    def test_tools_dir_multiconfig_layout_is_discovered(self, monkeypatch, tmp_path):
        """ICCDEV_TOOLS_DIR=Build/Tools should find MSVC Debug/Release binaries."""
        tools_dir = tmp_path / "Build" / "Tools"
        exe_name = "iccPawgReport.exe" if os.name == "nt" else "iccPawgReport"
        tool_path = tools_dir / "IccPawgReport" / "Debug" / exe_name
        tool_path.parent.mkdir(parents=True)
        tool_path.write_text("", encoding="ascii")
        tool_path.chmod(0o755)

        monkeypatch.setenv("ICCDEV_TOOLS_DIR", str(tools_dir))

        assert _find_tool("iccPawgReport") == str(tool_path)


# ---------------------------------------------------------------------------
# Server import tests
# ---------------------------------------------------------------------------

class TestServerImport:
    """Tests for server module import and structure."""

    def test_server_imports(self):
        from iccdev_mcp.server import mcp, main
        assert mcp is not None
        assert callable(main)

    def test_fastmcp_settings_are_resolved(self):
        from iccdev_mcp.server import mcp
        from mcp.server.fastmcp.server import Settings as FastMCPSettings

        assert mcp is not None
        assert FastMCPSettings.__pydantic_complete__

    def test_server_has_tools(self):
        from iccdev_mcp import __version__
        from iccdev_mcp.server import mcp
        # FastMCP should have registered tools
        assert mcp is not None
        options = mcp._mcp_server.create_initialization_options()
        assert options.server_version == __version__

    def test_package_version(self):
        from importlib.metadata import version
        from iccdev_mcp import __version__
        assert version("iccdev-mcp") == __version__


# ---------------------------------------------------------------------------
# Python-native tool tests (require iccdev)
# ---------------------------------------------------------------------------

# Check if iccdev is available
try:
    import iccdev
    HAS_ICCDEV = hasattr(iccdev, "IccProfile")
except ImportError:
    HAS_ICCDEV = False

# Find a test profile
_test_profile = None
if HAS_ICCDEV:
    testing = _find_testing_dir()
    if testing:
        candidates = list(testing.rglob("*.icc"))
        if candidates:
            _test_profile = str(candidates[0])


@pytest.mark.skipif(not HAS_ICCDEV, reason="iccdev not installed")
@pytest.mark.skipif(not _test_profile, reason="No test profiles found")
class TestNativeTools:
    """Tests for Python-native MCP tools (require iccdev package)."""

    def test_inspect_header(self):
        from iccdev_mcp.server import inspect_header
        result = inspect_header(_test_profile)
        assert isinstance(result, dict)
        assert "version_string" in result
        assert "color_space_name" in result
        assert "device_class_name" in result
        assert "file_path" in result

    def test_inspect_header_fields(self):
        from iccdev_mcp.server import inspect_header
        result = inspect_header(_test_profile)
        # 22 raw header fields + 6 computed + file_path
        assert "size" in result
        assert "version" in result
        assert "color_space" in result
        assert "pcs" in result
        assert "rendering_intent" in result

    def test_profile_summary(self):
        from iccdev_mcp.server import profile_summary
        result = profile_summary(_test_profile)
        assert isinstance(result, dict)
        assert "version_string" in result
        assert "color_space_name" in result
        assert "color_space_display" in result
        assert "device_class_name" in result
        assert "pcs_display" in result
        assert "platform_display" in result
        assert "profile_id" in result
        assert result["file_size"] > 0
        assert all(0x20 <= ord(ch) <= 0x7E for ch in result["color_space_display"])

    def test_validate_profile(self):
        import iccdev
        from iccdev_mcp.server import validate_profile
        if not iccdev.native_validation_available():
            pytest.skip("Native C validation ABI is not available")
        result = validate_profile(_test_profile)
        assert result["status_name"] in {"OK", "WARNING"}
        assert result["status"] in {0, 1}
        assert isinstance(result["report"], str)

    def test_printable_signature_accepts_int_and_str(self):
        from iccdev_mcp.server import _printable_signature
        assert _printable_signature(0x52474220) == "RGB "
        assert _printable_signature(0x6E630011) == "nc.."
        assert _printable_signature("\x00\x00\x00\x00") == "...."

    def test_enum_spaces(self):
        from iccdev_mcp.server import enum_spaces
        result = enum_spaces()
        assert "color_spaces" in result
        assert "count" in result
        assert result["count"] > 0
        # Check structure of each entry
        for space in result["color_spaces"]:
            assert "name" in space
            assert "value" in space

    def test_sig_to_str(self):
        from iccdev_mcp.server import icc_sig_to_str
        # 'desc' signature = 0x64657363
        result = icc_sig_to_str(0x64657363)
        assert result["string"] == "desc"
        assert result["hex"] == "0x64657363"

    def test_health_check(self):
        from iccdev_mcp import __version__
        from iccdev_mcp.server import health_check
        result = health_check()
        assert result["status"] == "ok"
        assert result["version"] == __version__
        assert result["python_api"]["available"] is True
        assert result["python_api"]["tools"] == len(
            result["python_api"]["available_tools"]
        )
        assert result["total_tools"] == (
            len(result["python_api"]["available_tools"])
            + len(result["cli_tools"]["available"])
            + len(result["service_tools"])
        )


@pytest.mark.skipif(not HAS_ICCDEV, reason="iccdev not installed")
class TestNativeToolsWithoutProfile:
    """Native tools that don't require specific test profiles."""

    def test_enum_spaces_has_rgb(self):
        from iccdev_mcp.server import enum_spaces
        result = enum_spaces()
        names = [s["name"] for s in result["color_spaces"]]
        assert "RGB" in names

    def test_list_available_profiles(self):
        from iccdev_mcp.server import list_available_profiles
        result = list_available_profiles()
        assert "profiles" in result
        assert "count" in result
        assert isinstance(result["profiles"], list)


# ---------------------------------------------------------------------------
# Subprocess tool tests (require CLI tools on PATH)
# ---------------------------------------------------------------------------

def _has_cli_tool(name: str) -> bool:
    """Check if a specific CLI tool is available."""
    from iccdev_mcp.cli_tools import _find_tool
    return _find_tool(name) is not None


@pytest.mark.skipif(
    not _has_cli_tool("iccDumpProfile") if HAS_ICCDEV else True,
    reason="iccDumpProfile not available",
)
@pytest.mark.skipif(not _test_profile, reason="No test profiles found")
class TestSubprocessTools:
    """Tests for subprocess-backed MCP tools."""

    def test_dump_profile(self):
        from iccdev_mcp.server import dump_profile
        result = dump_profile(_test_profile)
        assert result["returncode"] == 0
        assert len(result["stdout"]) > 0

    @pytest.mark.skipif(
        not _has_cli_tool("iccToXml"),
        reason="iccToXml not available",
    )
    def test_profile_to_xml(self):
        from iccdev_mcp.server import profile_to_xml
        result = profile_to_xml(_test_profile)
        assert result["returncode"] == 0
        assert "xml_content" in result
        assert "<?xml" in result["xml_content"]

    @pytest.mark.skipif(
        not _has_cli_tool("iccToJson"),
        reason="iccToJson not available",
    )
    def test_profile_to_json(self):
        from iccdev_mcp.server import profile_to_json
        result = profile_to_json(_test_profile)
        assert result["returncode"] == 0
        assert "json_content" in result

    @pytest.mark.skipif(
        not _has_cli_tool("iccRoundTrip"),
        reason="iccRoundTrip not available",
    )
    def test_round_trip_test(self):
        from iccdev_mcp.server import round_trip_test
        result = round_trip_test(_test_profile)
        assert "returncode" in result
        assert "stdout" in result

    @pytest.mark.skipif(
        not _has_cli_tool("iccPawgReport"),
        reason="iccPawgReport not available",
    )
    def test_pawg_report(self):
        from iccdev_mcp.server import pawg_report
        result = pawg_report(_test_profile)
        assert "returncode" in result
        assert "stdout" in result


# ---------------------------------------------------------------------------
# Error handling tests
# ---------------------------------------------------------------------------

class TestErrorHandling:
    """Tests for error handling in tools."""

    @pytest.mark.skipif(not HAS_ICCDEV, reason="iccdev not installed")
    def test_inspect_nonexistent(self):
        from iccdev_mcp.server import inspect_header
        with pytest.raises(FileNotFoundError):
            inspect_header("nonexistent_12345.icc")

    def test_dump_nonexistent(self):
        from iccdev_mcp.server import dump_profile
        with pytest.raises(FileNotFoundError):
            dump_profile("nonexistent_12345.icc")


class TestPixelValidation:
    """Tests for Python-native pixel validation before iccdev calls."""

    def test_color_transform_rejects_non_list_pixels(self):
        from iccdev_mcp.server import color_transform
        with pytest.raises(ValueError, match="pixels must be a list"):
            color_transform("src.icc", "dst.icc", "not pixels")

    def test_color_transform_rejects_non_numeric_pixel_value(self):
        from iccdev_mcp.server import color_transform
        with pytest.raises(ValueError, match="must be numeric"):
            color_transform("src.icc", "dst.icc", [[0.5, "bad", 0.1]])

    def test_roundtrip_delta_rejects_empty_pixel_row(self):
        from iccdev_mcp.server import roundtrip_delta
        with pytest.raises(ValueError, match="has no channels"):
            roundtrip_delta("profile.icc", [[]])

    def test_roundtrip_delta_rejects_non_finite_pixel_value(self):
        from iccdev_mcp.server import roundtrip_delta
        with pytest.raises(ValueError, match="must be finite"):
            roundtrip_delta("profile.icc", [[0.5, math.nan, 0.1]])


def test_dump_profile_passes_cli_options(monkeypatch, tmp_path):
    """MCP stdio dump_profile should expose the same core options as REST."""
    profile = tmp_path / "test.icc"
    profile.write_bytes(b"\0" * 128)
    captured = {}

    def fake_run_dump_profile(path, *, validate=False, verbosity=100, tag="ALL"):
        captured.update({
            "path": path,
            "validate": validate,
            "verbosity": verbosity,
            "tag": tag,
        })
        return {"returncode": 0, "stdout": "", "stderr": ""}

    monkeypatch.setattr(
        "iccdev_mcp.server.cli_tools.run_dump_profile",
        fake_run_dump_profile,
    )

    from iccdev_mcp.server import dump_profile
    result = dump_profile(str(profile), validate=True, verbosity=100, tag="desc")

    assert result["returncode"] == 0
    assert captured == {
        "path": str(profile.resolve()),
        "validate": True,
        "verbosity": 100,
        "tag": "desc",
    }


def test_round_trip_test_passes_cli_options(monkeypatch, tmp_path):
    """MCP stdio round_trip_test should expose intent and MPE selection."""
    profile = tmp_path / "test.icc"
    profile.write_bytes(b"\0" * 128)
    captured = {}

    def fake_run_round_trip(path, intent=1, use_mpe=0):
        captured.update({"path": path, "intent": intent, "use_mpe": use_mpe})
        return {"returncode": 0, "stdout": "", "stderr": ""}

    monkeypatch.setattr(
        "iccdev_mcp.server.cli_tools.run_round_trip",
        fake_run_round_trip,
    )

    from iccdev_mcp.server import round_trip_test
    result = round_trip_test(str(profile), intent=2, use_mpe=1)

    assert result["returncode"] == 0
    assert captured == {
        "path": str(profile.resolve()),
        "intent": 2,
        "use_mpe": 1,
    }


def test_pawg_report_passes_resolved_profile(monkeypatch, tmp_path):
    """MCP stdio pawg_report should pass a resolved profile to iccPawgReport."""
    profile = tmp_path / "test.icc"
    profile.write_bytes(b"\0" * 128)
    captured = {}

    def fake_run_pawg_report(path):
        captured["path"] = path
        return {"returncode": 0, "stdout": "", "stderr": ""}

    monkeypatch.setattr(
        "iccdev_mcp.server.cli_tools.run_pawg_report",
        fake_run_pawg_report,
    )

    from iccdev_mcp.server import pawg_report
    result = pawg_report(str(profile))

    assert result["returncode"] == 0
    assert captured["path"] == str(profile.resolve())


def test_apply_profiles_config_args(monkeypatch):
    """MCP stdio apply_profiles should support raw documented CLI args."""
    captured = {}

    def fake_run_apply_profiles_args(config_args):
        captured["config_args"] = config_args
        return {"returncode": 0, "stdout": "", "stderr": ""}

    monkeypatch.setattr(
        "iccdev_mcp.server.cli_tools.run_apply_profiles_args",
        fake_run_apply_profiles_args,
    )

    from iccdev_mcp.server import apply_profiles
    result = apply_profiles(config_args=["-cfg", "config.json"])

    assert result["returncode"] == 0
    assert captured["config_args"] == ["-cfg", "config.json"]


def test_apply_profiles_structured_args(monkeypatch, tmp_path):
    """MCP stdio apply_profiles should match REST structured arguments."""
    image = tmp_path / "in.tif"
    profile = tmp_path / "profile.icc"
    image.write_bytes(b"II*\0")
    profile.write_bytes(b"\0" * 128)
    captured = {}

    def fake_run_apply_profiles(
        src_image, dst_image, encoding, compress, planar, embed,
        interpolation, profiles, intents=None
    ):
        captured.update({
            "src_image": src_image,
            "encoding": encoding,
            "compress": compress,
            "planar": planar,
            "embed": embed,
            "interpolation": interpolation,
            "profiles": profiles,
            "intents": intents,
        })
        Path(dst_image).write_bytes(b"TIFF")
        return {"returncode": 0, "stdout": "", "stderr": ""}

    monkeypatch.setattr(
        "iccdev_mcp.server.cli_tools.run_apply_profiles",
        fake_run_apply_profiles,
    )

    from iccdev_mcp.server import apply_profiles
    result = apply_profiles(
        src_image=str(image),
        profiles=[str(profile)],
        intents=[1],
        encoding=1,
        compress=0,
        planar=0,
        embed=1,
        interpolation=0,
    )

    assert result["returncode"] == 0
    assert result["output_base64"] == "VElGRg=="
    assert captured["src_image"] == str(image.resolve())
    assert captured["profiles"] == [str(profile.resolve())]
    assert captured["intents"] == [1]
