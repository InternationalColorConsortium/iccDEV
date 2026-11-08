#!/usr/bin/env python3
# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Fail-closed, per-package eligibility for manual downstream OIDC publication.

The GitHub rolling release is independent of these decisions. This helper only
reads public registry metadata, verifies each existing package's provenance,
and emits fixed decisions. It never obtains an OIDC token or publishes a file.
"""

from __future__ import annotations

import argparse
import base64
import binascii
from dataclasses import asdict, dataclass
from datetime import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import tomllib
from typing import Any, Callable
from urllib.parse import quote, urlparse
from urllib.request import Request, urlopen


REPOSITORY = "InternationalColorConsortium/iccDEV"
WORKFLOW = "ci-latest-release.yml"
WORKFLOW_URI = f"https://github.com/{REPOSITORY}/.github/workflows/{WORKFLOW}@refs/heads/master"
PYPI_ORIGIN = "https://pypi.org"
NPM_ORIGIN = "https://registry.npmjs.org"
SHA_RE = re.compile(r"[0-9a-f]{40}\Z")
SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
MAX_JSON_BYTES = 8 * 1024 * 1024
MAX_NPM_COMPONENT = (1 << 53) - 1


class GateError(Exception):
    """Expected denial with a fixed, output-safe reason code."""

    def __init__(self, reason: str):
        super().__init__(reason)
        self.reason = reason


@dataclass(frozen=True)
class Decision:
    requested: bool
    eligible: bool
    reason: str
    target_version: str = ""
    baseline_version: str = ""
    baseline_core_version: str = ""
    baseline_source_sha: str = ""


def fetch_json(url: str) -> dict[str, Any]:
    """Read a bounded public registry response; never follow a foreign host."""
    host = urlparse(url).hostname
    if host not in {"pypi.org", "registry.npmjs.org"}:
        raise GateError("registry-unavailable")
    try:
        request = Request(url, headers={"Accept": "application/json", "User-Agent": "iccdev-oidc-gate/1"})
        with urlopen(request, timeout=20) as response:
            if urlparse(response.url).hostname != host:
                raise GateError("registry-unavailable")
            data = response.read(MAX_JSON_BYTES + 1)
        if len(data) > MAX_JSON_BYTES:
            raise GateError("registry-unavailable")
        parsed = json.loads(data)
        if not isinstance(parsed, dict):
            raise ValueError("Registry response must be an object")
        return parsed
    except GateError:
        raise
    except (OSError, TimeoutError, ValueError) as exc:
        raise GateError("registry-unavailable") from exc


def core_version_from_text(source: str) -> tuple[int, int, int, int]:
    parts = []
    for component in ("MAJOR", "MINOR", "MICRO", "PATCH"):
        pattern = rf"(?m)^\s*set\(\$\{{PROJECT_UP_NAME\}}_{component}_VERSION\s+([0-9]+)\s*\)"
        matches = re.findall(pattern, source)
        if len(matches) != 1:
            raise GateError("invalid-core-version")
        parts.append(int(matches[0]))
    return tuple(parts)  # type: ignore[return-value]


def format_core(version: tuple[int, int, int, int]) -> str:
    return ".".join(str(part) for part in version)


def source_versions(repo_root: Path) -> tuple[tuple[int, int, int, int], str, str]:
    try:
        core = core_version_from_text((repo_root / "Build/Cmake/CMakeLists.txt").read_text(encoding="utf-8"))
        pyproject = tomllib.loads((repo_root / "python/pyproject.toml").read_text(encoding="utf-8"))
        iccdev = pyproject["project"]["version"]
        mcp_text = (repo_root / "iccdev-mcp/iccdev_mcp/__init__.py").read_text(encoding="utf-8")
        matches = re.findall(r'(?m)^__version__ = "([^"]+)"$', mcp_text)
        if len(matches) != 1 or not isinstance(iccdev, str):
            raise ValueError("Package version metadata is ambiguous")
        return core, iccdev, matches[0]
    except (OSError, KeyError, TypeError, ValueError) as exc:
        raise GateError("invalid-source-version") from exc


def core_version_at_sha(repo_root: Path, sha: str) -> tuple[int, int, int, int]:
    if not SHA_RE.fullmatch(sha):
        raise GateError("invalid-provenance")
    ancestor = subprocess.run(
        ["git", "merge-base", "--is-ancestor", sha, "HEAD"],
        cwd=repo_root, capture_output=True, text=True, timeout=20, check=False,
    )
    if ancestor.returncode != 0:
        raise GateError("untrusted-source-commit")
    shown = subprocess.run(
        ["git", "show", f"{sha}:Build/Cmake/CMakeLists.txt"],
        cwd=repo_root, capture_output=True, text=True, timeout=20, check=False,
    )
    if shown.returncode != 0:
        raise GateError("untrusted-source-commit")
    return core_version_from_text(shown.stdout)


def _der_utf8(value: bytes) -> str:
    """Decode the Fulcio UTF8String extensions without trusting arbitrary ASN.1."""
    if len(value) < 2 or value[0] != 0x0C:
        raise GateError("invalid-provenance")
    length = value[1]
    offset = 2
    if length & 0x80:
        count = length & 0x7F
        if count == 0 or count > 2 or len(value) < offset + count:
            raise GateError("invalid-provenance")
        length = int.from_bytes(value[offset:offset + count], "big")
        offset += count
    if offset + length != len(value):
        raise GateError("invalid-provenance")
    try:
        return value[offset:].decode("utf-8")
    except UnicodeDecodeError as exc:
        raise GateError("invalid-provenance") from exc


def _certificate_value(cert: Any, suffix: str, *, legacy: bool = False) -> str | None:
    from cryptography.x509 import ObjectIdentifier
    from cryptography.x509.extensions import ExtensionNotFound

    try:
        raw = cert.extensions.get_extension_for_oid(ObjectIdentifier(f"1.3.6.1.4.1.57264.1.{suffix}")).value.value
    except ExtensionNotFound:
        return None
    try:
        return raw.decode("ascii") if legacy else _der_utf8(raw)
    except (UnicodeDecodeError, AttributeError) as exc:
        raise GateError("invalid-provenance") from exc


def verify_pypi_file(project: str, version: str, entry: dict[str, Any],
                     get_json: Callable[[str], dict[str, Any]] = fetch_json) -> str:
    """Cryptographically verify one PEP 740 file and return its source SHA."""
    try:
        from cryptography import x509
        from pypi_attestations import Distribution, GitHubPublisher, Provenance
    except ImportError as exc:
        raise GateError("verifier-unavailable") from exc

    name = entry.get("filename")
    digest = entry.get("digests", {}).get("sha256")
    if (not isinstance(name, str) or not name or "/" in name or "\\" in name or
        not isinstance(digest, str) or not SHA256_RE.fullmatch(digest)):
        raise GateError("invalid-registry-inventory")
    provenance_url = f"{PYPI_ORIGIN}/integrity/{project}/{quote(version, safe='')}/{quote(name, safe='')}/provenance"
    try:
        provenance = Provenance.model_validate(get_json(provenance_url))
        expected = GitHubPublisher(repository=REPOSITORY, workflow=WORKFLOW, environment="release")
        distribution = Distribution(name=name, digest=digest)
        shas = set()
        attestation_count = 0
        for bundle in provenance.attestation_bundles:
            if bundle.publisher != expected:
                raise GateError("invalid-provenance")
            for attestation in bundle.attestations:
                predicate, _ = attestation.verify(identity=expected, dist=distribution)
                if predicate != "https://docs.pypi.org/attestations/publish/v1":
                    raise GateError("invalid-provenance")
                cert = x509.load_der_x509_certificate(attestation.verification_material.certificate)
                sha = _certificate_value(cert, "13")
                legacy_sha = _certificate_value(cert, "3", legacy=True)
                if (not sha or not SHA_RE.fullmatch(sha) or
                    (legacy_sha is not None and legacy_sha != sha) or
                    _certificate_value(cert, "14") != "refs/heads/master" or
                    _certificate_value(cert, "18") != WORKFLOW_URI or
                    _certificate_value(cert, "20") != "workflow_dispatch" or
                    _certificate_value(cert, "23") != "release"):
                    raise GateError("invalid-provenance")
                shas.add(sha)
                attestation_count += 1
        if attestation_count == 0 or len(shas) != 1:
            raise GateError("invalid-provenance")
        return shas.pop()
    except GateError:
        raise
    except Exception as exc:
        raise GateError("invalid-provenance") from exc


def _upload_time(entry: dict[str, Any]) -> datetime:
    stamp = entry.get("upload_time_iso_8601")
    if not isinstance(stamp, str):
        raise GateError("invalid-registry-inventory")
    try:
        return datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except (binascii.Error, ValueError) as exc:
        raise GateError("invalid-registry-inventory") from exc


def latest_pypi_release(metadata: dict[str, Any]) -> tuple[str, list[dict[str, Any]]]:
    releases = metadata.get("releases")
    if not isinstance(releases, dict):
        raise GateError("invalid-registry-inventory")
    candidates = []
    for version, files in releases.items():
        if not isinstance(version, str) or not isinstance(files, list):
            raise GateError("invalid-registry-inventory")
        if files:
            if any(not isinstance(entry, dict) for entry in files):
                raise GateError("invalid-registry-inventory")
            candidates.append((max(_upload_time(entry) for entry in files), version, files))
    if not candidates:
        raise GateError("missing-baseline")
    _, version, files = max(candidates, key=lambda row: row[0])
    return version, files


def evaluate_pypi(project: str, target_version: str, current_core: tuple[int, int, int, int],
                  repo_root: Path, get_json: Callable[[str], dict[str, Any]] = fetch_json,
                  verify_file: Callable[..., str] = verify_pypi_file,
                  lookup_core: Callable[[Path, str], tuple[int, int, int, int]] = core_version_at_sha) -> Decision:
    metadata = get_json(f"{PYPI_ORIGIN}/pypi/{project}/json")
    releases = metadata.get("releases")
    if not isinstance(releases, dict):
        raise GateError("invalid-registry-inventory")
    if target_version in releases:
        raise GateError("target-version-occupied")
    baseline_version, files = latest_pypi_release(metadata)
    # A failed or partially deleted upload may be the newest public baseline.
    # Verify every remaining file; complete inventories are required for the
    # staged publication candidate, not for recovering from that baseline.
    if len(files) > (36 if project == "iccdev" else 2):
        raise GateError("invalid-registry-inventory")
    # Sigstore refreshes a shared TUF trust-root cache, so verification must be
    # sequential even when a release has all 36 compiled wheels.
    shas = [verify_file(project, baseline_version, entry, get_json) for entry in files]
    if len(set(shas)) != 1:
        raise GateError("conflicting-provenance")
    baseline_sha = shas[0]
    baseline_core = lookup_core(repo_root, baseline_sha)
    if current_core <= baseline_core:
        raise GateError("core-not-newer")
    return Decision(True, True, "eligible", target_version, baseline_version,
                    format_core(baseline_core), baseline_sha)


def _npm_integrity_digest(value: Any) -> str:
    if not isinstance(value, str) or not value.startswith("sha512-"):
        raise GateError("invalid-registry-inventory")
    try:
        decoded = base64.b64decode(value.removeprefix("sha512-"), validate=True)
    except ValueError as exc:
        raise GateError("invalid-registry-inventory") from exc
    if len(decoded) != 64:
        raise GateError("invalid-registry-inventory")
    return decoded.hex()


def valid_npm_version(value: Any) -> bool:
    """Validate the supported three-part SemVer form in linear time."""
    if not isinstance(value, str) or not value or len(value) > 256 or "+" in value:
        return False
    core, separator, prerelease = value.partition("-")
    numbers = core.split(".")
    if len(numbers) != 3 or any(
        not number or not number.isascii() or not number.isdecimal() or
        (len(number) > 1 and number[0] == "0") or
        int(number) > MAX_NPM_COMPONENT for number in numbers
    ):
        return False
    if not separator:
        return True
    if not prerelease:
        return False
    for identifier in prerelease.split("."):
        if (not identifier or
            any(not (character.isascii() and (character.isalnum() or character == "-"))
                for character in identifier) or
            (identifier.isdecimal() and len(identifier) > 1 and identifier[0] == "0")):
            return False
    return True


def verify_npm_bundle(bundle: dict[str, Any]) -> bytes:
    """Verify the fetched SLSA bundle itself against ICC's workflow identity."""
    try:
        from sigstore.errors import MetadataError, NetworkError, RootError, TUFError
        from sigstore.models import Bundle
        from sigstore.verify import Verifier
        from sigstore.verify.policy import Identity
    except ImportError as exc:
        raise GateError("verifier-unavailable") from exc

    try:
        parsed_bundle = Bundle.from_json(json.dumps(bundle))
        verified_type, payload = Verifier.production().verify_dsse(
            parsed_bundle,
            Identity(identity=WORKFLOW_URI, issuer="https://token.actions.githubusercontent.com"),
        )
    except (MetadataError, NetworkError, RootError, TUFError, OSError) as exc:
        raise GateError("verifier-unavailable") from exc
    except Exception as exc:
        raise GateError("invalid-provenance") from exc
    if verified_type != "application/vnd.in-toto+json" or not isinstance(payload, bytes):
        raise GateError("invalid-provenance")
    return payload


