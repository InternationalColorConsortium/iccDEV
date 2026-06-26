#!/usr/bin/env python3
"""Render and convert high-channel spectral TIFF files."""

import argparse
import json
import math
import os
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

import numpy as np
import tifffile
from PIL import Image, ImageDraw, ImageFont


DEFAULT_RED_NM = 650.0
DEFAULT_GREEN_NM = 550.0
DEFAULT_BLUE_NM = 450.0


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Project a high-channel spectral TIFF to an 8-bit RGB preview, "
            "optionally selecting spectral channels and writing a reduced TIFF. "
            "The default false-color mapping uses 650nm, 550nm, and 450nm."
        )
    )
    parser.add_argument("input", type=Path, help="Input spectral TIFF")
    parser.add_argument("output", type=Path, help="Output PNG or TIFF preview")
    parser.add_argument(
        "--start-nm",
        type=float,
        default=380.0,
        help="Wavelength of channel 0 in nm (default: 380)",
    )
    parser.add_argument(
        "--step-nm",
        type=float,
        default=5.0,
        help="Wavelength increment per channel in nm (default: 5)",
    )
    parser.add_argument(
        "--red-nm",
        type=float,
        default=DEFAULT_RED_NM,
        help="Wavelength to map to red (default: 650)",
    )
    parser.add_argument(
        "--green-nm",
        type=float,
        default=DEFAULT_GREEN_NM,
        help="Wavelength to map to green (default: 550)",
    )
    parser.add_argument(
        "--blue-nm",
        type=float,
        default=DEFAULT_BLUE_NM,
        help="Wavelength to map to blue (default: 450)",
    )
    parser.add_argument(
        "--low-percentile",
        type=float,
        default=1.0,
        help="Lower percentile for contrast stretching (default: 1)",
    )
    parser.add_argument(
        "--high-percentile",
        type=float,
        default=99.0,
        help="Upper percentile for contrast stretching (default: 99)",
    )
    parser.add_argument(
        "--no-contrast-stretch",
        action="store_true",
        help="Scale using the input dtype range instead of percentile stretch",
    )
    parser.add_argument(
        "--channels",
        help=(
            "Comma-separated source channel indexes or Python-style slices to keep, "
            "for example 4,6,8 or 4:65:2"
        ),
    )
    parser.add_argument(
        "--wavelengths",
        help=(
            "Inclusive wavelength range to keep as START:END[:STEP] in nm, "
            "for example 400:700:10"
        ),
    )
    parser.add_argument(
        "--channel-count",
        type=int,
        help=(
            "Resample the full source spectral range to this many evenly spaced "
            "channels"
        ),
    )
    parser.add_argument(
        "--converted-tiff",
        type=Path,
        help="Write the selected channels as a spectral TIFF",
    )
    parser.add_argument(
        "--converted-icc-profile",
        type=Path,
        help="ICC profile to embed in --converted-tiff",
    )
    parser.add_argument(
        "--converted-profile-xml-template",
        type=Path,
        help="ICC v5 spectral XML template used to generate a matching profile",
    )
    parser.add_argument(
        "--generated-icc-profile",
        type=Path,
        help="Output path for the generated ICC profile",
    )
    parser.add_argument(
        "--generated-profile-xml",
        type=Path,
        help="Output path for the generated ICC XML profile",
    )
    parser.add_argument(
        "--icc-from-xml",
        default="iccFromXml",
        help="iccFromXml executable for generated ICC profiles (default: iccFromXml)",
    )
    parser.add_argument(
        "--converted-compression",
        choices=("none", "lzw", "deflate", "packbits"),
        default="lzw",
        help="Compression for --converted-tiff (default: lzw)",
    )
    parser.add_argument(
        "--no-converted-predictor",
        action="store_false",
        dest="converted_predictor",
        default=True,
        help="Disable horizontal differencing predictor for --converted-tiff",
    )
    parser.add_argument(
        "--force-tiffcp",
        action="store_true",
        help="Normalize through libtiff tiffcp before decoding",
    )
    parser.add_argument(
        "--export-bands",
        type=Path,
        help="Directory for one grayscale PNG per spectral band",
    )
    parser.add_argument(
        "--contact-sheet",
        type=Path,
        help="Output PNG containing thumbnails for every spectral band",
    )
    parser.add_argument(
        "--metadata",
        type=Path,
        help="Output JSON metadata for generated previews and bands",
    )
    parser.add_argument(
        "--thumb-width",
        type=int,
        default=120,
        help="Band thumbnail width for --contact-sheet (default: 120)",
    )
    parser.add_argument(
        "--contact-cols",
        type=int,
        default=9,
        help="Band thumbnail columns for --contact-sheet (default: 9)",
    )
    return parser.parse_args()


