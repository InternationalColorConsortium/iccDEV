# iccdev-mcp -- Profile path resolution
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""
Resolve ICC profile paths from filenames, directory-qualified paths,
or absolute paths. Searches standard iccDEV locations.
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Optional


def _find_testing_dir() -> Optional[Path]:
    """Locate the iccDEV Testing/ directory."""
    # 1. Environment variable
    env_dir = os.environ.get("ICCDEV_TESTING_DIR")
    if env_dir:
        p = Path(env_dir)
        if p.is_dir():
            return p

    # 2. Relative to this package (iccDEV/iccdev-mcp/iccdev_mcp/)
    pkg_dir = Path(__file__).resolve().parent
    candidates = [
        pkg_dir.parent.parent / "Testing",      # iccDEV/Testing
        pkg_dir.parent.parent.parent / "Testing",  # one more level up
    ]
    for c in candidates:
        if c.is_dir():
            return c

    return None


def _find_profile_dirs() -> list[Path]:
    """Return ordered list of directories to search for profiles."""
    dirs: list[Path] = []

    testing = _find_testing_dir()
    if testing:
        dirs.append(testing)
        testing_root = testing.resolve()
        # Search the whole Testing/ tree; some suites keep profiles under
        # nested directories such as hybrid/ICC and mcs/Flexo-CMYKOGP.
        for sub in sorted(p for p in testing.rglob("*") if p.is_dir()):
            if sub.is_dir() and not sub.name.startswith("."):
                try:
                    sub.resolve().relative_to(testing_root)
                except ValueError:
                    continue
                dirs.append(sub)

    # Additional profile directories from environment
    dirs.extend(_find_extra_profile_dirs())

    return dirs


def _find_extra_profile_dirs() -> list[Path]:
    """Return explicit profile roots from ICCDEV_PROFILE_DIRS."""
    dirs: list[Path] = []
    extra = os.environ.get("ICCDEV_PROFILE_DIRS")
    if extra:
        for d in extra.split(os.pathsep):
            p = Path(d)
            if p.is_dir():
                dirs.append(p)
    return dirs


def _get_allowed_bases() -> list[Path]:
    """Return directories that are allowed for file access."""
    import tempfile

    bases: list[Path] = []

    testing = _find_testing_dir()
    if testing:
        bases.append(testing.resolve())

    # Upload temp directory
    upload_dir = Path(tempfile.gettempdir()) / "iccdev-mcp-uploads"
    bases.append(upload_dir.resolve())

    # System temp for tool outputs
    bases.append(Path(tempfile.gettempdir()).resolve())

    # CWD and profile dirs
    bases.append(Path.cwd().resolve())
    for d in _find_extra_profile_dirs():
        bases.append(d.resolve())

    return bases


def _is_under_allowed_base(resolved: Path) -> bool:
    """Check if resolved path is under an allowed base directory."""
    for base in _get_allowed_bases():
        try:
            resolved.relative_to(base)
            return True
        except ValueError:
            continue
    return False


def _is_under_base(resolved: Path, base: Path) -> bool:
    """Check if resolved path is under a specific resolved base directory."""
    try:
        resolved.relative_to(base.resolve())
        return True
    except ValueError:
        return False


def _subdirectories_under(base: Path) -> list[Path]:
    """Return real subdirectories whose resolved paths stay under base."""
    resolved_base = base.resolve()
    directories: list[Path] = []
    for candidate in sorted(p for p in base.rglob("*") if p.is_dir()):
        if _is_under_base(candidate.resolve(), resolved_base):
            directories.append(candidate)
    return directories


def resolve_profile_path(path: str) -> Path:
    """Resolve a profile path string to an absolute Path.

    Accepts:
    - Absolute path: /path/to/profile.icc
    - Relative path: subdir/profile.icc
    - Filename only: sRGB.icc (searched in Testing/ tree)

    Raises:
        FileNotFoundError: If the profile cannot be found.
        ValueError: If path contains null bytes or traversal.
    """
    if "\x00" in path:
        raise ValueError("Path contains null bytes")

    if ".." in Path(path).parts:
        raise ValueError("Path traversal (..) not allowed")

    p = Path(path)

    # Absolute path -- must be under allowed base
    if p.is_absolute():
        resolved = p.resolve()
        if not resolved.is_file():
            raise FileNotFoundError(f"Profile not found: {path}")
        if not _is_under_allowed_base(resolved):
            raise ValueError(
                "Access denied: path is outside allowed directories"
            )
        return resolved

    # Relative or filename -- search profile directories
    for search_dir in _find_profile_dirs():
        candidate = (search_dir / p).resolve()
        if candidate.is_file() and _is_under_allowed_base(candidate):
            return candidate
        if candidate.is_file():
            raise ValueError(
                "Access denied: path is outside allowed directories"
            )

    # Try current directory as last resort
    cwd_candidate = (Path.cwd() / p).resolve()
    if cwd_candidate.is_file() and _is_under_allowed_base(cwd_candidate):
        return cwd_candidate
    if cwd_candidate.is_file():
        raise ValueError(
            "Access denied: path is outside allowed directories"
        )

    raise FileNotFoundError(
        f"Profile not found: {path}. "
        f"Searched: {', '.join(str(d) for d in _find_profile_dirs()) or 'no directories configured'}. "
        f"Set ICCDEV_TESTING_DIR or use an absolute path."
    )


