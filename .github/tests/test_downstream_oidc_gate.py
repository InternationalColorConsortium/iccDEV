# Copyright (c) International Color Consortium.
# SPDX-License-Identifier: BSD-3-Clause
# See LICENSE.md for the complete license text.

"""Per-package OIDC gate controls; registry and signature checks are injected."""

import base64
from contextlib import nullcontext
from datetime import datetime, timedelta, timezone
from pathlib import Path
import json
import runpy

import pytest
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec
from cryptography.x509.oid import NameOID, ObjectIdentifier


ROOT = Path(__file__).resolve().parents[2]
GATE = runpy.run_path(str(ROOT / ".github/scripts/check-downstream-oidc-gate.py"))
Decision = GATE["Decision"]
GateError = GATE["GateError"]
evaluate = GATE["evaluate"]
evaluate_pypi = GATE["evaluate_pypi"]
evaluate_npm = GATE["evaluate_npm"]
evaluate_target_only = GATE["evaluate_target_only"]
output_lines = GATE["output_lines"]
npm_source_sha = GATE["npm_source_sha"]
latest_pypi_release = GATE["latest_pypi_release"]

OLD_SHA = "2ceb7bee6f252c2c67a6b7258f2f547c52c91cb1"
NEW_SHA = "0dc248a10fc5bdf5b3a9ea90831d048938d438fb"
NPM_SHA = "0116573e21b6454c088307e6825c6522909d61b8"
WORKFLOW_URI = GATE["WORKFLOW_URI"]


def source_tree(tmp_path, *, core_patch=5, iccdev="0.1.3", mcp="0.1.3"):
    cmake = tmp_path / "Build/Cmake/CMakeLists.txt"
    cmake.parent.mkdir(parents=True)
    cmake.write_text("".join(
        f"set(${{PROJECT_UP_NAME}}_{part}_VERSION {number})\n"
        for part, number in zip(("MAJOR", "MINOR", "MICRO", "PATCH"), (2, 3, 2, core_patch))
    ), encoding="utf-8")
    pyproject = tmp_path / "python/pyproject.toml"
    pyproject.parent.mkdir()
    pyproject.write_text(f'[project]\nname = "iccdev"\nversion = "{iccdev}"\n', encoding="utf-8")
    mcp_file = tmp_path / "iccdev-mcp/iccdev_mcp/__init__.py"
    mcp_file.parent.mkdir(parents=True)
    mcp_file.write_text(f'__version__ = "{mcp}"\n', encoding="utf-8")
    return tmp_path


def pypi_file(name, when="2026-11-07T12:00:00Z"):
    return {"filename": name, "upload_time_iso_8601": when,
            "digests": {"sha256": "a" * 64}}


def pypi_metadata(project, *, occupied=False, newer=False):
    count = 36 if project == "iccdev" else 2
    files = [pypi_file(f"{project}-0.1.2-{i}.whl") for i in range(count)]
    releases = {"0.1.2": files}
    if occupied:
        releases["0.1.3"] = []  # Deleted releases still cannot be reused.
    if newer:
        releases["0.1.1"] = [pypi_file(f"{project}-0.1.1.whl", "2026-11-08T12:00:00Z")]
    return {"info": {"version": "0.1.2"}, "releases": releases}


def npm_metadata(*, occupied=False):
    baseline = {"dist": {"attestations": {
        "url": "https://registry.npmjs.org/-/npm/v1/attestations/iccdev@2.3.27"},
        "integrity": "sha512-" + base64.b64encode(bytes(64)).decode("ascii")}}
    versions = {"2.3.27": baseline}
    times = {"2.3.27": "2026-11-07T12:00:00Z"}
    if occupied:
        versions["2.3.28"] = {}
        times["2.3.28"] = "2026-11-08T12:00:00Z"
    return {"versions": versions, "time": times}


def _der_utf8(value):
    encoded = value.encode("utf-8")
    length = bytes([len(encoded)]) if len(encoded) < 128 else b"\x81" + bytes([len(encoded)])
    return b"\x0c" + length + encoded


