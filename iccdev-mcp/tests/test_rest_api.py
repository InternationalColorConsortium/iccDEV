"""Tests for the iccdev-mcp REST API layer."""

from __future__ import annotations

import json
import re
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

# Add parent to path for imports
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

try:
    from starlette.testclient import TestClient
    HAS_STARLETTE = True
except ImportError:
    HAS_STARLETTE = False


def _skip_no_starlette(cls):
    """Skip entire class if starlette is not installed."""
    if not HAS_STARLETTE:
        return unittest.skip("starlette not installed")(cls)
    return cls


# -- Fixtures ---------------------------------------------------------------

def _make_client():
    """Create a test client for the REST API."""
    from iccdev_mcp.rest_api import create_app
    app = create_app()
    return TestClient(app)


# -- Test classes -----------------------------------------------------------

@_skip_no_starlette
class TestWebUi(unittest.TestCase):
    """Test the REST dashboard UI."""

    def setUp(self):
        self.client = _make_client()

    def test_index_returns_dashboard(self):
        resp = self.client.get("/")
        self.assertEqual(resp.status_code, 200)
        self.assertIn("text/html", resp.headers.get("content-type", ""))
        self.assertIn("iccdev-mcp REST Dashboard", resp.text)
        self.assertIn("profile_directory", resp.text)
        self.assertIn("testing_file", resp.text)
        self.assertIn("src_profile", resp.text)
        self.assertIn("display_profile", resp.text)
        self.assertIn("TOOL_GROUPS", resp.text)
        self.assertIn("profile_summary", resp.text)
        self.assertIn("verbosity", resp.text)
        self.assertIn("use_mpe", resp.text)

    def test_ui_alias_returns_dashboard(self):
        resp = self.client.get("/ui")
        self.assertEqual(resp.status_code, 200)
        self.assertIn("REST dashboard for iccDEV MCP tools", resp.text)

    def test_dashboard_security_headers(self):
        resp = self.client.get("/")
        csp = resp.headers.get("content-security-policy", "")
        self.assertIn("default-src 'none'", csp)
        self.assertIn("connect-src 'self'", csp)
        self.assertEqual(resp.headers.get("x-frame-options"), "DENY")
        self.assertEqual(resp.headers.get("x-content-type-options"), "nosniff")

    def test_dashboard_has_all_api_tools(self):
        resp = self.client.get("/")
        html = resp.text
        for tool in [
            "inspect_header", "profile_summary", "color_transform", "roundtrip_delta",
            "validate_profile", "sig_to_str", "enum_spaces", "profiles", "dump_profile", "pawg_report",
            "to_xml", "from_xml", "to_json", "from_json", "roundtrip",
            "tiff_dump", "jpeg_dump", "png_dump", "from_cube",
            "apply_profiles", "apply_named_cmm", "create_link",
            "v5_to_v4", "spec_sep", "apply_search", "upload",
        ]:
            self.assertRegex(html, rf"\b{re.escape(tool)}\b")

    def test_dashboard_avoids_inline_event_handlers(self):
        resp = self.client.get("/")
        self.assertNotIn("onclick=", resp.text)
        self.assertNotIn("document.write", resp.text)
        self.assertIn("addEventListener", resp.text)

    def test_favicon_probe_is_not_404(self):
        resp = self.client.get("/favicon.ico")
        self.assertEqual(resp.status_code, 204)


@_skip_no_starlette
class TestHealthEndpoints(unittest.TestCase):
    """Test /api/health and /api/tools endpoints."""

    def setUp(self):
        self.client = _make_client()

    def test_health_returns_ok(self):
        resp = self.client.get("/api/health")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertTrue(data["ok"])
        self.assertEqual(data["server"], "iccdev-mcp")
        self.assertTrue(data["python_api_available"])
        self.assertIn("cli_tools", data)
        self.assertEqual(data["tools_count"], 26)

    def test_health_has_version(self):
        from iccdev_mcp import __version__
        resp = self.client.get("/api/health")
        data = resp.json()
        self.assertEqual(data["version"], __version__)

    def test_tools_list(self):
        resp = self.client.get("/api/tools")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn("tools", data)
        self.assertIn("count", data)
        self.assertEqual(data["count"], 26)
        self.assertEqual(data["count"], len(data["tools"]))
        self.assertIn("rest_utility_routes", data)
        self.assertEqual(
            data["rest_utility_routes_count"],
            len(data["rest_utility_routes"]),
        )
        names = {tool["name"] for tool in data["tools"]}
        self.assertIn("round_trip_test", names)
        self.assertIn("pawg_report", names)
        self.assertIn("profile_summary", names)
        self.assertIn("validate_profile", names)
        utility_names = {route["name"] for route in data["rest_utility_routes"]}
        self.assertIn("upload_file", utility_names)
        self.assertIn("list_files", utility_names)

    def test_health_and_tools_count_stay_in_sync(self):
        health = self.client.get("/api/health").json()
        tools = self.client.get("/api/tools").json()
        self.assertEqual(health["tools_count"], tools["count"])
        self.assertEqual(
            health["rest_utility_routes_count"],
            tools["rest_utility_routes_count"],
        )

    def test_tools_have_required_fields(self):
        resp = self.client.get("/api/tools")
        data = resp.json()
        for tool in data["tools"]:
            self.assertIn("name", tool)
            self.assertIn("method", tool)
            self.assertIn("path", tool)
            self.assertIn("description", tool)
            self.assertIn("type", tool)


