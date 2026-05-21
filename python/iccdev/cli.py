"""CLI-backed helpers for ICC profile conversion and diagnostics.

The Cython bindings expose in-process profile/header and CMM APIs.  XML, JSON,
dump, and round-trip operations are implemented by the native iccDEV command
line tools, so these helpers discover and run those tools without using a shell.
"""

from __future__ import annotations

import json
import os
import shutil
# Fixed iccDEV tool paths are executed without a shell.
import subprocess  # nosec B404
import tempfile
from pathlib import Path
from typing import Any


_CONFIGS = ("Release", "Debug", "RelWithDebInfo", "MinSizeRel")
_LIB_DIRS = ("IccProfLib", "IccXML", "IccJSON", "IccConnect")
_TOOLS: dict[str, tuple[str, str]] = {
    "iccDumpProfile": ("IccDumpProfile", "iccDumpProfile"),
    "iccRoundTrip": ("IccRoundTrip", "iccRoundTrip"),
    "iccToXml": ("IccToXml", "iccToXml"),
    "iccFromXml": ("IccFromXml", "iccFromXml"),
    "iccToJson": ("IccToJson", "iccToJson"),
    "iccFromJson": ("IccFromJson", "iccFromJson"),
}
_ALIASES: dict[str, str] = {}
for _canonical, (_target, _executable) in _TOOLS.items():
    _ALIASES[_canonical.lower()] = _canonical
    _ALIASES[_target.lower()] = _canonical
    _ALIASES[_executable.lower()] = _canonical
    _ALIASES[f"{_executable}.exe".lower()] = _canonical


class IccToolError(RuntimeError):
    """Raised when a required iccDEV command line tool is missing or fails."""

    def __init__(
        self,
        message: str,
        *,
        command: tuple[str, ...] = (),
        returncode: int | None = None,
        stdout: str = "",
        stderr: str = "",
    ) -> None:
        super().__init__(message)
        self.command = command
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


def _canonical_tool(tool: str) -> str:
    key = Path(tool).name.lower()
    if key in _ALIASES:
        return _ALIASES[key]
    raise IccToolError(f"unknown iccDEV tool: {tool}")


def _exe_name(executable: str) -> str:
    if os.name == "nt" and not executable.lower().endswith(".exe"):
        return f"{executable}.exe"
    return executable


def _split_env_paths(value: str | None) -> list[Path]:
    if not value:
        return []
    return [Path(raw).expanduser() for raw in value.split(os.pathsep) if raw]


def _tool_candidates_from_build_dir(build_dir: Path, target: str, executable: str) -> list[Path]:
    exe_name = _exe_name(executable)
    base = build_dir / "Tools" / target
    candidates = [base / exe_name]
    candidates.extend(base / config / exe_name for config in _CONFIGS)
    return candidates


def _tool_candidates_from_tools_dir(tools_dir: Path, target: str, executable: str) -> list[Path]:
    exe_name = _exe_name(executable)
    base = tools_dir / target
    candidates = [base / exe_name]
    candidates.extend(base / config / exe_name for config in _CONFIGS)
    candidates.append(tools_dir / exe_name)
    return candidates


def _infer_build_dir(tool_path: Path) -> Path | None:
    parts = tool_path.resolve().parts
    lowered = [part.lower() for part in parts]
    if "tools" not in lowered:
        return None
    tools_index = lowered.index("tools")
    if tools_index == 0:
        return None
    return Path(*parts[:tools_index])


def _tool_environment(tool_path: Path) -> dict[str, str]:
    env = os.environ.copy()
    build_dir = _infer_build_dir(tool_path)
    if build_dir is None:
        return env

    path_entries: list[str] = []
    tools_dir = build_dir / "Tools"
    if tools_dir.is_dir():
        for target_dir in tools_dir.iterdir():
            if target_dir.is_dir():
                path_entries.append(str(target_dir))
                path_entries.extend(
                    str(target_dir / config)
                    for config in _CONFIGS
                    if (target_dir / config).is_dir()
                )

    for lib_name in _LIB_DIRS:
        lib_dir = build_dir / lib_name
        if lib_dir.is_dir():
            path_entries.append(str(lib_dir))
            path_entries.extend(
                str(lib_dir / config)
                for config in _CONFIGS
                if (lib_dir / config).is_dir()
            )

    path_entries.extend(
        str(path)
        for path in (
            build_dir / "vcpkg_installed" / "x64-windows" / "bin",
            build_dir / "vcpkg_installed" / "x64-windows" / "debug" / "bin",
            build_dir.parent / "installed" / "x64-windows" / "bin",
            build_dir.parent / "installed" / "x64-windows" / "debug" / "bin",
            build_dir.parent.parent / "installed" / "x64-windows" / "bin",
            build_dir.parent.parent / "installed" / "x64-windows" / "debug" / "bin",
        )
        if path.is_dir()
    )

    if path_entries:
        env["PATH"] = os.pathsep.join(
            entry for entry in path_entries + [env.get("PATH", "")] if entry
        )

    shared_paths = [str(build_dir / name) for name in _LIB_DIRS if (build_dir / name).is_dir()]
    if shared_paths:
        for var in ("LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"):
            env[var] = os.pathsep.join(
                entry for entry in shared_paths + [env.get(var, "")] if entry
            )

    env.setdefault("ASAN_OPTIONS", "halt_on_error=0,detect_leaks=0")
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=0,print_stacktrace=1")
    env.setdefault("LLVM_PROFILE_FILE", os.devnull)
    return env


