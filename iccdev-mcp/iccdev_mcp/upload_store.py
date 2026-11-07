# iccdev-mcp -- Bounded REST upload storage
#
# Copyright (c) International Color Consortium.
# BSD 3-Clause License. See LICENSE.md for details.

"""Private, quota-limited upload storage shared by REST worker processes."""

from __future__ import annotations

import os
import stat
import tempfile
import time
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator


class UploadQuotaExceeded(Exception):
    """The retained upload store has reached its byte limit."""


def default_upload_dir() -> Path:
    """Keep Windows service uploads under its private account profile."""
    if os.name == "nt":
        base = os.environ.get("LOCALAPPDATA")
        return Path(base) / "iccdev-mcp-uploads" if base else (
            Path.home() / "AppData" / "Local" / "iccdev-mcp-uploads"
        )
    return Path(tempfile.gettempdir()) / "iccdev-mcp-uploads"


@contextmanager
def _locked_store(directory: Path) -> Iterator[None]:
    directory.mkdir(mode=0o700, parents=True, exist_ok=True)
    directory_stat = directory.lstat()
    if not stat.S_ISDIR(directory_stat.st_mode):
        raise OSError("Upload directory is not a regular directory")
    if os.name == "nt":
        if (directory_stat.st_file_attributes &
                stat.FILE_ATTRIBUTE_REPARSE_POINT):
            raise OSError("Upload directory is a Windows reparse point")
    else:
        if directory_stat.st_uid != os.getuid():
            raise OSError("Upload directory has a different owner")
        directory.chmod(0o700)

    flags = os.O_RDWR | os.O_CREAT
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    fd = os.open(directory / ".lock", flags, 0o600)
    try:
        if os.name == "nt":
            import msvcrt

            if os.fstat(fd).st_size == 0:
                os.write(fd, b"\0")
            os.lseek(fd, 0, os.SEEK_SET)
            msvcrt.locking(fd, msvcrt.LK_LOCK, 1)
            try:
                yield
            finally:
                os.lseek(fd, 0, os.SEEK_SET)
                msvcrt.locking(fd, msvcrt.LK_UNLCK, 1)
        else:
            import fcntl

            fcntl.flock(fd, fcntl.LOCK_EX)
            try:
                yield
            finally:
                fcntl.flock(fd, fcntl.LOCK_UN)
    finally:
        os.close(fd)


def _retained_usage(directory: Path, retention_seconds: int) -> tuple[int, int]:
    """Remove expired owned files and count remaining bytes and files."""
    now = time.time()
    total = 0
    count = 0
    with os.scandir(directory) as entries:
        for entry in entries:
            if entry.name == ".lock":
                continue
            entry_stat = entry.stat(follow_symlinks=False)
            if not stat.S_ISREG(entry_stat.st_mode):
                raise OSError("Unexpected entry in upload directory")
            if now - entry_stat.st_mtime >= retention_seconds:
                os.unlink(entry.path)
            else:
                total += entry_stat.st_size
                count += 1
    return total, count


def store_upload(
    directory: Path,
    filename: str,
    content: bytes,
    *,
    max_total_bytes: int,
    max_file_count: int,
    retention_seconds: int,
) -> Path:
    """Atomically reserve quota and write a new private upload file."""
    with _locked_store(directory):
        retained_bytes, retained_files = _retained_usage(
            directory, retention_seconds
        )
        if (retained_bytes + len(content) > max_total_bytes or
                retained_files >= max_file_count):
            raise UploadQuotaExceeded

        dest = directory / filename
        flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
        if hasattr(os, "O_NOFOLLOW"):
            flags |= os.O_NOFOLLOW
        fd = os.open(dest, flags, 0o600)
        try:
            with os.fdopen(fd, "wb") as output:
                output.write(content)
        except BaseException:
            dest.unlink(missing_ok=True)
            raise
        return dest
