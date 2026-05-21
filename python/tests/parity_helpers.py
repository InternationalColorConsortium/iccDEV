"""Helpers for native-backed Python parity tests."""

from __future__ import annotations

import os
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

import pytest


REPO_ROOT = Path(__file__).resolve().parents[2]
TESTING_DIR = REPO_ROOT / "Testing"

_CONFIGS = ("Release", "Debug", "RelWithDebInfo", "MinSizeRel")
_LIB_DIRS = ("IccProfLib", "IccXML", "IccJSON", "IccConnect")


@dataclass(frozen=True)
class Tool:
    """Resolved iccDEV command-line tool."""

    name: str
    path: Path
    build_dir: Path | None


@dataclass(frozen=True)
class ToolResult:
    """Captured tool execution result with CI-style classification helpers."""

    command: tuple[str, ...]
    returncode: int
    stdout: str
    stderr: str
    timed_out: bool = False

    @property
    def output(self) -> str:
        return f"{self.stdout}\n{self.stderr}"

    @property
    def has_sanitizer_finding(self) -> bool:
        output = self.output
        return (
            "ERROR: AddressSanitizer" in output
            or "UndefinedBehaviorSanitizer" in output
            or "runtime error:" in output
        )

    @property
    def hard_exit(self) -> bool:
        if self.timed_out:
            return True
        if self.returncode < 0:
            return True
        if self.returncode in {124, 134, 136, 137, 139}:
            return True
        if os.name == "nt" and self.returncode >= 0xC0000000:
            return True
        return False

    def assert_success(self, label: str) -> None:
        if self.timed_out:
            pytest.fail(f"{label} timed out: {' '.join(self.command)}")
        if self.has_sanitizer_finding:
            pytest.fail(f"{label} produced sanitizer output:\n{self.output[:4000]}")
        if self.returncode != 0:
            pytest.fail(
                f"{label} failed with exit {self.returncode}:\n"
                f"command: {' '.join(self.command)}\n{self.output[:4000]}"
            )


def _candidate_build_dirs() -> list[Path]:
    candidates: list[Path] = []
    for env_name in ("ICCDEV_BUILD_DIR", "ICCDEV_TOOLS_DIR"):
        value = os.environ.get(env_name)
        if not value:
            continue
        for raw in value.split(os.pathsep):
            path = Path(raw)
            if env_name == "ICCDEV_TOOLS_DIR" and path.name == "Tools":
                path = path.parent
            candidates.append(path)

    candidates.extend(
        [
            REPO_ROOT / "Build",
            REPO_ROOT / "out" / "vs2022-vcpkg",
            REPO_ROOT / "out" / "ninja-vcpkg-debug",
            REPO_ROOT / "out" / "ninja-vcpkg-release",
        ]
    )

    seen: set[Path] = set()
    existing: list[Path] = []
    for path in candidates:
        resolved = path.resolve()
        if resolved in seen or not resolved.exists():
            continue
        seen.add(resolved)
        existing.append(resolved)
    return existing


def _tool_candidates(build_dir: Path, target: str, executable: str) -> list[Path]:
    suffix = ".exe" if os.name == "nt" else ""
    exe_name = executable if executable.endswith(suffix) else executable + suffix
    base = build_dir / "Tools" / target
    candidates = [base / exe_name]
    candidates.extend(base / config / exe_name for config in _CONFIGS)
    return candidates


def find_tool(target: str, executable: str | None = None) -> Tool | None:
    """Find a built iccDEV tool in common CMake layouts or PATH."""

    executable = executable or target[0].lower() + target[1:]
    for build_dir in _candidate_build_dirs():
        for candidate in _tool_candidates(build_dir, target, executable):
            if candidate.is_file():
                return Tool(executable, candidate, build_dir)

    path_hit = shutil.which(executable)
    if path_hit:
        return Tool(executable, Path(path_hit), None)
    if os.name == "nt":
        path_hit = shutil.which(executable + ".exe")
        if path_hit:
            return Tool(executable, Path(path_hit), None)
    return None


