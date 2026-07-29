#!/usr/bin/env python3
"""S1 contract test: extract_actor_transforms must yield REAL Block09 actor transforms.

Guards against the fabricated-placement regression where rotations were synthesized
as ``(i * 11) % 360`` from array index, scale hardcoded ``[1,1,1]``, and mesh names
guessed. This test asserts the extractor reads genuine tagged-property data from the
decompressed UE3 package.

Financial Block09's name table contains Location/Rotation/StaticMesh but NOT
DrawScale3D or PrePivot, so scale/pre_pivot are treated as OPTIONAL here.

Convention note: co-located in tools/scripts/ (not tests/) to match the existing
python test harness (test_document_2011_login_pipeline.py) which imports its sibling
module directly; tests/ holds only the C++ domain harness with no python import path.
"""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import extract_actor_transforms as ext  # noqa: E402

MAPS = Path(
    r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America"
    r"\APBGame\Content\FinancialDistrict\Maps"
)
BLOCK_STEM = "FinancialDistrict_Block09"


class Block09TransformExtraction(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = ext.extract_block(BLOCK_STEM, MAPS)

    def test_yields_actors(self):
        self.assertGreaterEqual(
            len(self.rows), 50, f"expected >=50 building actors, got {len(self.rows)}"
        )

    def test_each_row_has_required_shape(self):
        for r in self.rows[:200]:
            self.assertIn("location", r)
            self.assertIn("rotation", r)
            self.assertIn("mesh_name", r)
            self.assertEqual(len(r["location"]), 3, f"bad location {r}")
            self.assertEqual(len(r["rotation"]), 3, f"bad rotation {r}")

    def test_rotations_are_not_fabricated_index_pattern(self):
        """The dominant defect: yaw[i] == (i*11)%360. Real data must NOT match this."""
        yaws = [round(r["rotation"][1], 3) for r in self.rows]
        matches = sum(1 for i, y in enumerate(yaws) if abs(y - float((i * 11) % 360)) < 1e-6)
        frac = matches / max(1, len(yaws))
        self.assertLess(
            frac,
            0.05,
            f"{matches}/{len(yaws)} yaws match fabricated (i*11)%360 pattern "
            f"({frac:.1%}) -- data is synthesized, not extracted",
        )

    def test_rotations_have_real_variety(self):
        yaws = {round(r["rotation"][1], 2) for r in self.rows}
        self.assertGreaterEqual(
            len(yaws), 20, f"only {len(yaws)} distinct yaws -- suspiciously uniform"
        )

    def test_locations_are_varied_and_in_world_range(self):
        locs = {tuple(round(v, 1) for v in r["location"]) for r in self.rows}
        self.assertGreaterEqual(
            len(locs), max(10, len(self.rows) // 2), f"only {len(locs)} distinct locations"
        )
        for r in self.rows[:200]:
            x, y, z = r["location"]
            self.assertTrue(
                any(abs(c) > 1000 for c in (x, y)),
                f"location {r['location']} not in world-space range for {r.get('actor')}",
            )

    def test_building_shell_meshes_resolve_in_package(self):
        """Building shells carry their mesh via StaticMeshComponent.StaticMesh (import
        ref) in-package. Measured by UNIQUE LOCATION: the shell population contains
        LOD/collision-proxy duplicates at identical positions, so per-actor counts
        understate coverage. Props/PrefabInstances reference meshes in SEPARATE prefab
        packages (proven: cProp carries no mesh ref, PrefabInstance.TemplatePrefab is
        an import) and are covered by a separate cross-package pass, not this gate.
        """
        shells = [r for r in self.rows if r["actor_class"] == "cStreamedBuildingActor"]
        self.assertGreater(len(shells), 0, "no building shells extracted")
        loc_covered = {}
        for r in shells:
            key = tuple(round(v, 1) for v in r["location"])
            loc_covered[key] = loc_covered.get(key, False) or bool(r["mesh_name"])
        covered = sum(1 for v in loc_covered.values() if v)
        frac = covered / max(1, len(loc_covered))
        self.assertGreaterEqual(
            frac,
            0.7,
            f"building-shell unique-location mesh coverage {covered}/{len(loc_covered)} "
            f"({frac:.1%}) -- StaticMeshComponent->StaticMesh import lookup regressed",
        )

    def test_resolved_meshes_come_from_real_source(self):
        """Every resolved mesh must carry a real extraction source, never a fabricated
        round-robin assignment. Guards the ``imported[i % len]`` defect regression.
        """
        for r in self.rows:
            if r["mesh_name"]:
                self.assertIn(
                    r["mesh_source"],
                    ("component", "direct", "compset"),
                    f"{r['actor']} has mesh {r['mesh_name']!r} from non-real "
                    f"source {r['mesh_source']!r}",
                )

    def test_scale_optional_but_valid_when_present(self):
        for r in self.rows[:200]:
            sc = r.get("scale")
            if sc is not None:
                self.assertEqual(len(sc), 3, f"bad scale {sc}")
                self.assertTrue(all(c != 0 for c in sc), f"zero scale component {sc}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
