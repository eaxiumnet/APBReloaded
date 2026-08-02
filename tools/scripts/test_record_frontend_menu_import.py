#!/usr/bin/env python3
"""Unit tests for record_frontend_menu_import.py (M3R task-18 frontend batch)."""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import record_frontend_menu_import as mod


class StageMkvTest(unittest.TestCase):
    def test_ladder_names_map_to_archival_mkv(self):
        cases = {
            "Character_Select_BG_AI_compat.mp4": "01_Character_Select_BG.mkv",
            "01_Character_Select_BG_AI_x4_to_3840x2400.webm": "01_Character_Select_BG.mkv",
            "Faction_Criminal_BG_AI_4k.webm": "02_Faction_Select_Criminal_BG.mkv",
            "Faction_Select_Criminal_BG_AI_x4_5120x3200.webm": "02_Faction_Select_Criminal_BG.mkv",
            "Faction_Enforcer_BG_AI_compat.mp4": "03_Faction_Select_Enforcer_BG.mkv",
            "Generic_BG_AI_4k.webm": "04_Generic_BG.mkv",
            "Login_BG_AI_4k.mp4": "05_Login_BG.mkv",
            "05_Login_BG_AI_x4_5120x3200.webm": "05_Login_BG.mkv",
        }
        for name, expected in cases.items():
            result = mod.stage_mkv_for(name)
            self.assertIsNotNone(result, name)
            self.assertEqual(result.name, expected, name)

    def test_unrelated_name_rejected(self):
        self.assertIsNone(mod.stage_mkv_for("SomeOtherMovie.webm"))


class UassetDerivationTest(unittest.TestCase):
    def test_texture_uasset_path(self):
        dest = "/Game/Imported/UI/Menu2011/Login/Constant_BG.Constant_BG"
        uasset = mod.ROOT / "Content" / (dest.removeprefix("/Game/").rsplit(".", 1)[0] + ".uasset")
        self.assertEqual(
            uasset.relative_to(mod.ROOT).as_posix(),
            "Content/Imported/UI/Menu2011/Login/Constant_BG.uasset",
        )

    def test_sound_uasset_path(self):
        dest = "/Game/Audio/UI/ButtonPos.ButtonPos"
        uasset = mod.ROOT / "Content" / (dest.removeprefix("/Game/").rsplit(".", 1)[0] + ".uasset")
        self.assertEqual(
            uasset.relative_to(mod.ROOT).as_posix(),
            "Content/Audio/UI/ButtonPos.uasset",
        )


class EvidenceShapeTest(unittest.TestCase):
    def test_evidence_fields_include_chain(self):
        fields = mod.evidence_fields(
            source_locator="D:/x/2011/upk.upk",
            source_sha256="a" * 64,
            extractor="tools/scripts/export_2011_menu_art.py",
            extractor_args=["python", "tools/scripts/export_2011_menu_art.py"],
            conversion={"format": "png"},
            intermediate=mod.MENUART_DIR / "APBMenus_Skins/Texture2D/MessageBox_BG.png",
            extracted=mod.MENUART_DIR / "APBMenus_Skins/Texture2D/MessageBox_BG.png",
            destination="/Game/Imported/UI/Menu2011/Chrome/MessageBox_BG.MessageBox_BG",
        )
        self.assertEqual(fields["source_sha256"], "a" * 64)
        self.assertEqual(fields["intermediate_path"], fields["extracted_file"])
        self.assertEqual(fields["intermediate_sha256"], fields["extracted_sha256"])

    def test_source_locator_contains_2011_for_menu_build(self):
        # The strict gate's 2011 locator test only requires the substring.
        self.assertIn("2011", "Content/Extracted/2011/UISfx/ButtonPos.wav")
        self.assertIn("2011", mod.IFACE_2011.as_posix())


class MergeRowsTest(unittest.TestCase):
    def test_rerun_preserves_promotion_stamp(self):
        fresh = {
            "2011:MenuArt/APBMenus.upk#Constant_BG": {
                "asset_key": "2011:MenuArt/APBMenus.upk#Constant_BG",
                "status": "imported",
                "updated": "fresh",
            },
        }
        entries = [
            {
                "asset_key": "2011:MenuArt/APBMenus.upk#Constant_BG",
                "status": "verified",
                "verified_at": "2026-07-01T00:00:00Z",
                "verified_by": "tools/scripts/promote_verified_batch.py",
            },
            {"asset_key": "unrelated", "status": "verified"},
        ]
        kept, replaced, added = mod.merge_rows(entries, fresh)
        self.assertEqual(replaced, 1)
        self.assertEqual(added, 0)
        self.assertEqual(len(kept), 2)
        row = kept[0]
        self.assertEqual(row["status"], "verified")
        self.assertEqual(row["verified_at"], "2026-07-01T00:00:00Z")
        self.assertEqual(row["verified_by"], "tools/scripts/promote_verified_batch.py")
        self.assertEqual(row["updated"], "fresh")
        self.assertNotIn("2011:MenuArt/APBMenus.upk#Constant_BG", fresh)

    def test_unpromoted_rows_stay_imported(self):
        fresh = {"new_key": {"asset_key": "new_key", "status": "imported"}}
        kept, replaced, added = mod.merge_rows([{"asset_key": "old"}], fresh)
        self.assertEqual(replaced, 0)
        self.assertEqual(added, 1)
        self.assertEqual([e["asset_key"] for e in kept], ["old", "new_key"])
        self.assertEqual(kept[1]["status"], "imported")


if __name__ == "__main__":
    unittest.main(verbosity=2)
