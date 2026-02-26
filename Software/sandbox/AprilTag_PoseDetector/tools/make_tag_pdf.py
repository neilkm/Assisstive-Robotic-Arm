#!/usr/bin/env python3
"""
make_tag_pdf.py

Given an AprilTag PNG from apriltag-imgs, generate a PDF that prints the tag
at an exact physical size (default 100mm square for the black region).

Important: User must print at 100% scale ("Actual Size") in Preview.
"""

import argparse
from pathlib import Path

from PIL import Image
from reportlab.pdfgen import canvas
from reportlab.lib.units import mm


def generate_pdf(png_path: Path, out_pdf: Path, tag_size_mm: float = 100.0, dpi: int = 600) -> None:
    if not png_path.exists():
        raise FileNotFoundError(f"Input PNG not found: {png_path}")

    out_pdf.parent.mkdir(parents=True, exist_ok=True)

    # Load original tag (usually already crisp BW)
    im = Image.open(png_path).convert("RGB")

    # Make a high-resolution raster sized exactly for tag_size_mm at dpi.
    # Using NEAREST prevents gray anti-aliased edges.
    px = int(round(tag_size_mm / 25.4 * dpi))
    im_hr = im.resize((px, px), resample=Image.NEAREST)

    tmp_png = out_pdf.with_suffix(".tmp.png")
    im_hr.save(tmp_png, dpi=(dpi, dpi))

    # PDF page size: Letter in mm
    page_w_mm, page_h_mm = 215.9, 279.4

    c = canvas.Canvas(str(out_pdf), pagesize=(page_w_mm * mm, page_h_mm * mm))

    # Center the tag on the page
    x = (page_w_mm - tag_size_mm) / 2.0 * mm
    y = (page_h_mm - tag_size_mm) / 2.0 * mm

    c.drawImage(str(tmp_png), x, y, width=tag_size_mm * mm, height=tag_size_mm * mm)

    # Minimal print note
    c.setFont("Helvetica", 9)
    c.drawString(10 * mm, 10 * mm, f"Print at 100% (Actual Size). Tag black square size: {tag_size_mm:.1f} mm")

    c.showPage()
    c.save()

    tmp_png.unlink(missing_ok=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", required=True, help="Path to tag PNG")
    ap.add_argument("--out", required=True, help="Output PDF path")
    ap.add_argument("--size-mm", type=float, default=100.0, help="Tag size in mm (default 100mm)")
    ap.add_argument("--dpi", type=int, default=600, help="Rasterization DPI (default 600)")
    args = ap.parse_args()

    generate_pdf(Path(args.png), Path(args.out), tag_size_mm=args.size_mm, dpi=args.dpi)
    print(f"Wrote: {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


