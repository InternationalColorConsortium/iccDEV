"""Native-backed parity tests for IccProfLib behavior."""

from __future__ import annotations

import threading

import pytest

from .parity_helpers import (
    TESTING_DIR,
    all_profiles,
    first_rgb_profile,
    require_tool,
    run_tool,
    selected_profiles,
)


pytestmark = [pytest.mark.parity, pytest.mark.proflib]


@pytest.fixture
def iccdev_module():
    return pytest.importorskip("iccdev")


def test_generated_profile_headers_are_readable(iccdev_module):
    for profile_path in all_profiles():
        with iccdev_module.IccProfile(profile_path) as profile:
            header = profile.header
            assert header.size > 0, profile_path
            assert header.magic == 0x61637370, profile_path
            assert profile.version_string.count(".") == 2


def test_cmm_apply_single_multi_numpy_and_apply_handle(iccdev_module):
    profile_path = first_rgb_profile()
    cmm = iccdev_module.IccCmm()
    try:
        cmm.attach(profile_path)
        cmm.attach(profile_path)
        cmm.begin()

        source = [0.5] * cmm.src_channels
        single = cmm.apply(source)
        assert len(single) == cmm.dst_channels
        assert all(0.0 <= value <= 1.0 for value in single)

        multi = cmm.apply_multi([source, source])
        assert len(multi) == 2
        assert all(len(pixel) == cmm.dst_channels for pixel in multi)

        apply = cmm.get_apply()
        try:
            from_handle = apply.apply(source)
            assert len(from_handle) == cmm.dst_channels
        finally:
            apply.close()

        np = pytest.importorskip("numpy")
        pixels = np.array([source, source], dtype=np.float32)
        output = cmm.apply_ndarray(pixels)
        assert output.shape == (2, cmm.dst_channels)
        assert output.dtype == np.float32
    finally:
        cmm.close()


def test_apply_handles_work_from_multiple_threads(iccdev_module):
    profile_path = first_rgb_profile()
    cmm = iccdev_module.IccCmm()
    handles = []
    results = []
    errors = []
    lock = threading.Lock()
    try:
        cmm.attach(profile_path)
        cmm.attach(profile_path)
        cmm.begin()
        source = [0.25] * cmm.src_channels
        handles = [cmm.get_apply() for _ in range(4)]

        def worker(handle):
            try:
                result = handle.apply(source)
                with lock:
                    results.append(result)
            except Exception as exc:  # pragma: no cover - propagated below
                with lock:
                    errors.append(exc)

        threads = [threading.Thread(target=worker, args=(handle,)) for handle in handles]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()

        assert not errors
        assert len(results) == len(handles)
        assert all(len(result) == cmm.dst_channels for result in results)
    finally:
        for handle in handles:
            handle.close()
        cmm.close()


@pytest.mark.cli
def test_icc_dump_profile_all_selected_profiles():
    dump = require_tool("IccDumpProfile", "iccDumpProfile")
    for profile_path in selected_profiles():
        result = run_tool(dump, profile_path, "ALL")
        result.assert_success(f"iccDumpProfile ALL {profile_path.name}")
        assert "Profile" in result.output or "Header" in result.output


@pytest.mark.cli
def test_icc_round_trip_display_profile_smoke():
    roundtrip = require_tool("IccRoundTrip", "iccRoundTrip")
    profile_path = first_rgb_profile()
    result = run_tool(roundtrip, profile_path, timeout=60)
    result.assert_success(f"iccRoundTrip {profile_path.name}")
    assert "Round Trip" in result.output
    assert "DeltaE" in result.output


@pytest.mark.cli
def test_icc_pawg_report_known_srgb_profile_smoke():
    pawg = require_tool("IccPawgReport", "iccPawgReport")
    profile_path = TESTING_DIR / "sRGB_v4_ICC_preference.icc"
    if not profile_path.is_file():
        pytest.skip(f"Known-good PAWG profile not found: {profile_path}")

    result = run_tool(pawg, profile_path, timeout=60)
    result.assert_success(f"iccPawgReport {profile_path.name}")
    assert "ICC PROFILE ASSESSMENT REPORT (PAWG)" in result.output
    assert "Total checklist items:  31" in result.output
    assert "FAIL:                   0" in result.output
    assert "NOT RUN:                0" in result.output
    assert "Overall:" in result.output