def npm_certificate(*, san=WORKFLOW_URI, sha=NPM_SHA, workflow=WORKFLOW_URI):
    key = ec.generate_private_key(ec.SECP256R1())
    name = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "npm fixture")])
    start = datetime(2026, 1, 1, tzinfo=timezone.utc)
    builder = (x509.CertificateBuilder()
               .subject_name(name).issuer_name(name).public_key(key.public_key())
               .serial_number(1).not_valid_before(start)
               .not_valid_after(start + timedelta(days=365))
               .add_extension(x509.SubjectAlternativeName([
                   x509.UniformResourceIdentifier(san)]), critical=False))
    claims = {"13": sha, "14": "refs/heads/master", "18": workflow,
              "20": "workflow_dispatch", "23": "release"}
    for suffix, value in claims.items():
        builder = builder.add_extension(x509.UnrecognizedExtension(
            ObjectIdentifier(f"1.3.6.1.4.1.57264.1.{suffix}"), _der_utf8(value)),
            critical=False)
    certificate = builder.sign(key, hashes.SHA256())
    return base64.b64encode(certificate.public_bytes(serialization.Encoding.DER)).decode("ascii")


def npm_provenance(*, workflow_path=".github/workflows/ci-latest-release.yml",
                   ref="refs/heads/master", sha=NPM_SHA, cert_claims=None):
    statement = {"_type": "https://in-toto.io/Statement/v1",
                 "subject": [{"name": "pkg:npm/iccdev@2.3.27",
                              "digest": {"sha512": "0" * 128}}],
                 "predicateType": "https://slsa.dev/provenance/v1",
                 "predicate": {"buildDefinition": {
                     "externalParameters": {"workflow": {
                         "ref": ref, "repository": "https://github.com/InternationalColorConsortium/iccDEV",
                         "path": workflow_path}},
                     "internalParameters": {"github": {"event_name": "workflow_dispatch"}},
                     "resolvedDependencies": [{"uri": "git+https://github.com/InternationalColorConsortium/iccDEV@refs/heads/master",
                                               "digest": {"gitCommit": sha}}]}}}
    payload = base64.b64encode(json.dumps(statement).encode("utf-8")).decode("ascii")
    certificate = npm_certificate(**(cert_claims or {}))
    return {"attestations": [{"predicateType": "https://slsa.dev/provenance/v1",
                              "bundle": {"dsseEnvelope": {"payload": payload},
                                         "verificationMaterial": {"certificate": {
                                             "rawBytes": certificate}}}}]}


def fixture_verify_bundle(bundle):
    """Exercise statement parsing after a separately injected verifier succeeds."""
    return base64.b64decode(bundle["dsseEnvelope"]["payload"], validate=True)


def test_none_is_network_free_and_outputs_fixed_denials(tmp_path):
    source_tree(tmp_path)

    def network(_url):
        pytest.fail("none selections must not contact registries")

    result = evaluate(tmp_path, "none", "none", "", get_json=network)
    assert result["selected_but_blocked"] is False
    assert all(not item["eligible"] and item["reason"] == "not-selected"
               for item in result["packages"].values())
    assert output_lines(result).splitlines() == [
        "pypi_iccdev=false", "reason_pypi_iccdev=not-selected",
        "pypi_mcp=false", "reason_pypi_mcp=not-selected",
        "npm_iccdev=false", "reason_npm_iccdev=not-selected",
        "selected_but_blocked=false",
    ]
    target = evaluate_target_only(tmp_path, "none", "none", "", get_json=network)
    assert target["selected_but_blocked"] is False
    assert "target_pypi_iccdev_available=false" in output_lines(target)


@pytest.mark.parametrize("baseline_core,expected", [((2, 3, 2, 4), True),
                                                   ((2, 3, 2, 5), False),
                                                   ((2, 3, 2, 6), False)])
def test_pypi_requires_strict_core_bump(tmp_path, baseline_core, expected):
    source_tree(tmp_path)
    with pytest.raises(GateError, match="core-not-newer") if not expected else nullcontext():
        decision = evaluate_pypi(
            "iccdev-mcp", "0.1.3", (2, 3, 2, 5), tmp_path,
            lambda _url: pypi_metadata("iccdev-mcp"),
            lambda _project, _version, _entry, _fetch: OLD_SHA,
            lambda _root, _sha: baseline_core,
        )
        assert decision.eligible is True