@_skip_no_starlette
class TestProfileEndpoints(unittest.TestCase):
    """Test profile listing and upload endpoints."""

    def setUp(self):
        self.client = _make_client()

    def test_list_profiles(self):
        resp = self.client.get("/api/profiles")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn("profiles", data)
        self.assertIn("directories", data)

    def test_list_profiles_by_filename(self):
        all_resp = self.client.get("/api/profiles")
        self.assertEqual(all_resp.status_code, 200)
        profiles = all_resp.json()["profiles"]
        if not profiles:
            self.skipTest("No profiles available")
        selected = profiles[0]
        resp = self.client.get(
            "/api/profiles",
            params={"filename": selected["path"]},
        )
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertEqual(data["count"], 1)
        self.assertEqual(data["profiles"][0]["path"], selected["path"])

    def test_list_profiles_includes_nested_hybrid_directory(self):
        from iccdev_mcp.profiles import _find_testing_dir

        testing = _find_testing_dir()
        if testing is None or not (testing / "hybrid").is_dir():
            self.skipTest("Testing/hybrid directory not available")

        resp = self.client.get("/api/profiles")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        directory_names = {item["name"] for item in data["directories"]}
        if "hybrid" not in directory_names:
            self.skipTest("Testing/hybrid has no generated ICC profiles")
        self.assertIn("hybrid", directory_names)
        self.assertIn("hybrid/ICC", directory_names)

        hybrid_resp = self.client.get("/api/profiles?directory=hybrid")
        self.assertEqual(hybrid_resp.status_code, 200)
        hybrid_profiles = hybrid_resp.json()["profiles"]
        self.assertTrue(
            any(item["name"] == "CMYK_Hybrid_Profile.icc" for item in hybrid_profiles)
        )

    def test_list_files_by_kind(self):
        resp = self.client.get("/api/files?kind=xml")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn("files", data)
        self.assertIn("directories", data)
        self.assertEqual(data["kind"], "xml")

    def test_list_files_rejects_unknown_kind(self):
        resp = self.client.get("/api/files?kind=exe")
        self.assertEqual(resp.status_code, 400)

    def test_list_profiles_nonexistent_dir(self):
        resp = self.client.get("/api/profiles?directory=/nonexistent")
        # Returns 200 with empty list (directory just has no profiles)
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertEqual(data["profiles"], [])

    def test_upload_missing_file(self):
        resp = self.client.post(
            "/api/upload",
            headers={"content-type": "multipart/form-data; boundary=----"},
        )
        # Will fail because no file field
        self.assertIn(resp.status_code, [400, 422])

    def test_upload_wrong_content_type(self):
        resp = self.client.post(
            "/api/upload",
            content=b"not multipart",
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)
        data = resp.json()
        self.assertIn("error", data)

    def test_upload_sanitizes_traversal_filename(self):
        resp = self.client.post(
            "/api/upload",
            files={"file": ("../../etc/passwd", b"profile", "application/octet-stream")},
        )
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        uploaded = Path(data["path"])
        try:
            self.assertTrue(uploaded.name.endswith("_passwd"))
            self.assertNotIn("..", data["filename"])
            self.assertNotIn("/", data["filename"])
            self.assertEqual(uploaded.parent.name, "iccdev-mcp-uploads")
        finally:
            uploaded.unlink(missing_ok=True)

    def test_upload_sanitizes_backslash_filename(self):
        resp = self.client.post(
            "/api/upload",
            files={"file": ("..\\..\\evil file.icc", b"profile", "application/octet-stream")},
        )
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        uploaded = Path(data["path"])
        try:
            self.assertTrue(uploaded.name.endswith("_evil_file.icc"))
            self.assertNotIn("\\", data["filename"])
            self.assertNotIn("/", data["filename"])
            self.assertNotIn(" ", data["filename"])
            self.assertEqual(uploaded.parent.name, "iccdev-mcp-uploads")
        finally:
            uploaded.unlink(missing_ok=True)

    def test_upload_rejects_oversized_file(self):
        from iccdev_mcp import rest_api

        old_limit = rest_api.MAX_UPLOAD_BYTES
        rest_api.MAX_UPLOAD_BYTES = 4
        try:
            resp = self.client.post(
                "/api/upload",
                files={"file": ("large.icc", b"12345", "application/octet-stream")},
            )
        finally:
            rest_api.MAX_UPLOAD_BYTES = old_limit
        self.assertEqual(resp.status_code, 413)
        self.assertIn("too large", resp.json()["error"])