def require_tool(target: str, executable: str | None = None) -> Tool:
    tool = find_tool(target, executable)
    if tool is None:
        pytest.skip(f"{target} is not built or is not on PATH")
    return tool


def tool_environment(tool: Tool) -> dict[str, str]:
    env = os.environ.copy()
    if tool.build_dir is None:
        return env

    path_entries: list[str] = []
    for target_dir in (tool.build_dir / "Tools").glob("*"):
        if target_dir.is_dir():
            path_entries.append(str(target_dir))
            path_entries.extend(str(target_dir / config) for config in _CONFIGS if (target_dir / config).is_dir())

    for lib_name in _LIB_DIRS:
        lib_dir = tool.build_dir / lib_name
        if lib_dir.is_dir():
            path_entries.append(str(lib_dir))
            path_entries.extend(str(lib_dir / config) for config in _CONFIGS if (lib_dir / config).is_dir())

    path_entries.extend(
        str(path)
        for path in (
            REPO_ROOT / "installed" / "x64-windows" / "bin",
            REPO_ROOT / "installed" / "x64-windows" / "debug" / "bin",
            tool.build_dir / "vcpkg_installed" / "x64-windows" / "bin",
            tool.build_dir / "vcpkg_installed" / "x64-windows" / "debug" / "bin",
        )
        if path.is_dir()
    )

    if path_entries:
        env["PATH"] = os.pathsep.join(path_entries + [env.get("PATH", "")])

    shared_paths = [str(tool.build_dir / name) for name in _LIB_DIRS if (tool.build_dir / name).is_dir()]
    if shared_paths:
        for var in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
            env[var] = os.pathsep.join(shared_paths + [env.get(var, "")])

    env.setdefault("ASAN_OPTIONS", "halt_on_error=0,detect_leaks=0")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=0,print_stacktrace=1")
    env.setdefault("LLVM_PROFILE_FILE", os.devnull)
    return env


def run_tool(tool: Tool, *args: object, timeout: int = 30, cwd: Path | None = None) -> ToolResult:
    command = tuple(str(part) for part in (tool.path, *args))
    try:
        completed = subprocess.run(
            command,
            cwd=str(cwd or REPO_ROOT),
            env=tool_environment(tool),
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        return ToolResult(command, completed.returncode, completed.stdout, completed.stderr)
    except subprocess.TimeoutExpired as exc:
        return ToolResult(
            command,
            124,
            exc.stdout or "",
            exc.stderr or "",
            timed_out=True,
        )


def all_profiles(limit: int | None = None) -> list[Path]:
    if not TESTING_DIR.is_dir():
        pytest.skip(f"Testing directory not found: {TESTING_DIR}")
    profiles = sorted(TESTING_DIR.rglob("*.icc"))
    if not profiles:
        pytest.skip("No generated ICC profiles found; run CreateAllProfiles first")
    if limit is None:
        env_limit = os.environ.get("ICCDEV_PYTHON_PARITY_PROFILE_LIMIT", "25")
        limit = int(env_limit)
    return profiles[:limit]


def selected_profiles() -> list[Path]:
    names = [
        "sRGB_v4_ICC_preference.icc",
        "Display/sRGB_D65_MAT.icc",
        "Display/LCDDisplay.icc",
        "Named/NamedColor.icc",
        "Calc/srgbCalcTest.icc",
        "PCC/Lab_float-D50_2deg.icc",
    ]
    profiles = [TESTING_DIR / name for name in names if (TESTING_DIR / name).is_file()]
    if not profiles:
        profiles = all_profiles(limit=3)
    return profiles[: int(os.environ.get("ICCDEV_PYTHON_PARITY_SELECTED_LIMIT", "6"))]


def first_rgb_profile() -> Path:
    candidates = [
        TESTING_DIR / "sRGB_v4_ICC_preference.icc",
        TESTING_DIR / "Display" / "sRGB_D65_MAT.icc",
        TESTING_DIR / "Display" / "LCDDisplay.icc",
    ]
    for path in candidates:
        if path.is_file():
            return path
    return selected_profiles()[0]
