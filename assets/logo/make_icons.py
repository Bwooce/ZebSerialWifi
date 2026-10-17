#!/usr/bin/env python3
"""Generate the bitmap icons the bridge serves next to /favicon.svg.

Safari on iOS ignores an SVG icon and asks for /favicon.ico and /apple-touch-icon.png instead, so the bridge
serves these two as well (firmware/main/icons/, embedded by firmware/main/CMakeLists.txt):

- favicon.ico: the mark at 16, 32 and 48 px, dark on transparent.
- apple-touch-icon.png: 180 px, the size iOS asks for, dark mark on white (iOS fills transparency with black).

Usage: python3 make_icons.py   (needs rsvg-convert from librsvg and Pillow)
Run it again after changing the mark with make_logo.py.
"""
import io
import os
import re
import subprocess

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "firmware", "main", "icons")
INK = "#222"


def mark_d():
    with open(os.path.join(HERE, "zebserial-mark.svg")) as fh:
        return re.search(r' d="([^"]+)"', fh.read()).group(1)


def render(svg, px):
    png = subprocess.run(["rsvg-convert", "-w", str(px), "-h", str(px)], input=svg.encode(),
                         capture_output=True, check=True).stdout
    return Image.open(io.BytesIO(png)).convert("RGBA")


def main():
    d = mark_d()
    os.makedirs(OUT, exist_ok=True)
    bare = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
            f'<path fill="{INK}" fill-rule="evenodd" d="{d}"/></svg>')
    sizes = [16, 32, 48]
    imgs = [render(bare, s) for s in sizes]
    ico = os.path.join(OUT, "favicon.ico")
    imgs[-1].save(ico, format="ICO", sizes=[(s, s) for s in sizes], append_images=imgs[:-1])
    # Touch icon: the mark at 70 % on white, since iOS rounds the corners and crops close to the edge
    touch = (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">'
             f'<rect width="64" height="64" fill="#fff"/>'
             f'<g transform="translate(32 32) scale(0.7) translate(-32 -32)">'
             f'<path fill="{INK}" fill-rule="evenodd" d="{d}"/></g></svg>')
    apple = os.path.join(OUT, "apple-touch-icon.png")
    render(touch, 180).convert("RGB").save(apple, format="PNG", optimize=True)
    print(f"favicon.ico {os.path.getsize(ico)} B, apple-touch-icon.png {os.path.getsize(apple)} B")


if __name__ == "__main__":
    main()
