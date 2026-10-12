# Copyright (c) International Color Consortium.
#
# Redistribution and use in source and binary forms, with or without modification,
# are permitted provided that the following conditions are met:
#
#     1. Redistributions of source code must retain the above copyright notice,
#        this list of conditions and the following disclaimer.
#
#     2. Redistributions in binary form must reproduce the above copyright
#        notice, this list of conditions and the following disclaimer in the
#        documentation and/or other materials provided with the distribution.
#
#     3. Neither the name of International Color Consortium (ICC) nor the names of
#        its contributors may be used to endorse or promote products derived from
#        this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
# ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Regression checks for parity tool discovery across CMake output layouts."""

import os

import pytest

from . import parity_helpers


@pytest.fixture
def isolated_build(monkeypatch, tmp_path):
    build_dir = tmp_path / "build"
    build_dir.mkdir()
    monkeypatch.setattr(parity_helpers, "REPO_ROOT", tmp_path / "source")
    monkeypatch.setenv("ICCDEV_BUILD_DIR", str(build_dir))
    monkeypatch.delenv("ICCDEV_TOOLS_DIR", raising=False)
    monkeypatch.setattr(parity_helpers.shutil, "which", lambda _name: None)
    return build_dir


def make_tool(build_dir, layout):
    name = "iccDumpProfile.exe" if os.name == "nt" else "iccDumpProfile"
    tool = build_dir / layout / name
    tool.parent.mkdir(parents=True, exist_ok=True)
    tool.touch()
    return tool


@pytest.mark.parametrize("layout", [
    "bin", "bin/Release", "bin/Debug", "bin/RelWithDebInfo", "bin/MinSizeRel",
    "Tools/IccDumpProfile", "Tools/IccDumpProfile/Release",
])
def test_finds_current_and_legacy_cmake_tools(isolated_build, layout):
    expected = make_tool(isolated_build, layout)
    tool = parity_helpers.require_tool("IccDumpProfile", "iccDumpProfile")
    assert tool.path == expected
    assert tool.build_dir == isolated_build


def test_prefers_current_runtime_layout_over_legacy(isolated_build):
    expected = make_tool(isolated_build, "bin/Release")
    make_tool(isolated_build, "Tools/IccDumpProfile/Release")
    assert parity_helpers.require_tool("IccDumpProfile").path == expected


def test_missing_tool_keeps_explicit_skip(isolated_build):
    assert parity_helpers.find_tool("IccDumpProfile") is None
    with pytest.raises(pytest.skip.Exception, match="IccDumpProfile is not built"):
        parity_helpers.require_tool("IccDumpProfile")


def test_runtime_directory_precedes_preserved_library_paths(isolated_build, monkeypatch):
    expected = make_tool(isolated_build, "bin/Release")
    library_dir = isolated_build / "IccProfLib"
    library_dir.mkdir()
    for name in ("PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        monkeypatch.setenv(name, "existing-entry")
    environment = parity_helpers.tool_environment(parity_helpers.require_tool("IccDumpProfile"))
    for name in ("PATH", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
        entries = environment[name].split(os.pathsep)
        assert entries[0] == str(expected.parent)
        assert str(library_dir) in entries
        assert entries[-1] == "existing-entry"
        assert os.environ[name] == "existing-entry"
