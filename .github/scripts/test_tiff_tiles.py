#!/usr/bin/env python3
"""Cross-platform TIFF tile CLI regression using generated libtiff fixtures."""

import argparse
import hashlib
import json
import os
import pathlib
import struct
import subprocess
import tempfile


def run(*args, success=True):
    result = subprocess.run([str(arg) for arg in args], capture_output=True, timeout=60)
    if success and result.returncode != 0:
        raise AssertionError(f"{args}: {result.returncode}\n{result.stdout!r}\n{result.stderr!r}")
    clean_rejection = 0 < result.returncode < 128 or result.returncode == 255
    clean_rejection |= os.name == "nt" and result.returncode == 0xFFFFFFFF
    if not success and not clean_rejection:
        raise AssertionError(f"Expected clean rejection: {args}: {result.returncode}\n{result.stderr!r}")
    if b"runtime error:" in result.stderr or b"ERROR: AddressSanitizer" in result.stderr:
        raise AssertionError(result.stderr.decode("ascii", errors="backslashreplace"))
    return result


def replace_compression(path, value):
    data = bytearray(path.read_bytes())
    offset = struct.unpack_from("<I", data, 4)[0]
    count = struct.unpack_from("<H", data, offset)[0]
    for index in range(count):
        entry = offset + 2 + index * 12
        if struct.unpack_from("<H", data, entry)[0] == 259:
            struct.pack_into("<H", data, entry + 8, value)
            path.write_bytes(data)
            return
    raise AssertionError("Compression tag missing in fixture")


def replace_tile_offset(path, index, value):
    data = bytearray(path.read_bytes())
    offset = struct.unpack_from("<I", data, 4)[0]
    count = struct.unpack_from("<H", data, offset)[0]
    for entry_index in range(count):
        entry = offset + 2 + entry_index * 12
        if struct.unpack_from("<H", data, entry)[0] == 324:
            tile_count = struct.unpack_from("<I", data, entry + 4)[0]
            assert index < tile_count
            offsets = struct.unpack_from("<I", data, entry + 8)[0]
            struct.pack_into("<I", data, offsets + index * 4, value)
            path.write_bytes(data)
            return
    raise AssertionError("TileOffsets tag missing in fixture")