def npm_source_sha(version: str, version_metadata: dict[str, Any],
                   get_json: Callable[[str], dict[str, Any]] = fetch_json,
                   verify_bundle: Callable[[dict[str, Any]], bytes] = verify_npm_bundle) -> str:
    try:
        from cryptography import x509
    except ImportError as exc:
        raise GateError("verifier-unavailable") from exc

    dist = version_metadata.get("dist")
    if not isinstance(dist, dict):
        raise GateError("invalid-registry-inventory")
    expected_url = f"{NPM_ORIGIN}/-/npm/v1/attestations/iccdev@{version}"
    if dist.get("attestations", {}).get("url") != expected_url:
        raise GateError("invalid-provenance")
    integrity = _npm_integrity_digest(dist.get("integrity"))
    response = get_json(expected_url)
    attestations = response.get("attestations")
    if not isinstance(attestations, list) or not attestations:
        raise GateError("invalid-provenance")
    shas = set()
    for item in attestations:
        if not isinstance(item, dict) or item.get("predicateType") != "https://slsa.dev/provenance/v1":
            continue
        try:
            bundle = item["bundle"]
            statement = json.loads(verify_bundle(bundle))
            certificate = x509.load_der_x509_certificate(base64.b64decode(
                bundle["verificationMaterial"]["certificate"]["rawBytes"], validate=True))
            identities = certificate.extensions.get_extension_for_class(
                x509.SubjectAlternativeName).value.get_values_for_type(
                    x509.UniformResourceIdentifier)
            subjects = statement["subject"]
            predicate = statement["predicate"]
            build = predicate["buildDefinition"]
            workflow = build["externalParameters"]["workflow"]
            github = build["internalParameters"]["github"]
            dependencies = build["resolvedDependencies"]
        except (KeyError, IndexError, TypeError, ValueError, x509.ExtensionNotFound) as exc:
            raise GateError("invalid-provenance") from exc
        if (statement.get("predicateType") != "https://slsa.dev/provenance/v1" or
            subjects != [{"name": f"pkg:npm/iccdev@{version}", "digest": {"sha512": integrity}}] or
            workflow != {"ref": "refs/heads/master", "repository": f"https://github.com/{REPOSITORY}",
                         "path": f".github/workflows/{WORKFLOW}"} or
            github.get("event_name") != "workflow_dispatch" or
            not isinstance(dependencies, list)):
            raise GateError("invalid-provenance")
        matching = [entry.get("digest", {}).get("gitCommit") for entry in dependencies
                    if isinstance(entry, dict) and
                    entry.get("uri") == f"git+https://github.com/{REPOSITORY}@refs/heads/master"]
        if len(matching) != 1 or not isinstance(matching[0], str) or not SHA_RE.fullmatch(matching[0]):
            raise GateError("invalid-provenance")
        sha = matching[0]
        legacy_sha = _certificate_value(certificate, "3", legacy=True)
        if (identities != [WORKFLOW_URI] or
            _certificate_value(certificate, "13") != sha or
            (legacy_sha is not None and legacy_sha != sha) or
            _certificate_value(certificate, "14") != "refs/heads/master" or
            _certificate_value(certificate, "18") != WORKFLOW_URI or
            _certificate_value(certificate, "20") != "workflow_dispatch" or
            _certificate_value(certificate, "23") != "release"):
            raise GateError("invalid-provenance")
        shas.add(sha)
    if len(shas) != 1:
        raise GateError("invalid-provenance")
    return shas.pop()