@_skip_no_starlette
class TestNativeToolEndpoints(unittest.TestCase):
    """Test Python-native tool REST endpoints."""

    def setUp(self):
        self.client = _make_client()

    def test_inspect_header_missing_path(self):
        resp = self.client.get("/api/inspect-header")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("error", resp.json())

    def test_inspect_header_rejects_traversal_as_client_error(self):
        resp = self.client.get("/api/inspect-header?path=../../etc/passwd")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("traversal", resp.json()["error"])

    def test_profile_summary_missing_path(self):
        resp = self.client.get("/api/profile-summary")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("error", resp.json())

    def test_profile_summary_rejects_traversal_as_client_error(self):
        resp = self.client.get("/api/profile-summary?path=../../etc/passwd")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("traversal", resp.json()["error"])

    def test_validate_profile_missing_path(self):
        resp = self.client.get("/api/validate-profile")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("error", resp.json())

    def test_validate_profile_rejects_traversal_as_client_error(self):
        resp = self.client.get("/api/validate-profile?path=../../etc/passwd")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("traversal", resp.json()["error"])

    def test_validate_profile_returns_native_result(self):
        import iccdev
        if not iccdev.native_validation_available():
            self.skipTest("Native C validation ABI is not available")
        profiles = self.client.get("/api/profiles").json()["profiles"]
        if not profiles:
            self.skipTest("No test profiles available")

        resp = self.client.get(
            "/api/validate-profile",
            params={"path": profiles[0]["path"]},
        )
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertIn(data["status_name"], {"OK", "WARNING"})
        self.assertIn(data["status"], {0, 1})
        self.assertIsInstance(data["report"], str)

    def test_validate_profile_reports_missing_native_abi(self):
        with patch(
            "iccdev_mcp.server.validate_profile",
            side_effect=RuntimeError("Native validation is unavailable"),
        ):
            resp = self.client.get(
                "/api/validate-profile",
                params={"path": "sRGB_v4_ICC_preference.icc"},
            )
        self.assertEqual(resp.status_code, 503)
        self.assertIn("unavailable", resp.json()["error"])

    def test_sig_to_str_missing_param(self):
        resp = self.client.get("/api/sig-to-str")
        self.assertEqual(resp.status_code, 400)

    def test_sig_to_str_invalid_int(self):
        resp = self.client.get("/api/sig-to-str?sig=notanumber")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("integer", resp.json()["error"])

    def test_color_transform_missing_body(self):
        resp = self.client.post(
            "/api/color-transform",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_color_transform_missing_profiles(self):
        resp = self.client.post(
            "/api/color-transform",
            content=json.dumps({"pixels": [[0.5, 0.5, 0.5]]}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)
        self.assertIn("src_profile", resp.json()["error"])

    def test_color_transform_rejects_non_list_pixels(self):
        resp = self.client.post(
            "/api/color-transform",
            content=json.dumps({
                "src_profile": "/tmp/src.icc",
                "dst_profile": "/tmp/dst.icc",
                "pixels": "0.5,0.5,0.5",
            }),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)
        self.assertIn("pixels must be a list", resp.json()["error"])

    def test_color_transform_rejects_oversized_json_body(self):
        from iccdev_mcp import rest_api

        old_limit = rest_api.MAX_JSON_BYTES
        rest_api.MAX_JSON_BYTES = 32
        try:
            resp = self.client.post(
                "/api/color-transform",
                content=b'{"pixels":"' + (b"a" * 64) + b'"}',
                headers={"content-type": "application/json"},
            )
        finally:
            rest_api.MAX_JSON_BYTES = old_limit
        self.assertEqual(resp.status_code, 413)
        self.assertIn("too large", resp.json()["error"])

    def test_color_transform_rejects_json_array_body(self):
        resp = self.client.post(
            "/api/color-transform",
            content=json.dumps([]),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)
        self.assertIn("expected an object", resp.json()["error"])

    def test_roundtrip_delta_missing_body(self):
        resp = self.client.post(
            "/api/roundtrip-delta",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_roundtrip_delta_rejects_non_list_pixels(self):
        resp = self.client.post(
            "/api/roundtrip-delta",
            content=json.dumps({
                "profile": "/tmp/profile.icc",
                "pixels": None,
            }),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)
        self.assertIn("pixels", resp.json()["error"])

    def test_enum_spaces_returns_color_spaces(self):
        resp = self.client.get("/api/enum-spaces")
        self.assertEqual(resp.status_code, 200)
        data = resp.json()
        self.assertGreater(data["count"], 0)
        self.assertIn(
            "RGB", {space["name"] for space in data["color_spaces"]}
        )


@_skip_no_starlette
class TestSubprocessEndpoints(unittest.TestCase):
    """Test subprocess CLI tool REST endpoints."""

    def setUp(self):
        self.client = _make_client()

    def test_dump_missing_path(self):
        resp = self.client.get("/api/dump")
        self.assertEqual(resp.status_code, 400)
        self.assertIn("error", resp.json())

    def test_dump_accepts_cli_options(self):
        resp = self.client.get(
            "/api/dump",
            params={
                "path": "/nonexistent/profile.icc",
                "validate": "true",
                "verbosity": "100",
                "tag": "ALL",
            },
        )
        self.assertIn(resp.status_code, [404, 500])

    def test_dump_nonexistent_profile(self):
        resp = self.client.get("/api/dump?path=/nonexistent/profile.icc")
        self.assertIn(resp.status_code, [404, 500])

    def test_to_xml_missing_path(self):
        resp = self.client.get("/api/to-xml")
        self.assertEqual(resp.status_code, 400)

    def test_pawg_report_missing_path(self):
        resp = self.client.get("/api/pawg-report")
        self.assertEqual(resp.status_code, 400)

    def test_pawg_report_nonexistent_profile(self):
        resp = self.client.get("/api/pawg-report?path=/nonexistent/profile.icc")
        self.assertIn(resp.status_code, [404, 500])

    def test_to_json_missing_path(self):
        resp = self.client.get("/api/to-json")
        self.assertEqual(resp.status_code, 400)

    def test_from_xml_missing_body(self):
        resp = self.client.post(
            "/api/from-xml",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_from_json_missing_body(self):
        resp = self.client.post(
            "/api/from-json",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_from_cube_missing_body(self):
        resp = self.client.post(
            "/api/from-cube",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_apply_profiles_missing_fields(self):
        resp = self.client.post(
            "/api/apply-profiles",
            content=json.dumps({"output_tiff": "/tmp/out.tif"}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_apply_named_cmm_missing_config(self):
        resp = self.client.post(
            "/api/apply-named-cmm",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_create_link_missing_fields(self):
        resp = self.client.post(
            "/api/create-link",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_v5_to_v4_missing_fields(self):
        resp = self.client.post(
            "/api/v5-to-v4",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_spec_sep_missing_fields(self):
        resp = self.client.post(
            "/api/spec-sep",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_apply_search_missing_config(self):
        resp = self.client.post(
            "/api/apply-search",
            content=json.dumps({}),
            headers={"content-type": "application/json"},
        )
        self.assertEqual(resp.status_code, 400)

    def test_tiff_dump_missing_path(self):
        resp = self.client.get("/api/tiff-dump")
        self.assertEqual(resp.status_code, 400)

    def test_jpeg_dump_missing_path(self):
        resp = self.client.get("/api/jpeg-dump")
        self.assertEqual(resp.status_code, 400)

    def test_png_dump_missing_path(self):
        resp = self.client.get("/api/png-dump")
        self.assertEqual(resp.status_code, 400)

    def test_roundtrip_missing_path(self):
        resp = self.client.get("/api/roundtrip")
        self.assertEqual(resp.status_code, 400)

    def test_roundtrip_accepts_cli_options(self):
        resp = self.client.get(
            "/api/roundtrip",
            params={
                "path": "/nonexistent/profile.icc",
                "intent": "1",
                "use_mpe": "0",
            },
        )
        self.assertIn(resp.status_code, [404, 500])


@_skip_no_starlette
class TestAppCreation(unittest.TestCase):
    """Test application factory."""

    def test_create_app_returns_starlette(self):
        from iccdev_mcp.rest_api import create_app
        from starlette.applications import Starlette
        app = create_app()
        self.assertIsInstance(app, Starlette)

    def test_create_app_has_routes(self):
        from iccdev_mcp.rest_api import create_app
        app = create_app()
        self.assertGreater(len(app.routes), 20)


class TestWithoutStarlette(unittest.TestCase):
    """Test behavior when starlette is not installed."""

    def test_module_imports_without_starlette(self):
        """rest_api module should import even without starlette."""
        from iccdev_mcp import rest_api
        # Module should load, HAS_STARLETTE flag should exist
        self.assertIsInstance(rest_api.HAS_STARLETTE, bool)


if __name__ == "__main__":
    unittest.main()
