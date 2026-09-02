"""
setup.py - Build configuration for iccdev Python bindings.

Copyright (c) International Color Consortium.
BSD 3-Clause License. See LICENSE.md for details.

Install modes
-------------
    pip install iccdev          # from PyPI (wheel or sdist with bundled C++)
    pip install .               # from repo checkout
    pip install -e .            # editable install (development)

The build auto-detects whether IccProfLib2 is pre-built or needs to be
compiled from bundled sources.  Set ICCDEV_BUILD_DIR to point at an
existing build tree if you want to link against it; otherwise the portable
IccProfLib .cpp files are compiled as part of the extension.
"""

import os
import hashlib
import shutil
import sys
import sysconfig
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext as _build_ext
from setuptools.command.sdist import sdist as _sdist


# ---------------------------------------------------------------------------
# Paths - setuptools requires /-separated paths relative to setup.py dir
# ---------------------------------------------------------------------------

SETUP_DIR = os.path.abspath(os.path.dirname(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SETUP_DIR, ".."))
_AUTO_VENDORED_ICCPROFLIB = False

if sys.platform == "win32" and shutil.which("cl.exe"):
    os.environ.setdefault("DISTUTILS_USE_SDK", "1")
    os.environ.setdefault("MSSdk", "1")


def _relpath(abspath):
    """Convert an absolute path to a /-separated relative path from setup.py."""
    return os.path.relpath(abspath, SETUP_DIR).replace(os.sep, "/")


def _vendor_iccproflib_sources():
    """Copy IccProfLib sources into python/vendor and return True if created."""

    repo_src = os.path.join(REPO_ROOT, "IccProfLib")
    vendor_dir = os.path.join(SETUP_DIR, "vendor", "IccProfLib")
    manifest_path = os.path.join(vendor_dir, "MANIFEST.sha256")
    if os.path.isdir(vendor_dir) and os.path.isfile(manifest_path):
        return False
    if os.path.isdir(vendor_dir):
        shutil.rmtree(vendor_dir)
    if not os.path.isdir(repo_src):
        raise RuntimeError(
            "Cannot build an sdist without IccProfLib sources. Run from a full "
            "iccDEV checkout or use an existing sdist."
        )

    os.makedirs(vendor_dir, exist_ok=True)
    manifest_rows = []
    for name in sorted(os.listdir(repo_src)):
        if name.endswith((".cpp", ".h")):
            src = os.path.join(repo_src, name)
            dst = os.path.join(vendor_dir, name)
            shutil.copy2(src, dst)
            with open(dst, "rb") as f:
                manifest_rows.append(f"{hashlib.sha256(f.read()).hexdigest()}  {name}\n")
    with open(manifest_path, "w", encoding="ascii", newline="\n") as f:
        f.writelines(manifest_rows)
    return True


def _remove_empty_dir(path):
    try:
        os.rmdir(path)
    except OSError:
        pass


def _iccproflib_cpp_sources(src_dir):
    optional_simd_sources = {
        "IccTagLutAvx2.cpp",
        "IccTagLutAvx512.cpp",
    }
    return sorted(
        _relpath(os.path.join(src_dir, f))
        for f in os.listdir(src_dir)
        if f.endswith(".cpp") and f not in optional_simd_sources
    )


# IccProfLib source headers - try repo layout, then sdist vendored copy
_iccproflib_abs = os.path.join(REPO_ROOT, "IccProfLib")
if not os.path.isdir(_iccproflib_abs):
    _iccproflib_abs = os.path.join(SETUP_DIR, "vendor", "IccProfLib")

ICCPROFLIB_SRC = _iccproflib_abs
ICCPROFLIB_INCLUDE = _relpath(_iccproflib_abs)

# Check for pre-built library (developer / CI mode)
BUILD_DIR_ENV = os.environ.get("ICCDEV_BUILD_DIR")
BUILD_DIR = BUILD_DIR_ENV or os.path.join(REPO_ROOT, "Build")


def _candidate_lib_dirs(build_dir):
    return [
        os.path.join(build_dir, "IccProfLib", "Release"),
        os.path.join(build_dir, "Release"),
        os.path.join(build_dir, "IccProfLib"),
        os.path.join(build_dir, "lib"),
        build_dir,
    ]