def verify_npm_signatures(version: str) -> None:
    """Use npm's maintained signature/provenance verifier on an exact install."""
    npm = shutil.which("npm")
    if not npm:
        raise GateError("verifier-unavailable")
    with tempfile.TemporaryDirectory(prefix="iccdev-oidc-npm-") as temporary:
        directory = Path(temporary)
        (directory / "package.json").write_text(
            '{"name":"iccdev-oidc-gate","version":"0.0.0","private":true}\n', encoding="utf-8"
        )
        (directory / ".npmrc").write_text("", encoding="utf-8")
        environment = {key: value for key, value in os.environ.items()
                       if key not in {"NODE_AUTH_TOKEN", "NPM_TOKEN", "GITHUB_TOKEN", "GH_TOKEN"}}
        environment.update({"NPM_CONFIG_USERCONFIG": str(directory / ".npmrc"),
                            "NPM_CONFIG_REGISTRY": NPM_ORIGIN,
                            "NPM_CONFIG_IGNORE_SCRIPTS": "true",
                            "NPM_CONFIG_UPDATE_NOTIFIER": "false"})
        install = subprocess.run(
            [npm, "install", "--ignore-scripts", "--no-audit", "--no-fund", "--save-exact",
             f"iccdev@{version}"], cwd=directory, env=environment,
            capture_output=True, text=True, timeout=180, check=False,
        )
        if install.returncode != 0:
            raise GateError("verifier-unavailable")
        try:
            lock = json.loads((directory / "package-lock.json").read_text(encoding="utf-8"))
            if lock["packages"]["node_modules/iccdev"]["version"] != version:
                raise ValueError("npm resolved another package version")
        except (OSError, KeyError, TypeError, ValueError) as exc:
            raise GateError("invalid-provenance") from exc
        audit = subprocess.run([npm, "audit", "signatures", "--json"], cwd=directory,
                               env=environment, capture_output=True, text=True,
                               timeout=180, check=False)
        try:
            result = json.loads(audit.stdout)
        except ValueError as exc:
            raise GateError("invalid-provenance") from exc
        if audit.returncode != 0 or result.get("invalid") != [] or result.get("missing") != []:
            raise GateError("invalid-provenance")