def test_pypi_target_collision_blocks_before_provenance(tmp_path):
    source_tree(tmp_path)

    def verification(*_args):
        pytest.fail("occupied target needs no provenance work")

    with pytest.raises(GateError, match="target-version-occupied"):
        evaluate_pypi("iccdev", "0.1.3", (2, 3, 2, 5), tmp_path,
                      lambda _url: pypi_metadata("iccdev", occupied=True), verification)


def test_pypi_uses_latest_upload_not_info_version():
    version, _files = latest_pypi_release(pypi_metadata("iccdev-mcp", newer=True))
    assert version == "0.1.1"


def test_pypi_conflicting_file_sources_block(tmp_path):
    source_tree(tmp_path)
    calls = iter((OLD_SHA, NEW_SHA))
    with pytest.raises(GateError, match="conflicting-provenance"):
        evaluate_pypi("iccdev-mcp", "0.1.3", (2, 3, 2, 5), tmp_path,
                      lambda _url: pypi_metadata("iccdev-mcp"),
                      lambda *_args: next(calls))


def test_pypi_partial_latest_baseline_allows_newer_core_recovery(tmp_path):
    source_tree(tmp_path, core_patch=6, iccdev="2.3.2.6")
    metadata = pypi_metadata("iccdev")
    metadata["releases"]["2.3.2.5"] = [
        pypi_file("iccdev-2.3.2.5-cp314-cp314-manylinux_x86_64.whl",
                  "2026-11-08T12:00:00Z")
    ]
    verified = []

    def verify(project, version, entry, _get_json):
        verified.append((project, version, entry["filename"]))
        return NEW_SHA

    decision = evaluate_pypi(
        "iccdev", "2.3.2.6", (2, 3, 2, 6), tmp_path,
        lambda _url: metadata, verify,
        lambda _root, sha: (2, 3, 2, 5) if sha == NEW_SHA else (2, 3, 2, 4),
    )
    assert decision.eligible is True
    assert decision.baseline_version == "2.3.2.5"
    assert len(verified) == 1
    with pytest.raises(GateError, match="core-not-newer"):
        evaluate_pypi(
            "iccdev", "2.3.2.6", (2, 3, 2, 5), tmp_path,
            lambda _url: metadata, verify,
            lambda _root, _sha: (2, 3, 2, 5),
        )


def test_pypi_baseline_with_excess_files_is_rejected(tmp_path):
    source_tree(tmp_path)
    metadata = pypi_metadata("iccdev")
    metadata["releases"]["0.1.2"].append(pypi_file("extra.whl"))
    with pytest.raises(GateError, match="invalid-registry-inventory"):
        evaluate_pypi("iccdev", "2.3.2.5", (2, 3, 2, 5), tmp_path,
                      lambda _url: metadata)