def ensure_output_path(path):
    if path.exists() and path.is_dir():
        raise ValueError(f"output path is a directory: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)


def tag_value(page, name, default=None):
    tag = page.tags.get(name)
    if tag is None:
        return default
    return tag.value


def load_page_array(path):
    with tifffile.TiffFile(path) as tiff:
        if not tiff.pages:
            raise ValueError(f"no TIFF pages found: {path}")
        page = tiff.pages[0]
        axes = page.axes
        samples_per_pixel = int(page.samplesperpixel)
        page_info = {
            "resolution": (
                tag_value(page, "XResolution", (72, 1)),
                tag_value(page, "YResolution", (72, 1)),
            ),
            "resolutionunit": tag_value(page, "ResolutionUnit", 2),
        }
        array = page.asarray()
    return array, axes, samples_per_pixel, page_info


def load_via_tiffcp(path, output_path):
    tiffcp = shutil.which("tiffcp")
    if tiffcp is None:
        raise RuntimeError(
            "LZW TIFF decode requires either python imagecodecs or libtiff tiffcp"
        )
    temp_path = output_path.with_name(f"{output_path.stem}.uncompressed-tmp.tif")
    if temp_path.exists():
        raise FileExistsError(f"temporary file already exists: {temp_path}")
    try:
        subprocess.run(
            [tiffcp, "-c", "none", str(path), str(temp_path)],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        return load_page_array(temp_path)
    finally:
        if temp_path.exists():
            temp_path.unlink()


def load_spectral_array(path, output_path, force_tiffcp):
    if force_tiffcp:
        return load_via_tiffcp(path, output_path), "tiffcp"
    try:
        return load_page_array(path), "direct"
    except ValueError as error:
        if "requires the 'imagecodecs' package" not in str(error):
            raise
        return load_via_tiffcp(path, output_path), "tiffcp"


def move_sample_axis_last(array, axes, samples_per_pixel):
    if array.ndim != 3:
        raise ValueError(f"expected a 3D spectral TIFF array, got shape {array.shape}")
    if "S" in axes:
        sample_axis = axes.index("S")
    else:
        matching_axes = [
            index for index, size in enumerate(array.shape) if size == samples_per_pixel
        ]
        if len(matching_axes) != 1:
            raise ValueError(
                f"cannot identify sample axis for shape {array.shape} and SPP={samples_per_pixel}"
            )
        sample_axis = matching_axes[0]
    if sample_axis != array.ndim - 1:
        array = np.moveaxis(array, sample_axis, -1)
    return array


def wavelength_for_source_index(index, args):
    return args.start_nm + index * args.step_nm


def parse_channel_index(token, channel_count):
    if token == "":
        raise ValueError("empty channel index")
    index = int(token)
    if index < 0:
        index += channel_count
    if index < 0 or index >= channel_count:
        raise ValueError(f"channel index out of range: {token}")
    return index


def parse_channel_slice(token, channel_count):
    parts = token.split(":")
    if len(parts) not in (2, 3):
        raise ValueError(f"invalid channel slice: {token}")
    start = 0 if parts[0] == "" else int(parts[0])
    stop = channel_count if parts[1] == "" else int(parts[1])
    step = 1 if len(parts) == 2 or parts[2] == "" else int(parts[2])
    if step <= 0:
        raise ValueError(f"channel slice step must be positive: {token}")
    if start < 0:
        start += channel_count
    if stop < 0:
        stop += channel_count
    start = min(max(start, 0), channel_count)
    stop = min(max(stop, 0), channel_count)
    return list(range(start, stop, step))


def parse_channel_selection(spec, channel_count):
    indexes = []
    for token in spec.split(","):
        token = token.strip()
        if not token:
            continue
        if ":" in token:
            indexes.extend(parse_channel_slice(token, channel_count))
        else:
            indexes.append(parse_channel_index(token, channel_count))
    if not indexes:
        raise ValueError("--channels selected no channels")
    if len(set(indexes)) != len(indexes):
        raise ValueError("--channels contains duplicate indexes")
    return indexes


def parse_wavelength_selection(spec, args, channel_count):
    if args.step_nm <= 0:
        raise ValueError("--step-nm must be greater than zero")
    parts = spec.split(":")
    if len(parts) not in (2, 3):
        raise ValueError("--wavelengths must use START:END[:STEP]")
    start_nm = float(parts[0])
    end_nm = float(parts[1])
    step_nm = args.step_nm if len(parts) == 2 or parts[2] == "" else float(parts[2])
    if step_nm <= 0:
        raise ValueError("--wavelengths step must be greater than zero")
    if end_nm < start_nm:
        raise ValueError("--wavelengths END must be greater than or equal to START")

    indexes = []
    wavelength = start_nm
    epsilon = step_nm / 1000000.0
    while wavelength <= end_nm + epsilon:
        index = int(round((wavelength - args.start_nm) / args.step_nm))
        if index < 0 or index >= channel_count:
            raise ValueError(f"wavelength out of source range: {wavelength:g}nm")
        actual = wavelength_for_source_index(index, args)
        if abs(actual - wavelength) > args.step_nm / 1000.0:
            raise ValueError(
                f"wavelength {wavelength:g}nm does not align with source step"
            )
        indexes.append(index)
        wavelength += step_nm
    if len(set(indexes)) != len(indexes):
        raise ValueError("--wavelengths selected duplicate channels")
    return indexes


def selected_source_indexes(args, channel_count):
    selectors = [args.channels is not None, args.wavelengths is not None]
    if sum(selectors) > 1:
        raise ValueError("use only one of --channels or --wavelengths")
    if args.channels:
        return parse_channel_selection(args.channels, channel_count)
    if args.wavelengths:
        return parse_wavelength_selection(args.wavelengths, args, channel_count)
    return list(range(channel_count))


def select_or_resample_array(array, args):
    source_channel_count = array.shape[-1]
    source_end_nm = wavelength_for_source_index(source_channel_count - 1, args)
    if args.channel_count is not None:
        if args.channels or args.wavelengths:
            raise ValueError("use only one of --channel-count, --channels, or --wavelengths")
        if args.channel_count < 1:
            raise ValueError("--channel-count must be greater than zero")
        positions = np.linspace(0.0, source_channel_count - 1, args.channel_count)
        left = np.floor(positions).astype(np.int64)
        right = np.ceil(positions).astype(np.int64)
        weight = (positions - left).astype(np.float32)
        selected = (
            array[..., left].astype(np.float32) * (1.0 - weight)
            + array[..., right].astype(np.float32) * weight
        )
        if np.issubdtype(array.dtype, np.integer):
            info = np.iinfo(array.dtype)
            selected = np.rint(selected)
            selected = np.clip(selected, info.min, info.max).astype(array.dtype)
        else:
            selected = selected.astype(array.dtype)
        wavelengths = np.linspace(args.start_nm, source_end_nm, args.channel_count)
        return selected, None, [float(value) for value in wavelengths], True

    source_indexes = selected_source_indexes(args, source_channel_count)
    wavelengths = [wavelength_for_source_index(index, args) for index in source_indexes]
    if len(source_indexes) != source_channel_count:
        array = array[..., source_indexes]
    return array, source_indexes, wavelengths, False


def scale_to_uint8(values, source_dtype, args):
    values = values.astype(np.float32)
    if args.no_contrast_stretch:
        dtype_max = (
            np.iinfo(source_dtype).max
            if np.issubdtype(source_dtype, np.integer)
            else float(values.max())
        )
        low = 0.0
        high = float(dtype_max)
    else:
        low = float(np.percentile(values, args.low_percentile))
        high = float(np.percentile(values, args.high_percentile))

    if high <= low:
        raise ValueError(f"invalid contrast range: low={low} high={high}")
    scaled = np.clip((values - low) / (high - low), 0.0, 1.0)
    return (scaled * 255.0 + 0.5).astype(np.uint8), low, high


def nearest_wavelength_index(wavelength_nm, wavelengths):
    return min(
        range(len(wavelengths)),
        key=lambda index: abs(wavelengths[index] - wavelength_nm),
    )


def make_rgb_preview(array, wavelengths, args):
    indexes = (
        nearest_wavelength_index(args.red_nm, wavelengths),
        nearest_wavelength_index(args.green_nm, wavelengths),
        nearest_wavelength_index(args.blue_nm, wavelengths),
    )
    rgb = array[..., list(indexes)]
    preview, low, high = scale_to_uint8(rgb, array.dtype, args)
    return preview, indexes, low, high


def write_band_previews(array, wavelengths, args):
    if args.export_bands is None:
        return []
    args.export_bands.mkdir(parents=True, exist_ok=True)
    band_info = []
    for index in range(array.shape[-1]):
        wavelength = wavelengths[index]
        band = array[..., index]
        preview, low, high = scale_to_uint8(band, array.dtype, args)
        output = args.export_bands / f"band_{index:03d}_{int(round(wavelength)):04d}nm.png"
        Image.fromarray(preview, mode="L").save(output)
        band_info.append(
            {
                "index": index,
                "wavelength_nm": wavelength,
                "path": str(output),
                "low": low,
                "high": high,
            }
        )
    return band_info


def make_contact_sheet(bands, args):
    if args.contact_sheet is None:
        return None
    if not bands:
        raise ValueError("--contact-sheet requires --export-bands")
    if args.thumb_width < 8:
        raise ValueError("--thumb-width must be at least 8")
    if args.contact_cols < 1:
        raise ValueError("--contact-cols must be at least 1")

    images = []
    labels = []
    for band in bands:
        image = Image.open(band["path"]).convert("L")
        ratio = args.thumb_width / image.width
        thumb_height = max(1, int(round(image.height * ratio)))
        image = image.resize((args.thumb_width, thumb_height), Image.Resampling.LANCZOS)
        images.append(image.convert("RGB"))
        labels.append(f"{band['index']:02d} {band['wavelength_nm']:.0f}nm")

    label_height = 18
    gutter = 6
    cols = args.contact_cols
    rows = int(math.ceil(len(images) / cols))
    cell_width = args.thumb_width + gutter
    cell_height = images[0].height + label_height + gutter
    sheet = Image.new("RGB", (cols * cell_width, rows * cell_height), "white")
    draw = ImageDraw.Draw(sheet)
    font = ImageFont.load_default()

    for index, image in enumerate(images):
        row = index // cols
        col = index % cols
        x = col * cell_width
        y = row * cell_height
        sheet.paste(image, (x, y))
        draw.text((x, y + image.height + 2), labels[index], fill="black", font=font)

    args.contact_sheet.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.contact_sheet)
    return str(args.contact_sheet)


def write_metadata(path, report):
    if path is None:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="ascii")