def latest_npm_release(metadata: dict[str, Any]) -> tuple[str, dict[str, Any]]:
    versions = metadata.get("versions")
    times = metadata.get("time")
    if not isinstance(versions, dict) or not isinstance(times, dict) or not versions:
        raise GateError("invalid-registry-inventory")
    candidates = []
    for version, version_metadata in versions.items():
        if not isinstance(version, str) or not isinstance(version_metadata, dict):
            raise GateError("invalid-registry-inventory")
        stamp = times.get(version)
        if not isinstance(stamp, str):
            raise GateError("invalid-registry-inventory")
        try:
            when = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
        except ValueError as exc:
            raise GateError("invalid-registry-inventory") from exc
        candidates.append((when, version, version_metadata))
    _, version, version_metadata = max(candidates, key=lambda row: row[0])
    return version, version_metadata


def evaluate_npm(target_version: str, current_core: tuple[int, int, int, int], repo_root: Path,
                 get_json: Callable[[str], dict[str, Any]] = fetch_json,
                 verify_signatures: Callable[[str], None] = verify_npm_signatures,
                 lookup_core: Callable[[Path, str], tuple[int, int, int, int]] = core_version_at_sha,
                 verify_bundle: Callable[[dict[str, Any]], bytes] = verify_npm_bundle) -> Decision:
    if not valid_npm_version(target_version):
        raise GateError("invalid-target-version")
    metadata = get_json(f"{NPM_ORIGIN}/iccdev")
    versions = metadata.get("versions")
    if not isinstance(versions, dict):
        raise GateError("invalid-registry-inventory")
    if target_version in versions:
        raise GateError("target-version-occupied")
    baseline_version, baseline_metadata = latest_npm_release(metadata)
    if not valid_npm_version(baseline_version):
        raise GateError("invalid-registry-inventory")
    baseline_sha = npm_source_sha(baseline_version, baseline_metadata, get_json, verify_bundle)
    verify_signatures(baseline_version)
    baseline_core = lookup_core(repo_root, baseline_sha)
    if current_core <= baseline_core:
        raise GateError("core-not-newer")
    return Decision(True, True, "eligible", target_version, baseline_version,
                    format_core(baseline_core), baseline_sha)