def test_one_package_retry_is_independent(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        if url.endswith("/iccdev/json"):
            return pypi_metadata("iccdev")
        if url.endswith("/iccdev-mcp/json"):
            return pypi_metadata("iccdev-mcp")
        raise AssertionError(url)

    def verifier(project, *_args):
        return NEW_SHA if project == "iccdev" else OLD_SHA

    result = evaluate(tmp_path, "both", "none", "", fetch, verifier,
                      lambda _version: None,
                      lambda _root, sha: (2, 3, 2, 5) if sha == NEW_SHA else (2, 3, 2, 4))
    assert result["packages"]["pypi_iccdev"]["reason"] == "core-not-newer"
    assert result["packages"]["pypi_mcp"]["eligible"] is True
    assert result["packages"]["npm_iccdev"]["reason"] == "not-selected"
    assert result["selected_but_blocked"] is True


def test_registry_outage_denies_only_selected_package(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        if url.endswith("/iccdev/json"):
            raise GateError("registry-unavailable")
        return pypi_metadata("iccdev-mcp")

    result = evaluate(tmp_path, "both", "none", "", fetch,
                      lambda *_args: OLD_SHA, lambda _version: None,
                      lambda _root, _sha: (2, 3, 2, 4))
    assert result["packages"]["pypi_iccdev"]["reason"] == "registry-unavailable"
    assert result["packages"]["pypi_mcp"]["eligible"] is True


def test_bad_pypi_attestation_denies_only_its_channel(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        return pypi_metadata("iccdev") if url.endswith("/iccdev/json") else pypi_metadata("iccdev-mcp")

    def verifier(project, *_args):
        if project == "iccdev":
            raise GateError("invalid-provenance")
        return OLD_SHA

    result = evaluate(tmp_path, "both", "none", "", fetch, verifier,
                      lambda _version: None, lambda _root, _sha: (2, 3, 2, 4))
    assert result["packages"]["pypi_iccdev"]["reason"] == "invalid-provenance"
    assert result["packages"]["pypi_mcp"]["eligible"] is True


def test_untrusted_source_sha_denies_selected_package(tmp_path):
    source_tree(tmp_path)
    result = evaluate(tmp_path, "iccdev-mcp", "none", "",
                      lambda _url: pypi_metadata("iccdev-mcp"),
                      lambda *_args: OLD_SHA, lambda _version: None,
                      lambda _root, _sha: (_ for _ in ()).throw(GateError("untrusted-source-commit")))
    assert result["packages"]["pypi_mcp"]["reason"] == "untrusted-source-commit"
    assert result["selected_but_blocked"] is True


@pytest.mark.parametrize("bad_workflow,bad_ref", [
    (".github/workflows/other.yml", "refs/heads/master"),
    (".github/workflows/ci-latest-release.yml", "refs/heads/test"),
])
def test_npm_rejects_wrong_workflow_or_ref(bad_workflow, bad_ref):
    metadata = npm_metadata()["versions"]["2.3.27"]
    with pytest.raises(GateError, match="invalid-provenance"):
        npm_source_sha("2.3.27", metadata,
                       lambda _url: npm_provenance(workflow_path=bad_workflow, ref=bad_ref),
                       fixture_verify_bundle)


@pytest.mark.parametrize("cert_claims", [
    {"san": "https://github.com/foreign/repo/.github/workflows/release.yml@refs/heads/master"},
    {"workflow": "https://github.com/foreign/repo/.github/workflows/release.yml@refs/heads/master"},
    {"sha": OLD_SHA},
])
def test_npm_rejects_foreign_certificate_even_with_expected_payload(cert_claims):
    metadata = npm_metadata()["versions"]["2.3.27"]
    with pytest.raises(GateError, match="invalid-provenance"):
        npm_source_sha("2.3.27", metadata,
                       lambda _url: npm_provenance(cert_claims=cert_claims),
                       fixture_verify_bundle)


def test_npm_bundle_verification_failure_denies_before_statement_parsing():
    metadata = npm_metadata()["versions"]["2.3.27"]

    def deny(_bundle):
        raise GateError("invalid-provenance")

    with pytest.raises(GateError, match="invalid-provenance"):
        npm_source_sha("2.3.27", metadata, lambda _url: npm_provenance(), deny)


def test_npm_positive_and_malformed_target(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        return npm_metadata() if url.endswith("/iccdev") else npm_provenance()

    decision = evaluate_npm("2.3.28", (2, 3, 2, 5), tmp_path, fetch,
                            lambda _version: None,
                            lambda _root, _sha: (2, 3, 2, 4),
                            fixture_verify_bundle)
    assert decision.eligible is True
    with pytest.raises(GateError, match="invalid-target-version"):
        evaluate_npm("2.3.28+build", (2, 3, 2, 5), tmp_path, fetch)
    with pytest.raises(GateError, match="invalid-target-version"):
        evaluate_npm("2.3.28-01", (2, 3, 2, 5), tmp_path, fetch)
    with pytest.raises(GateError, match="invalid-target-version"):
        evaluate_npm("2.3.28-rc.01", (2, 3, 2, 5), tmp_path, fetch)
    with pytest.raises(GateError, match="target-version-occupied"):
        evaluate_npm("2.3.28", (2, 3, 2, 5), tmp_path,
                     lambda _url: npm_metadata(occupied=True))


def test_npm_version_parser_handles_prereleases_without_backtracking():
    valid = GATE["valid_npm_version"]
    assert valid("2.3.28-rc.1") is True
    assert valid("2.3.28-0") is True
    assert valid("2.3.28-rc--1") is True
    for value in ("2.3.28-01", "2.3.28-rc.01", "2.3.28+build",
                  "2.3.28-", "2.3.28-rc.", "2.3.28-α",
                  "9007199254740992.0.0",
                  "0.0.0-0." + "--." * 10000):
        assert valid(value) is False


def test_npm_malformed_public_baseline_denies_before_verifier(tmp_path):
    source_tree(tmp_path)
    metadata = npm_metadata()
    metadata["versions"]["2.3.027"] = metadata["versions"].pop("2.3.27")
    metadata["time"]["2.3.027"] = metadata["time"].pop("2.3.27")
    with pytest.raises(GateError, match="invalid-registry-inventory"):
        evaluate_npm("2.3.28", (2, 3, 2, 5), tmp_path,
                     lambda _url: metadata)


def test_npm_signature_failure_denies_independently(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        return npm_metadata() if url.endswith("/iccdev") else npm_provenance()

    result = evaluate(tmp_path, "none", "iccdev", "2.3.28", fetch,
                      lambda *_args: OLD_SHA,
                      lambda _version: (_ for _ in ()).throw(GateError("invalid-provenance")),
                      lambda _root, _sha: (2, 3, 2, 4),
                      fixture_verify_bundle)
    assert result["packages"]["npm_iccdev"]["reason"] == "invalid-provenance"
    assert result["packages"]["pypi_iccdev"]["reason"] == "not-selected"


def test_target_only_checks_each_registry_without_verifier(tmp_path):
    source_tree(tmp_path)

    def fetch(url):
        if url.endswith("/iccdev/json"):
            return pypi_metadata("iccdev", occupied=True)
        if url.endswith("/iccdev-mcp/json"):
            return pypi_metadata("iccdev-mcp")
        return npm_metadata()

    result = evaluate_target_only(tmp_path, "both", "iccdev", "2.3.28", fetch)
    assert result["packages"]["pypi_iccdev"]["reason"] == "target-version-occupied"
    assert result["packages"]["pypi_mcp"]["eligible"] is True
    assert result["packages"]["npm_iccdev"]["eligible"] is True
    assert result["selected_but_blocked"] is True
    assert "target_npm_iccdev_available=true" in output_lines(result)


def test_target_only_registry_outage_fails_closed(tmp_path):
    source_tree(tmp_path)
    result = evaluate_target_only(tmp_path, "iccdev", "none", "",
                                  lambda _url: (_ for _ in ()).throw(OSError("network down")))
    assert result["packages"]["pypi_iccdev"]["reason"] == "registry-unavailable"
    assert "target_pypi_iccdev_available=false" in output_lines(result)


def test_output_reason_cannot_inject_github_output():
    result = {"mode": "full", "selected_but_blocked": True,
              "packages": {name: {"eligible": False, "reason": "x\nforged=true"}
                           for name in ("pypi_iccdev", "pypi_mcp", "npm_iccdev")}}
    with pytest.raises(GateError, match="invalid-reason"):
        output_lines(result)


def test_cli_writes_fixed_github_outputs_and_json(tmp_path, capsys):
    source_tree(tmp_path)
    outputs = tmp_path / "github-output"
    details = tmp_path / "gate.json"
    status = GATE["main"](["--repo-root", str(tmp_path), "--pypi-packages", "none",
                           "--npm-packages", "none", "--github-output", str(outputs),
                           "--json-output", str(details)])
    assert status == 0
    assert outputs.read_text(encoding="utf-8") == capsys.readouterr().out
    assert json.loads(details.read_text(encoding="utf-8"))["selected_but_blocked"] is False


def test_bad_source_sha_and_der_string_are_rejected():
    with pytest.raises(GateError, match="invalid-provenance"):
        GATE["_der_utf8"](b"not-der")
    with pytest.raises(GateError, match="invalid-provenance"):
        GATE["core_version_at_sha"](ROOT, "not-a-sha")