def main():
    parser = argparse.ArgumentParser()
    for name in ("fixture", "dump", "apply", "specsep", "fromxml", "source"):
        parser.add_argument(f"--{name}", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="iccdev-tiff-tiles-") as temp:
        root = pathlib.Path(temp)
        run(args.fixture, "--fixtures", root)
        multi = root / "multi.tif"
        inventory = json.loads(run(args.dump, "--evidence-json", "--list-directories", multi).stdout)
        directories = inventory["directories"]
        assert len(directories) == 2
        assert directories[0]["tiled"] and not directories[1]["tiled"]
        assert all(item["bigTiff"] for item in directories)
        assert directories[0]["tileWidth"] == 16 and directories[0]["tileHeight"] == 32
        assert len(directories[0]["subIfdOffsets"]) == 1
        assert directories[0]["subIfdOffsets"][0] > 0
        assert directories[0]["iccLength"] == len(b"malformed-first-icc")
        for index, payload in enumerate((b"malformed-first-icc", b"malformed-second-icc")):
            exported = root / f"ifd{index}.icc"
            run(args.dump, "--ifd", index, multi, exported, success=False)
            assert exported.read_bytes() == payload
        for index in ("-1", "2", "bad", "4096", "1.5"):
            run(args.dump, "--ifd", index, multi, success=False)
        run(args.dump, "--list-directories", multi, root / "no-export.icc", success=False)
        run(args.dump, "--ifd", "1", "--list-directories", multi, success=False)
        run(args.dump, "--ifd", "0", success=False)
        run(args.dump, "--list-directories", success=False)
        bilevel = run(args.dump, root / "bilevel.tif")
        assert b"BitsPerSample:     1" in bilevel.stdout
        assert b"BytesPerLine:      5" in bilevel.stdout
        run(args.dump, "--ifd", "0", multi, multi, success=False)
        assert not (root / "no-export.icc").exists()
        unknown = root / "unknown-codec.tif"
        unknown.write_bytes((root / "tiles.tif").read_bytes())
        replace_compression(unknown, 65000)
        metadata = json.loads(run(args.dump, "--evidence-json", "--list-directories", unknown).stdout)
        assert not metadata["directories"][0]["codecAvailable"]
        run(args.dump, unknown)
        unknown_icc = root / "unknown-icc.tif"
        unknown_icc.write_bytes((root / "malformed-classic.tif").read_bytes())
        replace_compression(unknown_icc, 65000)
        forensic = root / "unknown-icc.icc"
        run(args.dump, unknown_icc, forensic, success=False)
        assert forensic.read_bytes() == b"malformed-classic-icc"
        cycle = root / "cycle.tif"
        cyclic_data = bytearray((root / "tiles.tif").read_bytes())
        first_ifd = struct.unpack_from("<I", cyclic_data, 4)[0]
        entry_count = struct.unpack_from("<H", cyclic_data, first_ifd)[0]
        struct.pack_into("<I", cyclic_data, first_ifd + 2 + entry_count * 12, first_ifd)
        cycle.write_bytes(cyclic_data)
        run(args.dump, "--list-directories", cycle, success=False)
        no_icc = json.loads(run(args.dump, "--evidence-json", root / "tiles.tif").stdout)
        assert no_icc["tiled"] and no_icc["directory"] == 0 and no_icc["outputDigest"] is None
        run(args.dump, root / "tiles.tif", root / "absent.icc", success=False)
        assert not (root / "absent.icc").exists()

        for bits in (8, 16, 32):
            prefix = root / f"gray{bits}_"
            for planar in (0, 1):
                strip = root / f"spec-strip-{bits}-{planar}.tif"
                tile = root / f"spec-tile-{bits}-{planar}.tif"
                run(args.specsep, strip, 1, planar, prefix, 1, 3, 1)
                run(args.specsep, "--tiles", 16, 32, "--bigtiff",
                    tile, 1, planar, prefix, 1, 3, 1)
                run(args.fixture, "--compare", strip, tile)
                info = json.loads(run(args.dump, "--evidence-json", tile).stdout)
                assert info["bigTiff"] and info["tiled"]
                text = run(args.dump, tile).stdout
                assert b"ExtraSamples" in text
        run(args.specsep, "--tiles", 17, 32, root / "invalid.tif",
            0, 0, root / "gray8_", 1, 3, 1, success=False)
        assert not (root / "invalid.tif").exists()

        # Creation limits must be checked before truncating an existing image.
        for big in (False, True):
            for planar in (0, 1):
                for existing in (False, True):
                    output = root / f"spec-oversized-{big}-{planar}-{existing}.tif"
                    before = (root / "tiles.tif").read_bytes()
                    if existing:
                        output.write_bytes(before)
                    switches = ["--tiles", 16, 16777216] + (["--bigtiff"] if big else [])
                    result = run(args.specsep, *switches, output, 0, planar,
                                 root / "gray8_", 1, 3, 1, success=False)
                    assert b"maximum 256 MiB" in result.stderr
                    assert output.read_bytes() == before if existing else not output.exists()

        profile = root / "display.icc"
        run(args.fromxml, pathlib.Path(args.source) / "Testing" / "Display" / "LCDDisplay.xml", profile)
        for threads in (1, 2):
            strip = root / f"apply-strip-{threads}.tif"
            tile = root / f"apply-tile-{threads}.tif"
            base = {
                "imageFiles": {
                    "srcImageFile": str(root / "tiles.tif"), "dstImageFile": str(strip),
                    "dstEncoding": "8Bit", "dstCompression": True, "dstPlanar": False,
                    "dstEmbedIcc": True,
                },
                "connect": {"threads": threads},
                "profileSequence": [
                    {"iccFile": str(profile), "intent": "relative"},
                    {"iccFile": str(profile), "intent": "relative"},
                ],
            }
            config = root / "apply.json"
            config.write_text(json.dumps(base), encoding="ascii")
            run(args.apply, "-cfg", config)
            base["imageFiles"].update(dstImageFile=str(tile), dstTileWidth=16,
                                      dstTileHeight=32, dstBigTiff=True)
            # Exercise integral JSON float syntax through the real CLI too.
            base["imageFiles"]["dstTileWidth"] = 16.0
            config.write_text(json.dumps(base), encoding="ascii")
            run(args.apply, "-cfg", config)
            run(args.fixture, "--compare", strip, tile)
            exported = root / f"applied-{threads}.icc"
            info = json.loads(run(args.dump, "--evidence-json", tile, exported).stdout)
            assert info["tiled"] and info["bigTiff"] and info["outputDigest"]
            assert exported.read_bytes() == profile.read_bytes()
            if threads == 2:
                run(args.fixture, "--compare", root / "apply-tile-1.tif", tile)
            base["imageFiles"]["dstTileWidth"] = -16
            config.write_text(json.dumps(base), encoding="ascii")
            run(args.apply, "-cfg", config, success=False)
            failed_output = root / "failed-output.tif"
            base["imageFiles"].update(srcImageFile=str(unknown), dstImageFile=str(failed_output),
                                      dstTileWidth=16)
            config.write_text(json.dumps(base), encoding="ascii")
            run(args.apply, "-cfg", config, success=False)
            assert not failed_output.exists()

        # Fail after the first tile band has been decoded and written. This
        # output is incomplete and must still be discarded.
        broken = root / "broken-second-band.tif"
        broken.write_bytes((root / "tiles.tif").read_bytes())
        replace_tile_offset(broken, 3, 0x7FFFFFFF)
        partial_output = root / "partial-output.tif"
        base["imageFiles"].update(srcImageFile=str(broken), dstImageFile=str(partial_output),
                                  dstTileWidth=16, dstTileHeight=32, dstBigTiff=True,
                                  dstPlanar=False)
        config.write_text(json.dumps(base), encoding="ascii")
        partial = run(args.apply, "-cfg", config, success=False)
        assert b"Error reading line 32" in partial.stdout
        assert not partial_output.exists()

        if b"--evidence-file" in run(args.apply, "--help").stdout:
            missing_evidence = root / "missing-sidecar-dir" / "evidence.json"
            assert not missing_evidence.parent.exists()
            for existing in (False, True):
                output = root / f"evidence-failed-{existing}.tif"
                if existing:
                    output.write_bytes((root / "tiles.tif").read_bytes())
                base["imageFiles"].update(srcImageFile=str(root / "tiles.tif"),
                                          dstImageFile=str(output))
                config.write_text(json.dumps(base), encoding="ascii")
                telemetry = root / f"evidence-failed-{existing}.jsonl"
                failed = run(args.apply, "--telemetry=jsonl", "--telemetry-file", telemetry,
                             "--evidence-file", missing_evidence,
                             "-cfg", config, success=False)
                assert b"Unable to create evidence file" in failed.stderr
                run(args.fixture, "--compare", root / "apply-tile-1.tif", output)
                digest = "sha256:" + hashlib.sha256(output.read_bytes()).hexdigest()
                events = [json.loads(line) for line in telemetry.read_text(encoding="ascii").splitlines()]
                assert any(event.get("event") == "output_closed" and
                           event.get("output_digest") == digest for event in events)
                assert events[-1]["event"] == "run_failed"
                assert events[-1]["reason"] == "evidence_write_failed"
            print("[OK] finalized TIFF preserved after evidence sidecar failure")
        else:
            print("[SKIP] TIFF evidence sidecar cleanup (telemetry unavailable)")

        for big in (False, True):
            for planar in (False, True):
                for existing in (False, True):
                    output = root / f"apply-oversized-{big}-{planar}-{existing}.tif"
                    before = (root / "tiles.tif").read_bytes()
                    if existing:
                        output.write_bytes(before)
                    base["imageFiles"].update(srcImageFile=str(root / "tiles.tif"),
                                              dstImageFile=str(output), dstTileWidth=16,
                                              dstTileHeight=16777216, dstBigTiff=big,
                                              dstPlanar=planar)
                    config.write_text(json.dumps(base), encoding="ascii")
                    result = run(args.apply, "-cfg", config, success=False)
                    assert b"maximum 256 MiB" in result.stderr
                    assert output.read_bytes() == before if existing else not output.exists()
    print("[OK] TIFF dump, apply, and spectral separator tile CLI regression")


if __name__ == "__main__":
    main()