def evaluate(repo_root: Path, pypi_packages: str, npm_packages: str, npm_version: str,
             get_json: Callable[[str], dict[str, Any]] = fetch_json,
             verify_pypi: Callable[..., str] = verify_pypi_file,
             verify_npm: Callable[[str], None] = verify_npm_signatures,
             lookup_core: Callable[[Path, str], tuple[int, int, int, int]] = core_version_at_sha,
             verify_bundle: Callable[[dict[str, Any]], bytes] = verify_npm_bundle) -> dict[str, Any]:
    if pypi_packages not in {"none", "iccdev", "iccdev-mcp", "both"} or npm_packages not in {"none", "iccdev"}:
        raise GateError("invalid-selection")
    current_core, iccdev_version, mcp_version = source_versions(repo_root)
    requested = {
        "pypi_iccdev": pypi_packages in {"iccdev", "both"},
        "pypi_mcp": pypi_packages in {"iccdev-mcp", "both"},
        "npm_iccdev": npm_packages == "iccdev",
    }
    targets = {"pypi_iccdev": iccdev_version, "pypi_mcp": mcp_version, "npm_iccdev": npm_version}
    decisions = {}
    for name, selected in requested.items():
        if not selected:
            decisions[name] = Decision(False, False, "not-selected")
            continue
        try:
            if name == "npm_iccdev":
                decisions[name] = evaluate_npm(targets[name], current_core, repo_root,
                                               get_json, verify_npm, lookup_core, verify_bundle)
            else:
                project = "iccdev" if name == "pypi_iccdev" else "iccdev-mcp"
                decisions[name] = evaluate_pypi(project, targets[name], current_core,
                                                repo_root, get_json, verify_pypi, lookup_core)
        except GateError as exc:
            decisions[name] = Decision(True, False, exc.reason, targets[name])
        except Exception:
            decisions[name] = Decision(True, False, "verification-unavailable", targets[name])
    return {"mode": "full", "core_version": format_core(current_core),
            "selected_but_blocked": any(item.requested and not item.eligible for item in decisions.values()),
            "packages": {name: asdict(item) for name, item in decisions.items()}}


