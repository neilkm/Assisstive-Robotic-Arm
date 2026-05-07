#!/usr/bin/env python3
"""
Generate a printable multipage PDF with unique AprilTag 36h11 markers.

The PDF uses vector rectangles for the black tag cells so the 1x1 inch markers
stay crisp when printed at 100% scale.
"""

from __future__ import annotations

import argparse
import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


TAG_FAMILY_NAME = "DICT_APRILTAG_36h11"
DEFAULT_TAG_COUNT = 24
DEFAULT_START_ID = 0
DEFAULT_TAG_SIZE_INCHES = 1.0
DEFAULT_TAGS_PER_PAGE = 12
REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_OUTPUT_PATH = REPO_ROOT / "generated_pdfs/apriltag_36h11_24_tags_1in.pdf"
LETTER_PAGE_SIZE_POINTS = (612.0, 792.0)
POINTS_PER_INCH = 72.0
PAGE_MARGIN_INCHES = 0.5
TAG_LABEL_HEIGHT_INCHES = 0.22
TAG_GAP_INCHES = 0.35
MARKER_RENDER_PIXELS = 140
BLACK_PIXEL_THRESHOLD = 128
PDF_FLOAT_PRECISION = 4


@dataclass(frozen=True)
class TagPlacement:
    tag_id: int
    x_points: float
    y_points: float
    size_points: float


def pdf_number(value: float) -> str:
    text = f"{value:.{PDF_FLOAT_PRECISION}f}"
    return text.rstrip("0").rstrip(".") if "." in text else text


def escape_pdf_text(text: str) -> str:
    return text.replace("\\", "\\\\").replace("(", "\\(").replace(")", "\\)")


def import_opencv():
    try:
        import cv2
    except ImportError as error:
        print(
            "ERROR: Could not import OpenCV Python bindings. Install python3-opencv "
            "on Jetson, or fix the local OpenCV Python installation.",
            file=sys.stderr,
        )
        print(f"OpenCV import error: {error}", file=sys.stderr)
        raise SystemExit(os.EX_SOFTWARE) from error

    return cv2


def require_apriltag_dictionary(cv2):
    if not hasattr(cv2, "aruco"):
        raise RuntimeError(
            "cv2.aruco is missing. Install OpenCV with aruco/objdetect support."
        )

    if not hasattr(cv2.aruco, TAG_FAMILY_NAME):
        raise RuntimeError(f"OpenCV does not expose {TAG_FAMILY_NAME}.")

    return cv2.aruco.getPredefinedDictionary(getattr(cv2.aruco, TAG_FAMILY_NAME))


def generate_marker(cv2, dictionary, tag_id: int):
    import numpy as np

    marker = np.zeros((MARKER_RENDER_PIXELS, MARKER_RENDER_PIXELS), dtype=np.uint8)
    if hasattr(cv2.aruco, "generateImageMarker"):
        cv2.aruco.generateImageMarker(dictionary, tag_id, MARKER_RENDER_PIXELS, marker, 1)
    elif hasattr(cv2.aruco, "drawMarker"):
        marker = cv2.aruco.drawMarker(dictionary, tag_id, MARKER_RENDER_PIXELS, marker, 1)
    else:
        raise RuntimeError("OpenCV aruco has neither generateImageMarker nor drawMarker.")
    return marker


def compressed_black_runs(marker) -> Iterable[tuple[int, int, int]]:
    black = marker < BLACK_PIXEL_THRESHOLD
    for row_index in range(black.shape[0]):
        col_index = 0
        while col_index < black.shape[1]:
            if not black[row_index, col_index]:
                col_index += 1
                continue

            run_start = col_index
            while col_index < black.shape[1] and black[row_index, col_index]:
                col_index += 1
            yield row_index, run_start, col_index - run_start


def marker_pdf_commands(
    marker, x_points: float, y_points: float, size_points: float
) -> str:
    pixel_size = size_points / marker.shape[0]
    commands = ["0 0 0 rg\n"]

    # Adjacent black pixels in each raster row are emitted as one rectangle.
    for row_index, run_start, run_length in compressed_black_runs(marker):
        rect_x = x_points + run_start * pixel_size
        rect_y = y_points + (marker.shape[0] - row_index - 1) * pixel_size
        rect_w = run_length * pixel_size
        commands.append(
            f"{pdf_number(rect_x)} {pdf_number(rect_y)} "
            f"{pdf_number(rect_w)} {pdf_number(pixel_size)} re f\n"
        )

    return "".join(commands)


def text_pdf_commands(
    text: str, x_points: float, y_points: float, font_size: float = 8.0
) -> str:
    return (
        "BT\n"
        f"/F1 {pdf_number(font_size)} Tf\n"
        f"{pdf_number(x_points)} {pdf_number(y_points)} Td\n"
        f"({escape_pdf_text(text)}) Tj\n"
        "ET\n"
    )


def page_grid(
    page_width: float, page_height: float, tag_size: float
) -> tuple[int, int, float, float]:
    margin = PAGE_MARGIN_INCHES * POINTS_PER_INCH
    label_height = TAG_LABEL_HEIGHT_INCHES * POINTS_PER_INCH
    gap = TAG_GAP_INCHES * POINTS_PER_INCH
    cell_width = tag_size + gap
    cell_height = tag_size + label_height + gap
    columns = max(1, math.floor((page_width - 2.0 * margin + gap) / cell_width))
    rows = max(1, math.floor((page_height - 2.0 * margin + gap) / cell_height))
    return columns, rows, cell_width, cell_height


