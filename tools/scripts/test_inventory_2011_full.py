#!/usr/bin/env python3
"""Tests for inventory_2011_full.py — real 2011 tree + shipped classifiers/header parse."""
from __future__ import annotations

import json
import struct
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))

import inventory_2011_full as inv  # noqa: E402

CONTENT = inv.CONTENT
OUT = inv.OUT_DIR
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-e4916a0f3e26\implementer")


class TestInventory2011Full(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not CONTENT.is_dir():
            raise unittest.SkipTest(f"2011 Content missing: {CONTENT}")

    def test_upk_header_is_547_31_on_live_packages(self):
        samples = [
            CONTENT / "Packages" / "Weapon_M16.upk",
            CONTENT / "Character" / "Contact" / "Contact_Bloodrose.upk",
            CONTENT / "DesignObjects" / "Weapons" / "Weapon_AssaultRifle.upk",
        ]
        # vehicle: any V_ under APB_Vehicles
        veh_dir = CONTENT / "Packages" / "APB_Vehicles"
        if veh_dir.is_dir():
            any_v = next(veh_dir.rglob("V_*.upk"), None)
            if any_v:
                samples.append(any_v)
        found = 0
        for p in samples:
            if not p.is_file():
                continue
            h = inv.upk_header(p)
            self.assertIsNotNone(h, msg=f"header fail {p}")
            assert h is not None
            self.assertEqual(h, (547, 31), msg=f"{p} got {h}")
            found += 1
        self.assertGreaterEqual(found, 3, "need >=3 live 2011 UPK samples")

    def test_classify_weapon_character_vehicle_paths(self):
        self.assertEqual(
            inv.classify_upk("DesignObjects/Weapons/Weapon_AssaultRifle.upk"),
            "weapons",
        )
        self.assertEqual(
            inv.classify_upk("Character/Contact/Contact_Bloodrose.upk"),
            "characters",
        )
        self.assertEqual(
            inv.classify_upk("Packages/APB_Vehicles/V_A_2DrCoupe/V_A_2DrCoupe_Badge_1.upk"),
            "vehicles",
        )
        self.assertEqual(inv.classify_upk("Interface/APBMenus_FrontEnd.upk"), "interface_ui")
        self.assertEqual(inv.classify_upk("MaterialDatabase/GolemobileMaterials.upk"), "materials")
        self.assertEqual(inv.classify_upk("VFX/Character/VFX_Character.upk"), "vfx")
        self.assertEqual(inv.classify_upk("Anim/Weapon/Animation_Weapon.upk"), "animations")

    def test_content_scale_spot_check(self):
        """Spot-check live tree still has thousands of UPKs + ItemAssets bins + districts."""
        upk = sum(1 for _ in CONTENT.rglob("*.upk"))
        self.assertGreaterEqual(upk, 4000, f"upk count too low: {upk}")
        bins = sum(1 for _ in (CONTENT / "ItemAssets").rglob("*.bin"))
        self.assertGreaterEqual(bins, 1000, f"ItemAssets bins: {bins}")
        fin = CONTENT / "FinancialDistrict"
        wat = CONTENT / "WaterfrontDistrict"
        self.assertTrue(fin.is_dir() and any(fin.rglob("*.apb")))
        self.assertTrue(wat.is_dir() and any(wat.rglob("*.apb")))
        # audio dump exists from prior goal
        audio = ROOT / "Content" / "Extracted" / "Audio" / "2011"
        self.assertTrue(audio.is_dir(), "2011 audio dump missing")
        self.assertGreater(sum(1 for _ in audio.rglob("*.wav")), 1000)

    def test_report_artifacts_exist_with_required_sections(self):
        md = OUT / "EXTRACTABILITY_INVENTORY.md"
        js = OUT / "inventory_2011_full.json"
        self.assertTrue(md.is_file(), f"missing report {md}")
        self.assertTrue(js.is_file(), f"missing json {js}")
        text = md.read_text(encoding="utf-8")
        lower = text.lower()
        for needle in (
            "package version fact",
            "547",
            "31",
            "564",
            "33",
            "upk families",
            "priority extract matrix",
            "representative umodel probes",
            "weapons",
            "characters",
            "vehicles",
            "normal",
        ):
            self.assertIn(needle, lower)
        self.assertIn("## Package version fact", text)
        self.assertIn("## Priority extract matrix", text)
        self.assertIn("## Representative umodel probes", text)
        data = json.loads(js.read_text(encoding="utf-8"))
        self.assertEqual(data.get("expected_version"), "547/31")
        self.assertIn("547/31", data.get("version_hist", {}))
        self.assertGreaterEqual(data.get("content_upk_count", 0), 4000)
        fam = data.get("families") or {}
        self.assertGreater(fam.get("vehicles", {}).get("count", 0), 1000)
        self.assertGreater(fam.get("characters", {}).get("count", 0), 200)
        self.assertGreater(fam.get("weapons", {}).get("count", 0), 20)

    def test_probe_evidence_three_families(self):
        probes_path = SCRATCH / "probe_results.json"
        self.assertTrue(probes_path.is_file(), f"missing {probes_path}; run inventory with --probe")
        probes = json.loads(probes_path.read_text(encoding="utf-8"))
        labels = {p.get("label") for p in probes}
        for need in ("weapons", "characters", "vehicles"):
            self.assertIn(need, labels, f"missing probe family {need}")
        oks = [p for p in probes if p.get("ok")]
        self.assertGreaterEqual(len(oks), 3, f"expected >=3 successful probes, got {probes}")
        for p in probes:
            if p.get("ok"):
                log = p.get("log_file")
                self.assertTrue(log and Path(log).is_file(), f"probe log missing for {p.get('package')}")

    def test_pick_probes_resolves_real_files(self):
        specs = inv.pick_probes(CONTENT)
        self.assertGreaterEqual(len(specs), 3)
        families = {s[2] for s in specs}
        self.assertTrue({"weapons", "characters", "vehicles"} <= families)
        for name, root, _fam in specs:
            hits = list(root.rglob(f"{name}.upk"))
            self.assertTrue(hits, f"probe package not found: {name} under {root}")


if __name__ == "__main__":
    unittest.main()