def find_tool(tool: str) -> Path:
    """Find an iccDEV tool by canonical CLI name, target name, or executable name."""

    canonical = _canonical_tool(tool)
    target, executable = _TOOLS[canonical]

    explicit = Path(tool).expanduser()
    if explicit.is_file():
        return explicit.resolve()

    for tools_dir in _split_env_paths(os.environ.get("ICCDEV_TOOLS_DIR")):
        for candidate in _tool_candidates_from_tools_dir(tools_dir, target, executable):
            if candidate.is_file():
                return candidate.resolve()

    for build_dir in _split_env_paths(os.environ.get("ICCDEV_BUILD_DIR")):
        for candidate in _tool_candidates_from_build_dir(build_dir, target, executable):
            if candidate.is_file():
                return candidate.resolve()

    path_hit = shutil.which(_exe_name(executable)) or shutil.which(executable)
    if path_hit:
        return Path(path_hit).resolve()

    raise IccToolError(
        f"{canonical} was not found; set ICCDEV_TOOLS_DIR, ICCDEV_BUILD_DIR, or PATH"
    )


def available_tools() -> dict[str, Path]:
    """Return the currently discoverable CLI-backed helper tools."""

    found: dict[str, Path] = {}
    for tool in _TOOLS:
        try:
            found[tool] = find_tool(tool)
        except IccToolError:
            continue
    return found


def _decode(data: bytes, encoding: str = "utf-8") -> str:
    return data.decode(encoding, errors="replace")


def _run_tool(tool: str, *args: os.PathLike[str] | str, timeout: int = 60) -> bytes:
    tool_path = find_tool(tool)
    command = tuple(str(part) for part in (tool_path, *args))
    kwargs: dict[str, Any] = {}
    if os.name == "nt":
        kwargs["creationflags"] = subprocess.CREATE_NO_WINDOW
    try:
        # Command is an argv tuple with a resolved executable and shell disabled.
        completed = subprocess.run(  # nosec B603
            command,
            check=False,
            env=_tool_environment(tool_path),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            **kwargs,
        )
    except subprocess.TimeoutExpired as exc:
        raise IccToolError(
            f"{tool} timed out after {timeout} seconds",
            command=command,
            returncode=124,
            stdout=_decode(exc.stdout or b""),
            stderr=_decode(exc.stderr or b""),
        ) from exc

    if completed.returncode != 0:
        raise IccToolError(
            f"{tool} failed with exit code {completed.returncode}",
            command=command,
            returncode=completed.returncode,
            stdout=_decode(completed.stdout),
            stderr=_decode(completed.stderr),
        )
    return completed.stdout + completed.stderr


def _input_path(work_dir: Path, value: bytes | str | os.PathLike[str], suffix: str) -> Path:
    if isinstance(value, bytes):
        path = work_dir / f"input{suffix}"
        path.write_bytes(value)
        return path

    path_value = Path(os.fspath(value))
    try:
        if path_value.exists():
            return path_value
    except OSError:
        pass

    path = work_dir / f"input{suffix}"
    path.write_text(str(value), encoding="utf-8")
    return path


def icc_to_xml(profile: bytes | str | os.PathLike[str], *, timeout: int = 60) -> str:
    """Convert an ICC profile path or ICC profile bytes to XML text using iccToXml."""

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, profile, ".icc")
        output_path = work_dir / "output.xml"
        _run_tool("iccToXml", input_path, output_path, timeout=timeout)
        return output_path.read_text(encoding="utf-8", errors="replace")


def icc_from_xml(xml: bytes | str | os.PathLike[str], *, timeout: int = 60) -> bytes:
    """Convert XML text, XML bytes, or an XML path to ICC profile bytes using iccFromXml."""

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, xml, ".xml")
        output_path = work_dir / "output.icc"
        _run_tool("iccFromXml", input_path, output_path, timeout=timeout)
        return output_path.read_bytes()


def icc_to_json(profile: bytes | str | os.PathLike[str], *, timeout: int = 60) -> str:
    """Convert an ICC profile path or ICC profile bytes to JSON text using iccToJson."""

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, profile, ".icc")
        output_path = work_dir / "output.json"
        _run_tool("iccToJson", input_path, output_path, timeout=timeout)
        return output_path.read_text(encoding="utf-8", errors="replace")


def icc_from_json(json_data: bytes | str | os.PathLike[str] | Any, *, timeout: int = 60) -> bytes:
    """Convert JSON text, JSON data, or a JSON path to ICC profile bytes using iccFromJson."""

    if not isinstance(json_data, (bytes, str, os.PathLike)):
        json_data = json.dumps(json_data, indent=2)

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, json_data, ".json")
        output_path = work_dir / "output.icc"
        _run_tool("iccFromJson", input_path, output_path, timeout=timeout)
        return output_path.read_bytes()


def dump_profile(
    profile: bytes | str | os.PathLike[str],
    mode: str = "ALL",
    *,
    timeout: int = 60,
) -> str:
    """Run iccDumpProfile for an ICC profile path or ICC profile bytes."""

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, profile, ".icc")
        return _decode(_run_tool("iccDumpProfile", input_path, mode, timeout=timeout))


def round_trip(profile: bytes | str | os.PathLike[str], *, timeout: int = 60) -> str:
    """Run iccRoundTrip for an ICC profile path or ICC profile bytes."""

    with tempfile.TemporaryDirectory(prefix="iccdev-cli-") as tmp:
        work_dir = Path(tmp)
        input_path = _input_path(work_dir, profile, ".icc")
        return _decode(_run_tool("iccRoundTrip", input_path, timeout=timeout))