def list_profiles(directory: Optional[str] = None) -> list[dict]:
    """List available ICC profiles.

    Args:
        directory: Optional subdirectory name to list. If None, lists
                   all profiles in the Testing/ tree.

    Returns:
        List of dicts with 'name', 'path', and 'size' keys.
    """
    results: list[dict] = []

    if directory:
        if "\x00" in directory or ".." in Path(directory).parts:
            return results
        testing = _find_testing_dir()
        if not testing:
            return results
        target = (testing / directory).resolve()
        # Boundary check: must remain under Testing/
        try:
            target.relative_to(testing.resolve())
        except ValueError:
            return results
        if not target.is_dir():
            return results
        dirs_to_scan = [target, *_subdirectories_under(target)]
    else:
        dirs_to_scan = _find_profile_dirs()

    seen: set[Path] = set()
    for d in dirs_to_scan:
        scan_root = d.resolve()
        for ext in ("*.icc", "*.icm", "*.ICC", "*.ICM"):
            for f in d.glob(ext):
                resolved = f.resolve()
                if (
                    resolved not in seen
                    and resolved.is_file()
                    and _is_under_base(resolved, scan_root)
                    and _is_under_allowed_base(resolved)
                ):
                    seen.add(resolved)
                    results.append({
                        "name": f.name,
                        "path": str(resolved),
                        "size": resolved.stat().st_size,
                    })

    results.sort(key=lambda x: x["name"].lower())
    return results


def _count_matching_files(directory: Path, extensions: tuple[str, ...]) -> int:
    """Count files matching extensions below a Testing/ directory."""
    count = 0
    root = directory.resolve()
    for ext in extensions:
        count += sum(
            1 for candidate in directory.rglob(ext)
            if _is_under_base(candidate.resolve(), root)
        )
    return count


def _matching_directory_counts(
    testing: Path,
    extensions: tuple[str, ...],
) -> dict[Path, int]:
    """Count matching files once, attributing each file to ancestor dirs."""
    root = testing.resolve()
    counts: dict[Path, int] = {}
    for ext in extensions:
        for candidate in testing.rglob(ext):
            resolved = candidate.resolve()
            if not resolved.is_file() or not _is_under_base(resolved, root):
                continue
            try:
                rel_parent = resolved.parent.relative_to(root)
            except ValueError:
                continue
            current = root
            counts[current] = counts.get(current, 0) + 1
            for part in rel_parent.parts:
                current = current / part
                counts[current] = counts.get(current, 0) + 1
    return counts


def _list_testing_directories(extensions: tuple[str, ...]) -> list[dict]:
    """List Testing/ directories that contain files with the given extensions."""
    testing = _find_testing_dir()
    if not testing:
        return []

    results: list[dict] = []
    counts = _matching_directory_counts(testing, extensions)
    for directory, count in counts.items():
        if directory == testing:
            name = ""
            label = "Testing root"
        else:
            name = directory.relative_to(testing).as_posix()
            label = name
        results.append({
            "name": name,
            "label": label,
            "path": str(directory.resolve()),
            "count": count,
        })

    results.sort(key=lambda x: (x["name"] != "", x["label"].lower()))
    return results


def list_profile_directories() -> list[dict]:
    """List Testing/ directories that contain ICC profiles."""
    return _list_testing_directories(("*.icc", "*.icm", "*.ICC", "*.ICM"))


def list_file_directories(extensions: tuple[str, ...]) -> list[dict]:
    """List Testing/ directories that contain files matching extensions."""
    return _list_testing_directories(extensions)


def list_files(directory: Optional[str], extensions: tuple[str, ...]) -> list[dict]:
    """List files in Testing/ matching the provided glob extensions."""
    testing = _find_testing_dir()
    if not testing:
        return []

    if directory:
        if "\x00" in directory or ".." in Path(directory).parts:
            return []
        target = (testing / directory).resolve()
        try:
            target.relative_to(testing.resolve())
        except ValueError:
            return []
        if not target.is_dir():
            return []
        dirs_to_scan = [target, *_subdirectories_under(target)]
    else:
        dirs_to_scan = [testing, *_subdirectories_under(testing)]

    results: list[dict] = []
    seen: set[Path] = set()
    for d in dirs_to_scan:
        scan_root = d.resolve()
        for ext in extensions:
            for f in d.glob(ext):
                resolved = f.resolve()
                if (
                    resolved not in seen
                    and resolved.is_file()
                    and _is_under_base(resolved, scan_root)
                    and _is_under_allowed_base(resolved)
                ):
                    seen.add(resolved)
                    results.append({
                        "name": f.name,
                        "path": str(resolved),
                        "directory": (
                            "" if d.resolve() == testing.resolve()
                            else d.resolve().relative_to(testing.resolve()).as_posix()
                        ),
                        "size": resolved.stat().st_size,
                    })

    results.sort(key=lambda x: (x["directory"].lower(), x["name"].lower()))
    return results