def _candidate_debug_lib_dirs(build_dir):
    return [
        os.path.join(build_dir, "IccProfLib", "Debug"),
        os.path.join(build_dir, "Debug"),
    ]


def _read_cmake_cache_value(build_dir, key):
    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_path):
        return None

    prefix = key + ":"
    with open(cache_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not line.startswith(prefix):
                continue
            _typed_key, value = line.rstrip("\n").split("=", 1)
            return value or None
    return None


def _read_cmake_cache_bool(build_dir, key):
    value = _read_cmake_cache_value(build_dir, key)
    if value is None:
        return None
    return value.strip().upper() not in {
        "",
        "0",
        "FALSE",
        "IGNORE",
        "N",
        "NO",
        "NOTFOUND",
        "OFF",
    }


def _candidate_vcpkg_lib_dirs(build_dir):
    roots = []
    installed_dir = _read_cmake_cache_value(build_dir, "VCPKG_INSTALLED_DIR")
    target_triplet = _read_cmake_cache_value(build_dir, "VCPKG_TARGET_TRIPLET")

    if installed_dir:
        roots.append(installed_dir)
    roots.append(os.path.join(build_dir, "vcpkg_installed"))
    roots.append(os.path.join(REPO_ROOT, "vcpkg_installed"))
    roots.append(os.path.join(REPO_ROOT, "installed"))

    triplets = []
    if target_triplet:
        triplets.append(target_triplet)
    triplets.extend(["x64-windows", "x64-windows-static", "x64-windows-static-md"])

    lib_dirs = []
    for root in roots:
        for triplet in triplets:
            for subdir in ("lib", os.path.join("debug", "lib")):
                candidate = os.path.join(root, triplet, subdir)
                if os.path.isdir(candidate) and candidate not in lib_dirs:
                    lib_dirs.append(candidate)
    return lib_dirs


def _candidate_zlib_lib_dirs(build_dir):
    lib_dirs = []
    for key in ("ZLIB_LIBRARY_RELEASE", "ZLIB_LIBRARY"):
        zlib_path = _read_cmake_cache_value(build_dir, key)
        if not zlib_path:
            continue
        for part in zlib_path.split(";"):
            if part.lower() in ("debug", "optimized", "general"):
                continue
            candidate = os.path.dirname(part)
            if os.path.isdir(candidate) and candidate not in lib_dirs:
                lib_dirs.append(candidate)
    for candidate in _candidate_vcpkg_lib_dirs(build_dir):
        if candidate not in lib_dirs:
            lib_dirs.append(candidate)
    return lib_dirs


def _find_zlib_library(build_dir):
    for key in ("ZLIB_LIBRARY_RELEASE", "ZLIB_LIBRARY"):
        zlib_path = _read_cmake_cache_value(build_dir, key)
        if not zlib_path:
            continue
        for part in zlib_path.split(";"):
            if part.lower() in ("debug", "optimized", "general"):
                continue
            if os.path.isfile(part):
                return part

    for lib_dir in _candidate_zlib_lib_dirs(build_dir):
        for name in ("zlib.lib", "zlibstatic.lib", "zlib1.lib"):
            candidate = os.path.join(lib_dir, name)
            if os.path.isfile(candidate):
                return candidate
    return None


def _find_zlib_runtime_dll(build_dir):
    lib_dirs = _candidate_zlib_lib_dirs(build_dir)
    runtime_dirs = []

    for lib_dir in lib_dirs:
        norm = os.path.normpath(lib_dir)
        parent = os.path.dirname(norm)
        if os.path.basename(norm).lower() == "lib":
            runtime_dirs.append(os.path.join(parent, "bin"))
        if os.path.basename(parent).lower() == "debug":
            runtime_dirs.append(os.path.join(os.path.dirname(parent), "debug", "bin"))

    installed_dir = _read_cmake_cache_value(build_dir, "VCPKG_INSTALLED_DIR")
    target_triplet = _read_cmake_cache_value(build_dir, "VCPKG_TARGET_TRIPLET")
    roots = [
        root for root in (
            installed_dir,
            os.path.join(build_dir, "vcpkg_installed"),
            os.path.join(REPO_ROOT, "vcpkg_installed"),
            os.path.join(REPO_ROOT, "installed"),
        )
        if root
    ]
    triplets = [triplet for triplet in (target_triplet, "x64-windows") if triplet]
    for root in roots:
        for triplet in triplets:
            runtime_dirs.append(os.path.join(root, triplet, "bin"))

    for runtime_dir in runtime_dirs:
        for name in ("zlib1.dll", "zlib.dll"):
            candidate = os.path.join(runtime_dir, name)
            if os.path.isfile(candidate):
                return candidate
    return None


_fallback_lib_search_dirs = [
    os.path.join(REPO_ROOT, "out", "vs2022-vcpkg", "IccProfLib", "Release"),
    os.path.join(REPO_ROOT, "out", "ninja-vcpkg-release", "IccProfLib"),
]
_lib_search_dirs = _candidate_lib_dirs(BUILD_DIR)
if not BUILD_DIR_ENV:
    _lib_search_dirs.extend(_fallback_lib_search_dirs)
LIB_DIRS = [d for d in _lib_search_dirs if os.path.isdir(d)]


def _is_debug_python():
    return bool(
        getattr(sys, "gettotalrefcount", None)
        or sysconfig.get_config_var("Py_DEBUG")
        or sys.executable.lower().endswith("_d.exe")
    )


def _find_prebuilt_lib():
    """Return (library name, library dir, error) for a compatible pre-built lib."""

    release_names = (
        ("libIccProfLib2-static.a", "IccProfLib2-static"),
        ("IccProfLib2-static.lib", "IccProfLib2-static"),
    )
    debug_names = (
        ("libIccProfLib2-staticd.a", "IccProfLib2-staticd"),
        ("IccProfLib2-staticd.lib", "IccProfLib2-staticd"),
    )
    debug_requested = (
        _is_debug_python()
        or os.environ.get("ICCDEV_ALLOW_DEBUG_PYTHON_LIB") == "1"
    )
    if debug_requested:
        names = debug_names
        search_dirs = [
            d for d in _candidate_debug_lib_dirs(BUILD_DIR) + _candidate_lib_dirs(BUILD_DIR)
            if os.path.isdir(d)
        ]
        if not BUILD_DIR_ENV:
            search_dirs.extend(LIB_DIRS)
    else:
        names = release_names
        search_dirs = LIB_DIRS

    for d in search_dirs:
        for filename, library in names:
            if os.path.isfile(os.path.join(d, filename)):
                return library, d, None

    if BUILD_DIR_ENV and not debug_requested:
        debug_hits = []
        for d in _candidate_debug_lib_dirs(BUILD_DIR):
            for filename, _library in debug_names:
                path = os.path.join(d, filename)
                if os.path.isfile(path):
                    debug_hits.append(path)
        if debug_hits:
            return None, None, (
                "ICCDEV_BUILD_DIR points to a Debug-only IccProfLib build, but "
                "this Python interpreter is building a release extension. Build "
                "iccDEV Release so IccProfLib2-static.lib is available, set "
                "ICCDEV_BUILD_DIR to the Release build root, or use a debug "
                "Python interpreter. Found: " + ", ".join(debug_hits)
            )

    return None, None, None


# ---------------------------------------------------------------------------
# Source collection - either link pre-built or compile from source
# ---------------------------------------------------------------------------

PREBUILT_LIBRARY, PREBUILT_LIB_DIR, PREBUILT_ERROR = _find_prebuilt_lib()

extra_sources = []
libraries = []
prebuilt_extra_link_args = []
prebuilt_runtime_dlls = []

if PREBUILT_LIBRARY:
    # Developer / CI mode: link against the pre-built static library
    libraries = [PREBUILT_LIBRARY]
    LIB_DIRS = [PREBUILT_LIB_DIR]
    if _read_cmake_cache_bool(BUILD_DIR, "ICC_USE_ZLIB") is not False:
        if sys.platform == "win32":
            zlib_library = _find_zlib_library(BUILD_DIR)
            if zlib_library:
                prebuilt_extra_link_args.append(zlib_library)
                zlib_runtime_dll = _find_zlib_runtime_dll(BUILD_DIR)
                if zlib_runtime_dll:
                    prebuilt_runtime_dlls.append(zlib_runtime_dll)
            else:
                libraries.append("zlib")
                LIB_DIRS.extend(_candidate_zlib_lib_dirs(BUILD_DIR))
                zlib_runtime_dll = _find_zlib_runtime_dll(BUILD_DIR)
                if zlib_runtime_dll:
                    prebuilt_runtime_dlls.append(zlib_runtime_dll)
        else:
            libraries.append("z")
    # IccWrapper.cpp is part of IccProfLib2 in current CMake builds.
    extra_sources = []
else:
    # Standalone / PyPI mode: compile all IccProfLib sources inline.
    # Source checkout builds vendor just-in-time in build_ext so metadata and
    # sdist preparation do not leave generated files behind.
    vendor_dir = os.path.join(SETUP_DIR, "vendor", "IccProfLib")
    repo_src = os.path.join(REPO_ROOT, "IccProfLib")

    if os.path.isdir(vendor_dir):
        if not os.path.isfile(os.path.join(vendor_dir, "MANIFEST.sha256")):
            _AUTO_VENDORED_ICCPROFLIB = _vendor_iccproflib_sources()
        src_dir = vendor_dir
        ICCPROFLIB_INCLUDE = _relpath(src_dir)
    elif os.path.isdir(repo_src):
        src_dir = repo_src
        ICCPROFLIB_INCLUDE = _relpath(src_dir)
    else:
        raise RuntimeError(
            "Cannot find IccProfLib source directory.  Either install from "
            "a proper sdist/wheel, or set ICCDEV_BUILD_DIR to point at a "
            "pre-built IccProfLib2 static library."
        )

    extra_sources = _iccproflib_cpp_sources(src_dir) if os.path.isdir(vendor_dir) else []
    LIB_DIRS = []  # no external lib dirs needed


# ---------------------------------------------------------------------------
# Platform-specific compiler / linker flags
# ---------------------------------------------------------------------------

extra_compile_args = []
extra_link_args = []
define_macros = []

if sys.platform == "win32":
    extra_compile_args = ["/std:c++17", "/EHsc", "/GS", "/DYNAMICBASE"]
    extra_link_args = prebuilt_extra_link_args
    define_macros = [("WIN32", "1")]
elif sys.platform == "darwin":
    extra_compile_args = ["-std=c++17", "-stdlib=libc++",
                          "-fstack-protector-strong", "-D_FORTIFY_SOURCE=2",
                          "-Wno-unreachable-code-fallthrough"]
    extra_link_args = ["-stdlib=libc++"]
else:
    extra_compile_args = ["-std=c++17",
                          "-fstack-protector-strong", "-D_FORTIFY_SOURCE=2"]
    extra_link_args = ["-Wl,-z,relro", "-Wl,-z,now"]


# ---------------------------------------------------------------------------
# Extension module
# ---------------------------------------------------------------------------

ext_source = os.path.join("iccdev", "_iccdev.pyx")

extensions = [
    Extension(
        name="iccdev._iccdev",
        sources=[ext_source] + extra_sources,
        include_dirs=[ICCPROFLIB_INCLUDE],
        library_dirs=LIB_DIRS,
        libraries=libraries,
        language="c++",
        extra_compile_args=extra_compile_args,
        extra_link_args=extra_link_args,
        define_macros=define_macros,
    ),
]

class build_ext(_build_ext):
    """Cythonize only for extension builds so sdists ship .pyx, not .cpp."""

    def build_extensions(self):
        global _AUTO_VENDORED_ICCPROFLIB

        if PREBUILT_ERROR:
            raise RuntimeError(PREBUILT_ERROR)

        if not PREBUILT_LIBRARY:
            vendor_dir = os.path.join(SETUP_DIR, "vendor", "IccProfLib")
            if not os.path.isdir(vendor_dir) or not os.path.isfile(
                os.path.join(vendor_dir, "MANIFEST.sha256")
            ):
                _AUTO_VENDORED_ICCPROFLIB = _vendor_iccproflib_sources()
            vendor_include = _relpath(vendor_dir)
            vendor_sources = _iccproflib_cpp_sources(vendor_dir)
            for ext in self.extensions:
                if ext.name != "iccdev._iccdev":
                    continue
                ext.include_dirs = [
                    vendor_include if path in (ICCPROFLIB_INCLUDE, _relpath(os.path.join(REPO_ROOT, "IccProfLib"))) else path
                    for path in ext.include_dirs
                ]
                ext.sources = [
                    source
                    for source in ext.sources
                    if not source.startswith("vendor/IccProfLib/")
                    and not source.startswith("../IccProfLib/")
                ] + vendor_sources

        try:
            from Cython.Build import cythonize
        except ImportError as exc:
            raise RuntimeError(
                "Cython is required to build iccdev from source. "
                "Install a pre-built wheel, or install build dependencies with: "
                "pip install 'cython>=3.0,<3.2'"
            ) from exc

        self.extensions = cythonize(
            self.extensions,
            compiler_directives={
                "language_level": "3",
                "boundscheck": True,
                "wraparound": False,
                "embedsignature": True,
            },
        )
        for ext in self.extensions:
            if not hasattr(ext, "_needs_stub"):
                ext._needs_stub = False
        try:
            super().build_extensions()
            for runtime_dll in prebuilt_runtime_dlls:
                package_dir = os.path.dirname(self.get_ext_fullpath("iccdev._iccdev"))
                os.makedirs(package_dir, exist_ok=True)
                shutil.copy2(runtime_dll, os.path.join(package_dir, os.path.basename(runtime_dll)))
        finally:
            if _AUTO_VENDORED_ICCPROFLIB:
                shutil.rmtree(
                    os.path.join(SETUP_DIR, "vendor", "IccProfLib"),
                    ignore_errors=True,
                )
                _remove_empty_dir(os.path.join(SETUP_DIR, "vendor"))


class sdist(_sdist):
    """Create self-contained sdists even when a pre-built library is present."""

    def run(self):
        vendor_dir = os.path.join(SETUP_DIR, "vendor", "IccProfLib")
        leaked_source_dir = os.path.join(SETUP_DIR, "IccProfLib")
        sources_manifest = os.path.join(SETUP_DIR, "iccdev.egg-info", "SOURCES.txt")
        if os.path.isfile(sources_manifest):
            os.remove(sources_manifest)
        created_vendor = _vendor_iccproflib_sources()
        original_ext_state = []
        vendor_include = _relpath(vendor_dir)
        vendor_sources = _iccproflib_cpp_sources(vendor_dir)
        repo_include = _relpath(os.path.join(REPO_ROOT, "IccProfLib"))
        for ext in self.distribution.ext_modules or []:
            original_ext_state.append(
                (
                    ext,
                    list(ext.sources),
                    list(ext.include_dirs or []),
                    list(ext.libraries or []),
                    list(ext.library_dirs or []),
                )
            )
            if ext.name != "iccdev._iccdev":
                continue
            ext.include_dirs = [
                vendor_include if path in (ICCPROFLIB_INCLUDE, repo_include) else path
                for path in (ext.include_dirs or [])
            ]
            ext.sources = [
                source
                for source in ext.sources
                if not source.startswith("../IccProfLib/")
                and not source.startswith("vendor/IccProfLib/")
            ] + vendor_sources
            ext.libraries = []
            ext.library_dirs = []
        try:
            super().run()
        finally:
            for ext, sources, include_dirs, libraries, library_dirs in original_ext_state:
                ext.sources = sources
                ext.include_dirs = include_dirs
                ext.libraries = libraries
                ext.library_dirs = library_dirs
            shutil.rmtree(leaked_source_dir, ignore_errors=True)
            if created_vendor or _AUTO_VENDORED_ICCPROFLIB:
                shutil.rmtree(vendor_dir, ignore_errors=True)
                _remove_empty_dir(os.path.join(SETUP_DIR, "vendor"))


# ---------------------------------------------------------------------------
# Package metadata lives in pyproject.toml.  Keep setup.py focused on the
# dynamic extension build so setuptools does not emit overwrite warnings.
# ---------------------------------------------------------------------------

with open(os.path.join(os.path.dirname(__file__), "README.md"), encoding="utf-8") as f:
    long_description = f.read()

setup(
    name="iccdev",
    version="0.1.0",
    description="Python bindings for RefIccMAX (iccDEV) - ICC color profile library",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/InternationalColorConsortium/iccDEV",
    author="International Color Consortium",
    packages=find_packages(),
    ext_modules=extensions,
    cmdclass={"build_ext": build_ext, "sdist": sdist},
    python_requires=">=3.9",
    install_requires=[],
    keywords="icc color profile cmm color-management cython",
    package_data={
        "iccdev": ["py.typed", "_iccdev.pyi", "*.pxd", "*.dll"],
    },
)
