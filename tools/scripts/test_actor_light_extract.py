#!/usr/bin/env python3
"""W3 contract test: extract_actor_lights must yield REAL Block09 light placements.

Mirrors the transform-extraction contract (test_actor_transform_extract.py). The
Financial Block09 family carries 460 light actors (PointLight, PointNightLight,
SpotLight, SpotNightLight); every actor serializes a real Location + Rotation and a
LightComponent object ref. Per-instance param overrides (Brightness/Radius/
OuterConeAngle/FalloffExponent/LightColor) are SPARSE -- most lights inherit archetype
defaults -- so those fields are asserted as optional-but-real-when-present, and any
applied default MUST be flagged (is_default) rather than passed off as extracted.

Guards against fabrication: no synthesized index-pattern positions, real world-space
coordinates, and no unflagged invented light parameters.

Co-located in tools/scripts/ to match the existing python test harness import path.
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_actor_lights as ext  # noqa: E402

MAPS = Path(
    r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
    r"\APBGame\Content\FinancialDistrict\Maps"
)
BLOCK_STEM = "FinancialDistrict_Block09"

POINT_CLASSES = {"PointLight", "PointNightLight"}
SPOT_CLASSES = {"SpotLight", "SpotNightLight"}


class Block09LightExtraction(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = ext.extract_block_lights(BLOCK_STEM, MAPS)

    def test_yields_expected_light_count(self):
        self.assertGreaterEqual(
            len(self.rows), 400, f"expected >=400 light actors, got {len(self.rows)}"
        )

    def test_each_row_has_required_shape(self):
        for r in self.rows[:300]:
            for key in ("actor", "light_class", "light_type", "location",
                        "rotation", "radius", "brightness", "color"):
                self.assertIn(key, r, f"missing {key} in {r}")
            self.assertEqual(len(r["location"]), 3, f"bad location {r}")
            self.assertEqual(len(r["rotation"]), 3, f"bad rotation {r}")
            self.assertEqual(len(r["color"]), 3, f"color must be RGB triple {r}")

    def test_light_type_classified_point_or_spot(self):
        for r in self.rows:
            if r["light_class"] in POINT_CLASSES:
                self.assertEqual(r["light_type"], "point", f"{r['actor']} misclassified")
            elif r["light_class"] in SPOT_CLASSES:
                self.assertEqual(r["light_type"], "spot", f"{r['actor']} misclassified")
            else:
                self.fail(f"unknown light_class {r['light_class']!r}")

    def test_spot_lights_present(self):
        spots = [r for r in self.rows if r["light_type"] == "spot"]
        self.assertGreaterEqual(len(spots), 50, f"only {len(spots)} spot lights")

    def test_locations_are_varied_and_in_world_range(self):
        locs = {tuple(round(v, 1) for v in r["location"]) for r in self.rows}
        self.assertGreaterEqual(
            len(locs), len(self.rows) // 2, f"only {len(locs)} distinct light locations"
        )
        for r in self.rows[:300]:
            x, y, _z = r["location"]
            self.assertTrue(
                any(abs(c) > 1000 for c in (x, y)),
                f"light {r['location']} not in world-space range for {r['actor']}",
            )

    def test_locations_not_fabricated_index_pattern(self):
        """Guard the (i*11)%360 / index-derived fabrication class: light X coords must
        not be a linear function of array index.
        """
        xs = [r["location"][0] for r in self.rows]
        linear = sum(1 for i, x in enumerate(xs) if abs(x - float(i)) < 1e-6)
        self.assertLess(linear, max(2, len(xs) // 20),
                        "light positions look index-derived, not extracted")

    def test_param_overrides_are_real_when_present(self):
        """Sparse per-instance overrides must be real values, and any inherited default
        MUST be flagged is_default -- never presented as extracted.
        """
        real_radius = 0
        for r in self.rows:
            self.assertIn("is_default", r, f"{r['actor']} missing is_default map")
            if not r["is_default"].get("radius", True):
                self.assertGreater(r["radius"], 0, f"{r['actor']} zero real radius")
                real_radius += 1
            if not r["is_default"].get("brightness", True):
                self.assertGreaterEqual(r["brightness"], 0, f"{r['actor']} bad brightness")
        self.assertGreaterEqual(
            real_radius, 50,
            f"only {real_radius} real radius overrides -- extractor not reading component",
        )

    def test_spot_cone_present_for_spots(self):
        spots = [r for r in self.rows if r["light_type"] == "spot"]
        with_cone = [r for r in spots if not r["is_default"].get("outer_cone", True)]
        self.assertGreaterEqual(
            len(with_cone), 30,
            f"only {len(with_cone)} spots carry real OuterConeAngle override",
        )

    def test_color_is_valid_rgb(self):
        for r in self.rows[:300]:
            for c in r["color"]:
                self.assertTrue(0.0 <= c <= 1.0, f"color channel out of range {r['color']}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
