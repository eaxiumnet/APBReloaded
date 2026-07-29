"""Regenerate Financial Block09 light placements from REAL extracted light data.

Companion to regenerate_financial_placements.py. Each light carries the genuine
Location/Rotation read from the UE3 package via extract_actor_lights, plus real
component parameter overrides (Radius/Brightness/FalloffExponent/OuterConeAngle/
LightColor) where the package serialized them. Lights that override nothing inherit
APB-typical archetype defaults, which remain flagged per field via ``is_default`` so no
default value is ever presented as an extracted one.

Emitted to a NEW file alongside the placement manifest.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_actor_lights as ext  # noqa: E402

MAPS = Path(
    r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
    r"\APBGame\Content\FinancialDistrict\Maps"
)
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements\Financial_Block09_lights.json")


def main() -> int:
    recs = ext.extract_block_lights("FinancialDistrict_Block09", MAPS)

    lights: list[dict] = []
    seen: set[tuple] = set()
    for r in recs:
        key = (
            round(r["location"][0], 1),
            round(r["location"][1], 1),
            round(r["location"][2], 1),
            r["light_class"],
        )
        if key in seen:
            continue
        seen.add(key)
        lights.append(r)

    real_radius = sum(1 for r in lights if not r["is_default"]["radius"])
    real_bri = sum(1 for r in lights if not r["is_default"]["brightness"])
    real_cone = sum(
        1 for r in lights if r["light_type"] == "spot" and not r["is_default"].get("outer_cone", True)
    )
    n_point = sum(1 for r in lights if r["light_type"] == "point")
    n_spot = sum(1 for r in lights if r["light_type"] == "spot")

    manifest = {
        "district_id": "Financial",
        "source_package": "FinancialDistrict_Block09",
        "source_packages": ext._block_family("FinancialDistrict_Block09", MAPS),
        "layout": "real_ue3_light_actors",
        "layout_note": (
            "Real Location/Rotation(URU->deg) from UE3 light actors + real component "
            "param overrides where present. Fields with is_default=true inherit "
            "APB-typical archetype defaults (radius=1024, brightness=1, falloff=2, "
            "outer_cone=44, color=white); these are NOT extracted values."
        ),
        "light_count": len(lights),
        "point_count": n_point,
        "spot_count": n_spot,
        "real_override_counts": {
            "radius": real_radius,
            "brightness": real_bri,
            "outer_cone": real_cone,
        },
        "lights": lights,
    }
    OUT.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(
        f"WROTE {OUT} lights={len(lights)} point={n_point} spot={n_spot} "
        f"real_overrides(radius={real_radius} brightness={real_bri} cone={real_cone})"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
