"""Extract REAL light-actor placements + param overrides from APB UE3 district packages.

Companion to extract_actor_transforms.py. The Financial Block09 family carries 461 light
actors across four classes:

    PointLight, PointNightLight   -> UE point lights
    SpotLight,  SpotNightLight    -> UE spot lights

Each actor serializes a genuine Location + Rotation and a ``LightComponent`` object
reference. Per-instance parameter overrides live on that component and are SPARSE:
type-verified counts across the family are radius=62, brightness=70, outer_cone=69,
falloff=43, color=0. Every other light inherits its parameters from an archetype whose
package is not imported (same reality as the props in extract_actor_transforms).

This extractor therefore reads REAL Location/Rotation for all actors and REAL component
overrides where present, and fills the remainder with APB-typical archetype defaults that
are explicitly flagged per field via ``is_default`` -- defaults are never presented as
extracted values.

Reuses the proven decompress -> umodel-list -> tagged-prop chain from
extract_actor_transforms; see that module's docstring for the format details.
"""
from __future__ import annotations

import struct
from pathlib import Path

from extract_actor_transforms import (
    _block_family,
    decompress_package,
    parse_package,
    scan_prop,
    umodel_list,
    uru_to_deg,
)

LIGHT_ACTOR_CLASSES = ("PointLight", "PointNightLight", "SpotLight", "SpotNightLight")
POINT_CLASSES = {"PointLight", "PointNightLight"}
SPOT_CLASSES = {"SpotLight", "SpotNightLight"}

# APB/UE3 archetype-typical fallbacks for lights that override nothing in-package.
# Flagged is_default=True per field so consumers know these are inherited, not extracted.
DEFAULT_RADIUS = 1024.0
DEFAULT_BRIGHTNESS = 1.0
DEFAULT_FALLOFF = 2.0
DEFAULT_OUTER_CONE = 44.0
DEFAULT_INNER_CONE = 0.0
DEFAULT_COLOR = [1.0, 1.0, 1.0]


def scan_color(blob: bytes, names: list[str], target_idx: int | None) -> list[float] | None:
    """Decode a StructProperty(Color) tagged prop to normalized RGB, or None if absent.

    UE3 FColor serializes as BGRA bytes after the 8-byte struct-name header.
    """
    if target_idx is None:
        return None
    for p in range(0, max(0, len(blob) - 24)):
        if struct.unpack_from("<i", blob, p)[0] != target_idx:
            continue
        ti = struct.unpack_from("<i", blob, p + 8)[0]
        if not (0 <= ti < len(names)) or names[ti] != "StructProperty":
            continue
        si = struct.unpack_from("<i", blob, p + 24)[0]
        if not (0 <= si < len(names)) or names[si] != "Color":
            continue
        payload = p + 24 + 8
        if payload + 4 > len(blob):
            return None
        b, g, r, _a = struct.unpack_from("<4B", blob, payload)
        return [round(r / 255.0, 4), round(g / 255.0, 4), round(b / 255.0, 4)]
    return None


def _read_component_params(cblob: bytes, names: list[str], ni: dict[str, int],
                           is_spot: bool) -> tuple[dict, dict]:
    """Return (values, is_default) for a light component's parameters."""
    values: dict = {}
    is_default: dict = {}

    def read_float(key_name: str, default: float, flag: str) -> None:
        v = scan_prop(cblob, names, ni.get(key_name))
        if v and v[0] == "FloatProperty":
            values[flag] = round(float(v[1]), 4)
            is_default[flag] = False
        else:
            values[flag] = default
            is_default[flag] = True

    read_float("Radius", DEFAULT_RADIUS, "radius")
    read_float("Brightness", DEFAULT_BRIGHTNESS, "brightness")
    read_float("FalloffExponent", DEFAULT_FALLOFF, "falloff")

    color = scan_color(cblob, names, ni.get("LightColor"))
    if color:
        values["color"] = color
        is_default["color"] = False
    else:
        values["color"] = list(DEFAULT_COLOR)
        is_default["color"] = True

    if is_spot:
        read_float("OuterConeAngle", DEFAULT_OUTER_CONE, "outer_cone")
        read_float("InnerConeAngle", DEFAULT_INNER_CONE, "inner_cone")

    return values, is_default


