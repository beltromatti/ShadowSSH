#!/usr/bin/env python3
"""Generate the ShadowSSH app icon (all platforms).

Design: solid black square, with "Shadow" in mid-gray and "SSH" in white
rendered as one continuous word "ShadowSSH" centered both horizontally and
vertically. The result is rendered at 1024x1024 and exported as PNG. The
.icns and .ico bundles are produced by the helper scripts below.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

OUT_DIR = Path(__file__).resolve().parent.parent / "assets" / "icon"
OUT_DIR.mkdir(parents=True, exist_ok=True)

CANVAS = 1024
BG = (0, 0, 0, 255)        # solid black
SHADOW_COLOR = (155, 155, 155, 255)  # mid gray
SSH_COLOR = (255, 255, 255, 255)     # pure white

FONT_CANDIDATES = [
    "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    "/System/Library/Fonts/Supplemental/Verdana Bold.ttf",
    "/System/Library/Fonts/Supplemental/Tahoma Bold.ttf",
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/Helvetica.ttc",
    "/Library/Fonts/Arial Bold.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    "C:\\Windows\\Fonts\\arialbd.ttf",
]


def load_font(size: int) -> ImageFont.FreeTypeFont:
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            try:
                return ImageFont.truetype(path, size=size)
            except OSError:
                continue
    return ImageFont.load_default()


def measure(draw: ImageDraw.ImageDraw, text: str, font) -> tuple[int, int]:
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0], bbox[3] - bbox[1]


def fit_font(text: str, max_width: int, max_height: int) -> ImageFont.FreeTypeFont:
    lo, hi = 16, max_height
    best = load_font(lo)
    probe = Image.new("RGBA", (max_width, max_height))
    draw = ImageDraw.Draw(probe)
    while lo <= hi:
        mid = (lo + hi) // 2
        candidate = load_font(mid)
        w, h = measure(draw, text, candidate)
        if w <= max_width and h <= max_height:
            best = candidate
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def render_icon() -> Image.Image:
    img = Image.new("RGBA", (CANVAS, CANVAS), BG)
    draw = ImageDraw.Draw(img)

    margin = int(CANVAS * 0.04)
    inner_w = CANVAS - 2 * margin
    inner_h = int(CANVAS * 0.55)

    full = "ShadowSSH"
    font = fit_font(full, inner_w, inner_h)

    # Use the actual glyph bounding box for true geometric centring.
    full_bbox = draw.textbbox((0, 0), full, font=font, anchor="lt")
    full_w = full_bbox[2] - full_bbox[0]
    full_h = full_bbox[3] - full_bbox[1]

    # Top-left of the rendered glyphs once we anchor at (left, top).
    base_x = (CANVAS - full_w) // 2 - full_bbox[0]
    base_y = (CANVAS - full_h) // 2 - full_bbox[1]

    shadow = "Shadow"
    ssh = "SSH"
    sb = draw.textbbox((0, 0), shadow, font=font, anchor="lt")
    shadow_advance = sb[2] - sb[0]

    draw.text((base_x, base_y), shadow, font=font, fill=SHADOW_COLOR, anchor="lt")
    draw.text((base_x + shadow_advance, base_y), ssh, font=font, fill=SSH_COLOR, anchor="lt")

    return img


def main() -> int:
    img = render_icon()
    base = OUT_DIR / "appicon.png"
    img.save(base, "PNG")

    # Common sizes for downstream packaging
    for size in (16, 32, 48, 64, 128, 256, 512):
        img.resize((size, size), Image.LANCZOS).save(OUT_DIR / f"appicon-{size}.png", "PNG")

    # Windows .ico (multi-resolution)
    img.save(
        OUT_DIR / "appicon.ico",
        format="ICO",
        sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
    )

    print(f"Wrote icons under {OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