def set_required_text(root, path, value):
    element = root.find(path)
    if element is None:
        raise ValueError(f"profile XML missing {path}")
    element.text = value


def update_profile_xml(template_path, output_path, wavelengths):
    if len(wavelengths) < 1:
        raise ValueError("cannot generate profile for zero channels")
    if len(wavelengths) > 1:
        step = wavelengths[1] - wavelengths[0]
        for left, right in zip(wavelengths, wavelengths[1:]):
            if abs((right - left) - step) > 0.0001:
                raise ValueError("generated ICC profile requires evenly spaced wavelengths")
    else:
        step = 0.0

    channel_count = len(wavelengths)
    start_nm = wavelengths[0]
    end_nm = wavelengths[-1]
    tree = ET.parse(template_path)
    root = tree.getroot()
    set_required_text(root, "./Header/DataColourSpace", f"nc{channel_count:04X}")
    set_required_text(root, "./Header/SpectralPCS", f"rs{channel_count:04X}")

    range_element = root.find("./Header/SpectralRange/Wavelengths")
    if range_element is None:
        raise ValueError("profile XML missing Header/SpectralRange/Wavelengths")
    range_element.set("start", f"{start_nm:g}")
    range_element.set("end", f"{end_nm:g}")
    range_element.set("steps", str(channel_count))

    description = (
        f"{start_nm:g}nm to {end_nm:g}nm in {step:g}nm steps spectral "
        "reflectance using D50 and standard 2deg observer with Wpt MAT based PCC"
    )
    desc_element = root.find(
        "./Tags/multiLocalizedUnicodeType/LocalizedText[@LanguageCountry='enUS']"
    )
    if desc_element is not None:
        desc_element.text = description

    for mpe_type in root.findall("./Tags/multiProcessElementType"):
        signature = mpe_type.findtext("TagSignature")
        if signature in ("D2B3", "B2D3"):
            mpe = mpe_type.find("MultiProcessElements")
            if mpe is None:
                raise ValueError(f"profile XML missing MultiProcessElements for {signature}")
            mpe.set("InputChannels", str(channel_count))
            mpe.set("OutputChannels", str(channel_count))

    for float_type in root.findall("./Tags/float16NumberType"):
        if float_type.findtext("TagSignature") == "swpt":
            data = float_type.find("Data")
            if data is None:
                raise ValueError("profile XML missing swpt Data")
            data.text = "\n\t\t " + " ".join(["1.000"] * channel_count) + "\n\t\t"

    output_path.parent.mkdir(parents=True, exist_ok=True)
    tree.write(output_path, encoding="ascii", xml_declaration=True)
    return description


