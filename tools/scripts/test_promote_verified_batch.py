from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from promote_verified_batch import (
    LEDGER_DEFAULT,
    check_verified_entry,
    entry_is_allowlist_eligible,
    locator_test,
    promote,
)

ROOT = Path(__file__).resolve().parents[2]
BATCH_KEYS = {
    "retail:MaterialDatabase/AsylumDistrict/PropMaterials/PortableSpotlight.upk#PortableSpotlight01_LOD0",
    "retail:MaterialDatabase/Decals/LitterDecals.upk#LitterGroup_1",
    "retail:MaterialDatabase/Decals/LitterDecals.upk#LitterGroup_2",
    "retail:MaterialDatabase/Decals/LitterDecals.upk#LitterGroup_3",
    "retail:MaterialDatabase/Decals/LitterDecals.upk#LitterGroup_4",
    "retail:MaterialDatabase/Decals/LitterDecals.upk#LitterGroup_5",
    "retail:MaterialDatabase/FinancialDistrict/PropMaterials/FD_Lanterns.upk#FloodlightBase",
    "retail:MaterialDatabase/FinancialDistrict/PropMaterials/FD_Lanterns.upk#WoodenLanterns",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Bush_Healthy_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#LargeFoliage_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#LargeGrass_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#LargeRoundBush_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Plants_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Tree_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Tree_02",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Tree_Group_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Weed_01",
    "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Weed_Clump_01",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_PaperStack012_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_PaperStack01_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_PaperStack032_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_PaperStack03_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_Set1_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_Set2_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_Set3_LOD0",
    "retail:MaterialDatabase/GenericDistrict/Props/Enforcer_Office_Clutter.upk#Enforcer_OfficeCluttter_Set4_LOD0",
}


def unreconciled_ledger() -> dict:
    ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
    reconciler_fields = {
        "extractor",
        "extractor_version",
        "extractor_args",
        "intermediate_path",
        "intermediate_sha256",
        "conversion_settings",
        "validation",
        "d17_evidence",
        "d17_conversion_settings",
    }
    for entry in ledger.get("entries", []):
        for field in reconciler_fields:
            entry.pop(field, None)
    return ledger


def find_entry(ledger: dict, asset_key: str) -> dict:
    return next(entry for entry in ledger["entries"] if entry["asset_key"] == asset_key)


class LocatorTestTests(unittest.TestCase):
    def test_retail_steam_prefix(self) -> None:
        self.assertTrue(
            locator_test(
                "retail",
                "${retail_steam}/APBGame/Content/Release/Packages/MaterialDatabase/Foliage/Generic_Foliage.upk",
            )
        )

    def test_retail_extracted_prefix(self) -> None:
        self.assertTrue(locator_test("retail", "d:/apbreloaded/content/extracted/mesh.obj"))

    def test_2011_and_apbdb(self) -> None:
        self.assertTrue(locator_test("2011", r"C:\2011 apb\Client\APBGame\Content\Interface\x.upk"))
        self.assertTrue(locator_test("apbdb", "apbdb.com/vehicles/ntec"))

    def test_rejects_unknown_build_and_absolute_mismatch(self) -> None:
        self.assertFalse(locator_test("unknown", "anything"))
        self.assertFalse(locator_test("retail", "C:/Program Files/x.upk"))


class VerifiedEntryCheckTests(unittest.TestCase):
    def test_reconciled_payload_row_has_no_failures(self) -> None:
        ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
        key = "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Bush_Healthy_01"
        entry = find_entry(ledger, key)
        self.assertEqual(check_verified_entry(entry), [])

    def test_unreconciled_row_fails_evidence_checks(self) -> None:
        ledger = unreconciled_ledger()
        key = "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Bush_Healthy_01"
        entry = find_entry(ledger, key)
        failures = check_verified_entry(entry)
        self.assertTrue(any("verified_missing_d17_evidence" in f for f in failures))

    def test_corrupted_intermediate_hash_blocks_promotion(self) -> None:
        ledger = unreconciled_ledger()
        key = "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Bush_Healthy_01"
        entry = find_entry(ledger, key)
        promoted, report = promote(ledger)
        self.assertEqual(report["promoted_count"], 0)
        entry["status"] = "verified"
        entry["intermediate_sha256"] = "deadbeef"
        failures = check_verified_entry(entry)
        self.assertTrue(any("intermediate" in failure for failure in failures))


class EligibilityTests(unittest.TestCase):
    def test_payload_row_is_allowlist_eligible(self) -> None:
        ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
        key = "retail:MaterialDatabase/Foliage/Generic_Foliage.upk#Bush_Healthy_01"
        entry = find_entry(ledger, key)
        self.assertEqual(entry_is_allowlist_eligible(entry), [])

    def test_extraction_row_is_not_allowlist_eligible(self) -> None:
        ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
        key = "retail:Maps/FinancialDistrict/FinancialDistrict_ArtProps_Block16.APB"
        entry = find_entry(ledger, key)
        failures = entry_is_allowlist_eligible(entry)
        self.assertTrue(any("invalid_destination" in failure for failure in failures))


class BatchPromotionTests(unittest.TestCase):
    def test_real_ledger_batch_is_exactly_26(self) -> None:
        ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
        promoted, report = promote(ledger)
        self.assertEqual(report["promoted_count"], 26)
        self.assertEqual(set(report["promoted"]), BATCH_KEYS)
        self.assertEqual(report["already_verified_count"], 0)

    def test_promotion_preserves_other_rows(self) -> None:
        ledger = json.loads(LEDGER_DEFAULT.read_text(encoding="utf-8"))
        statuses_before = {entry["asset_key"]: entry.get("status") for entry in ledger["entries"]}
        promoted, _ = promote(ledger)
        for entry in promoted["entries"]:
            if entry["asset_key"] not in BATCH_KEYS:
                self.assertEqual(entry.get("status"), statuses_before[entry["asset_key"]])

    def test_idempotent_apply_via_cli(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temp_root = Path(directory)
            temp_ledger = temp_root / "import_ledger.json"
            temp_report = temp_root / "promotion_report.json"
            shutil.copyfile(LEDGER_DEFAULT, temp_ledger)
            first = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "scripts" / "promote_verified_batch.py"),
                    "--ledger",
                    str(temp_ledger),
                    "--report",
                    str(temp_report),
                    "--apply",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(first.returncode, 0, first.stderr)
            first_bytes = temp_ledger.read_bytes()
            updated = json.loads(temp_ledger.read_text(encoding="utf-8"))
            self.assertEqual(
                sum(1 for entry in updated["entries"] if entry.get("status") == "verified"),
                26,
            )
            second = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "tools" / "scripts" / "promote_verified_batch.py"),
                    "--ledger",
                    str(temp_ledger),
                    "--report",
                    str(temp_report),
                    "--apply",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(second.returncode, 0, second.stderr)
            self.assertEqual(first_bytes, temp_ledger.read_bytes())
            self.assertIn("already_verified=26", second.stdout)


if __name__ == "__main__":
    unittest.main()