def placements_for_page(
    tag_ids: Sequence[int], page_width: float, page_height: float
) -> list[TagPlacement]:
    tag_size = DEFAULT_TAG_SIZE_INCHES * POINTS_PER_INCH
    margin = PAGE_MARGIN_INCHES * POINTS_PER_INCH
    columns, rows, cell_width, cell_height = page_grid(page_width, page_height, tag_size)
    placements: list[TagPlacement] = []

    for index, tag_id in enumerate(tag_ids[: columns * rows]):
        row = index // columns
        column = index % columns
        x = margin + column * cell_width
        y = page_height - margin - tag_size - row * cell_height
        placements.append(
            TagPlacement(tag_id=tag_id, x_points=x, y_points=y, size_points=tag_size)
        )

    return placements


def chunked(values: Sequence[int], chunk_size: int) -> Iterable[Sequence[int]]:
    for index in range(0, len(values), chunk_size):
        yield values[index : index + chunk_size]


def build_page_stream(
    cv2,
    dictionary,
    placements: Sequence[TagPlacement],
    page_number: int,
    total_pages: int,
) -> bytes:
    commands = [
        text_pdf_commands(
            f"AprilTag 36h11, 1.000 inch tags. Print at 100% actual size. Page {page_number}/{total_pages}.",
            PAGE_MARGIN_INCHES * POINTS_PER_INCH,
            22.0,
            8.0,
        )
    ]

    for placement in placements:
        marker = generate_marker(cv2, dictionary, placement.tag_id)
        commands.append(
            marker_pdf_commands(
                marker, placement.x_points, placement.y_points, placement.size_points
            )
        )
        commands.append(
            text_pdf_commands(
                f"ID {placement.tag_id}",
                placement.x_points,
                placement.y_points - 11.0,
                8.0,
            )
        )

    return "".join(commands).encode("ascii")


def build_pdf(page_streams: Sequence[bytes], page_size: tuple[float, float]) -> bytes:
    page_width, page_height = page_size
    object_count = 3 + len(page_streams) * 2
    objects: list[bytes] = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids "
        + b"["
        + b" ".join(
            f"{4 + index * 2} 0 R".encode("ascii")
            for index in range(len(page_streams))
        )
        + b"]"
        + f" /Count {len(page_streams)} >>".encode("ascii"),
        b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]

    for index, stream in enumerate(page_streams):
        content_object_id = 5 + index * 2
        page_object = (
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {pdf_number(page_width)} {pdf_number(page_height)}] "
            f"/Resources << /Font << /F1 3 0 R >> >> /Contents {content_object_id} 0 R >>"
        ).encode("ascii")
        stream_object = (
            f"<< /Length {len(stream)} >>\nstream\n".encode("ascii")
            + stream
            + b"\nendstream"
        )
        objects.extend([page_object, stream_object])

    if len(objects) != object_count:
        raise RuntimeError("Internal PDF object count mismatch.")

    pdf = bytearray(b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n")
    offsets = [0]
    for object_id, body in enumerate(objects, start=1):
        offsets.append(len(pdf))
        pdf.extend(f"{object_id} 0 obj\n".encode("ascii"))
        pdf.extend(body)
        pdf.extend(b"\nendobj\n")

    xref_offset = len(pdf)
    pdf.extend(f"xref\n0 {len(objects) + 1}\n".encode("ascii"))
    pdf.extend(b"0000000000 65535 f \n")
    for offset in offsets[1:]:
        pdf.extend(f"{offset:010d} 00000 n \n".encode("ascii"))
    pdf.extend(
        f"trailer\n<< /Size {len(objects) + 1} /Root 1 0 R >>\n"
        f"startxref\n{xref_offset}\n%%EOF\n".encode("ascii")
    )
    return bytes(pdf)


def generate_pdf(output_path: Path, start_id: int, tag_count: int) -> None:
    if tag_count <= 0:
        raise ValueError("tag_count must be greater than zero.")
    if start_id < 0:
        raise ValueError("start_id must be non-negative.")

    cv2 = import_opencv()
    dictionary = require_apriltag_dictionary(cv2)
    page_width, page_height = LETTER_PAGE_SIZE_POINTS
    tag_size = DEFAULT_TAG_SIZE_INCHES * POINTS_PER_INCH
    columns, rows, _, _ = page_grid(page_width, page_height, tag_size)
    tags_per_page = min(DEFAULT_TAGS_PER_PAGE, columns * rows)
    tag_ids = list(range(start_id, start_id + tag_count))
    total_pages = math.ceil(tag_count / tags_per_page)

    page_streams = [
        build_page_stream(
            cv2,
            dictionary,
            placements_for_page(page_tag_ids, page_width, page_height),
            page_index + 1,
            total_pages,
        )
        for page_index, page_tag_ids in enumerate(chunked(tag_ids, tags_per_page))
    ]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(build_pdf(page_streams, LETTER_PAGE_SIZE_POINTS))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a multipage PDF with 24 unique 1x1 inch AprilTag 36h11 markers."
    )
    parser.add_argument("--out", type=Path, default=DEFAULT_OUTPUT_PATH, help="Output PDF path.")
    parser.add_argument(
        "--start-id", type=int, default=DEFAULT_START_ID, help="First AprilTag ID."
    )
    parser.add_argument(
        "--count", type=int, default=DEFAULT_TAG_COUNT, help="Number of unique tags."
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generate_pdf(args.out, args.start_id, args.count)
    print(f"Wrote {args.out}")
    print("Print at 100% scale / Actual Size. Do not use Scale to Fit.")
    return os.EX_OK


if __name__ == "__main__":
    raise SystemExit(main())
