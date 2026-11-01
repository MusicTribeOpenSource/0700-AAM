#!/usr/bin/env python3
"""Draws the KICK Surface app icon (a 4x4 pad grid, a few pads lit) and writes a PNG and an .icns.

Usage: make_icon.py [out_dir]   (default: emu/surface/icon)
Needs Pillow. The .icns step needs macOS iconutil and is skipped elsewhere.
"""
import shutil
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZE = 1024
SS = 3  # supersample factor for smooth edges

BG_TOP = (38, 40, 48)
BG_BOTTOM = (16, 17, 21)
PAD_OFF = (44, 47, 56)
PAD_EDGE = (62, 66, 78)
ORANGE = (255, 150, 28)
CYAN = (62, 170, 220)

# (row from top, col) -> colour. Reads as a pattern being played.
LIT = {
    (0, 0): ORANGE, (1, 1): ORANGE, (2, 2): ORANGE, (3, 3): ORANGE,
    (0, 3): CYAN, (3, 0): CYAN,
}


def rounded(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=radius, fill=fill)


def glow(base, box, radius, colour, spread):
    layer = Image.new("RGBA", base.size, (0, 0, 0, 0))
    ImageDraw.Draw(layer).rounded_rectangle(box, radius=radius, fill=colour + (200,))
    layer = layer.filter(ImageFilter.GaussianBlur(spread))
    base.alpha_composite(layer)


def draw_icon() -> Image.Image:
    s = SIZE * SS
    img = Image.new("RGBA", (s, s), (0, 0, 0, 0))

    # macOS-style icon: artwork fills ~82% of the canvas, rounded square.
    inset = int(s * 0.09)
    body = (inset, inset, s - inset, s - inset)
    body_w = s - 2 * inset
    radius = int(body_w * 0.225)

    # vertical gradient clipped to the rounded body
    grad = Image.new("RGBA", (s, s))
    gd = ImageDraw.Draw(grad)
    for y in range(s):
        t = y / (s - 1)
        gd.line([(0, y), (s, y)], fill=tuple(int(BG_TOP[i] + (BG_BOTTOM[i] - BG_TOP[i]) * t) for i in range(3)) + (255,))
    mask = Image.new("L", (s, s), 0)
    rounded(ImageDraw.Draw(mask), body, radius, 255)
    img.paste(grad, (0, 0), mask)

    # soft inner edge highlight
    edge = ImageDraw.Draw(img)
    edge.rounded_rectangle(body, radius=radius, outline=(255, 255, 255, 28), width=SS * 3)

    # 4x4 pad grid
    pad_area = body_w * 0.70
    gap = pad_area * 0.055
    pad = (pad_area - 3 * gap) / 4
    x0 = inset + (body_w - pad_area) / 2
    y0 = inset + (body_w - pad_area) / 2
    pad_r = pad * 0.22

    boxes = {}
    for r in range(4):
        for c in range(4):
            x = x0 + c * (pad + gap)
            y = y0 + r * (pad + gap)
            boxes[(r, c)] = (x, y, x + pad, y + pad)

    for key, box in boxes.items():
        if key in LIT:
            glow(img, box, pad_r, LIT[key], pad * 0.30)

    d = ImageDraw.Draw(img)
    for key, box in boxes.items():
        colour = LIT.get(key)
        if colour:
            rounded(d, box, pad_r, colour)
            hi = (box[0] + pad * 0.08, box[1] + pad * 0.08, box[2] - pad * 0.08, box[1] + pad * 0.42)
            rounded(d, hi, pad_r * 0.7, tuple(min(255, v + 55) for v in colour) + (110,))
        else:
            rounded(d, box, pad_r, PAD_OFF)
            d.rounded_rectangle(box, radius=pad_r, outline=PAD_EDGE, width=SS * 2)

    return img.resize((SIZE, SIZE), Image.LANCZOS)


def write_icns(png: Image.Image, out: Path) -> None:
    if not shutil.which("iconutil"):
        print("iconutil not found, skipping .icns")
        return
    iconset = out.with_suffix(".iconset")
    iconset.mkdir(exist_ok=True)
    for base in (16, 32, 128, 256, 512):
        png.resize((base, base), Image.LANCZOS).save(iconset / f"icon_{base}x{base}.png")
        png.resize((base * 2, base * 2), Image.LANCZOS).save(iconset / f"icon_{base}x{base}@2x.png")
    subprocess.run(["iconutil", "-c", "icns", str(iconset), "-o", str(out)], check=True)
    shutil.rmtree(iconset)


def main() -> None:
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "surface" / "icon"
    out_dir.mkdir(parents=True, exist_ok=True)
    icon = draw_icon()
    icon.save(out_dir / "KickSurface.png")
    write_icns(icon, out_dir / "KickSurface.icns")
    print(f"wrote {out_dir}/KickSurface.png and .icns")


if __name__ == "__main__":
    main()