def resolve_executable(executable):
    if os.sep in executable:
        path = Path(executable)
        if path.is_file():
            return str(path)
        raise ValueError(f"executable not found: {executable}")
    resolved = shutil.which(executable)
    if resolved is None:
        raise ValueError(f"executable not found in PATH: {executable}")
    return resolved


def generated_profile_path(args, wavelengths):
    if args.converted_profile_xml_template is None:
        return None
    if args.converted_icc_profile is not None:
        raise ValueError(
            "use only one of --converted-icc-profile or "
            "--converted-profile-xml-template"
        )
    if args.generated_icc_profile is None:
        raise ValueError("--generated-icc-profile is required with XML profile templates")
    if not args.converted_profile_xml_template.is_file():
        raise ValueError(f"profile XML template not found: {args.converted_profile_xml_template}")

    xml_output = args.generated_profile_xml
    if xml_output is None:
        xml_output = args.generated_icc_profile.with_suffix(".xml")
    update_profile_xml(args.converted_profile_xml_template, xml_output, wavelengths)
    icc_from_xml = resolve_executable(args.icc_from_xml)
    subprocess.run(
        [icc_from_xml, str(xml_output), str(args.generated_icc_profile)],
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return args.generated_icc_profile


def write_converted_tiff(array, page_info, args, converted_profile):
    if args.converted_tiff is None:
        return None
    ensure_output_path(args.converted_tiff)
    if args.converted_tiff == args.output:
        raise ValueError("--converted-tiff must be different from output preview")

    icc_profile = None
    icc_profile_path = None
    if converted_profile is not None:
        if not converted_profile.is_file():
            raise ValueError(f"ICC profile not found: {converted_profile}")
        icc_profile = converted_profile.read_bytes()
        icc_profile_path = str(converted_profile)

    compression = None if args.converted_compression == "none" else args.converted_compression
    predictor = args.converted_predictor if compression is not None else None
    tifffile.imwrite(
        args.converted_tiff,
        array,
        photometric="minisblack",
        planarconfig="contig",
        compression=compression,
        predictor=predictor,
        resolution=page_info["resolution"],
        resolutionunit=page_info["resolutionunit"],
        metadata=None,
        iccprofile=icc_profile,
    )
    return {
        "path": str(args.converted_tiff),
        "shape": list(array.shape),
        "compression": args.converted_compression,
        "predictor": bool(args.converted_predictor and compression is not None),
        "icc_profile": icc_profile_path,
        "icc_profile_bytes": 0 if icc_profile is None else len(icc_profile),
    }


def main():
    args = parse_args()
    if not args.input.is_file():
        raise SystemExit(f"input file not found: {args.input}")
    if not 0.0 <= args.low_percentile < args.high_percentile <= 100.0:
        raise SystemExit("percentiles must satisfy 0 <= low < high <= 100")

    try:
        ensure_output_path(args.output)
        force_tiffcp = args.force_tiffcp or os.environ.get(
            "ICCDEV_SPECTRAL_PREVIEW_FORCE_TIFFCP"
        ) == "1"
        (array, axes, samples_per_pixel, page_info), decode_path = load_spectral_array(
            args.input, args.output, force_tiffcp
        )
        array = move_sample_axis_last(array, axes, samples_per_pixel)
        source_channel_count = array.shape[-1]
        array, source_indexes, wavelengths, resampled = select_or_resample_array(array, args)
        rgb, indexes, low, high = make_rgb_preview(array, wavelengths, args)
        Image.fromarray(rgb).save(args.output)
        converted_profile = generated_profile_path(args, wavelengths)
        if converted_profile is None:
            converted_profile = args.converted_icc_profile
        converted_tiff = write_converted_tiff(array, page_info, args, converted_profile)
        band_info = write_band_previews(array, wavelengths, args)
        contact_sheet = make_contact_sheet(band_info, args)
        report = {
            "input": str(args.input),
            "output": str(args.output),
            "decode_path": decode_path,
            "array_shape": list(array.shape),
            "axes": axes,
            "samples_per_pixel": samples_per_pixel,
            "source_channel_count": source_channel_count,
            "selected_source_indexes": source_indexes,
            "selected_wavelengths_nm": wavelengths,
            "resampled": resampled,
            "dtype": str(array.dtype),
            "min": int(array.min()),
            "max": int(array.max()),
            "rgb_preview": {
                "indexes": list(indexes),
                "source_indexes": (
                    None
                    if source_indexes is None
                    else [source_indexes[index] for index in indexes]
                ),
                "wavelengths_nm": [wavelengths[index] for index in indexes],
                "low": low,
                "high": high,
            },
            "converted_tiff": converted_tiff,
            "bands": band_info,
            "contact_sheet": contact_sheet,
        }
        write_metadata(args.metadata, report)
    except (
        OSError,
        RuntimeError,
        ValueError,
        FileExistsError,
        subprocess.CalledProcessError,
    ) as error:
        raise SystemExit(f"error: {error}") from error

    print(
        "wrote {output} from shape={shape} axes={axes} SPP={spp} bands=R:{red} G:{green} B:{blue}".format(
            output=args.output,
            shape=array.shape,
            axes=axes,
            spp=samples_per_pixel,
            red=indexes[0],
            green=indexes[1],
            blue=indexes[2],
        )
    )
    if converted_tiff:
        print(f"wrote converted TIFF {converted_tiff['path']}")
    if band_info:
        print(f"wrote {len(band_info)} band previews to {args.export_bands}")
    if contact_sheet:
        print(f"wrote {contact_sheet}")
    if args.metadata:
        print(f"wrote {args.metadata}")


if __name__ == "__main__":
    main()
