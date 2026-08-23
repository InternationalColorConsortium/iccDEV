"""Packaging configuration regression tests."""

import runpy
import sys
from pathlib import Path

import pytest


def _load_setup(monkeypatch, build_dir):
    setuptools = pytest.importorskip(
        "setuptools",
        reason="packaging configuration tests require build dependencies",
    )
    captured = {}

    def capture_setup(**kwargs):
        captured.update(kwargs)

    monkeypatch.setattr(setuptools, "setup", capture_setup)
    monkeypatch.setenv("ICCDEV_BUILD_DIR", str(build_dir))
    setup_path = Path(__file__).resolve().parents[1] / "setup.py"
    namespace = runpy.run_path(str(setup_path))
    return namespace, captured


def test_inline_sources_exclude_cmake_managed_simd(monkeypatch, tmp_path):
    namespace, _captured = _load_setup(monkeypatch, tmp_path / "missing-build")
    source_dir = Path(__file__).resolve().parents[2] / "IccProfLib"

    sources = {
        Path(source).name
        for source in namespace["_iccproflib_cpp_sources"](str(source_dir))
    }

    assert "IccTagLut.cpp" in sources
    assert "IccTagLutAvx2.cpp" not in sources
    assert "IccTagLutAvx512.cpp" not in sources


def test_unix_prebuilt_static_library_links_zlib(monkeypatch, tmp_path):
    build_dir = tmp_path / "build"
    library_dir = build_dir / "IccProfLib"
    library_dir.mkdir(parents=True)
    (library_dir / "libIccProfLib2-static.a").write_bytes(b"")
    monkeypatch.setattr(sys, "platform", "linux")

    _namespace, captured = _load_setup(monkeypatch, build_dir)
    extension = captured["ext_modules"][0]

    assert extension.libraries == ["IccProfLib2-static", "z"]
