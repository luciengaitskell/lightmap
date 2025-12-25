import numpy as np
import gpxpy
from rasterio.transform import from_bounds

OUT_W = 64
OUT_H = 64

# Fixed map square (set to False to auto-fit to GPX bounds instead)
USE_FIXED_BBOX = True
NORTH = 42.367502
WEST = -71.097639
SOUTH = 42.350859
EAST = -71.075271


def load_gpx_lonlat(path):
    with open(path, "r", encoding="utf-8") as f:
        gpx = gpxpy.parse(f)
    pts = []
    for trk in gpx.tracks:
        for seg in trk.segments:
            for p in seg.points:
                pts.append((p.longitude, p.latitude))  # lon, lat
    if not pts:
        raise ValueError("No track points found in GPX")
    return np.array(pts, dtype=np.float64)


def bounds_from_lonlat(lonlat):
    lons = lonlat[:, 0]
    lats = lonlat[:, 1]
    west, east = float(lons.min()), float(lons.max())
    south, north = float(lats.min()), float(lats.max())
    return north, south, east, west


def lonlat_to_pixels(lonlat, north, south, east, west, w, h):
    tr = from_bounds(west, south, east, north, w, h)
    # Affine: col=(x-c)/a ; row=(y-f)/e (e negative for north-up)
    cols = (lonlat[:, 0] - tr.c) / tr.a
    rows = (lonlat[:, 1] - tr.f) / tr.e
    cols = np.floor(cols).astype(int)
    rows = np.floor(rows).astype(int)

    # Sentinel values: -1 for west/north underflow, -2 for east/south overflow
    cols = np.where(cols < 0, -1, cols)
    cols = np.where(cols >= w, -2, cols)
    rows = np.where(rows < 0, -1, rows)
    rows = np.where(rows >= h, -2, rows)

    return np.column_stack([cols, rows]).astype(int)


def bresenham(x0, y0, x1, y1):
    # Returns every pixel coordinate along the segment
    dx = abs(x1 - x0)
    dy = -abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx + dy
    x, y = x0, y0
    out = []
    while True:
        out.append((x, y))
        if x == x1 and y == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy
            x += sx
        if e2 <= dx:
            err += dx
            y += sy
    return out


def iter_path_pixels(px_points):
    for i in range(len(px_points) - 1):
        x0, y0 = map(int, px_points[i])
        x1, y1 = map(int, px_points[i + 1])

        on0 = x0 >= 0 and y0 >= 0
        on1 = x1 >= 0 and y1 >= 0

        if on0 and on1:
            # Normal in-bounds segment
            yield from bresenham(x0, y0, x1, y1)
        else:
            # Off-grid involved: emit the endpoints only, avoid drawing across the map
            yield from ((x0, y0), (x1, y1))


def dedupe_consecutive(points):
    last = None
    for p in points:
        if p != last:
            yield p
            last = p


def main():
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("gpx", help="Input GPX file")
    args = ap.parse_args()

    lonlat = load_gpx_lonlat(args.gpx)

    if USE_FIXED_BBOX:
        north, south, east, west = NORTH, SOUTH, EAST, WEST
    else:
        north, south, east, west = bounds_from_lonlat(lonlat)

    px = lonlat_to_pixels(lonlat, north, south, east, west, OUT_W, OUT_H)

    count = 0
    print("pixels = {")
    for x, y in dedupe_consecutive(iter_path_pixels(px)):
        count += 1
        print(f"  {{{x}, {y}}},")
    print("}")

    print(f"length = {count}")


if __name__ == "__main__":
    main()
