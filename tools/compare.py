"""Compare a UI snapshot against a reference shot.

Writes <out>_side.png (reference | snapshot), <out>_diff.png (amplified abs diff) and prints
the mean absolute error overall and per named region.

usage: python tools/compare.py snapshot.png reference.png out_prefix [x0,y0,x1,y1]
"""
import sys
import numpy as np
from PIL import Image

REGIONS = {
    "toolbar": (0, 0, 2396, 60),
    "input": (80, 200, 490, 640),
    "output": (545, 200, 960, 640),
    "att_rel": (1030, 125, 1245, 705),
    "ratio": (1340, 125, 1485, 625),
    "vu": (1555, 115, 2115, 570),
    "logo": (1690, 595, 1975, 695),
    "meter_btn": (2165, 125, 2396, 625),
    "bottom": (0, 712, 2396, 865),
}


def main():
    snap = Image.open(sys.argv[1]).convert("RGB")
    ref = Image.open(sys.argv[2]).convert("RGB")
    h = min(snap.height, ref.height)
    w = min(snap.width, ref.width)
    snap, ref = snap.crop((0, 0, w, h)), ref.crop((0, 0, w, h))
    if len(sys.argv) > 4:
        box = tuple(map(int, sys.argv[4].split(",")))
        snap, ref = snap.crop(box), ref.crop(box)

    a, b = np.asarray(snap).astype(int), np.asarray(ref).astype(int)
    d = np.abs(a - b).mean(axis=2)
    print(f"overall MAE {d.mean():.2f}")
    if len(sys.argv) <= 4:
        for k, (x0, y0, x1, y1) in REGIONS.items():
            print(f"  {k:10s} {d[y0:y1, x0:x1].mean():6.2f}")

    side = Image.new("RGB", (snap.width * 2, snap.height))
    side.paste(ref, (0, 0))
    side.paste(snap, (snap.width, 0))
    side.save(sys.argv[3] + "_side.png")
    Image.fromarray(np.clip(d * 4, 0, 255).astype(np.uint8)).save(sys.argv[3] + "_diff.png")


if __name__ == "__main__":
    main()
