import numpy as np
import osmnx as ox
from shapely.geometry import box as shapely_box
from rasterio.features import rasterize
from rasterio.transform import from_bounds

# --- Input bbox (NW and SE corners) ---
NW_LAT, NW_LON = 42.367502, -71.097639
SE_LAT, SE_LON = 42.350859, -71.075271

NORTH = max(NW_LAT, SE_LAT)
SOUTH = min(NW_LAT, SE_LAT)
WEST = min(NW_LON, SE_LON)
EAST = max(NW_LON, SE_LON)

OUT_W = 64
OUT_H = 64

TAGS_LIST = [
    {"natural": "water"},
]


def validate_bbox(north, south, east, west):
    if not (north > south):
        raise ValueError(f"Invalid bbox: north({north}) must be > south({south})")
    if not (east > west):
        raise ValueError(f"Invalid bbox: east({east}) must be > west({west})")
    # rough sanity range checks
    if not (-90 <= south <= 90 and -90 <= north <= 90):
        raise ValueError("Latitudes out of range")
    if not (-180 <= west <= 180 and -180 <= east <= 180):
        raise ValueError("Longitudes out of range")


def fetch_water_geoms(north, south, east, west):
    # left, bottom, right, top
    bbox = (west, south, east, north)
    geoms = []

    for tags in TAGS_LIST:
        gdf = ox.features.features_from_bbox(bbox=bbox, tags=dict(tags))
        if gdf is None or len(gdf) == 0:
            continue
        for geom in gdf.geometry:
            if geom is None:
                continue
            if geom.geom_type in ("Polygon", "MultiPolygon"):
                geoms.append(geom)

    return geoms


def rasterize_mask(geoms, north, south, east, west, h=64, w=64) -> np.ndarray:
    bbox_poly = shapely_box(west, south, east, north)
    clipped = []
    for g in geoms:
        gg = g.intersection(bbox_poly)
        if not gg.is_empty:
            clipped.append(gg)

    transform = from_bounds(west, south, east, north, w, h)
    mask = rasterize(
        [(g, 1) for g in clipped],
        out_shape=(h, w),
        transform=transform,
        fill=0,
        dtype=np.uint8,
        all_touched=False,
    )
    return mask


def main():
    validate_bbox(NORTH, SOUTH, EAST, WEST)

    ox.settings.cache_folder = ".cache/osmnx"
    ox.settings.log_console = True
    ox.settings.use_cache = True

    print(f"bbox=(N={NORTH}, S={SOUTH}, E={EAST}, W={WEST})")

    geoms = fetch_water_geoms(NORTH, SOUTH, EAST, WEST)
    mask = rasterize_mask(geoms, NORTH, SOUTH, EAST, WEST, OUT_H, OUT_W)

    # ASCII preview
    for y in range(mask.shape[0]):
        print("".join("█" if mask[y, x] else "·" for x in range(mask.shape[1])))


if __name__ == "__main__":
    main()