def extract_package_lights(stem: str, maps_dir: Path) -> list[dict]:
    """Extract every light actor's real transform + component param overrides."""
    flat = decompress_package(stem, maps_dir)
    data = flat.read_bytes()
    pkg = parse_package(data)
    rows = umodel_list(stem, maps_dir)
    ni = pkg.name_index
    loc_i, rot_i, lc_i = ni.get("Location"), ni.get("Rotation"), ni.get("LightComponent")

    out: list[dict] = []
    for row in rows:
        if row["cls"] not in LIGHT_ACTOR_CLASSES:
            continue
        if row["off"] + row["size"] > len(data):
            continue
        blob = data[row["off"] : row["off"] + row["size"]]
        loc = scan_prop(blob, pkg.names, loc_i)
        if not (loc and loc[0] == "Vector"):
            continue
        location = [round(v, 3) for v in loc[1]]

        rot = scan_prop(blob, pkg.names, rot_i)
        if rot and rot[0] == "Rotator":
            pitch, yaw, roll = rot[1]
            rotation = [uru_to_deg(pitch), uru_to_deg(yaw), uru_to_deg(roll)]
        else:
            rotation = [0.0, 0.0, 0.0]

        is_spot = row["cls"] in SPOT_CLASSES
        values: dict
        is_default: dict
        lc = scan_prop(blob, pkg.names, lc_i)
        if lc and lc[0] == "Object" and lc[1] > 0 and lc[1] - 1 < len(rows):
            crow = rows[lc[1] - 1]
            cblob = data[crow["off"] : crow["off"] + crow["size"]]
            values, is_default = _read_component_params(cblob, pkg.names, ni, is_spot)
        else:
            values, is_default = _read_component_params(b"", pkg.names, ni, is_spot)

        rec = {
            "actor": row["name"],
            "light_class": row["cls"],
            "light_type": "spot" if is_spot else "point",
            "package": stem,
            "location": location,
            "rotation": rotation,
            "radius": values["radius"],
            "brightness": values["brightness"],
            "falloff": values["falloff"],
            "color": values["color"],
            "is_default": is_default,
        }
        if is_spot:
            rec["outer_cone"] = values["outer_cone"]
            rec["inner_cone"] = values["inner_cone"]
        out.append(rec)
    return out


def extract_block_lights(block_stem: str, maps_dir: Path) -> list[dict]:
    """Aggregate real light placements across a block's whole package family."""
    rows: list[dict] = []
    for stem in _block_family(block_stem, maps_dir):
        try:
            rows.extend(extract_package_lights(stem, maps_dir))
        except (FileNotFoundError, RuntimeError, ValueError):
            continue
    return rows


if __name__ == "__main__":
    import collections
    import json
    import sys

    stem = sys.argv[1] if len(sys.argv) > 1 else "FinancialDistrict_Block09"
    maps = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(
        r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
        r"\APBGame\Content\FinancialDistrict\Maps"
    )
    recs = extract_block_lights(stem, maps)
    types = collections.Counter(r["light_type"] for r in recs)
    classes = collections.Counter(r["light_class"] for r in recs)
    real_radius = sum(1 for r in recs if not r["is_default"]["radius"])
    real_bri = sum(1 for r in recs if not r["is_default"]["brightness"])
    real_cone = sum(1 for r in recs if r["light_type"] == "spot" and not r["is_default"]["outer_cone"])
    print(f"lights={len(recs)} types={dict(types)} classes={dict(classes)}")
    print(f"real_overrides: radius={real_radius} brightness={real_bri} outer_cone={real_cone}")
    for r in recs[:4]:
        print(json.dumps(r))