def evaluate_target_only(repo_root: Path, pypi_packages: str, npm_packages: str,
                         npm_version: str,
                         get_json: Callable[[str], dict[str, Any]] = fetch_json) -> dict[str, Any]:
    """Check the selected target versions again immediately before publication."""
    if pypi_packages not in {"none", "iccdev", "iccdev-mcp", "both"} or npm_packages not in {"none", "iccdev"}:
        raise GateError("invalid-selection")
    current_core, iccdev_version, mcp_version = source_versions(repo_root)
    requested = {
        "pypi_iccdev": pypi_packages in {"iccdev", "both"},
        "pypi_mcp": pypi_packages in {"iccdev-mcp", "both"},
        "npm_iccdev": npm_packages == "iccdev",
    }
    targets = {"pypi_iccdev": iccdev_version, "pypi_mcp": mcp_version,
               "npm_iccdev": npm_version}
    decisions = {}
    for name, selected in requested.items():
        if not selected:
            decisions[name] = Decision(False, False, "not-selected")
            continue
        try:
            if name == "npm_iccdev":
                if not valid_npm_version(targets[name]):
                    raise GateError("invalid-target-version")
                metadata = get_json(f"{NPM_ORIGIN}/iccdev")
                versions = metadata.get("versions")
            else:
                project = "iccdev" if name == "pypi_iccdev" else "iccdev-mcp"
                metadata = get_json(f"{PYPI_ORIGIN}/pypi/{project}/json")
                versions = metadata.get("releases")
            if not isinstance(versions, dict):
                raise GateError("invalid-registry-inventory")
            if targets[name] in versions:
                raise GateError("target-version-occupied")
            decisions[name] = Decision(True, True, "target-available", targets[name])
        except GateError as exc:
            decisions[name] = Decision(True, False, exc.reason, targets[name])
        except Exception:
            decisions[name] = Decision(True, False, "registry-unavailable", targets[name])
    return {"mode": "target-only", "core_version": format_core(current_core),
            "selected_but_blocked": any(item.requested and not item.eligible for item in decisions.values()),
            "packages": {name: asdict(item) for name, item in decisions.items()}}


