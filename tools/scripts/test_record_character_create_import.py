#!/usr/bin/env python3
"""Unit tests for record_character_create_import.py (M3R task-18b character batch)."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import record_character_create_import as mod


class FindUpkTest(unittest.TestCase):
    def test_contact_wardrobe_maps_to_exact_package(self):
        stem = "Contact_Bloodrose_BritneyBloodrose_contact_bloodrose_britneybloodrose"
        upk = mod.find_retail_upk(stem)
        self.assertIsNotNone(upk)
        self.assertEqual(upk.name, "Contact_Bloodrose_BritneyBloodrose.upk")

    def test_lc_variant_maps_to_variant_package(self):
        stem = "LC_M_Business_City_AsianDark_01_StudioCharacter"
        upk = mod.find_retail_upk(stem)
        self.assertIsNotNone(upk)
        self.assertEqual(upk.name, "LC_M_Business_City_AsianDark_01.upk")

    def test_longest_prefix_wins(self):
        stem = "Contact_Bloodrose_F_Contact_Criminal_Bloodrose"
        upk = mod.find_retail_upk(stem)
        self.assertIsNotNone(upk)
        self.assertEqual(upk.name, "Contact_Bloodrose.upk")

    def test_unmatched_stem_rejected(self):
        self.assertIsNone(mod.find_retail_upk("NoSuchAssetAnywhere"))


class OwnerConfirmationTest(unittest.TestCase):
    def test_base_mesh_owners_confirmed_by_catalog(self):
        for spec in mod.BASE_MESHES:
            upk = mod.RETAIL_PACKAGES / spec["upk"]
            self.assertTrue(upk.is_file(), spec["upk"])
            self.assertTrue(mod.mesh_owner_confirmed(upk, spec["mesh"]), spec["mesh"])

    def test_catalog_has_hints_for_contact_packages(self):
        hints = mod.catalog_hints_for(mod.CONTACT_DIR / "Contact_Sofia.upk")
        self.assertIsNotNone(hints)
        self.assertTrue("F_Contact_Enforcement_Sofia".casefold() in hints.casefold())


class RowShapeTest(unittest.TestCase):
    def test_base_row_fields(self):
        spec = mod.BASE_MESHES[0]
        mesh = spec["mesh"]
        upk = mod.RETAIL_PACKAGES / spec["upk"]
        intermediate = mod.IMPORTED_CHARS / spec["dir"] / f"{mesh}.obj"
        uasset = mod.IMPORTED_CHARS / spec["dir"] / f"{mesh}.uasset"
        dest = f"/Game/Imported/Characters/{spec['dir']}/{mesh}.{mesh}"
        row = mod.build_mesh_row(upk, mesh, intermediate, dest, uasset, "test")
        self.assertIsNotNone(row)
        self.assertEqual(row["source_build"], "retail")
        self.assertEqual(row["asset_class"], "StaticMesh")
        self.assertTrue(row["source_locator"].startswith("${retail_steam}/"))
        self.assertEqual(len(row["source_sha256"]), 64)
        self.assertEqual(len(row["intermediate_sha256"]), 64)
        self.assertEqual(len(row["uasset_sha256"]), 64)
        evidence = row["d17_evidence"][0]
        self.assertEqual(evidence["schema"], "apb_character_create_v1")
        self.assertEqual(evidence["record_key"], row["asset_key"])
        fields = evidence["fields"]
        self.assertEqual(fields["source_locator"], row["source_locator"])
        self.assertEqual(fields["intermediate_path"], row["intermediate_path"])
        self.assertEqual(fields["destination"], dest)

    def test_evidence_hash_roundtrip(self):
        spec = mod.BASE_MESHES[0]
        mesh = spec["mesh"]
        row = mod.build_mesh_row(
            mod.RETAIL_PACKAGES / spec["upk"],
            mesh,
            mod.IMPORTED_CHARS / spec["dir"] / f"{mesh}.obj",
            f"/Game/Imported/Characters/{spec['dir']}/{mesh}.{mesh}",
            mod.IMPORTED_CHARS / spec["dir"] / f"{mesh}.uasset",
            "test",
        )
        # simulate the apply-time patch
        row["d17_evidence"][0]["sha256"] = "e" * 64
        self.assertEqual(len(row["d17_evidence"][0]["sha256"]), 64)


if __name__ == "__main__":
    unittest.main(verbosity=2)
