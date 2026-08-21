#!/usr/bin/env python3
"""Run a bounded iccSpecSepToTiff option and output-validation matrix."""

import argparse
import collections
import os
import pathlib
import shutil
import subprocess
import sys
import time

import numpy as np
import tifffile


# iccSpecSepToTiff's main() returns -1 on every error path, which the process
# status reports as 255.  #2233 kept that #1514 contract deliberately rather than
# standardizing on EXIT_FAILURE, because three shipped regression suites pin it.
REJECT_EXIT = 255

SANITIZER_MARKERS = (
    "ERROR: AddressSanitizer",
    "LeakSanitizer: detected memory leaks",
    "runtime error:",
)


class Campaign:
    def __init__(self, tools_dir, output_dir, duration):
        self.tools_dir = tools_dir
        self.output_dir = output_dir
        self.duration = duration
        self.specsep = tools_dir / "IccSpecSepToTiff" / "iccSpecSepToTiff"
        self.tiffdump = tools_dir / "IccTiffDump" / "iccTiffDump"
        self.repo_root = pathlib.Path(__file__).resolve().parents[2]
        self.profile = self.repo_root / "Testing" / "sRGB_v4_ICC_preference.icc"
        self.text_profile = (
            self.repo_root / "Tools" / "CmdLine" / "IccSpecSepToTiff" / "Readme.md"
        )
        self.inputs_dir = output_dir / "inputs"
        self.outputs_dir = output_dir / "outputs"
        self.logs_dir = output_dir / "logs"
        self.inspection_dir = output_dir / "inspection"
        self.counts = collections.Counter()
        self.failures = []
        self.iterations = 0
        self.started = time.monotonic()
        self.datasets = {}
        self.audit_data = {}

    def prepare(self):
        for directory in (
            self.inputs_dir,
            self.outputs_dir,
            self.logs_dir,
            self.inspection_dir,
        ):
            directory.mkdir(parents=True, exist_ok=True)

        for required in (self.specsep, self.tiffdump, self.profile, self.text_profile):
            if not required.is_file():
                raise RuntimeError(f"missing required file: {required}")

        self.datasets = {
            "u8-black": self.write_sequence("u8-black", np.uint8, "minisblack"),
            "u8-white": self.write_sequence("u8-white", np.uint8, "miniswhite"),
            "u16-black": self.write_sequence("u16-black", np.uint16, "minisblack"),
            "u16-white": self.write_sequence("u16-white", np.uint16, "miniswhite"),
            "f32-black": self.write_sequence("f32-black", np.float32, "minisblack"),
        }
        self.write_mismatch_sequences()
        self.write_spec_audit_inputs()
        (self.inputs_dir / "empty-profile.icc").write_bytes(b"")

    def make_array(self, dtype, channel, width=7, height=5):
        pixels = np.arange(width * height, dtype=np.float64).reshape(height, width)
        if dtype == np.float32:
            return (channel * 1.25 + pixels / 128.0).astype(dtype)
        limit = np.iinfo(dtype).max
        return ((channel * 97 + pixels * 13) % (limit + 1)).astype(dtype)

    def write_tiff(
        self,
        path,
        array,
        photometric,
        resolution=(144.0, 96.0),
        resolutionunit="INCH",
        extratags=None,
    ):
        tifffile.imwrite(
            path,
            array,
            photometric=photometric,
            compression=None,
            metadata=None,
            resolution=resolution,
            resolutionunit=resolutionunit,
            rowsperstrip=1,
            extratags=extratags,
        )

    def write_sequence(self, name, dtype, photometric):
        root = self.inputs_dir / name
        root.mkdir(parents=True, exist_ok=True)
        arrays = {}
        for channel in range(1, 6):
            array = self.make_array(dtype, channel)
            self.write_tiff(root / f"spec_{channel}", array, photometric)
            arrays[channel] = array
        return {
            "prefix": root / "spec_",
            "arrays": arrays,
            "dtype": np.dtype(dtype),
            "photometric": photometric,
        }

    def write_mismatch_sequences(self):
        base = self.make_array(np.uint8, 1)
        cases = {
            "width": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint8, 2, width=8), "minisblack", (144.0, 96.0)),
            ),
            "height": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint8, 2, height=6), "minisblack", (144.0, 96.0)),
            ),
            "bits": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint16, 2), "minisblack", (144.0, 96.0)),
            ),
            "photo": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint8, 2), "miniswhite", (144.0, 96.0)),
            ),
            "xres": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint8, 2), "minisblack", (72.0, 96.0)),
            ),
            "yres": (
                (base, "minisblack", (144.0, 96.0)),
                (self.make_array(np.uint8, 2), "minisblack", (144.0, 72.0)),
            ),
        }
        for name, sequence in cases.items():
            root = self.inputs_dir / f"mismatch-{name}"
            root.mkdir(parents=True, exist_ok=True)
            for channel, (array, photometric, resolution) in enumerate(sequence, 1):
                self.write_tiff(root / f"spec_{channel}", array, photometric, resolution)

        root = self.inputs_dir / "multisample"
        root.mkdir(parents=True, exist_ok=True)
        rgb = np.stack((base, base + 1, base + 2), axis=-1)
        self.write_tiff(root / "spec_1", rgb, "rgb")

    def write_spec_audit_inputs(self):
        cm_root = self.inputs_dir / "audit-centimeter"
        float_white_root = self.inputs_dir / "audit-float-white"
        cm_root.mkdir(parents=True, exist_ok=True)
        float_white_root.mkdir(parents=True, exist_ok=True)
        cm_arrays = {}
        float_white_arrays = {}
        for channel in range(1, 4):
            cm_array = self.make_array(np.uint8, channel, width=4, height=3)
            float_white_array = np.linspace(
                0.05 * channel,
                0.75 + 0.05 * channel,
                12,
                dtype=np.float32,
            ).reshape(3, 4)
            self.write_tiff(
                cm_root / f"spec_{channel}",
                cm_array,
                "minisblack",
                resolution=(60.0, 40.0),
                resolutionunit="CENTIMETER",
            )
            self.write_tiff(
                float_white_root / f"spec_{channel}",
                float_white_array,
                "miniswhite",
                resolution=(60.0, 40.0),
                extratags=(
                    (340, "f", 1, 0.0, False),
                    (341, "f", 1, 1.0, False),
                ),
            )
            cm_arrays[channel] = cm_array
            float_white_arrays[channel] = float_white_array
        self.audit_data = {
            "centimeter": {"prefix": cm_root / "spec_", "arrays": cm_arrays},
            "float-white": {
                "prefix": float_white_root / "spec_",
                "arrays": float_white_arrays,
            },
        }

    def command(self, arguments, timeout=20):
        environment = os.environ.copy()
        result = subprocess.run(
            [str(self.specsep), *[str(value) for value in arguments]],
            capture_output=True,
            text=True,
            timeout=timeout,
            env=environment,
            check=False,
        )
        combined = result.stdout + result.stderr
        sanitizer = next((item for item in SANITIZER_MARKERS if item in combined), None)
        return result, sanitizer

    def record(self, category, name, passed, detail):
        self.counts[f"{category}.total"] += 1
        if passed:
            self.counts[f"{category}.pass"] += 1
        else:
            self.counts[f"{category}.fail"] += 1
            if len(self.failures) < 100:
                self.failures.append(f"{category}/{name}: {detail}")

    def expected_pixels(self, dataset, order):
        arrays = [dataset["arrays"][channel] for channel in order]
        if dataset["photometric"] == "miniswhite":
            limit = np.iinfo(dataset["dtype"]).max
            arrays = [limit - array for array in arrays]
        return np.stack(arrays, axis=-1)

    def validate_output(self, path, dataset, order, compress, separate, profile):
        expected = self.expected_pixels(dataset, order)
        with tifffile.TiffFile(path) as image:
            if len(image.pages) != 1:
                return False, f"expected one IFD, found {len(image.pages)}"
            page = image.pages[0]
            pixels = page.asarray()
            if separate and len(order) > 1:
                pixels = np.moveaxis(pixels, 0, -1)
            if len(order) == 1 and pixels.ndim == 2:
                pixels = pixels[:, :, np.newaxis]

            # .get(), like every other tag on these lines: a missing
            # XResolution is exactly the regression these two checks exist to
            # catch, and page.tags[282] would raise KeyError past main()'s
            # handler -- an unhandled traceback and no matrix-summary.txt
            # instead of a recorded failure.
            x_tag = page.tags.get(282)
            y_tag = page.tags.get(283)
            if x_tag is None or y_tag is None:
                return False, "XResolution or YResolution tag absent"
            x_resolution = x_tag.value
            y_resolution = y_tag.value
            if isinstance(x_resolution, tuple):
                x_resolution = x_resolution[0] / x_resolution[1]
            if isinstance(y_resolution, tuple):
                y_resolution = y_resolution[0] / y_resolution[1]
            resolution_unit = page.tags.get(296)
            orientation = page.tags.get(274)
            rows_per_strip = page.tags.get(278)
            predictor = page.tags.get(317)
            expected_predictor = (
                3
                if compress and dataset["dtype"] == np.dtype(np.float32)
                else 2 if compress else 1
            )

            checks = {
                "shape": pixels.shape == expected.shape,
                "pixels": np.array_equal(pixels, expected),
                "width": page.imagewidth == expected.shape[1],
                "height": page.imagelength == expected.shape[0],
                "bits": page.bitspersample == dataset["dtype"].itemsize * 8,
                "samples": page.samplesperpixel == len(order),
                "planar": int(page.planarconfig) == (2 if separate else 1),
                "compression": int(page.compression) == (5 if compress else 1),
                "photometric": int(page.photometric) == 1,
                "sampleformat": int(page.sampleformat) == (
                    3 if dataset["dtype"] == np.dtype(np.float32) else 1
                ),
                "extrasamples": len(page.extrasamples) == max(0, len(order) - 1)
                and all(int(value) == 0 for value in page.extrasamples),
                "x-resolution": abs(float(x_resolution) - 144.0) < 0.001,
                "y-resolution": abs(float(y_resolution) - 96.0) < 0.001,
                "resolution-unit": resolution_unit is None
                or int(resolution_unit.value) == 2,
                "orientation": orientation is None or int(orientation.value) == 1,
                "rows-per-strip": rows_per_strip is not None
                and int(rows_per_strip.value) == 1,
                "predictor": (int(predictor.value) if predictor else 1)
                == expected_predictor,
            }
            icc_tag = page.tags.get(34675)
            if profile:
                checks["profile"] = icc_tag is not None and icc_tag.value == self.profile.read_bytes()
            else:
                checks["profile"] = icc_tag is None

        failed = [name for name, passed in checks.items() if not passed]
        if failed:
            return False, "failed checks: " + ", ".join(failed)
        return True, "metadata and pixels match"

    def run_success(self, dataset_name, range_name, range_values, compress, separate, profile=False):
        dataset = self.datasets[dataset_name]
        start, end, increment, order = range_values
        profile_name = "profile" if profile else "no-profile"
        case_name = (
            f"{dataset_name}-{range_name}-c{compress}-s{separate}-{profile_name}"
        )
        output = self.outputs_dir / f"{case_name}.tif"
        arguments = [
            output,
            compress,
            separate,
            dataset["prefix"],
            start,
            end,
            increment,
        ]
        if profile:
            arguments.append(self.profile)

        result, sanitizer = self.command(arguments)
        if sanitizer:
            self.record("success", case_name, False, f"sanitizer marker: {sanitizer}")
            return
        if result.returncode != 0 or not output.is_file():
            detail = f"exit={result.returncode}; stderr={result.stderr.strip()[:240]}"
            self.record("success", case_name, False, detail)
            return

        summary_fields = (
            f"Output:            {output}",
            f"BitsPerSample:     {dataset['dtype'].itemsize * 8}",
            f"SamplesPerPixel:   {len(order)}",
            f"Planar:            {'separate' if separate else 'interleaved'}",
            f"Compression:       {'LZW' if compress else 'none'}",
            f"Profile:           {'embedded' if profile else 'none'}",
            "Image successfully written!",
        )
        missing = [field for field in summary_fields if field not in result.stdout]
        if missing:
            self.record("success", case_name, False, f"summary missing: {missing[0]}")
            return

        passed, detail = self.validate_output(
            output, dataset, order, compress, separate, profile
        )
        self.record("success", case_name, passed, detail)

    def run_reject(self, name, arguments, expected):
        output = pathlib.Path(arguments[0]) if arguments else None
        preexisting = output is not None and (output.exists() or output.is_symlink())
        if output and output.exists() and output.is_file() and not output.is_symlink():
            output.unlink()
        result, sanitizer = self.command(arguments)
        passed = (
            sanitizer is None
            and result.returncode == REJECT_EXIT
            and expected in result.stderr
            and (preexisting or output is None or not output.exists())
        )
        detail = (
            f"exit={result.returncode}; sanitizer={sanitizer}; "
            f"stderr={result.stderr.strip()[:240]}"
        )
        self.record("reject", name, passed, detail)

    def run_metadata(self):
        cases = (
            ("short-help", ["-h"], 0, "Usage:", "stdout"),
            ("long-help", ["--help"], 0, "Limits:", "stdout"),
            ("version", ["--version"], 0, "iccSpecSepToTiff", "stdout"),
            ("no-args", [], REJECT_EXIT, "Missing arguments:", "stderr"),
            ("too-few", ["out.tif", "0", "0"], REJECT_EXIT, "Usage:", "stderr"),
        )
        for name, arguments, exit_code, expected, stream in cases:
            result, sanitizer = self.command(arguments)
            text = result.stdout if stream == "stdout" else result.stderr
            passed = sanitizer is None and result.returncode == exit_code and expected in text
            self.record(
                "metadata",
                name,
                passed,
                f"exit={result.returncode}; sanitizer={sanitizer}",
            )

    def run_rejections(self):
        prefix = self.datasets["u8-black"]["prefix"]
        reject_dir = self.outputs_dir / "reject-directory"
        reject_dir.mkdir(exist_ok=True)
        cases = (
            ("compress-two", [self.outputs_dir / "bad-compress.tif", 2, 0, prefix, 1, 3, 1], "Invalid boolean"),
            ("sep-negative", [self.outputs_dir / "bad-sep.tif", 0, -1, prefix, 1, 3, 1], "Invalid boolean"),
            ("increment-zero", [self.outputs_dir / "step-zero.tif", 0, 0, prefix, 1, 3, 0], "increment cannot be zero"),
            ("wrong-positive", [self.outputs_dir / "wrong-positive.tif", 0, 0, prefix, 3, 1, 1], "Bad steps values"),
            ("wrong-negative", [self.outputs_dir / "wrong-negative.tif", 0, 0, prefix, 1, 3, -1], "Bad steps values"),
            ("non-divisible", [self.outputs_dir / "non-divisible.tif", 0, 0, prefix, 1, 5, 3], "does not land on end"),
            ("float-range", [self.outputs_dir / "float.tif", 0, 0, prefix, "1.0", 3, 1], "Invalid channel range"),
            ("plus-range", [self.outputs_dir / "plus.tif", 0, 0, prefix, "+1", 3, 1], "Invalid channel range"),
            ("hex-range", [self.outputs_dir / "hex.tif", 0, 0, prefix, "0x1", 3, 1], "Invalid channel range"),
            ("overflow-range", [self.outputs_dir / "overflow.tif", 0, 0, prefix, 2147483648, 3, 1], "Invalid channel range"),
            ("missing-input", [self.outputs_dir / "missing-input.tif", 0, 0, self.inputs_dir / "missing/spec_", 1, 3, 1], "Cannot open input"),
            ("missing-profile", [self.outputs_dir / "missing-profile.tif", 0, 0, prefix, 1, 3, 1, self.inputs_dir / "missing.icc"], "Cannot open profile"),
            ("empty-profile", [self.outputs_dir / "empty-profile.tif", 0, 0, prefix, 1, 3, 1, self.inputs_dir / "empty-profile.icc"], "zero-length ICC data"),
            ("text-profile", [self.outputs_dir / "text-profile.tif", 0, 0, prefix, 1, 3, 1, self.text_profile], "Cannot parse profile"),
            ("profile-mismatch", [self.outputs_dir / "profile-mismatch.tif", 0, 0, prefix, 1, 5, 1, self.profile], "do not match TIFF SamplesPerPixel"),
            ("directory-output", [reject_dir, 0, 0, prefix, 1, 3, 1], "Output image must be a regular file"),
            ("missing-parent", [self.outputs_dir / "missing/child/out.tif", 0, 0, prefix, 1, 3, 1], "Unable to create"),
            ("mismatch-width", [self.outputs_dir / "mismatch-width.tif", 0, 0, self.inputs_dir / "mismatch-width/spec_", 1, 2, 1], "does not have same format"),
            ("mismatch-height", [self.outputs_dir / "mismatch-height.tif", 0, 0, self.inputs_dir / "mismatch-height/spec_", 1, 2, 1], "does not have same format"),
            ("mismatch-bits", [self.outputs_dir / "mismatch-bits.tif", 0, 0, self.inputs_dir / "mismatch-bits/spec_", 1, 2, 1], "does not have same format"),
            ("mismatch-photo", [self.outputs_dir / "mismatch-photo.tif", 0, 0, self.inputs_dir / "mismatch-photo/spec_", 1, 2, 1], "does not have same format"),
            ("mismatch-xres", [self.outputs_dir / "mismatch-xres.tif", 0, 0, self.inputs_dir / "mismatch-xres/spec_", 1, 2, 1], "does not have same format"),
            ("mismatch-yres", [self.outputs_dir / "mismatch-yres.tif", 0, 0, self.inputs_dir / "mismatch-yres/spec_", 1, 2, 1], "does not have same format"),
            ("multisample-input", [self.outputs_dir / "multisample.tif", 0, 0, self.inputs_dir / "multisample/spec_", 1, 1, 1], "does not have 1 sample per pixel"),
        )
        for name, arguments, expected in cases:
            self.run_reject(name, arguments, expected)


    # Iterations are byte-identical by design: the case set is deterministic and
    # writes to fixed paths, so --seconds buys repetition, not coverage.  That is
    # the point -- it is a soak that surfaces nondeterministic or flaky
    # behaviour.  Widening coverage means adding cases, not raising --seconds.
    def run_iteration(self):
        self.run_metadata()
        ranges = {
            "ascending": (1, 3, 1, [1, 2, 3]),
            "descending": (3, 1, -1, [3, 2, 1]),
            "sparse": (1, 5, 2, [1, 3, 5]),
            "single": (4, 4, 1, [4]),
        }
        for dataset_name in self.datasets:
            for range_name, range_values in ranges.items():
                for compress in (0, 1):
                    for separate in (0, 1):
                        self.run_success(
                            dataset_name,
                            range_name,
                            range_values,
                            compress,
                            separate,
                        )

        for compress in (0, 1):
            for separate in (0, 1):
                self.run_success(
                    "u8-black",
                    "ascending",
                    ranges["ascending"],
                    compress,
                    separate,
                    profile=True,
                )
        self.run_rejections()
        self.iterations += 1

    def run_spec_audits(self):
        cm_output = self.outputs_dir / "audit-centimeter-output.tif"
        cm_data = self.audit_data["centimeter"]
        result, sanitizer = self.command(
            [cm_output, 0, 0, cm_data["prefix"], 1, 3, 1]
        )
        cm_passed = False
        cm_detail = f"exit={result.returncode}; sanitizer={sanitizer}"
        if result.returncode == 0 and sanitizer is None and cm_output.is_file():
            with tifffile.TiffFile(cm_output) as image:
                unit = image.pages[0].tags.get(296)
                cm_passed = unit is not None and int(unit.value) == 3
                cm_detail = (
                    f"ResolutionUnit={int(unit.value) if unit is not None else 'absent'}; "
                    "expected centimeter (3)"
                )
        self.record(
            "specification",
            "centimeter-resolution-unit-preserved",
            cm_passed,
            cm_detail,
        )

        white_output = self.outputs_dir / "audit-float-white-output.tif"
        white_data = self.audit_data["float-white"]
        result, sanitizer = self.command(
            [white_output, 0, 0, white_data["prefix"], 1, 3, 1]
        )
        white_passed = result.returncode == REJECT_EXIT and sanitizer is None and not white_output.exists()
        white_detail = f"exit={result.returncode}; sanitizer={sanitizer}"
        if result.returncode == 0 and sanitizer is None and white_output.is_file():
            actual = tifffile.imread(white_output)
            source = np.stack(
                [white_data["arrays"][channel] for channel in (1, 2, 3)],
                axis=-1,
            )
            expected = 1.0 - source
            if np.array_equal(actual, expected):
                white_passed = True
                white_detail = "accepted with exact normalized MinIsWhite conversion"
            else:
                max_error = float(np.nanmax(np.abs(actual - expected)))
                white_detail = (
                    f"accepted with byte-corrupted float samples; max_abs_error={max_error:.9g}; "
                    f"range={float(np.nanmin(actual)):.9g}..{float(np.nanmax(actual)):.9g}"
                )
        self.record(
            "specification",
            "floating-miniswhite-safe-conversion-or-rejection",
            white_passed,
            white_detail,
        )

    def inspect_representatives(self):
        names = (
            "u8-black-ascending-c0-s0-no-profile",
            "u16-white-descending-c1-s1-no-profile",
            "f32-black-sparse-c1-s0-no-profile",
            "u8-black-ascending-c1-s1-profile",
        )
        tiffinfo = shutil.which("tiffinfo")
        for name in names:
            path = self.outputs_dir / f"{name}.tif"
            commands = (("iccTiffDump", str(self.tiffdump)),)
            if tiffinfo:
                commands += (("tiffinfo", tiffinfo),)
            for label, executable in commands:
                result = subprocess.run(
                    [executable, str(path)],
                    capture_output=True,
                    text=True,
                    timeout=20,
                    check=False,
                )
                text = result.stdout + result.stderr
                report = self.inspection_dir / f"{name}.{label}.txt"
                report.write_text(text, encoding="ascii", errors="backslashreplace")
                passed = result.returncode == 0 and "Bits" in text and "Samples" in text
                self.record(
                    "inspection",
                    f"{name}-{label}",
                    passed,
                    f"exit={result.returncode}; report={report}",
                )

    def write_summary(self):
        elapsed = time.monotonic() - self.started
        total = sum(value for key, value in self.counts.items() if key.endswith(".total"))
        passed = sum(value for key, value in self.counts.items() if key.endswith(".pass"))
        failed = sum(value for key, value in self.counts.items() if key.endswith(".fail"))
        lines = [
            "iccSpecSepToTiff option and output matrix",
            "==========================================",
            "",
            f"duration requested: {self.duration} seconds",
            f"elapsed: {elapsed:.3f} seconds",
            f"iterations: {self.iterations}",
            f"cases: {total}",
            f"passed: {passed}",
            f"failed: {failed}",
            f"tool: {self.specsep}",
            f"outputs: {self.outputs_dir}",
            f"inspection: {self.inspection_dir}",
            "",
            "category totals:",
        ]
        for category in (
            "metadata",
            "success",
            "reject",
            "inspection",
            "specification",
        ):
            lines.append(
                f"  {category}: {self.counts[f'{category}.pass']} passed, "
                f"{self.counts[f'{category}.fail']} failed, "
                f"{self.counts[f'{category}.total']} total"
            )
        lines.extend(("", "failures:"))
        if self.failures:
            lines.extend(f"  {failure}" for failure in self.failures)
        else:
            lines.append("  none")
        summary = self.output_dir / "matrix-summary.txt"
        summary.write_text("\n".join(lines) + "\n", encoding="ascii")
        print("\n".join(lines))
        print(f"summary: {summary}")
        return failed

    def run(self):
        self.prepare()
        deadline = self.started + self.duration
        while self.iterations == 0 or time.monotonic() < deadline:
            self.run_iteration()
            elapsed = time.monotonic() - self.started
            print(
                f"iteration={self.iterations} elapsed={elapsed:.1f}s "
                f"success={self.counts['success.pass']} failures={len(self.failures)}",
                flush=True,
            )
        self.run_spec_audits()
        self.inspect_representatives()
        return self.write_summary()


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tools-dir", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    parser.add_argument("--seconds", type=int, default=300)
    arguments = parser.parse_args()
    if arguments.seconds < 0 or arguments.seconds > 3600:
        parser.error("--seconds must be between 0 and 3600")
    return arguments


def main():
    arguments = parse_args()
    campaign = Campaign(
        arguments.tools_dir.resolve(),
        arguments.output_dir.resolve(),
        arguments.seconds,
    )
    try:
        failures = campaign.run()
    except (OSError, RuntimeError, subprocess.SubprocessError, tifffile.TiffFileError) as error:
        print(f"campaign setup or execution failed: {error}", file=sys.stderr)
        return 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