def output_lines(result: dict[str, Any]) -> str:
    lines = []
    target_only = result.get("mode") == "target-only"
    for name in ("pypi_iccdev", "pypi_mcp", "npm_iccdev"):
        decision = result["packages"][name]
        reason = decision["reason"]
        if not re.fullmatch(r"[a-z][a-z0-9-]*", reason):
            raise GateError("invalid-reason")
        output_name = f"target_{name}_available" if target_only else name
        reason_name = f"reason_target_{name}" if target_only else f"reason_{name}"
        lines.extend((f"{output_name}={str(decision['eligible']).lower()}", f"{reason_name}={reason}"))
    blocked_name = "target_selected_but_blocked" if target_only else "selected_but_blocked"
    lines.append(f"{blocked_name}={str(result['selected_but_blocked']).lower()}")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--pypi-packages", choices=("none", "iccdev", "iccdev-mcp", "both"), required=True)
    parser.add_argument("--npm-packages", choices=("none", "iccdev"), required=True)
    parser.add_argument("--npm-version", default="")
    parser.add_argument("--check-target-only", action="store_true",
                        help="Only recheck occupied target versions; no provenance verifier is used")
    parser.add_argument("--github-output", type=Path)
    parser.add_argument("--json-output", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.check_target_only:
            result = evaluate_target_only(args.repo_root.resolve(), args.pypi_packages,
                                          args.npm_packages, args.npm_version)
        else:
            result = evaluate(args.repo_root.resolve(), args.pypi_packages,
                              args.npm_packages, args.npm_version)
        lines = output_lines(result)
        if args.github_output:
            with args.github_output.open("a", encoding="utf-8", newline="\n") as output:
                output.write(lines)
        if args.json_output:
            args.json_output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(lines, end="")
        return 0
    except GateError as exc:
        print(f"Downstream gate could not run: {exc.reason}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
